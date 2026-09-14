// Copyright GMTCK CX.

#include "CXMRTuningWindowComponent.h"
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRVarjoInputComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SSpinBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CXMRTuning"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRTuningWindow, Log, All);

static FAutoConsoleCommandWithWorld GCXMRTuningWindow(
	TEXT("CXMR.Tuning"),
	TEXT("Open or close the tuning window."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			if (UCXMRTuningWindowComponent* Tuning = Pawn->FindComponentByClass<UCXMRTuningWindowComponent>())
			{
				Tuning->ToggleWindow();
			}
		}
	}));

namespace
{
	using FWeakTuning = TWeakObjectPtr<UCXMRTuningSubsystem>;

	float AsValue(bool bOn) { return bOn ? 1.0f : 0.0f; }

	/** The level-wide post-process volume the exposure row writes to. */
	APostProcessVolume* FindUnboundVolume(UWorld* World)
	{
		if (World)
		{
			for (TActorIterator<APostProcessVolume> It(World); It; ++It)
			{
				if (It->bUnbound)
				{
					return *It;
				}
			}
		}
		return nullptr;
	}

	TSharedRef<SWidget> MakeRow(const FWeakTuning& Weak, const FCXMRTunable& Tunable)
	{
		const FName Id = Tunable.Id;
		TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);

		if (Tunable.Kind == ECXMRTunableKind::Action)
		{
			Row->AddSlot().FillWidth(1.0f)
			[
				SNew(SButton)
				.HAlign(HAlign_Center)
				.Text(Tunable.Label)
				.OnClicked_Lambda([Weak, Id]
				{
					if (Weak.IsValid()) { Weak->InvokeTunable(Id); }
					return FReply::Handled();
				})
			];
			return Row;
		}

		Row->AddSlot().FillWidth(1.0f).VAlign(VAlign_Center).Padding(0.0f, 0.0f, 8.0f, 0.0f)
		[
			SNew(STextBlock).Text(Tunable.Label)
		];

		switch (Tunable.Kind)
		{
		case ECXMRTunableKind::Bool:
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SCheckBox)
				.IsChecked_Lambda([Weak, Id]
				{
					return (Weak.IsValid() && Weak->GetTunableValue(Id) > 0.5f) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
				})
				.OnCheckStateChanged_Lambda([Weak, Id](ECheckBoxState State)
				{
					if (Weak.IsValid()) { Weak->ApplyValue(Id, State == ECheckBoxState::Checked ? 1.0f : 0.0f, true); }
				})
			];
			break;

		case ECXMRTunableKind::Float:
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(150.0f)
				[
					SNew(SSpinBox<float>)
					.MinValue(Tunable.Min).MaxValue(Tunable.Max)
					.MinSliderValue(Tunable.Min).MaxSliderValue(Tunable.Max)
					.Delta(Tunable.Delta)
					.Value_Lambda([Weak, Id] { return Weak.IsValid() ? Weak->GetTunableValue(Id) : 0.0f; })
					// Applied live while dragging, saved once the drag or the typed edit ends.
					.OnValueChanged_Lambda([Weak, Id](float Value) { if (Weak.IsValid()) { Weak->ApplyValue(Id, Value, false); } })
					.OnValueCommitted_Lambda([Weak, Id](float Value, ETextCommit::Type) { if (Weak.IsValid()) { Weak->ApplyValue(Id, Value, true); } })
					.OnEndSliderMovement_Lambda([Weak, Id](float Value) { if (Weak.IsValid()) { Weak->ApplyValue(Id, Value, true); } })
				]
			];
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(52.0f) [ SNew(STextBlock).Text(Tunable.Unit) ]
			];
			break;

		case ECXMRTunableKind::Choice:
		{
			const TArray<FText> Options = Tunable.Options;
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(150.0f)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text_Lambda([Weak, Id, Options]
					{
						const int32 Index = Weak.IsValid() ? FMath::RoundToInt(Weak->GetTunableValue(Id)) : 0;
						return Options.IsValidIndex(Index) ? Options[Index] : FText::GetEmpty();
					})
					.OnClicked_Lambda([Weak, Id, Count = Options.Num()]
					{
						if (Weak.IsValid() && Count > 0)
						{
							const int32 Index = FMath::RoundToInt(Weak->GetTunableValue(Id));
							Weak->ApplyValue(Id, static_cast<float>((Index + 1) % Count), true);
						}
						return FReply::Handled();
					})
				]
			];
			break;
		}

		case ECXMRTunableKind::Stepper:
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(SBox).WidthOverride(44.0f)
				[
					SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("Minus", "-"))
					.OnClicked_Lambda([Weak, Id] { if (Weak.IsValid()) { Weak->InvokeTunable(Id, -1.0f); } return FReply::Handled(); })
				]
			];
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(4.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SBox).WidthOverride(44.0f)
				[
					SNew(SButton).HAlign(HAlign_Center).Text(LOCTEXT("Plus", "+"))
					.OnClicked_Lambda([Weak, Id] { if (Weak.IsValid()) { Weak->InvokeTunable(Id, 1.0f); } return FReply::Handled(); })
				]
			];
			break;

		case ECXMRTunableKind::Readout:
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor(0.75f, 0.85f, 0.75f))
				.Text_Lambda([Weak, Id] { return Weak.IsValid() ? FText::FromString(Weak->GetTunableText(Id)) : FText::GetEmpty(); })
			];
			break;

		default:
			break;
		}

		if (Tunable.bPersist)
		{
			Row->AddSlot().AutoWidth().VAlign(VAlign_Center).Padding(6.0f, 0.0f, 0.0f, 0.0f)
			[
				SNew(SButton)
				.Text(LOCTEXT("Default", "Default"))
				.ToolTipText(LOCTEXT("DefaultTip", "Back to the default value, and forget the saved one"))
				.OnClicked_Lambda([Weak, Id] { if (Weak.IsValid()) { Weak->ResetToDefault(Id); } return FReply::Handled(); })
			];
		}
		return Row;
	}
}

UCXMRTuningWindowComponent::UCXMRTuningWindowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	WindowTitle = LOCTEXT("WindowTitle", "CXMR Tuning");
}

UCXMRTuningSubsystem* UCXMRTuningWindowComponent::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

void UCXMRTuningWindowComponent::BeginPlay()
{
	Super::BeginPlay();

	RegisterCoreTunables();

	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		// Features keep registering as their actors begin play; an open window follows along.
		TunablesChangedHandle = Tuning->OnTunablesChanged.AddUObject(this, &UCXMRTuningWindowComponent::RebuildContent);
	}

	if (bOpenOnBeginPlay)
	{
		OpenWindow();
	}
}

void UCXMRTuningWindowComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	// A Slate window is not garbage collected: without this it floats on the editor desktop after PIE stops.
	CloseWindow();

	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->OnTunablesChanged.Remove(TunablesChangedHandle);
		Tuning->UnregisterOwner(this);
	}
	Super::EndPlay(Reason);
}

void UCXMRTuningWindowComponent::RegisterCoreTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UCXMRSubsystem* CXMR = GI ? GI->GetSubsystem<UCXMRSubsystem>() : nullptr;
	if (!Tuning || !CXMR)
	{
		return;
	}

	const TWeakObjectPtr<UCXMRSubsystem> Weak(CXMR);
	auto Make = [this](FName Id, const FText& Category, const FText& Label, ECXMRTunableKind Kind)
	{
		FCXMRTunable Tunable;
		Tunable.Id = Id;
		Tunable.Category = Category;
		Tunable.Label = Label;
		Tunable.Kind = Kind;
		Tunable.Owner = this;
		return Tunable;
	};

	// ---- Mixed reality ----
	const FText MR = LOCTEXT("CatMR", "Mixed reality");
	{
		FCXMRTunable T = Make("MR.MixedReality", MR, LOCTEXT("MixedReality", "Mixed reality (passthrough)"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsMixedRealityOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetMixedReality(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("MR.VRBackground", MR, LOCTEXT("VRBackground", "VR background (sky, floor)"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsVRBackgroundVisible()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetVRBackgroundVisible(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("MR.Masking", MR, LOCTEXT("Masking", "Masking (mask meshes cut through)"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsMaskingOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetMasking(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("MR.ViewOffset", MR, LOCTEXT("ViewOffset", "View offset 0 eye / 1 cam"), ECXMRTunableKind::Float);
		T.Min = 0.0f; T.Max = 1.0f; T.Delta = 0.05f; T.Default = 1.0f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->GetViewOffset() : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetViewOffset(V); } };
		Tuning->Register(MoveTemp(T));
	}

	// ---- Depth ----
	const FText Depth = LOCTEXT("CatDepth", "Depth");
	{
		FCXMRTunable T = Make("Depth.Test", Depth, LOCTEXT("DepthTest", "Depth test (real in front of virtual)"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsDepthTestOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetDepthTest(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Depth.Range", Depth, LOCTEXT("DepthRange", "Limit depth test to range"), ECXMRTunableKind::Bool);
		T.Default = 1.0f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsDepthTestRangeOn()) : 0.0f; };
		T.Set = [Weak](float V)
		{
			if (Weak.IsValid()) { Weak->SetDepthTestRange(V > 0.5f, Weak->GetDepthTestRangeNearZ(), Weak->GetDepthTestRangeFarZ()); }
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Depth.NearZ", Depth, LOCTEXT("NearZ", "Range near"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("Metres", "m"); T.Min = 0.0f; T.Max = 2.0f; T.Delta = 0.01f; T.Default = 0.0f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->GetDepthTestRangeNearZ() : 0.0f; };
		T.Set = [Weak](float V)
		{
			if (Weak.IsValid()) { Weak->SetDepthTestRange(Weak->IsDepthTestRangeOn(), V, Weak->GetDepthTestRangeFarZ()); }
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Depth.FarZ", Depth, LOCTEXT("FarZ", "Range far"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("Metres", "m"); T.Min = 0.05f; T.Max = 10.0f; T.Delta = 0.05f; T.Default = 0.75f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->GetDepthTestRangeFarZ() : 0.0f; };
		T.Set = [Weak](float V)
		{
			if (Weak.IsValid()) { Weak->SetDepthTestRange(Weak->IsDepthTestRangeOn(), Weak->GetDepthTestRangeNearZ(), V); }
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Depth.EnvEstimation", Depth, LOCTEXT("EnvDepth", "Environment depth estimation"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsEnvironmentDepthEstimationOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetEnvironmentDepthEstimation(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}

	// ---- Display ----
	{
		// Exposure goes through the level's unbound post-process volume rather than a console variable, so it also
		// works in a packaged build.
		FCXMRTunable T = Make("Display.Exposure", LOCTEXT("CatDisplay", "Display"), LOCTEXT("Exposure", "Exposure compensation"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("EV", "EV"); T.Min = -6.0f; T.Max = 6.0f; T.Delta = 0.1f; T.Default = 0.0f; T.bPersist = true;
		T.Get = [this]
		{
			const APostProcessVolume* Volume = FindUnboundVolume(GetWorld());
			return (Volume && Volume->Settings.bOverride_AutoExposureBias) ? Volume->Settings.AutoExposureBias : 0.0f;
		};
		T.Set = [this](float V)
		{
			if (APostProcessVolume* Volume = FindUnboundVolume(GetWorld()))
			{
				Volume->Settings.bOverride_AutoExposureBias = true;
				Volume->Settings.AutoExposureBias = V;
			}
		};
		Tuning->Register(MoveTemp(T));
	}

	// ---- Input ----
	if (UCXMRVarjoInputComponent* Input = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVarjoInputComponent>() : nullptr)
	{
		const TWeakObjectPtr<UCXMRVarjoInputComponent> WeakInput(Input);
		FCXMRTunable T = Make("Input.OffsetAdjustSpeed", LOCTEXT("CatInput", "Input"), LOCTEXT("AdjustSpeed", "NumPad speed (cm, deg)"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("PerSecond", "per s"); T.Min = 0.5f; T.Max = 200.0f; T.Delta = 1.0f; T.Default = 10.0f; T.bPersist = true;
		T.Get = [WeakInput] { return WeakInput.IsValid() ? WeakInput->OffsetAdjustSpeed : 0.0f; };
		T.Set = [WeakInput](float V) { if (WeakInput.IsValid()) { WeakInput->OffsetAdjustSpeed = V; } };
		Tuning->Register(MoveTemp(T));
	}
}

bool UCXMRTuningWindowComponent::IsWindowOpen() const
{
	return Window.IsValid();
}

TSharedRef<SWidget> UCXMRTuningWindowComponent::BuildPanel() const
{
	const FWeakTuning Weak(GetTuning());
	TSharedRef<SVerticalBox> List = SNew(SVerticalBox);

	if (UCXMRTuningSubsystem* Tuning = Weak.Get())
	{
		const TArray<const FCXMRTunable*> Rows = Tuning->GetTunables();

		// Group by category in order of first appearance — features register as their actors begin play, so rows of
		// one category can arrive between rows of another.
		TArray<FString> Categories;
		for (const FCXMRTunable* Row : Rows)
		{
			Categories.AddUnique(Row->Category.ToString());
		}

		for (const FString& Category : Categories)
		{
			List->AddSlot().AutoHeight().Padding(10.0f, 14.0f, 10.0f, 4.0f)
			[
				SNew(STextBlock)
				.Text(FText::FromString(Category))
				.Font(FCoreStyle::GetDefaultFontStyle("Bold", 12))
				.ColorAndOpacity(FLinearColor(0.55f, 0.75f, 1.0f))
			];
			for (const FCXMRTunable* Row : Rows)
			{
				if (Row->Category.ToString() == Category)
				{
					List->AddSlot().AutoHeight().Padding(10.0f, 2.0f)[ MakeRow(Weak, *Row) ];
				}
			}
		}
	}
	else
	{
		List->AddSlot().AutoHeight().Padding(10.0f)[ SNew(STextBlock).Text(LOCTEXT("NoRegistry", "Tuning registry is not available.")) ];
	}

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(FLinearColor(0.035f, 0.037f, 0.042f))
		.Padding(4.0f)
		[
			SNew(SScrollBox) + SScrollBox::Slot()[ List ]
		];
}

void UCXMRTuningWindowComponent::RebuildContent()
{
	if (Window.IsValid())
	{
		Window->SetContent(BuildPanel());
	}
}

void UCXMRTuningWindowComponent::OpenWindow()
{
	if (Window.IsValid())
	{
		Window->BringToFront();
		return;
	}

	// Commandlets and dedicated servers have no Slate. Asking for a window there is a crash.
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized())
	{
		UE_LOG(LogCXMRTuningWindow, Log, TEXT("Tuning window skipped: this build has no Slate application."));
		return;
	}

	Window = SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(WindowSize)
		.ScreenPosition(WindowPosition)
		.AutoCenter(EAutoCenter::None)
		.SupportsMaximize(false)
		.SupportsMinimize(true);

	Window->SetContent(BuildPanel());

	// Closing from the title bar must clear the handle, or OpenWindow would bring a destroyed window to the front.
	Window->SetOnWindowClosed(FOnWindowClosed::CreateWeakLambda(this, [this](const TSharedRef<SWindow>&) { Window.Reset(); }));

	FSlateApplication::Get().AddWindow(Window.ToSharedRef(), /*bShowImmediately*/ true);

	UE_LOG(LogCXMRTuningWindow, Log, TEXT("Tuning window opened (%d rows)."),
		GetTuning() ? GetTuning()->GetTunables().Num() : 0);
}

void UCXMRTuningWindowComponent::CloseWindow()
{
	if (Window.IsValid())
	{
		Window->SetOnWindowClosed(FOnWindowClosed());   // do not re-enter the lambda while tearing down
		Window->RequestDestroyWindow();
		Window.Reset();
	}
}

void UCXMRTuningWindowComponent::ToggleWindow()
{
	if (Window.IsValid())
	{
		CloseWindow();
	}
	else
	{
		OpenWindow();
	}
}

#undef LOCTEXT_NAMESPACE
