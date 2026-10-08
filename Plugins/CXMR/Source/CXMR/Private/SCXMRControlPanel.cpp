// Copyright GMTCK CX.

#include "SCXMRControlPanel.h"
#include "CXMRControlPanelWidget.h"
#include "CXMRPanelUI.h"
#include "CXMRPlacementComponent.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRTuningWindowComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CXMRControl"

namespace
{
	using FWeakControl = TWeakObjectPtr<UCXMRTuningWindowComponent>;

	UCXMRSubsystem* FindCXMR(FWeakControl Control)
	{
		UGameInstance* GI = Control.IsValid() && Control->GetWorld() ? Control->GetWorld()->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UCXMRSubsystem>() : nullptr;
	}

	bool CanSwitchMode(FWeakControl Control)
	{
		const UCXMRSubsystem* CXMR = FindCXMR(Control);
		return CXMR && CXMR->IsMixedRealitySupported();
	}

	bool IsModeActive(FWeakControl Control, bool bMR)
	{
		const UCXMRSubsystem* CXMR = FindCXMR(Control);
		return CXMR ? CXMR->IsMixedRealityOn() == bMR : !bMR;
	}

	FReply SwitchMode(FWeakControl Control, bool bMR)
	{
		if (UCXMRSubsystem* CXMR = FindCXMR(Control)) { CXMR->SetMixedReality(bMR); }
		return FReply::Handled();
	}

	int32 RowRank(const FCXMRTunable& Row)
	{
		const FString Id = Row.Id.ToString();
		if (Id.StartsWith(TEXT("Display."))) { return 0; }
		if (Id.StartsWith(TEXT("Spectator.")) || Id == TEXT("MR.VRBackground")) { return 1; }
		if (Id.StartsWith(TEXT("MR.")) || Id.StartsWith(TEXT("Depth."))) { return 2; }
		return Id.StartsWith(TEXT("Port.")) ? 3 : 4;
	}

	bool CompareRows(const CXMRPanelUI::FRowSpec& A, const CXMRPanelUI::FRowSpec& B)
	{
		return RowRank(A.Tunable) < RowRank(B.Tunable);
	}

	FReply RunAlignmentStep(FWeakControl Control, int32 Index)
	{
		if (Control.IsValid())
		{
			if (Index == 0) { Control->StartInitialAlignment(); }
			else if (Index == 1) { Control->ConfirmCalibration(); }
			else { Control->SaveCalibration(); }
		}
		return FReply::Handled();
	}

	bool CanRunAlignmentStep(FWeakControl Control, int32 Index)
	{
		if (!Control.IsValid()) { return false; }
		if (Index == 0) { return Control->CanStartInitialAlignment() || Control->IsInitialAlignmentPending(); }
		return Index == 1 ? Control->CanConfirmCalibration() : Control->CanSaveCalibration();
	}

	/** 0 잠김, 1 진행 가능, 2 완료. 단계 i는 진행 단계가 i보다 크면 완료임. */
	int32 AlignmentStepState(FWeakControl Control, int32 Index)
	{
		if (Control.IsValid() && Control->GetCalibrationPhase() > Index) { return 2; }
		return CanRunAlignmentStep(Control, Index) ? 1 : 0;
	}

	UCXMRPlacementComponent* FindPlacement(FWeakControl Control)
	{
		return Control.IsValid() ? Control->FindPlacement() : nullptr;
	}

	FText AlignmentScope(FWeakControl Control)
	{
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		const FName Group = Placement ? Placement->GetAlignmentGroup() : NAME_None;
		return Group.IsNone() ? LOCTEXT("VehicleAlignmentScope", "This vehicle keeps its own alignment.")
			: FText::Format(LOCTEXT("GroupAlignmentScope", "Shared rig alignment: {0}. Saving or resetting affects every vehicle in this group."), FText::FromName(Group));
	}

	FText AlignmentStorage(FWeakControl Control)
	{
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		return Placement ? Placement->GetAlignmentStorageMessage() : FText::GetEmpty();
	}

	FText EyeReferenceHelp(FWeakControl Control)
	{
		FTransform Eye;
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		return Placement && Placement->GetDriverEyeWorld(Eye)
			? LOCTEXT("EyeReady", "Driver eye reference is set. Turns pivot around it.")
			: LOCTEXT("EyeNotConfigured", "Set Driver Eye Reference in the vehicle Data Asset, in imported vehicle-local coordinates with +X forward.");
	}

	bool HasError(FWeakControl Control) { return Control.IsValid() && Control->HasCalibrationError(); }

	FText CalibrationMessage(FWeakControl Control, bool bError)
	{
		return Control.IsValid() && HasError(Control) == bError ? Control->GetCalibrationMessage() : FText::GetEmpty();
	}

	EVisibility RestoreVisibility(FWeakControl Control)
	{
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		return Placement && Placement->NeedsRestoreConfirmation() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GroupResetVisibility(FWeakControl Control)
	{
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		return Placement && !Placement->GetAlignmentGroup().IsNone() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	FSlateColor ToneLamp(ECXMRAlignmentTone Tone)
	{
		using CXMRPanelUI::ETone;
		switch (Tone)
		{
		case ECXMRAlignmentTone::Aligned:   return CXMRPanelUI::ToneColor(ETone::Positive);
		case ECXMRAlignmentTone::Attention: return CXMRPanelUI::ToneColor(ETone::Attention);
		case ECXMRAlignmentTone::Error:     return CXMRPanelUI::ToneColor(ETone::Negative);
		default:                            return CXMRPanelUI::RailMutedColor();
		}
	}

	bool IsBasicCalibration(FName Id)
	{
		return Id == "Vehicle.Markers" || Id == "Vehicle.LayoutFit" || Id == "Vehicle.MoveStep" || Id == "Vehicle.YawStep"
			|| Id == "Vehicle.Away" || Id == "Vehicle.Right" || Id == "Vehicle.Up" || Id == "Vehicle.Turn" || Id == "Vehicle.ResetOffset";
	}

	void DescribeRow(FCXMRTunable& Row)
	{
		// 저장 ID와 동작은 유지하고 표시 문구만 바꿈.
		struct FDescription { const TCHAR* Id; const TCHAR* Label; const TCHAR* Help; };
		static const FDescription Descriptions[] = {
			{TEXT("Vehicle.Markers"), TEXT("Marker status"), TEXT("Current marker observations and calibration status.")},
			{TEXT("Vehicle.LayoutFit"), TEXT("Alignment error"), TEXT("Difference between the saved marker layout and observations. Lower is better.")},
			{TEXT("Vehicle.MoveStep"), TEXT("Move step"), TEXT("Distance per adjustment step.")},
			{TEXT("Vehicle.YawStep"), TEXT("Rotation step"), TEXT("Angle per rotation step.")},
			{TEXT("Vehicle.Away"), TEXT("Distance: away (+) / closer (-)"), TEXT("Move the vehicle along the current adjustment heading.")},
			{TEXT("Vehicle.Right"), TEXT("Lateral: right (+) / left (-)"), TEXT("Move sideways along the current adjustment heading.")},
			{TEXT("Vehicle.Up"), TEXT("Height: up (+) / down (-)"), TEXT("Adjust vehicle height.")},
			{TEXT("Vehicle.Turn"), TEXT("Yaw: clockwise (+) / counterclockwise (-)"), TEXT("Rotate around the selected pivot.")},
			{TEXT("Vehicle.ResetOffset"), TEXT("Reset adjustment"), TEXT("Discard unsaved adjustments and return to the current base pose.")},
			{TEXT("Vehicle.LearnMarkers"), TEXT("Learn markers from this pose (saves immediately)"), TEXT("Learn and save recently observed markers relative to the current vehicle pose.")},
			{TEXT("Vehicle.SaveOffset"), TEXT("Save adjustment now"), TEXT("Apply the adjustment to the marker layout. Prefer the guided save above.")},
			{TEXT("Vehicle.Recalibrate"), TEXT("Re-read markers"), TEXT("Clear calibration and solve again from markers.")},
			{TEXT("MR.MixedReality"), TEXT("Passthrough"), TEXT("Show the camera view with the virtual vehicle.")},
			{TEXT("MR.VRBackground"), TEXT("Virtual background"), TEXT("Show the virtual sky and floor.")},
			{TEXT("MR.Masking"), TEXT("Scene masks"), TEXT("Show passthrough through masks placed in the level.")},
			{TEXT("MR.ViewOffsetGlide"), TEXT("View transition time"), TEXT("Transition duration between eye and camera origins.")},
			{TEXT("MR.ViewOffset"), TEXT("View origin: eye 0 / camera 1"), TEXT("Affects real and virtual alignment. Adjust while checking in the headset.")},
			{TEXT("Depth.Test"), TEXT("Depth test"), TEXT("Use depth to show real objects in front of virtual objects.")},
			{TEXT("Depth.Range"), TEXT("Limit depth range"), TEXT("Apply depth testing only within this range. Cable depth accuracy depends on the headset.")},
			{TEXT("Depth.NearZ"), TEXT("Depth near"), TEXT("Distance from the camera. Must be less than Depth far.")},
			{TEXT("Depth.FarZ"), TEXT("Depth far"), TEXT("Real-object occlusion may stop beyond this range.")},
			{TEXT("Depth.EnvEstimation"), TEXT("Environment depth"), TEXT("Runtime environment depth estimation. Requires a headset session.")},
			{TEXT("Port.Live"), TEXT("Hand contact status"), TEXT("Contact with the generated tracked hand. Does not measure cable insertion.")},
			{TEXT("Port.Debug"), TEXT("Show USB contact bounds"), TEXT("Draw contact and release margins at the USB centre.")},
			{TEXT("Port.HandContactDistance"), TEXT("Hand contact margin"), TEXT("Extra distance beyond the generated hand surface that counts as touch.")},
			{TEXT("Port.HandReleaseDistance"), TEXT("Hand release margin"), TEXT("Release when the hand moves beyond this margin. Keep larger than the contact margin.")},
		};
		for (const FDescription& Description : Descriptions)
		{
			if (Row.Id == Description.Id)
			{
				Row.Label = FText::FromString(Description.Label);
				Row.Help = FText::FromString(Description.Help);
				break;
			}
		}
		const FString Id = Row.Id.ToString();
		if (Id.StartsWith(TEXT("Vehicle."))) { Row.Category = LOCTEXT("Placement", "Placement adjustment"); }
		else if (Id.StartsWith(TEXT("Depth."))) { Row.Category = LOCTEXT("Depth", "Real-object occlusion"); }
		else if (Id.StartsWith(TEXT("MR."))) { Row.Category = Id == TEXT("MR.VRBackground") ? LOCTEXT("BackgroundCategory", "Visual quality") : LOCTEXT("MR", "Real-object occlusion"); }
		else if (Id.StartsWith(TEXT("Port."))) { Row.Category = LOCTEXT("USB", "Hand contact"); }
		else if (Id.StartsWith(TEXT("Display.")) || Id.StartsWith(TEXT("Spectator."))) { Row.Category = LOCTEXT("Picture", "Visual quality"); }
	}
}

TSharedRef<SWidget> SCXMRControlPanel::RegistryPage(ECXMRControlPage Page, bool bAdvanced)
{
	TArray<CXMRPanelUI::FRowSpec> Rows;
	if (Tuning.IsValid())
	{
		for (const FCXMRTunable* Registered : Tuning->GetTunables())
		{
			const FString Id = Registered->Id.ToString();
			const bool bPlacement = Id.StartsWith(TEXT("Vehicle.")) || Id.StartsWith(TEXT("Input."));
			const bool bDisplay = Id.StartsWith(TEXT("MR.")) || Id.StartsWith(TEXT("Depth.")) || Id.StartsWith(TEXT("Display.")) || Id.StartsWith(TEXT("Spectator."));
			// 설정 화면에 손 접촉과 진단을 모음.
			const bool bUSB = Id == TEXT("Port.Live") || Id == TEXT("Port.Debug")
				|| Id == TEXT("Port.HandContactDistance") || Id == TEXT("Port.HandReleaseDistance");
			bool bInclude = Page == ECXMRControlPage::Display ? !bPlacement
				: Page == ECXMRControlPage::USB ? bUSB
				: Page == ECXMRControlPage::Diagnostics ? !bPlacement && !bDisplay && !bUSB
				: Page == ECXMRControlPage::Calibration && bPlacement && (bAdvanced != IsBasicCalibration(Registered->Id));
			if (Registered->Id == "Vehicle.PlaceInFront" || Registered->Id == "Vehicle.Recalibrate"
				|| Registered->Id == "Vehicle.SaveOffset" || Registered->Id == "Vehicle.LearnMarkers"
				|| Registered->Id == "Vehicle.Pivot" || Registered->Id == "MR.MixedReality" || Id.StartsWith(TEXT("PlugTip."))
				|| Id.StartsWith(TEXT("Box."))) { bInclude = false; }
			if (bInclude)
			{
				FCXMRTunable Row = *Registered;
				DescribeRow(Row);
				Rows.Add({ Row, CXMRPanelUI::BindToRegistry(Tuning.Get(), Row) });
			}
		}
	}
	if (Rows.IsEmpty())
	{
		return CXMRPanelUI::MakeCaption(LOCTEXT("NoFeature", "Not available in this session."));
	}
	Rows.StableSort(CompareRows);
	// 고급 영역은 펼침 제목이 이미 있어 분류 제목을 생략함.
	return CXMRPanelUI::MakeRowList(Rows, !bAdvanced);
}

FText SCXMRControlPanel::Status() const
{
	return Control.IsValid() ? Control->GetAlignmentStatus(Viewer.IsValid() ? Viewer->FindLoader() : nullptr) : FText::GetEmpty();
}

TSharedRef<SWidget> SCXMRControlPanel::Navigation(ECXMRControlPage Page, const FText& Label)
{
	const FWeakControl Weak = Control;
	return CXMRPanelUI::MakeNavItem(Label,
		TAttribute<bool>::CreateLambda([Weak, Page] { return Weak.IsValid() && Weak->GetActivePage() == Page; }),
		[Weak, Page] { if (Weak.IsValid()) { Weak->SelectPage(Page); } return FReply::Handled(); });
}

TSharedRef<SWidget> SCXMRControlPanel::Header()
{
	const FWeakControl Weak = Control;
	const TWeakObjectPtr<UCXMRControlPanelWidget> WeakViewer = Viewer;
	auto Tone = [Weak, WeakViewer]
	{
		ECXMRAlignmentTone Value = ECXMRAlignmentTone::Pending;
		if (Weak.IsValid()) { Weak->GetAlignmentState(WeakViewer.IsValid() ? WeakViewer->FindLoader() : nullptr, Value); }
		return Value;
	};
	auto State = [Weak, WeakViewer]
	{
		ECXMRAlignmentTone Value;
		return Weak.IsValid() ? Weak->GetAlignmentState(WeakViewer.IsValid() ? WeakViewer->FindLoader() : nullptr, Value) : FText::GetEmpty();
	};
	auto Vehicle = [WeakViewer]
	{
		const UCXMRVehicleLoaderComponent* Loader = WeakViewer.IsValid() ? WeakViewer->FindLoader() : nullptr;
		return Loader && Loader->Profile && Loader->GetSpawnedVehicle() ? Loader->Profile->DisplayName : LOCTEXT("NoVehicle", "No vehicle loaded");
	};

	TSharedRef<SHorizontalBox> Modes = SNew(SHorizontalBox);
	struct FMode { bool bMR; FText Title; FText Detail; };
	for (const FMode& Mode : { FMode{ true, LOCTEXT("MRTitle", "MR"), LOCTEXT("MRDetail", "Real environment") },
							   FMode{ false, LOCTEXT("VRTitle", "VR"), LOCTEXT("VRDetail", "Virtual environment") } })
	{
		const bool bMR = Mode.bMR;
		Modes->AddSlot().AutoWidth()
		[
			CXMRPanelUI::MakeRailSegment(Mode.Title, Mode.Detail,
				TAttribute<bool>::CreateLambda([Weak, bMR] { return IsModeActive(Weak, bMR); }),
				TAttribute<bool>::CreateLambda([Weak] { return CanSwitchMode(Weak); }),
				[Weak, bMR] { return SwitchMode(Weak, bMR); })
		];
	}
	TSharedRef<SWidget> ModeSwitch = CXMRPanelUI::MakeRailGroup(Modes);
	ModeSwitch->SetToolTipText(LOCTEXT("ModeHelp", "Switch between the real room (passthrough) and the virtual environment. Needs a running headset session; the renderer stays the same."));

	return SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
		.BorderBackgroundColor(CXMRPanelUI::RailColor())
		.Padding(FMargin(28.f, 18.f, 22.f, 18.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight()
				[ SNew(STextBlock).Text(LOCTEXT("Title", "CXMR Design Review")).Font(CXMRPanelUI::HeadingFont(21)).ColorAndOpacity(CXMRPanelUI::RailInkColor()) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 7.f, 0.f, 0.f)
				[
					SNew(SHorizontalBox).ToolTipText(this, &SCXMRControlPanel::Status)
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 8.f, 0.f)
					[ CXMRPanelUI::MakeLamp(TAttribute<FSlateColor>::CreateLambda([Tone] { return ToneLamp(Tone()); })) ]
					+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
					[ SNew(STextBlock).Text_Lambda(State).Font(CXMRPanelUI::StrongFont(12)).ColorAndOpacity(CXMRPanelUI::RailInkColor()) ]
					+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center).Padding(18.f, 0.f, 12.f, 0.f)
					[ SNew(STextBlock).Text_Lambda(Vehicle).Font(CXMRPanelUI::BodyFont(12)).ColorAndOpacity(CXMRPanelUI::RailMutedColor())
						.OverflowPolicy(ETextOverflowPolicy::Ellipsis) ]
				]
			]
			+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[ ModeSwitch ]
		];
}

TSharedRef<SWidget> SCXMRControlPanel::AlignmentSteps()
{
	const FWeakControl Weak = Control;
	struct FStep { FText Label; FText Detail; };
	const FStep Steps[] = {
		{ LOCTEXT("Initial", "Align to eyes"), LOCTEXT("InitialDetail", "Seated, facing ahead") },
		{ LOCTEXT("Confirm", "Confirm fit"), LOCTEXT("ConfirmDetail", "Check against the rig") },
		{ LOCTEXT("SaveAlignment", "Save alignment"), LOCTEXT("SaveDetail", "Reused next session") } };
	const FMargin StepPadding(8.f, 6.f, 12.f, 6.f);
	TSharedRef<SHorizontalBox> Row = SNew(SHorizontalBox);
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Steps); ++Index)
	{
		const FText Label = Steps[Index].Label;
		const FText Detail = Steps[Index].Detail;
		auto Content = [Weak, Index, Label, Detail]() -> TSharedRef<SWidget>
		{
			return SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center).Padding(0.f, 0.f, 10.f, 0.f)
				[ CXMRPanelUI::MakeStepMarker(Index + 1, TAttribute<int32>::CreateLambda([Weak, Index] { return AlignmentStepState(Weak, Index); })) ]
				+ SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(STextBlock).Font(CXMRPanelUI::StrongFont(13))
						.Text_Lambda([Weak, Index, Label] { return Index == 0 && Weak.IsValid() && Weak->IsInitialAlignmentPending() ? LOCTEXT("CancelInitial", "Cancel alignment") : Label; })
						.ColorAndOpacity_Lambda([Weak, Index] { return FSlateColor(AlignmentStepState(Weak, Index) > 0 ? CXMRPanelUI::InkColor() : CXMRPanelUI::ReadoutColor()); })
					]
					+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 2.f, 0.f, 0.f)
					[ SNew(STextBlock).Text(Detail).Font(CXMRPanelUI::BodyFont(11)).ColorAndOpacity(CXMRPanelUI::ReadoutColor()) ]
				];
		};
		// 잠긴 단계는 버튼 대신 그림만 보여 줌. 비활성 효과로 글자가 흐려지지 않게 함.
		Row->AddSlot().AutoWidth().VAlign(VAlign_Center)
		[
			SNew(SWidgetSwitcher).WidgetIndex_Lambda([Weak, Index] { return CanRunAlignmentStep(Weak, Index) ? 0 : 1; })
			+ SWidgetSwitcher::Slot()
			[
				SNew(SButton).ButtonStyle(CXMRPanelUI::FlatButtonStyle()).ContentPadding(StepPadding)
				.OnClicked_Lambda([Weak, Index] { return CanRunAlignmentStep(Weak, Index) ? RunAlignmentStep(Weak, Index) : FReply::Handled(); })
				[ Content() ]
			]
			+ SWidgetSwitcher::Slot()[ SNew(SBox).Padding(StepPadding)[ Content() ] ]
		];
		if (Index + 1 < UE_ARRAY_COUNT(Steps))
		{
			// 이전 단계가 끝나면 연결선도 선택색으로 바뀜.
			Row->AddSlot().FillWidth(1.f).VAlign(VAlign_Center).Padding(4.f, 0.f)
			[
				SNew(SBox).HeightOverride(2.f).MinDesiredWidth(16.f)
				[ SNew(SImage).Image(FCoreStyle::Get().GetBrush("WhiteBrush"))
					.ColorAndOpacity_Lambda([Weak, Index] { return FSlateColor(AlignmentStepState(Weak, Index) == 2 ? CXMRPanelUI::SelectionColor() : CXMRPanelUI::FaintColor().CopyWithNewOpacity(0.45f)); }) ]
			];
		}
	}
	return Row;
}

TSharedRef<SWidget> SCXMRControlPanel::CalibrationPage()
{
	const FWeakControl Weak = Control;
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 12.f)
		[ CXMRPanelUI::MakeHeader(LOCTEXT("EyeAlignment", "Driver-eye alignment")) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)
		[ CXMRPanelUI::MakeNotice(TAttribute<FText>::CreateLambda([Weak] { return AlignmentScope(Weak); }), CXMRPanelUI::ETone::Neutral) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ CXMRPanelUI::MakeCaption(TAttribute<FText>::CreateLambda([Weak] { return AlignmentStorage(Weak); })) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 18.f)[ AlignmentSteps() ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[ CXMRPanelUI::MakeNotice(TAttribute<FText>::CreateLambda([Weak] { return CalibrationMessage(Weak, true); }), CXMRPanelUI::ETone::Negative) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 0.f, 0.f, 10.f)
		[ CXMRPanelUI::MakeCaption(TAttribute<FText>::CreateLambda([Weak] { return CalibrationMessage(Weak, false); })) ]
		+ SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Left).Padding(0.f, 0.f, 0.f, 10.f)
		[ SNew(SButton).ButtonStyle(CXMRPanelUI::PrimaryButtonStyle()).TextStyle(CXMRPanelUI::BodyTextStyle()).Text(LOCTEXT("AcceptRestored", "Use restored alignment"))
			.Visibility_Lambda([Weak] { return RestoreVisibility(Weak); })
			.OnClicked_Lambda([Weak] { if (Weak.IsValid()) { Weak->AcceptRestoredAlignment(); } return FReply::Handled(); }) ]
		+ SVerticalBox::Slot().AutoHeight()
		[ CXMRPanelUI::MakeCaption(TAttribute<FText>::CreateLambda([Weak] { return EyeReferenceHelp(Weak); })) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 30.f, 0.f, 0.f)[ RegistryPage(ECXMRControlPage::Calibration) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 26.f, 0.f, 0.f)[ CXMRPanelUI::MakeDivider() ]
		+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 6.f, 0.f, 0.f)
		[
			SNew(SExpandableArea).InitiallyCollapsed(true)
			.BorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
			.BodyBorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
			.HeaderPadding(FMargin(0.f, 8.f))
			.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			.HeaderContent()
			[ SNew(STextBlock).Text(LOCTEXT("AdvancedPlacement", "Advanced placement")).Font(CXMRPanelUI::StrongFont(13)).ColorAndOpacity(CXMRPanelUI::InkColor()) ]
			.BodyContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(0.f, 4.f, 0.f, 18.f)
				[
					// 확인 문구가 길어지면 다음 줄로 내려감.
					SNew(SWrapBox).UseAllottedSize(true).InnerSlotPadding(FVector2D(10.f, 10.f))
					+ SWrapBox::Slot()
					[ SNew(SButton).ButtonStyle(CXMRPanelUI::ButtonStyle()).TextStyle(CXMRPanelUI::BodyTextStyle()).Text(LOCTEXT("ReadSavedAgain", "Restore from markers again"))
						.OnClicked_Lambda([Weak] { if (Weak.IsValid()) { Weak->StartCalibration(); } return FReply::Handled(); }) ]
					+ SWrapBox::Slot()
					[
						SNew(SBox).Visibility_Lambda([Weak] { return GroupResetVisibility(Weak); })
						.ToolTipText(LOCTEXT("ResetGroupHelp", "Click twice within five seconds to delete the shared alignment for this group. Reset adjustment only discards temporary adjustments."))
						[
							CXMRPanelUI::MakeDangerButton(
								TAttribute<FText>::CreateLambda([Weak] { return Weak.IsValid() ? Weak->GetSharedAlignmentResetLabel() : FText::GetEmpty(); }),
								TAttribute<bool>::CreateLambda([Weak] { return Weak.IsValid() && Weak->IsSharedAlignmentResetArmed(); }),
								[Weak] { if (Weak.IsValid()) { Weak->ResetSharedAlignment(); } return FReply::Handled(); })
						]
					]
				]
				+ SVerticalBox::Slot().AutoHeight()[ RegistryPage(ECXMRControlPage::Calibration, true) ]
			]
		];
}

TSharedRef<SWidget> SCXMRControlPanel::SettingsPage(UCXMRControlPanelWidget* InViewer)
{
	TSharedRef<SVerticalBox> Settings = SNew(SVerticalBox);
	Settings->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 24.f)
	[ CXMRPanelUI::MakeCaption(LOCTEXT("Pipeline", "MR and VR keep separate virtual exposure. Camera exposure is set in Varjo Base. Real-object occlusion works only in MR.")) ];
	Settings->AddSlot().AutoHeight()[ RegistryPage(ECXMRControlPage::Display) ];
	if (InViewer) { Settings->AddSlot().AutoHeight().Padding(0.f, 30.f, 0.f, 0.f)[ InViewer->BuildDisplayPage() ]; }
	return Settings;
}

void SCXMRControlPanel::Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime)
{
	SCompoundWidget::Tick(Geometry, CurrentTime, DeltaTime);
	UCXMRVehicleLoaderComponent* Loader = Viewer.IsValid() ? Viewer->FindLoader() : nullptr;
	UObject* Profile = Loader ? Loader->Profile.Get() : nullptr;
	UObject* Catalog = Loader ? Loader->Catalog.Get() : nullptr;
	AActor* Vehicle = Loader ? Loader->GetSpawnedVehicle() : nullptr;
	if (ReviewHost.IsValid() && Viewer.IsValid() && (Loader != LastLoader.Get() || Profile != LastProfile.Get() || Catalog != LastCatalog.Get() || Vehicle != LastVehicle.Get()))
	{
		LastLoader = Loader; LastProfile = Profile; LastCatalog = Catalog; LastVehicle = Vehicle;
		ReviewHost->SetContent(Viewer->BuildViewerPage());
	}
}

void SCXMRControlPanel::Construct(const FArguments& Args)
{
	Control = Args._Control;
	Tuning = Args._Tuning;
	Viewer = Args._Viewer;
	const FWeakControl Weak = Control;

	TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	Menu->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)[ Navigation(ECXMRControlPage::Vehicle, LOCTEXT("Review", "Review")) ];
	Menu->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 4.f)[ Navigation(ECXMRControlPage::Calibration, LOCTEXT("Alignment", "Alignment")) ];
	Menu->AddSlot().AutoHeight()[ Navigation(ECXMRControlPage::Display, LOCTEXT("Settings", "Settings")) ];

	SAssignNew(ReviewHost, SBox);
	if (Args._Viewer) { ReviewHost->SetContent(Args._Viewer->BuildViewerPage()); }
	else { ReviewHost->SetContent(CXMRPanelUI::MakeCaption(LOCTEXT("NoSession", "Start a session to review vehicles and design options."))); }

	auto Page = [](const TSharedRef<SWidget>& Content) { return CXMRPanelUI::MakeScroll(CXMRPanelUI::MakeSurface(Content)); };

	ChildSlot
	[
		CXMRPanelUI::MakeBackground(SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight()[ Header() ]
			+ SVerticalBox::Slot().FillHeight(1.f).Padding(0.f, 20.f, 0.f, 0.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(16.f, 0.f, 16.f, 0.f)[ SNew(SBox).WidthOverride(164.f)[ Menu ] ]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 18.f, 0.f)
				[
					SNew(SWidgetSwitcher).WidgetIndex_Lambda([Weak]
					{
						const ECXMRControlPage Active = Weak.IsValid() ? Weak->GetActivePage() : ECXMRControlPage::Vehicle;
						return Active == ECXMRControlPage::Calibration ? 1 : Active == ECXMRControlPage::Display ? 2 : 0;
					})
					+ SWidgetSwitcher::Slot()[ Page(ReviewHost.ToSharedRef()) ]
					+ SWidgetSwitcher::Slot()[ Page(CalibrationPage()) ]
					+ SWidgetSwitcher::Slot()[ Page(SettingsPage(Args._Viewer)) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(FMargin(196.f, 10.f, 24.f, 12.f))
			[
				SNew(STextBlock).Font(CXMRPanelUI::BodyFont(11)).ColorAndOpacity(CXMRPanelUI::FaintColor()).AutoWrapText(true)
				.Text(LOCTEXT("SaveHelp", "Exposure presets save automatically. Alignment saves only through Save alignment."))
			])
	];
}

#undef LOCTEXT_NAMESPACE
