// Copyright GMTCK CX.

#include "CXMRTuningWindowComponent.h"
#include "CXMRPanelUI.h"
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
	{
		FCXMRTunable T = Make("MR.ViewOffsetGlide", MR, LOCTEXT("ViewOffsetGlide", "View offset glide (K)"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("Seconds", "s"); T.Min = 0.0f; T.Max = 2.0f; T.Delta = 0.05f; T.Default = 0.5f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->ViewOffsetTransitionSeconds : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->ViewOffsetTransitionSeconds = V; } };
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
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return CXMRPanelUI::MakeBackground(SNew(STextBlock).Text(LOCTEXT("NoRegistry", "Tuning registry is not available.")));
	}

	// Every row goes through the registry by id, so a row whose owner has left reads as empty instead of calling
	// into a destroyed component.
	TArray<CXMRPanelUI::FRowSpec> Rows;
	for (const FCXMRTunable* Tunable : Tuning->GetTunables())
	{
		Rows.Add({ *Tunable, CXMRPanelUI::BindToRegistry(Tuning, *Tunable) });
	}
	return CXMRPanelUI::MakeBackground(CXMRPanelUI::MakeScroll(CXMRPanelUI::MakeRowList(Rows)));
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
