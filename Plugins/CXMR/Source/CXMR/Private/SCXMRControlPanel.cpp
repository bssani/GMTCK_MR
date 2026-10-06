// Copyright GMTCK CX.

#include "SCXMRControlPanel.h"
#include "CXMRControlPanelWidget.h"
#include "CXMRPanelUI.h"
#include "CXMRPlacementComponent.h"
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
			{TEXT("Display.Exposure"), TEXT("Virtual Exposure"), TEXT("Virtual scene exposure. Adjust camera exposure in Varjo Base.")},
			{TEXT("Port.Live"), TEXT("Nearest USB Status"), TEXT("Plug distance and angle estimated from hand joints. Does not measure insertion.")},
			{TEXT("Port.Debug"), TEXT("Show USB Detection Bounds"), TEXT("Draw the distance and angle limits.")},
			{TEXT("Port.NearDistance"), TEXT("USB Near Distance"), TEXT("Give near feedback within this distance, regardless of orientation.")},
			{TEXT("Port.EnterDistance"), TEXT("Alignment Enter Distance"), TEXT("Hold the distance and angle conditions briefly to confirm alignment.")},
			{TEXT("Port.ExitDistance"), TEXT("Alignment Exit Distance"), TEXT("Keep larger than the enter distance to reduce flicker.")},
			{TEXT("Port.MaxAngle"), TEXT("Maximum Angle"), TEXT("Allowed angle between the plug and insertion axis.")},
			{TEXT("Port.ApproachDistance"), TEXT("Approach Distance"), TEXT("Distance at which the nearest USB starts approach feedback.")},
			{TEXT("PlugTip.Reach"), TEXT("Plug Tip Reach"), TEXT("Match the estimated tip to the physical plug tip.")},
			{TEXT("PlugTip.State"), TEXT("Plug Hand Tracking"), TEXT("Hand tracking or desktop preview status.")},
			{TEXT("PlugTip.Debug"), TEXT("Show Estimated Plug Tip"), TEXT("Draw the estimated plug tip.")},
			{TEXT("PlugTip.Preview"), TEXT("Hand Preview (No Headset)"), TEXT("Use a fixed hand pose to preview USB feedback on the desktop.")},
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
		else if (Id.StartsWith(TEXT("Depth."))) { Row.Category = LOCTEXT("Depth", "Occlusion / Depth"); }
		else if (Id.StartsWith(TEXT("MR."))) { Row.Category = LOCTEXT("MR", "Passthrough"); }
		else if (Id.StartsWith(TEXT("Port."))) { Row.Category = LOCTEXT("USB", "USB Feedback"); }
		else if (Id.StartsWith(TEXT("PlugTip."))) { Row.Category = LOCTEXT("Plug", "Plug Tip Estimate"); }
		else if (Id.StartsWith(TEXT("Display."))) { Row.Category = LOCTEXT("Picture", "Virtual Image"); }
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
			const bool bPlacement = Id.StartsWith(TEXT("Vehicle.")) || Id.StartsWith(TEXT("Box.")) || Id.StartsWith(TEXT("Input."));
			const bool bDisplay = Id.StartsWith(TEXT("MR.")) || Id.StartsWith(TEXT("Depth.")) || Id.StartsWith(TEXT("Display.")) || Id.StartsWith(TEXT("Spectator."));
			const bool bUSB = Id.StartsWith(TEXT("Port.")) || Id.StartsWith(TEXT("PlugTip."));
			bool bInclude = Page == ECXMRControlPage::Display ? bDisplay
				: Page == ECXMRControlPage::USB ? bUSB
				: Page == ECXMRControlPage::Diagnostics ? !bPlacement && !bDisplay && !bUSB
				: Page == ECXMRControlPage::Calibration && bPlacement && (bAdvanced != IsBasicCalibration(Registered->Id));
			if (Registered->Id == "Vehicle.PlaceInFront" || Registered->Id == "Vehicle.Recalibrate"
				|| Registered->Id == "Vehicle.SaveOffset" || Registered->Id == "Vehicle.LearnMarkers"
				|| Registered->Id == "Vehicle.Pivot") { bInclude = false; }
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
		return SNew(STextBlock).Text(LOCTEXT("NoFeature", "This feature is not available in the current session.")).AutoWrapText(true);
	}
	return CXMRPanelUI::MakeRowList(Rows);
}

FText SCXMRControlPanel::Status() const
{
	const UGameInstance* GI = Control.IsValid() && Control->GetWorld() ? Control->GetWorld()->GetGameInstance() : nullptr;
	const UCXMRSubsystem* CXMR = GI ? GI->GetSubsystem<UCXMRSubsystem>() : nullptr;
	const UCXMRPlacementComponent* Placement = Control.IsValid() ? Control->FindPlacement() : nullptr;
	const FString Vehicle = CXMR && CXMR->GetVehicleCount() > 0 ? CXMR->GetVehicleName().ToString() : TEXT("No vehicle");
	return FText::FromString(FString::Printf(TEXT("%s   |   Passthrough %s   |   %s"), *Vehicle,
		CXMR && CXMR->IsMixedRealityOn() ? TEXT("On") : TEXT("Off"),
		Placement && Placement->NeedsRestoreConfirmation() ? TEXT("Restore needs confirmation")
		: Placement && Placement->IsManualAlignment() ? TEXT("Manual alignment")
		: Placement && Placement->bCalibrated ? TEXT("Placed") : TEXT("Alignment pending")));
}

TSharedRef<SWidget> SCXMRControlPanel::Navigation(ECXMRControlPage Page, const FText& Label)
{
	const TWeakObjectPtr<UCXMRTuningWindowComponent> Weak = Control;
	return SNew(SButton)
		.ContentPadding(FMargin(14.f, 11.f)).HAlign(HAlign_Left)
		.Visibility_Lambda([Weak, Page] { return Page < ECXMRControlPage::Display || (Weak.IsValid() && Weak->IsSetupMode()) ? EVisibility::Visible : EVisibility::Collapsed; })
		.ButtonColorAndOpacity_Lambda([Weak, Page] { return FSlateColor(Weak.IsValid() && Weak->GetActivePage() == Page ? CXMRPanelUI::HeaderColor() * 0.55f : FLinearColor(0.07f, 0.08f, 0.1f)); })
		.Text(Label)
		.OnClicked_Lambda([Weak, Page] { if (Weak.IsValid()) { Weak->SelectPage(Page); } return FReply::Handled(); });
}

TSharedRef<SWidget> SCXMRControlPanel::CalibrationPage()
{
	const TWeakObjectPtr<UCXMRTuningWindowComponent> Weak = Control;
	TSharedRef<SHorizontalBox> Steps = SNew(SHorizontalBox);
	const FText Labels[] = { LOCTEXT("Initial", "1  Initial Alignment"), LOCTEXT("Confirm", "2  Confirm Alignment"), LOCTEXT("SaveAlignment", "3  Save Alignment") };
	for (int32 Index = 0; Index < 3; ++Index)
	{
		Steps->AddSlot().FillWidth(1.f).Padding(3.f)
		[
			SNew(SButton).Text_Lambda([Weak, Index, Label = Labels[Index]] { return Index == 0 && Weak.IsValid() && Weak->IsInitialAlignmentPending() ? LOCTEXT("CancelInitial", "Cancel Initial Alignment") : Label; }).ContentPadding(FMargin(8.f, 12.f))
			.IsEnabled_Lambda([Weak, Index] { return Weak.IsValid() && (Index == 0 ? (Weak->CanStartInitialAlignment() || Weak->IsInitialAlignmentPending()) : (Index == 1 ? Weak->CanConfirmCalibration() : Weak->CanSaveCalibration())); })
			.OnClicked_Lambda([Weak, Index]
			{
				if (Weak.IsValid())
				{
					if (Index == 0) { Weak->StartInitialAlignment(); }
					else if (Index == 1) { Weak->ConfirmCalibration(); }
					else { Weak->SaveCalibration(); }
				}
				return FReply::Handled();
			})
		];
	}
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
		[ SNew(STextBlock).Text(LOCTEXT("EyeHelp", "First setup: sit in the driver seat and face straight ahead. Align once, fine-adjust, then save. Later sessions restore from markers.")).AutoWrapText(true) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
		[ SNew(STextBlock).Text_Lambda([Weak]
		{
			FTransform Eye;
			UCXMRPlacementComponent* Placement = Weak.IsValid() ? Weak->FindPlacement() : nullptr;
			return Placement && Placement->GetDriverEyeWorld(Eye)
				? LOCTEXT("EyeReady", "Driver Eye Reference is configured. Turns pivot around this point.")
				: LOCTEXT("EyeNotConfigured", "Set Driver Eye Reference in the vehicle Data Asset. Use imported vehicle-local coordinates; +X faces forward.");
		}).AutoWrapText(true).ColorAndOpacity(CXMRPanelUI::HeaderColor()) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(7.f)[ Steps ]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
		[ SNew(SButton).Text(LOCTEXT("AcceptRestored", "Use Restored Alignment"))
			.Visibility_Lambda([Weak] { const UCXMRPlacementComponent* Placement = Weak.IsValid() ? Weak->FindPlacement() : nullptr; return Placement && Placement->NeedsRestoreConfirmation() ? EVisibility::Visible : EVisibility::Collapsed; })
			.OnClicked_Lambda([Weak] { if (Weak.IsValid()) { Weak->AcceptRestoredAlignment(); } return FReply::Handled(); }) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
		[ SNew(STextBlock).Text_Lambda([Weak] { return Weak.IsValid() ? Weak->GetCalibrationMessage() : FText::GetEmpty(); }).AutoWrapText(true).ColorAndOpacity(CXMRPanelUI::ReadoutColor()) ]
		+ SVerticalBox::Slot().AutoHeight()[ RegistryPage(ECXMRControlPage::Calibration) ]
		+ SVerticalBox::Slot().AutoHeight().Padding(10.f, 18.f)
		[
			SNew(SExpandableArea).InitiallyCollapsed(true)
			.Visibility_Lambda([Weak] { return Weak.IsValid() && Weak->IsSetupMode() ? EVisibility::Visible : EVisibility::Collapsed; })
			.HeaderContent()[ SNew(STextBlock).Text(LOCTEXT("AdvancedPlacement", "Advanced Placement")) ]
			.BodyContent()
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(10.f)
				[ SNew(SButton).Text(LOCTEXT("ReadSavedAgain", "Restore from Markers Again"))
					.OnClicked_Lambda([Weak] { if (Weak.IsValid()) { Weak->StartCalibration(); } return FReply::Handled(); }) ]
				+ SVerticalBox::Slot().AutoHeight()[ RegistryPage(ECXMRControlPage::Calibration, true) ]
			]
		];
}

void SCXMRControlPanel::Construct(const FArguments& Args)
{
	Control = Args._Control;
	Tuning = Args._Tuning;
	const TWeakObjectPtr<UCXMRTuningWindowComponent> Weak = Control;
	TSharedRef<SVerticalBox> Menu = SNew(SVerticalBox);
	const FText Labels[] = { LOCTEXT("Vehicle", "Vehicle"), LOCTEXT("View", "View"), LOCTEXT("Calibration", "Placement"), LOCTEXT("Display", "Display / Depth"), LOCTEXT("USBPage", "USB / Hands"), LOCTEXT("Diagnostics", "Diagnostics") };
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Menu->AddSlot().AutoHeight().Padding(0.f, 2.f)[ Navigation(static_cast<ECXMRControlPage>(Index), Labels[Index]) ];
	}
	auto ViewerPage = [&Args](bool bDisplay) -> TSharedRef<SWidget>
	{
		if (Args._Viewer) { return bDisplay ? Args._Viewer->BuildDisplayPage() : Args._Viewer->BuildViewerPage(); }
		return SNew(STextBlock).Text(LOCTEXT("NoSession", "Start a session to control the vehicle."));
	};
	ChildSlot
	[
		CXMRPanelUI::MakeBackground(SNew(SVerticalBox)
			+ SVerticalBox::Slot().AutoHeight().Padding(16.f, 12.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().FillWidth(1.f).VAlign(VAlign_Center)
				[ SNew(STextBlock).Text(LOCTEXT("Title", "CXMR Control")).Font(FCoreStyle::GetDefaultFontStyle("Bold", 16)) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)
				[ SNew(SButton).Text(LOCTEXT("UseMode", "Use")).ContentPadding(FMargin(16.f, 6.f))
					.ButtonColorAndOpacity_Lambda([Weak] { return FSlateColor(Weak.IsValid() && !Weak->IsSetupMode() ? CXMRPanelUI::HeaderColor() * 0.55f : FLinearColor(0.1f, 0.11f, 0.13f)); })
					.OnClicked_Lambda([Weak] { if (Weak.IsValid()) { Weak->SetSetupMode(false); } return FReply::Handled(); }) ]
				+ SHorizontalBox::Slot().AutoWidth().Padding(3.f)
				[ SNew(SButton).Text(LOCTEXT("SetupMode", "Setup")).ContentPadding(FMargin(16.f, 6.f))
					.ButtonColorAndOpacity_Lambda([Weak] { return FSlateColor(Weak.IsValid() && Weak->IsSetupMode() ? CXMRPanelUI::HeaderColor() * 0.55f : FLinearColor(0.1f, 0.11f, 0.13f)); })
					.OnClicked_Lambda([Weak] { if (Weak.IsValid()) { Weak->SetSetupMode(true); } return FReply::Handled(); }) ]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.f, 0.f, 16.f, 12.f)
			[ SNew(STextBlock).Text(this, &SCXMRControlPanel::Status).AutoWrapText(true).ColorAndOpacity(CXMRPanelUI::ReadoutColor()) ]
			+ SVerticalBox::Slot().FillHeight(1.f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot().AutoWidth().Padding(10.f, 0.f, 12.f, 0.f)[ SNew(SBox).WidthOverride(150.f)[ Menu ] ]
				+ SHorizontalBox::Slot().FillWidth(1.f).Padding(0.f, 0.f, 14.f, 0.f)
				[
					SNew(SWidgetSwitcher).WidgetIndex_Lambda([Weak] { return Weak.IsValid() ? static_cast<int32>(Weak->GetActivePage()) : 0; })
					+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(ViewerPage(false)) ]
					+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(ViewerPage(true)) ]
					+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(CalibrationPage()) ]
					+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(RegistryPage(ECXMRControlPage::Display)) ]
					+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(RegistryPage(ECXMRControlPage::USB)) ]
					+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(RegistryPage(ECXMRControlPage::Diagnostics)) ]
				]
			]
			+ SVerticalBox::Slot().AutoHeight().Padding(16.f, 12.f)
			[ SNew(STextBlock).Text(LOCTEXT("SettingsHelp", "Settings save automatically. Vehicle alignment saves only when you select Save Alignment.")).AutoWrapText(true).ColorAndOpacity(FLinearColor(0.6f, 0.63f, 0.68f)) ]
		)
	];
}

#undef LOCTEXT_NAMESPACE
