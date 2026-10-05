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

	/** "Sedan  2 / 3". A dash when nothing is loaded — a stepper with no value reads as broken, not empty.
	 *  ASCII only: the default font draws a box for anything else. */
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
	// The base does the user-widget bookkeeping (initialisation, player context). Its content — the widget tree,
	// empty for this class — is not used.
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

// ---- 보기: 자주 쓰는 화면 조작만 둠 ----

TSharedRef<SWidget> UCXMRControlPanelWidget::BuildDisplayPage()
{
	const TWeakObjectPtr<UCXMRControlPanelWidget> Weak(this);
	FRows Rows;
	const FText Category = LOCTEXT("UserView", "View");
	FCXMRTunable MR = MakeRow("Panel.MR", Category, LOCTEXT("UserMR", "Show the real room"), ECXMRTunableKind::Bool);
	MR.Get = [Weak] { const UCXMRSubsystem* S = Weak.IsValid() ? Weak->GetCXMR() : nullptr; return S && S->IsMixedRealityOn() ? 1.f : 0.f; };
	MR.Set = [Weak](float Value) { if (UCXMRSubsystem* S = Weak.IsValid() ? Weak->GetCXMR() : nullptr) { S->SetMixedReality(Value > 0.5f); } };
	MR.IsEnabled = [Weak] { const UCXMRSubsystem* S = Weak.IsValid() ? Weak->GetCXMR() : nullptr; return S && S->IsMixedRealitySupported(); };
	AddRow(Rows, MoveTemp(MR));

	FCXMRTunable Monitor = MakeRow("Panel.Spectator", Category, LOCTEXT("UserMonitor", "Monitor view"), ECXMRTunableKind::Choice);
	Monitor.Options = { LOCTEXT("Mirror", "Headset"), LOCTEXT("Smooth", "Smoothed"), LOCTEXT("Orbit", "Around vehicle") };
	Monitor.Get = [Weak] { const UCXMRTuningSubsystem* T = Weak.IsValid() ? Weak->GetTuning() : nullptr; return T ? T->GetTunableValue("Spectator.Mode") : 0.f; };
	Monitor.Set = [Weak](float Value) { if (UCXMRTuningSubsystem* T = Weak.IsValid() ? Weak->GetTuning() : nullptr) { T->SetTunableValue("Spectator.Mode", Value); } };
	Monitor.IsEnabled = [Weak] { const UCXMRTuningSubsystem* T = Weak.IsValid() ? Weak->GetTuning() : nullptr; return T && T->GetTunables().ContainsByPredicate([](const FCXMRTunable* Row) { return Row->Id == "Spectator.Mode"; }); };
	AddRow(Rows, MoveTemp(Monitor));
	return CXMRPanelUI::MakeRowList(Rows);
}
// ---- Viewer: what is being reviewed ----

TSharedRef<SWidget> UCXMRControlPanelWidget::BuildViewerPage()
{
	const TWeakObjectPtr<UCXMRControlPanelWidget> Weak(this);
	auto CXMR = [Weak]() -> UCXMRSubsystem* { return Weak.IsValid() ? Weak->GetCXMR() : nullptr; };

	FRows Rows;
	const FText Vehicle = LOCTEXT("CatVehicle", "Vehicle");
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
	{
		FCXMRTunable Row = MakeRow("Panel.Trim", Vehicle, LOCTEXT("TrimName", "Trim"), ECXMRTunableKind::Stepper);
		Row.Text = [CXMR]
		{
			const UCXMRSubsystem* S = CXMR();
			return S ? NameAndPosition(S->GetTrimName(), S->GetTrimIndex(), S->GetTrimCount()) : FText::GetEmpty();
		};
		Row.Step = [CXMR](float Direction)
		{
			if (UCXMRSubsystem* S = CXMR())
			{
				S->RequestViewerAction(Direction > 0.0f ? ECXMRViewerAction::NextTrim : ECXMRViewerAction::PreviousTrim);
			}
		};
		AddRow(Rows, MoveTemp(Row));
	}
	{
		FCXMRTunable Row = MakeRow("Panel.CMF", Vehicle, LOCTEXT("CMF", "Colour / finish"), ECXMRTunableKind::Readout);
		Row.Text = [CXMR] { const UCXMRSubsystem* S = CXMR(); return S && S->GetTrimCount() > 0 ? FText::AsNumber(S->GetCMFIndex() + 1) : LOCTEXT("NoFinish", "-"); };
		AddRow(Rows, MoveTemp(Row));
	}
	{
		// The loader only cycles CMF forwards, so this is one button rather than a [-] [+] pair with a dead [-].
		FCXMRTunable Row = MakeRow("Panel.NextCMF", Vehicle, LOCTEXT("NextCMF", "Next colour / finish"), ECXMRTunableKind::Action);
		Row.Invoke = [CXMR] { if (UCXMRSubsystem* S = CXMR()) { S->RequestViewerAction(ECXMRViewerAction::NextCMF); } };
		AddRow(Rows, MoveTemp(Row));
	}

	const FText Human = LOCTEXT("CatHuman", "Seating position");
	{
		FCXMRTunable Row = MakeRow("Panel.Manikin", Human, LOCTEXT("Manikin", "Eye position"), ECXMRTunableKind::Stepper);
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

#undef LOCTEXT_NAMESPACE
