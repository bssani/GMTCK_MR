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
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "CXMRControl"

namespace
{
	UCXMRSubsystem* FindCXMR(TWeakObjectPtr<UCXMRTuningWindowComponent> Control)
	{
		UGameInstance* GI = Control.IsValid() && Control->GetWorld() ? Control->GetWorld()->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UCXMRSubsystem>() : nullptr;
	}

	bool CanSwitchMode(TWeakObjectPtr<UCXMRTuningWindowComponent> Control)
	{
		const UCXMRSubsystem* CXMR = FindCXMR(Control);
		return CXMR && CXMR->IsMixedRealitySupported();
	}

	FLinearColor ModeColor(TWeakObjectPtr<UCXMRTuningWindowComponent> Control, bool bMR)
	{
		const UCXMRSubsystem* CXMR = FindCXMR(Control);
		return CXMR && CXMR->IsMixedRealityOn() == bMR
			? FLinearColor::FromSRGBColor(FColor(222, 235, 253)) : FLinearColor::White;
	}

	FReply SwitchMode(TWeakObjectPtr<UCXMRTuningWindowComponent> Control, bool bMR)
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

	FReply RunAlignmentStep(TWeakObjectPtr<UCXMRTuningWindowComponent> Control, int32 Index)
	{
		if (Control.IsValid())
		{
			if (Index == 0) { Control->StartInitialAlignment(); }
			else if (Index == 1) { Control->ConfirmCalibration(); }
			else { Control->SaveCalibration(); }
		}
		return FReply::Handled();
	}

	bool CanRunAlignmentStep(TWeakObjectPtr<UCXMRTuningWindowComponent> Control, int32 Index)
	{
		if (!Control.IsValid()) { return false; }
		if (Index == 0) { return Control->CanStartInitialAlignment() || Control->IsInitialAlignmentPending(); }
		return Index == 1 ? Control->CanConfirmCalibration() : Control->CanSaveCalibration();
	}

	UCXMRPlacementComponent* FindPlacement(TWeakObjectPtr<UCXMRTuningWindowComponent> Control)
	{
		return Control.IsValid() ? Control->FindPlacement() : nullptr;
	}

	FText AlignmentScope(TWeakObjectPtr<UCXMRTuningWindowComponent> Control)
	{
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		const FName Group = Placement ? Placement->GetAlignmentGroup() : NAME_None;
		return Group.IsNone() ? LOCTEXT("VehicleAlignmentScope", "Alignment: Vehicle only")
			: FText::Format(LOCTEXT("GroupAlignmentScope", "Alignment Group: {0}. Save Alignment and Reset Group Alignment affect all vehicles in this group."), FText::FromName(Group));
	}

	FText AlignmentStorage(TWeakObjectPtr<UCXMRTuningWindowComponent> Control)
	{
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		return Placement ? Placement->GetAlignmentStorageMessage() : FText::GetEmpty();
	}

	FText EyeReferenceHelp(TWeakObjectPtr<UCXMRTuningWindowComponent> Control)
	{
		FTransform Eye;
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		return Placement && Placement->GetDriverEyeWorld(Eye)
			? LOCTEXT("EyeReady", "Driver Eye Reference is configured. Turns pivot around this point.")
			: LOCTEXT("EyeNotConfigured", "Set Driver Eye Reference in the vehicle Data Asset. Use imported vehicle-local coordinates; +X faces forward.");
	}

	EVisibility RestoreVisibility(TWeakObjectPtr<UCXMRTuningWindowComponent> Control)
	{
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		return Placement && Placement->NeedsRestoreConfirmation() ? EVisibility::Visible : EVisibility::Collapsed;
	}

	EVisibility GroupResetVisibility(TWeakObjectPtr<UCXMRTuningWindowComponent> Control)
	{
		const UCXMRPlacementComponent* Placement = FindPlacement(Control);
		return Placement && !Placement->GetAlignmentGroup().IsNone() ? EVisibility::Visible : EVisibility::Collapsed;
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
			{TEXT("Vehicle.Markers"), TEXT("Marker Status"), TEXT("Current marker observations and calibration status.")},
			{TEXT("Vehicle.LayoutFit"), TEXT("Alignment Error"), TEXT("Difference between the saved marker layout and observations. Lower is better.")},
			{TEXT("Vehicle.MoveStep"), TEXT("Move Step"), TEXT("Distance per adjustment step.")},
			{TEXT("Vehicle.YawStep"), TEXT("Rotation Step"), TEXT("Angle per rotation step.")},
			{TEXT("Vehicle.Away"), TEXT("Distance: Away (+) / Closer (-)"), TEXT("Move the vehicle along the current adjustment heading.")},
			{TEXT("Vehicle.Right"), TEXT("Lateral: Right (+) / Left (-)"), TEXT("Move sideways along the current adjustment heading.")},
			{TEXT("Vehicle.Up"), TEXT("Height: Up (+) / Down (-)"), TEXT("Adjust vehicle height.")},
			{TEXT("Vehicle.Turn"), TEXT("Yaw: Clockwise (+) / Counterclockwise (-)"), TEXT("Rotate around the selected pivot.")},
			{TEXT("Vehicle.ResetOffset"), TEXT("Reset Adjustment"), TEXT("Discard unsaved adjustments and return to the current base pose.")},
			{TEXT("Vehicle.LearnMarkers"), TEXT("Learn Markers from This Pose (Saves Immediately)"), TEXT("Learn and save recently observed markers relative to the current vehicle pose.")},
			{TEXT("Vehicle.SaveOffset"), TEXT("Save Adjustment Now"), TEXT("Apply the adjustment to the marker layout. Prefer the guided save above.")},
			{TEXT("Vehicle.Recalibrate"), TEXT("Re-read Markers"), TEXT("Clear calibration and solve again from markers.")},
			{TEXT("MR.MixedReality"), TEXT("Passthrough"), TEXT("Show the camera view with the virtual vehicle.")},
			{TEXT("MR.VRBackground"), TEXT("Virtual Background"), TEXT("Show the virtual sky and floor.")},
			{TEXT("MR.Masking"), TEXT("Scene Masks"), TEXT("Show passthrough through masks placed in the level.")},
			{TEXT("MR.ViewOffsetGlide"), TEXT("View Transition Time"), TEXT("Transition duration between eye and camera origins.")},
			{TEXT("MR.ViewOffset"), TEXT("View Origin: Eye 0 / Camera 1"), TEXT("Affects real and virtual alignment. Adjust while checking in the headset.")},
			{TEXT("Depth.Test"), TEXT("Depth Test"), TEXT("Use depth to show real objects in front of virtual objects.")},
			{TEXT("Depth.Range"), TEXT("Limit Depth Range"), TEXT("Apply depth testing only within this range. Cable depth accuracy depends on the headset.")},
			{TEXT("Depth.NearZ"), TEXT("Depth Near"), TEXT("Distance from the camera. Must be less than Depth Far.")},
			{TEXT("Depth.FarZ"), TEXT("Depth Far"), TEXT("Real-object occlusion may stop beyond this range.")},
			{TEXT("Depth.EnvEstimation"), TEXT("Environment Depth"), TEXT("Runtime environment depth estimation. Requires a headset session.")},
			{TEXT("Port.Live"), TEXT("Hand Contact Status"), TEXT("Contact with the generated tracked hand. Does not measure cable insertion.")},
			{TEXT("Port.Debug"), TEXT("Show USB Contact Bounds"), TEXT("Draw contact and release margins at the USB centre.")},
			{TEXT("Port.HandContactDistance"), TEXT("Hand Contact Margin"), TEXT("Extra distance beyond the generated hand surface that counts as touch.")},
			{TEXT("Port.HandReleaseDistance"), TEXT("Hand Release Margin"), TEXT("Release when the hand moves beyond this margin. Keep larger than the contact margin.")},
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
		if (Id.StartsWith(TEXT("Vehicle."))) { Row.Category = LOCTEXT("Placement", "Placement Adjustment"); }
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
		return SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text(LOCTEXT("NoFeature", "This feature is not available in the current session.")).AutoWrapText(true);
	}
	Rows.StableSort(CompareRows);
	return CXMRPanelUI::MakeRowList(Rows);
}

FText SCXMRControlPanel::Status() const
{
	return Control.IsValid() ? Control->GetAlignmentStatus(Viewer.IsValid() ? Viewer->FindLoader() : nullptr) : FText::GetEmpty();
}

TSharedRef<SWidget> SCXMRControlPanel::Navigation(ECXMRControlPage Page, const FText& Label)
{
	const TWeakObjectPtr<UCXMRTuningWindowComponent> Weak = Control;
	return SNew(SButton).ButtonStyle(CXMRPanelUI::ButtonStyle()).TextStyle(CXMRPanelUI::BodyTextStyle())
		.ContentPadding(FMargin(16.f, 14.f)).HAlign(HAlign_Left)
		.ButtonColorAndOpacity_Lambda([Weak, Page] { return Weak.IsValid() && Weak->GetActivePage() == Page ? FLinearColor::FromSRGBColor(FColor(222, 235, 253)) : FLinearColor::White; })
		.Text(Label).OnClicked_Lambda([Weak, Page] { if (Weak.IsValid()) { Weak->SelectPage(Page); } return FReply::Handled(); });
}

TSharedRef<SWidget> SCXMRControlPanel::CalibrationPage()
{
	const TWeakObjectPtr<UCXMRTuningWindowComponent> Weak = Control;
	TSharedRef<SHorizontalBox> Steps = SNew(SHorizontalBox);
	const FText Labels[] = { LOCTEXT("Initial", "1  Align to eyes"), LOCTEXT("Confirm", "2  Confirm fit"), LOCTEXT("SaveAlignment", "3  Save Alignment") };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Steps->AddSlot().FillWidth(1.f).Padding(3.f)
		[
			SNew(SButton).ButtonStyle(CXMRPanelUI::ButtonStyle()).TextStyle(CXMRPanelUI::BodyTextStyle()).Text_Lambda([Weak, Index, Label = Labels[Index]] { return Index == 0 && Weak.IsValid() && Weak->IsInitialAlignmentPending() ? LOCTEXT("CancelInitial", "Cancel alignment") : Label; }).ContentPadding(FMargin(8.f, 12.f))
			.IsEnabled_Lambda([Weak, Index] { return CanRunAlignmentStep(Weak, Index); })
			.OnClicked_Lambda([Weak, Index] { return RunAlignmentStep(Weak, Index); })
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
		[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text_Lambda([Weak] { return AlignmentScope(Weak); }).AutoWrapText(true).ColorAndOpacity(CXMRPanelUI::HeaderColor()) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 0.f)
		[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text_Lambda([Weak] { return AlignmentStorage(Weak); }).AutoWrapText(true).ColorAndOpacity(CXMRPanelUI::ReadoutColor()) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
		[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text_Lambda([Weak] { return EyeReferenceHelp(Weak); }).AutoWrapText(true).ColorAndOpacity(CXMRPanelUI::HeaderColor()) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(7.f)[ Steps ]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
		[ SNew(SButton).ButtonStyle(CXMRPanelUI::ButtonStyle()).TextStyle(CXMRPanelUI::BodyTextStyle()).Text(LOCTEXT("AcceptRestored", "Use Restored Alignment"))
			.Visibility_Lambda([Weak] { return RestoreVisibility(Weak); })
			.OnClicked_Lambda([Weak] { if (Weak.IsValid()) { Weak->AcceptRestoredAlignment(); } return FReply::Handled(); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
		[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text_Lambda([Weak] { return Weak.IsValid() ? Weak->GetCalibrationMessage() : FText::GetEmpty(); }).AutoWrapText(true).ColorAndOpacity(CXMRPanelUI::ReadoutColor()) ]
		+ SVerticalBox::Slot().AutoHeight()[ RegistryPage(ECXMRControlPage::Calibration) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 18.f)
		[
			SNew(SExpandableArea).InitiallyCollapsed(true)
			.BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush"))
			.BorderBackgroundColor(CXMRPanelUI::BackgroundColor())
			.BodyBorderImage(FCoreStyle::Get().GetBrush("NoBrush"))
			.HeaderPadding(FMargin(10.f))
			.HeaderContent()[ SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text(LOCTEXT("AdvancedPlacement", "Advanced Placement")).ColorAndOpacity(CXMRPanelUI::InkColor()) ]
			.BodyContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
				[ SNew(SButton).ButtonStyle(CXMRPanelUI::ButtonStyle()).TextStyle(CXMRPanelUI::BodyTextStyle()).Text(LOCTEXT("ReadSavedAgain", "Restore from Markers Again"))
					.OnClicked_Lambda([Weak] { if (Weak.IsValid()) { Weak->StartCalibration(); } return FReply::Handled(); }) ]
				+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
				[ SNew(SButton).ButtonStyle(CXMRPanelUI::ButtonStyle()).TextStyle(CXMRPanelUI::BodyTextStyle()).Text_Lambda([Weak] { return Weak.IsValid() ? Weak->GetSharedAlignmentResetLabel() : FText::GetEmpty(); })
					.ToolTipText(LOCTEXT("ResetGroupHelp", "Click twice within five seconds to delete the shared alignment for this group. Reset Adjustment only discards temporary adjustments."))
					.Visibility_Lambda([Weak] { return GroupResetVisibility(Weak); })
					.OnClicked_Lambda([Weak] { if (Weak.IsValid()) { Weak->ResetSharedAlignment(); } return FReply::Handled(); }) ]
				+ SVerticalBox::Slot().AutoHeight()[ RegistryPage(ECXMRControlPage::Calibration, true) ]
			]
		];
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
	Control = Args._Control; Tuning = Args._Tuning; Viewer = Args._Viewer;
	const TWeakObjectPtr<UCXMRTuningWindowComponent> Weak = Control;
	TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	Menu->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[Navigation(ECXMRControlPage::Vehicle, LOCTEXT("Review", "Review"))];
	Menu->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 8.f)[Navigation(ECXMRControlPage::Calibration, LOCTEXT("Alignment", "Alignment"))];
	Menu->AddSlot().AutoHeight()[Navigation(ECXMRControlPage::Display, LOCTEXT("Settings", "Settings"))];
	TSharedRef<SHorizontalBox> Modes = SNew(SHorizontalBox);
	for (bool bMR : {true, false})
	{
		Modes->AddSlot().AutoWidth().Padding(4.f, 0.f)
		[SNew(SButton).ButtonStyle(CXMRPanelUI::ButtonStyle()).TextStyle(CXMRPanelUI::BodyTextStyle()).ContentPadding(FMargin(16.f, 12.f))
			.Text(bMR ? LOCTEXT("RealEnvironment", "MR  Real environment") : LOCTEXT("VirtualEnvironment", "VR  Virtual environment"))
			.IsEnabled_Lambda([Weak] { return CanSwitchMode(Weak); })
			.ToolTipText(LOCTEXT("ModeHelp", "Select the environment blend mode. Requires the runtime environment-blend control; the renderer and view configuration remain fixed."))
			.ButtonColorAndOpacity_Lambda([Weak, bMR] { return ModeColor(Weak, bMR); })
			.OnClicked_Lambda([Weak, bMR] { return SwitchMode(Weak, bMR); })];
	}
	TSharedRef<SVerticalBox> Settings = SNew(SVerticalBox);
	Settings->AddSlot().AutoHeight().Padding(0.f, 0.f, 0.f, 16.f)
	[CXMRPanelUI::MakeSurface(SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular", 13)).Text(LOCTEXT("Pipeline", "Separate MR and VR exposure. Camera settings are in Varjo Base. Real-object occlusion is available only in MR.")).AutoWrapText(true))];
	Settings->AddSlot().AutoHeight()[RegistryPage(ECXMRControlPage::Display)];
	if (Args._Viewer) { Settings->AddSlot().AutoHeight()[Args._Viewer->BuildDisplayPage()]; }
	SAssignNew(ReviewHost, SBox);
	if (Args._Viewer) { ReviewHost->SetContent(Args._Viewer->BuildViewerPage()); }
	else { ReviewHost->SetContent(SNew(STextBlock).Text(LOCTEXT("NoSession", "Start a session to review vehicles and design options."))); }
	ChildSlot
	[CXMRPanelUI::MakeBackground(SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(24.f, 22.f, 20.f, 12.f)
		[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)[SNew(STextBlock).Text(LOCTEXT("Title", "CXMR Design Review")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 22)).ColorAndOpacity(CXMRPanelUI::InkColor())]
			+ SHorizontalBox::Slot().AutoWidth()[Modes]]
		+ SVerticalBox::Slot().AutoHeight().Padding(24.f, 0.f, 24.f, 20.f)
		[SNew(STextBlock).Text(this, &SCXMRControlPanel::Status).Font(FCoreStyle::GetDefaultFontStyle("Regular", 14)).AutoWrapText(true).ColorAndOpacity(CXMRPanelUI::ReadoutColor())]
		+ SVerticalBox::Slot().FillHeight(1.f)
		[SNew(SHorizontalBox)
			+ SHorizontalBox::Slot().AutoWidth().Padding(20.f, 0.f, 20.f, 0.f)[SNew(SBox).WidthOverride(156.f)[Menu]]
			+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 24.f, 0.f)
			[SNew(SWidgetSwitcher).WidgetIndex_Lambda([Weak] { return Weak.IsValid() && Weak->GetActivePage() == ECXMRControlPage::Calibration ? 1 : Weak.IsValid() && Weak->GetActivePage() == ECXMRControlPage::Display ? 2 : 0; })
				+ SWidgetSwitcher::Slot()[CXMRPanelUI::MakeScroll(ReviewHost.ToSharedRef())]
				+ SWidgetSwitcher::Slot()[CXMRPanelUI::MakeScroll(CalibrationPage())]
				+ SWidgetSwitcher::Slot()[CXMRPanelUI::MakeScroll(Settings)]]]
		+ SVerticalBox::Slot().AutoHeight().Padding(24.f, 18.f)
		[SNew(STextBlock).Text(LOCTEXT("SaveHelp", "Exposure presets save automatically. Alignment changes save only when you select Save Alignment.")).Font(FCoreStyle::GetDefaultFontStyle("Regular", 12)).ColorAndOpacity(CXMRPanelUI::ReadoutColor())])];
}

#undef LOCTEXT_NAMESPACE
