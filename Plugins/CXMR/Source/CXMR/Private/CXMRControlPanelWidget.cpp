// Copyright GMTCK CX.

#include "CXMRControlPanelWidget.h"
#include "CXMRPanelUI.h"
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRDesignOption.h"
#include "CXMRPartVariantComponent.h"
#include "EngineUtils.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/Text/STextBlock.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "CXMRControlPanel"

namespace
{

	UCXMRVehicleLoaderComponent* FindReviewLoader(TWeakObjectPtr<UCXMRControlPanelWidget> Viewer)
	{
		return Viewer.IsValid() ? Viewer->FindLoader() : nullptr;
	}

	bool IsVehicleSelected(TWeakObjectPtr<UCXMRControlPanelWidget> Viewer, TWeakObjectPtr<UCXMRVehicleProfile> Profile)
	{
		const UCXMRVehicleLoaderComponent* Loader = FindReviewLoader(Viewer);
		return Loader && Loader->Profile == Profile.Get() && Loader->GetSpawnedVehicle();
	}

	FReply SelectVehicle(TWeakObjectPtr<UCXMRControlPanelWidget> Viewer, int32 Index, TWeakObjectPtr<UCXMRVehicleProfile> Profile)
	{
		UCXMRVehicleLoaderComponent* Loader = FindReviewLoader(Viewer);
		if (Loader && Loader->Catalog && Loader->Catalog->Vehicles.IsValidIndex(Index)
			&& Loader->Catalog->Vehicles[Index].Get() == Profile.Get()) { Loader->SelectVehicle(Index); }
		return FReply::Handled();
	}

	bool CanSelectOption(TWeakObjectPtr<UCXMRControlPanelWidget> Viewer, TWeakObjectPtr<UCXMRVehicleProfile> Profile, TWeakObjectPtr<UCXMRDesignOption> Option)
	{
		const UCXMRVehicleLoaderComponent* Loader = FindReviewLoader(Viewer);
		return Loader && Loader->Profile == Profile.Get() && Option.IsValid() && Loader->GetPartVariants();
	}

	bool IsOptionSelected(TWeakObjectPtr<UCXMRControlPanelWidget> Viewer, TWeakObjectPtr<UCXMRDesignOption> Option)
	{
		const UCXMRVehicleLoaderComponent* Loader = FindReviewLoader(Viewer);
		const UCXMRPartVariantComponent* Parts = Loader ? Loader->GetPartVariants() : nullptr;
		return Parts && Option.IsValid() && Parts->GetActiveOption(Option->SlotId) == Option.Get();
	}

	FReply SelectOption(TWeakObjectPtr<UCXMRControlPanelWidget> Viewer, TWeakObjectPtr<UCXMRVehicleProfile> Profile, TWeakObjectPtr<UCXMRDesignOption> Option)
	{
		UCXMRVehicleLoaderComponent* Loader = FindReviewLoader(Viewer);
		if (Loader && Loader->Profile == Profile.Get() && Option.IsValid()) { Loader->SelectDesignOption(Option.Get()); }
		return FReply::Handled();
	}

	FText SelectionFailure(TWeakObjectPtr<UCXMRControlPanelWidget> Viewer)
	{
		const UCXMRVehicleLoaderComponent* Loader = FindReviewLoader(Viewer);
		return Loader ? FText::FromString(Loader->GetLastFailureReason()) : FText::GetEmpty();
	}

	using FRows = TArray<CXMRPanelUI::FRowSpec>;

	FCXMRTunable MakeRow(FName Id, const FText& Category, const FText& Label, ECXMRTunableKind Kind)
	{
		FCXMRTunable Row;
		Row.Id = Id;
		Row.Category = Category;
		Row.Label = Label;
		Row.Kind = Kind;
		return Row;
	}

	void AddRow(FRows& Rows, FCXMRTunable&& Row)
	{
		CXMRPanelUI::FRowBinding Binding = CXMRPanelUI::BindDirect(Row);
		Rows.Add({ MoveTemp(Row), MoveTemp(Binding) });
	}

	/** 차량 이름과 순번 표시함. 로드 전에는 '-'로 표시함. */
	FText NameAndPosition(const FText& Name, int32 Index, int32 Count)
	{
		if (Count <= 0)
		{
			return FText::FromString(TEXT("-"));
		}
		return FText::FromString(FString::Printf(TEXT("%s  %d / %d"),
			Name.IsEmpty() ? TEXT("-") : *Name.ToString(), Index + 1, Count));
	}
	FText ManikinText(const UCXMRSubsystem* CXMR)
	{
		return CXMR ? NameAndPosition(CXMR->GetManikinName(), CXMR->GetManikinIndex(), CXMR->GetManikinCount()) : FText::GetEmpty();
	}
}

UCXMRSubsystem* UCXMRControlPanelWidget::GetCXMR() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRSubsystem>() : nullptr;
}

UCXMRTuningSubsystem* UCXMRControlPanelWidget::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

void UCXMRControlPanelWidget::SetActiveTab(int32 TabIndex)
{
	ActiveTab = FMath::Clamp(TabIndex, 0, 1);
}

TSharedRef<SWidget> UCXMRControlPanelWidget::RebuildWidget()
{
	// 기본 위젯 초기화만 사용함.

	Super::RebuildWidget();

	const TWeakObjectPtr<UCXMRControlPanelWidget> Weak(this);
	const TArray<FText> Tabs = {
		LOCTEXT("TabVehicle", "Review"),
		LOCTEXT("TabView", "Settings") };

	SAssignNew(Pages, SWidgetSwitcher)
		.WidgetIndex_Lambda([Weak] { return Weak.IsValid() ? Weak->ActiveTab : 0; })
		+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(CXMRPanelUI::MakeSurface(BuildViewerPage())) ]
		+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(CXMRPanelUI::MakeSurface(BuildDisplayPage())) ];

	return CXMRPanelUI::MakeBackground(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(16.0f, 16.0f, 16.0f, 12.0f)
		[
			CXMRPanelUI::MakeTabBar(Tabs,
				[Weak] { return Weak.IsValid() ? Weak->ActiveTab : 0; },
				[Weak](int32 Index) { if (Weak.IsValid()) { Weak->SetActiveTab(Index); } })
		]
		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(16.0f, 0.0f, 12.0f, 16.0f)
		[
			Pages.ToSharedRef()
		]);
}

void UCXMRControlPanelWidget::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);
	Pages.Reset();
}

// 보기 조작

TSharedRef<SWidget> UCXMRControlPanelWidget::BuildDisplayPage()
{
	const TWeakObjectPtr<UCXMRControlPanelWidget> Weak(this);
	auto CXMR = [Weak]() -> UCXMRSubsystem* { return Weak.IsValid() ? Weak->GetCXMR() : nullptr; };
	FRows Rows;


	const FText Human = LOCTEXT("CatHuman", "Optional eye references");
	{
		FCXMRTunable Row = MakeRow("Panel.Manikin", Human, LOCTEXT("Manikin", "Authored eye position"), ECXMRTunableKind::Stepper);
		Row.Text = [CXMR] { return ManikinText(CXMR()); };
		Row.IsEnabled = [CXMR] { const UCXMRSubsystem* S = CXMR(); return S && S->GetManikinCount() > 0; };
		Row.Step = [CXMR](float Direction) { if (UCXMRSubsystem* S = CXMR()) { S->RequestErgonomicsStep(Direction > 0.0f ? 1 : -1); } };
		AddRow(Rows, MoveTemp(Row));
	}

	return CXMRPanelUI::MakeRowList(Rows);
}
// 차량 선택

UCXMRVehicleLoaderComponent* UCXMRControlPanelWidget::FindLoader() const
{
	if (CachedLoader.IsValid() && CachedLoader->GetWorld() == GetWorld()
		&& IsValid(CachedLoader->GetOwner()) && !CachedLoader->GetOwner()->IsActorBeingDestroyed())
	{
		return CachedLoader.Get();
	}
	CachedLoader.Reset();
	if (GetWorld())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (!It->IsActorBeingDestroyed())
			{
				if (UCXMRVehicleLoaderComponent* Loader = It->FindComponentByClass<UCXMRVehicleLoaderComponent>()) { CachedLoader = Loader; return Loader; }
			}
		}
	}
	return nullptr;
}

TSharedRef<SWidget> UCXMRControlPanelWidget::BuildViewerPage()
{
	const TWeakObjectPtr<UCXMRControlPanelWidget> Weak(this);
	UCXMRVehicleLoaderComponent* Loader = FindLoader();
	ReviewAssets.Reset();
	TSharedRef<SVerticalBox> Review = SNew(SVerticalBox);

	// 실패 이유는 스크롤 없이 보이도록 맨 위에 둠.
	Review->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 20.f)
	[ CXMRPanelUI::MakeNotice(TAttribute<FText>::CreateLambda([Weak] { return SelectionFailure(Weak); }), CXMRPanelUI::ETone::Negative) ];

	Review->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)[ CXMRPanelUI::MakeHeader(LOCTEXT("Vehicles", "Vehicle")) ];
	if (Loader && Loader->Catalog && !Loader->Catalog->Vehicles.IsEmpty())
	{
		for (int32 Index = 0; Index < Loader->Catalog->Vehicles.Num(); ++Index)
		{
			UCXMRVehicleProfile* Profile = Loader->Catalog->Vehicles[Index].LoadSynchronous();
			if (!Profile) { continue; }
			ReviewAssets.Add(Profile);
			const TWeakObjectPtr<UCXMRVehicleProfile> Expected(Profile);
			Review->AddSlot().AutoHeight().Padding(0.f, 3.f)
			[
				CXMRPanelUI::MakeChoice(Profile->DisplayName.IsEmpty() ? FText::FromString(Profile->GetName()) : Profile->DisplayName,
					TAttribute<bool>::CreateLambda([Weak, Expected] { return IsVehicleSelected(Weak, Expected); }),
					[Weak, Index, Expected] { return SelectVehicle(Weak, Index, Expected); })
			];
		}
	}
	else
	{
		Review->AddSlot().AutoHeight()[ CXMRPanelUI::MakeCaption(LOCTEXT("NoCatalog", "No vehicle catalog. Assign one to the Loader to choose vehicles here.")) ];
	}

	Review->AddSlot().AutoHeight().Padding(0.f, 28.f)[ CXMRPanelUI::MakeDivider() ];
	Review->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)[ CXMRPanelUI::MakeHeader(LOCTEXT("Options", "Design options")) ];

	// 같은 장착 위치의 옵션끼리 묶어 서로 바뀐다는 것을 보여 줌.
	TArray<FName> SlotOrder;
	TMap<FName, TArray<UCXMRDesignOption*>> BySlot;
	if (Loader && Loader->Profile)
	{
		for (const TSoftObjectPtr<UCXMRDesignOption>& OptionAsset : Loader->Profile->DesignOptions)
		{
			UCXMRDesignOption* Option = OptionAsset.LoadSynchronous();
			if (!Option) { continue; }
			ReviewAssets.Add(Option);
			SlotOrder.AddUnique(Option->SlotId);
			BySlot.FindOrAdd(Option->SlotId).Add(Option);
		}
	}
	if (SlotOrder.IsEmpty())
	{
		Review->AddSlot().AutoHeight()[ CXMRPanelUI::MakeCaption(LOCTEXT("NoOptions", "This vehicle has no design options. Add Design Option assets to its Vehicle Profile.")) ];
		return Review;
	}

	Review->AddSlot().AutoHeight()
	[ CXMRPanelUI::MakeCaption(LOCTEXT("OptionsHelp", "Options on the same mount replace each other. The vehicle and its alignment stay in place.")) ];
	const TWeakObjectPtr<UCXMRVehicleProfile> ExpectedProfile(Loader->Profile);
	for (const FName SlotId : SlotOrder)
	{
		Review->AddSlot().AutoHeight().Padding(0.f, 20.f, 0.f, 8.f)
		[ CXMRPanelUI::MakeSubheader(FText::FromString(FName::NameToDisplayString(SlotId.ToString(), false))) ];
		TSharedRef<SWrapBox> Pills = SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(8.f, 8.f));
		for (UCXMRDesignOption* Option : BySlot[SlotId])
		{
			const TWeakObjectPtr<UCXMRDesignOption> ExpectedOption(Option);
			Pills->AddSlot()
			[
				CXMRPanelUI::MakePill(Option->DisplayName.IsEmpty() ? FText::FromName(Option->OptionId) : Option->DisplayName,
					TAttribute<bool>::CreateLambda([Weak, ExpectedOption] { return IsOptionSelected(Weak, ExpectedOption); }),
					[Weak, ExpectedProfile, ExpectedOption] { return SelectOption(Weak, ExpectedProfile, ExpectedOption); },
					TAttribute<bool>::CreateLambda([Weak, ExpectedProfile, ExpectedOption] { return CanSelectOption(Weak, ExpectedProfile, ExpectedOption); }))
			];
		}
		Review->AddSlot().AutoHeight()[ Pills ];
	}
	return Review;
}

#undef LOCTEXT_NAMESPACE
