// Copyright GMTCK CX.

#include "CXMRControlPanelWidget.h"
#include "CXMRPanelUI.h"
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"

#define LOCTEXT_NAMESPACE "CXMRControlPanel"

namespace
{
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
		LOCTEXT("TabVehicle", "Vehicle"),
		LOCTEXT("TabView", "View") };

	SAssignNew(Pages, SWidgetSwitcher)
		.WidgetIndex_Lambda([Weak] { return Weak.IsValid() ? Weak->ActiveTab : 0; })
		+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(BuildViewerPage()) ]
		+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(BuildDisplayPage()) ];

	return CXMRPanelUI::MakeBackground(
		SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(6.0f, 6.0f, 6.0f, 2.0f)
		[
			CXMRPanelUI::MakeTabBar(Tabs,
				[Weak] { return Weak.IsValid() ? Weak->ActiveTab : 0; },
				[Weak](int32 Index) { if (Weak.IsValid()) { Weak->SetActiveTab(Index); } })
		]
		+ SVerticalBox::Slot().FillHeight(1.0f)
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
	const FText Category = LOCTEXT("UserView", "View");
	FCXMRTunable MR = MakeRow("Panel.MR", Category, LOCTEXT("UserMR", "Passthrough"), ECXMRTunableKind::Bool);
	MR.Get = [Weak] { const UCXMRSubsystem* S = Weak.IsValid() ? Weak->GetCXMR() : nullptr; return S && S->IsMixedRealityOn() ? 1.f : 0.f; };
	MR.Set = [Weak](float Value) { if (UCXMRSubsystem* S = Weak.IsValid() ? Weak->GetCXMR() : nullptr) { S->SetMixedReality(Value > 0.5f); } };
	MR.IsEnabled = [Weak] { const UCXMRSubsystem* S = Weak.IsValid() ? Weak->GetCXMR() : nullptr; return S && S->IsMixedRealitySupported(); };
	AddRow(Rows, MoveTemp(MR));

	FCXMRTunable Monitor = MakeRow("Panel.Spectator", Category, LOCTEXT("UserMonitor", "Monitor View"), ECXMRTunableKind::Choice);
	Monitor.Options = { LOCTEXT("Mirror", "Headset Mirror"), LOCTEXT("Smooth", "Smooth View"), LOCTEXT("Orbit", "Vehicle Orbit") };
	Monitor.Get = [Weak] { const UCXMRTuningSubsystem* T = Weak.IsValid() ? Weak->GetTuning() : nullptr; return T ? T->GetTunableValue("Spectator.Mode") : 0.f; };
	Monitor.Set = [Weak](float Value) { if (UCXMRTuningSubsystem* T = Weak.IsValid() ? Weak->GetTuning() : nullptr) { T->SetTunableValue("Spectator.Mode", Value); } };
	Monitor.IsEnabled = [Weak] { const UCXMRTuningSubsystem* T = Weak.IsValid() ? Weak->GetTuning() : nullptr; return T && T->GetTunables().ContainsByPredicate([](const FCXMRTunable* Row) { return Row->Id == "Spectator.Mode"; }); };
	AddRow(Rows, MoveTemp(Monitor));
	const FText Human = LOCTEXT("CatHuman", "Seating View");
	{
		FCXMRTunable Row = MakeRow("Panel.Manikin", Human, LOCTEXT("Manikin", "Eye Position"), ECXMRTunableKind::Stepper);
		Row.Text = [CXMR]
		{
			const UCXMRSubsystem* S = CXMR();
			return S ? NameAndPosition(S->GetManikinName(), S->GetManikinIndex(), S->GetManikinCount()) : FText::GetEmpty();
		};
		Row.Step = [CXMR](float Direction) { if (UCXMRSubsystem* S = CXMR()) { S->RequestErgonomicsStep(Direction > 0.0f ? 1 : -1); } };
		AddRow(Rows, MoveTemp(Row));
	}

	return CXMRPanelUI::MakeRowList(Rows);
}
// 차량 선택

TSharedRef<SWidget> UCXMRControlPanelWidget::BuildViewerPage()
{
	const TWeakObjectPtr<UCXMRControlPanelWidget> Weak(this);
	auto CXMR = [Weak]() -> UCXMRSubsystem* { return Weak.IsValid() ? Weak->GetCXMR() : nullptr; };

	FRows Rows;
	const FText Vehicle = LOCTEXT("CatVehicle", "Vehicle Selection");
	{
		FCXMRTunable Row = MakeRow("Panel.Vehicle", Vehicle, LOCTEXT("VehicleName", "Vehicle"), ECXMRTunableKind::Stepper);
		Row.Text = [CXMR]
		{
			const UCXMRSubsystem* S = CXMR();
			return S ? NameAndPosition(S->GetVehicleName(), S->GetVehicleIndex(), S->GetVehicleCount()) : FText::GetEmpty();
		};
		Row.Step = [CXMR](float Direction)
		{
			if (UCXMRSubsystem* S = CXMR())
			{
				S->RequestViewerAction(Direction > 0.0f ? ECXMRViewerAction::NextVehicle : ECXMRViewerAction::PreviousVehicle);
			}
		};
		AddRow(Rows, MoveTemp(Row));
	}
	return CXMRPanelUI::MakeRowList(Rows);
}

#undef LOCTEXT_NAMESPACE
