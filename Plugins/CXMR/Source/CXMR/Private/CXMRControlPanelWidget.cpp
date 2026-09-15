// Copyright GMTCK CX.

#include "CXMRControlPanelWidget.h"
#include "CXMRPanelUI.h"
#include "CXMRMarkerDebugComponent.h"
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
	ActiveTab = FMath::Clamp(TabIndex, 0, 2);   // the switcher and the tab bar read it on their next paint
}

TSharedRef<SWidget> UCXMRControlPanelWidget::RebuildWidget()
{
	// The base does the user-widget bookkeeping (initialisation, player context). Its content — the widget tree,
	// empty for this class — is not used.
	Super::RebuildWidget();

	const TWeakObjectPtr<UCXMRControlPanelWidget> Weak(this);
	const TArray<FText> Tabs = {
		LOCTEXT("TabDisplay", "Display"),
		LOCTEXT("TabCalibration", "Calibration"),
		LOCTEXT("TabViewer", "Viewer") };

	SAssignNew(Pages, SWidgetSwitcher)
		.WidgetIndex_Lambda([Weak] { return Weak.IsValid() ? Weak->ActiveTab : 0; })
		+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(BuildDisplayPage()) ]
		+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(BuildCalibrationPage()) ]
		+ SWidgetSwitcher::Slot()[ CXMRPanelUI::MakeScroll(BuildViewerPage()) ];

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

// ---- Display: what the wearer sees ----

TSharedRef<SWidget> UCXMRControlPanelWidget::BuildDisplayPage()
{
	const TWeakObjectPtr<UCXMRControlPanelWidget> Weak(this);
	auto CXMR = [Weak]() -> UCXMRSubsystem* { return Weak.IsValid() ? Weak->GetCXMR() : nullptr; };

	FRows Rows;
	auto Toggle = [&Rows, CXMR](FName Id, const FText& Category, const FText& Label,
		TFunction<bool(const UCXMRSubsystem&)> IsOn, TFunction<void(UCXMRSubsystem&, bool)> SetOn,
		TFunction<bool(const UCXMRSubsystem&)> IsSupported)
	{
		FCXMRTunable Row = MakeRow(Id, Category, Label, ECXMRTunableKind::Bool);
		Row.Get = [CXMR, IsOn] { const UCXMRSubsystem* S = CXMR(); return (S && IsOn(*S)) ? 1.0f : 0.0f; };
		Row.Set = [CXMR, SetOn](float Value) { if (UCXMRSubsystem* S = CXMR()) { SetOn(*S, Value > 0.5f); } };
		if (IsSupported)
		{
			// Greyed out, not hidden, on a headset that cannot do it — the operator should see it is unavailable.
			Row.IsEnabled = [CXMR, IsSupported] { const UCXMRSubsystem* S = CXMR(); return S && IsSupported(*S); };
		}
		AddRow(Rows, MoveTemp(Row));
	};

	const FText MR = LOCTEXT("CatMR", "Mixed reality");
	Toggle("Panel.MR", MR, LOCTEXT("MR", "Mixed reality (passthrough)"),
		[](const UCXMRSubsystem& S) { return S.IsMixedRealityOn(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetMixedReality(bOn); },
		[](const UCXMRSubsystem& S) { return S.IsMixedRealitySupported(); });
	Toggle("Panel.VRBackground", MR, LOCTEXT("VRBackground", "VR background (sky, floor)"),
		[](const UCXMRSubsystem& S) { return S.IsVRBackgroundVisible(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetVRBackgroundVisible(bOn); },
		nullptr);
	{
		// A mode, not a switch: both positions are deliberate choices, so it is a choice button rather than a checkbox.
		FCXMRTunable Row = MakeRow("Panel.ViewOffset", MR, LOCTEXT("ViewOffset", "Render from"), ECXMRTunableKind::Choice);
		Row.Options = { LOCTEXT("Eye", "Eyes"), LOCTEXT("Camera", "Cameras") };
		Row.Get = [CXMR] { const UCXMRSubsystem* S = CXMR(); return (S && S->GetViewOffset() > 0.5f) ? 1.0f : 0.0f; };
		Row.Set = [CXMR](float Value) { if (UCXMRSubsystem* S = CXMR()) { S->TransitionViewOffset(Value > 0.5f ? 1.0f : 0.0f, S->ViewOffsetTransitionSeconds); } };
		AddRow(Rows, MoveTemp(Row));
	}
	Toggle("Panel.Masking", MR, LOCTEXT("Masking", "Masking (mask meshes cut through)"),
		[](const UCXMRSubsystem& S) { return S.IsMaskingOn(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetMasking(bOn); },
		nullptr);

	const FText Depth = LOCTEXT("CatDepth", "Depth");
	Toggle("Panel.DepthTest", Depth, LOCTEXT("DepthTest", "Depth test (real in front of virtual)"),
		[](const UCXMRSubsystem& S) { return S.IsDepthTestOn(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetDepthTest(bOn); },
		nullptr);
	Toggle("Panel.EnvDepth", Depth, LOCTEXT("EnvDepth", "Environment depth estimation"),
		[](const UCXMRSubsystem& S) { return S.IsEnvironmentDepthEstimationOn(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetEnvironmentDepthEstimation(bOn); },
		nullptr);
	Toggle("Panel.DepthRange", Depth, LOCTEXT("DepthRange", "Limit depth test to range"),
		[](const UCXMRSubsystem& S) { return S.IsDepthTestRangeOn(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetDepthTestRange(bOn, S.GetDepthTestRangeNearZ(), S.GetDepthTestRangeFarZ()); },
		nullptr);
	{
		FCXMRTunable Row = MakeRow("Panel.DepthRangeValue", Depth, LOCTEXT("DepthRangeValue", "Range"), ECXMRTunableKind::Readout);
		Row.Text = [CXMR]
		{
			const UCXMRSubsystem* S = CXMR();
			if (!S)
			{
				return FText::GetEmpty();
			}
			// "unbounded" is what the compositor really gets with the range off (farZ = HUGE_VALF), and it is the
			// state that makes the room flicker, so it is worth naming.
			return S->IsDepthTestRangeOn()
				? FText::FromString(FString::Printf(TEXT("%.2f - %.2f m"), S->GetDepthTestRangeNearZ(), S->GetDepthTestRangeFarZ()))
				: LOCTEXT("Unbounded", "unbounded");
		};
		AddRow(Rows, MoveTemp(Row));
	}

	const FText Tracking = LOCTEXT("CatTracking", "Tracking");
	Toggle("Panel.Markers", Tracking, LOCTEXT("Markers", "Marker tracking"),
		[](const UCXMRSubsystem& S) { return S.IsMarkerTrackingOn(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetMarkerTracking(bOn); },
		[](const UCXMRSubsystem& S) { return S.IsMarkerTrackingSupported(); });
	Toggle("Panel.Hands", Tracking, LOCTEXT("Hands", "Hand skeleton"),
		[](const UCXMRSubsystem& S) { return S.IsHandVisualizationOn(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetHandVisualization(bOn); },
		nullptr);
	{
		// The console variable CXMR.DebugMarkers underneath, so this checkbox and the console are one switch.
		FCXMRTunable Row = MakeRow("Panel.MarkerLabels", Tracking, LOCTEXT("MarkerLabels", "Marker axes and labels (ID, mode)"), ECXMRTunableKind::Bool);
		Row.Get = [] { return UCXMRMarkerDebugComponent::IsMarkerDrawingOn() ? 1.0f : 0.0f; };
		Row.Set = [](float Value) { UCXMRMarkerDebugComponent::SetMarkerDrawing(Value > 0.5f); };
		AddRow(Rows, MoveTemp(Row));
	}

	const FText Eyes = LOCTEXT("CatEyes", "Eyes");
	Toggle("Panel.Gaze", Eyes, LOCTEXT("Gaze", "Gaze dot"),
		[](const UCXMRSubsystem& S) { return S.IsGazeVisualizationOn(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetGazeVisualization(bOn); },
		nullptr);
	Toggle("Panel.Foveation", Eyes, LOCTEXT("Foveation", "Foveated area overlay"),
		[](const UCXMRSubsystem& S) { return S.IsFoveationVisualizationOn(); },
		[](UCXMRSubsystem& S, bool bOn) { S.SetFoveationVisualization(bOn); },
		nullptr);

	{
		// The spectator camera is a pawn component, so the row goes through its tuning entry, which also saves the choice.
		FCXMRTunable Row = MakeRow("Panel.Spectator", LOCTEXT("CatMonitor", "Monitor"), LOCTEXT("Spectator", "Monitor shows"), ECXMRTunableKind::Choice);
		Row.Options = { LOCTEXT("SpectatorMirror", "Headset mirror"), LOCTEXT("SpectatorSmoothed", "Smoothed view"), LOCTEXT("SpectatorOrbit", "Orbit vehicle") };
		Row.Get = [Weak]
		{
			const UCXMRTuningSubsystem* Tuning = Weak.IsValid() ? Weak->GetTuning() : nullptr;
			return Tuning ? Tuning->GetTunableValue("Spectator.Mode") : 0.0f;
		};
		Row.Set = [Weak](float Value)
		{
			if (UCXMRTuningSubsystem* Tuning = Weak.IsValid() ? Weak->GetTuning() : nullptr)
			{
				Tuning->SetTunableValue("Spectator.Mode", Value);
			}
		};
		AddRow(Rows, MoveTemp(Row));
	}

	return CXMRPanelUI::MakeRowList(Rows);
}

// ---- Calibration: where the vehicle sits ----

TSharedRef<SWidget> UCXMRControlPanelWidget::BuildCalibrationPage()
{
	const TWeakObjectPtr<UCXMRControlPanelWidget> Weak(this);
	auto CXMR = [Weak]() -> UCXMRSubsystem* { return Weak.IsValid() ? Weak->GetCXMR() : nullptr; };
	// Placement lives on the vehicle rig, not the pawn; it publishes its readouts through the tuning registry.
	auto Published = [Weak](FName Id)
	{
		return [Weak, Id]
		{
			const UCXMRTuningSubsystem* Tuning = Weak.IsValid() ? Weak->GetTuning() : nullptr;
			const FString Text = Tuning ? Tuning->GetTunableText(Id) : FString();
			return FText::FromString(Text.IsEmpty() ? TEXT("-") : Text);
		};
	};
	auto Action = [CXMR](TFunction<void(UCXMRSubsystem&)> Do)
	{
		return [CXMR, Do] { if (UCXMRSubsystem* S = CXMR()) { Do(*S); } };
	};

	FRows Rows;
	const FText Placement = LOCTEXT("CatPlacement", "Vehicle placement");
	{
		FCXMRTunable Row = MakeRow("Panel.VehiclePose", Placement, LOCTEXT("VehiclePose", "Vehicle (world)"), ECXMRTunableKind::Readout);
		Row.Text = Published("Vehicle.Pose");
		AddRow(Rows, MoveTemp(Row));
	}
	{
		FCXMRTunable Row = MakeRow("Panel.Markers", Placement, LOCTEXT("MarkersSeen", "Calibration markers"), ECXMRTunableKind::Readout);
		Row.Text = Published("Vehicle.Markers");
		AddRow(Rows, MoveTemp(Row));
	}
	{
		FCXMRTunable Row = MakeRow("Panel.Recalibrate", Placement, LOCTEXT("Recalibrate", "Re-read markers"), ECXMRTunableKind::Action);
		Row.Invoke = Action([](UCXMRSubsystem& S) { S.RequestRecalibrate(); });
		AddRow(Rows, MoveTemp(Row));
	}
	{
		FCXMRTunable Row = MakeRow("Panel.PlaceInFront", Placement, LOCTEXT("PlaceInFront", "Place vehicle in front of me"), ECXMRTunableKind::Action);
		Row.Invoke = Action([](UCXMRSubsystem& S) { S.RequestPlaceInFront(); });
		AddRow(Rows, MoveTemp(Row));
	}

	const FText Adjustment = LOCTEXT("CatAdjustment", "Manual adjustment");
	{
		FCXMRTunable Row = MakeRow("Panel.AdjustHint", Adjustment, LOCTEXT("AdjustHow", "Move the vehicle with"), ECXMRTunableKind::Readout);
		Row.Text = [] { return LOCTEXT("AdjustHint", "NumPad, or the tuning window"); };
		AddRow(Rows, MoveTemp(Row));
	}
	{
		FCXMRTunable Row = MakeRow("Panel.SaveOffset", Adjustment, LOCTEXT("SaveOffset", "Save adjustment into the marker layout"), ECXMRTunableKind::Action);
		Row.Invoke = Action([](UCXMRSubsystem& S) { S.RequestSaveMarkerOffset(); });
		AddRow(Rows, MoveTemp(Row));
	}
	{
		FCXMRTunable Row = MakeRow("Panel.ResetOffset", Adjustment, LOCTEXT("ResetOffset", "Discard unsaved adjustment"), ECXMRTunableKind::Action);
		Row.Invoke = Action([](UCXMRSubsystem& S) { S.RequestResetMarkerOffset(); });
		AddRow(Rows, MoveTemp(Row));
	}

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
		FCXMRTunable Row = MakeRow("Panel.CMF", Vehicle, LOCTEXT("CMF", "CMF"), ECXMRTunableKind::Readout);
		Row.Text = [CXMR] { const UCXMRSubsystem* S = CXMR(); return S ? FText::AsNumber(S->GetCMFIndex()) : FText::GetEmpty(); };
		AddRow(Rows, MoveTemp(Row));
	}
	{
		// The loader only cycles CMF forwards, so this is one button rather than a [-] [+] pair with a dead [-].
		FCXMRTunable Row = MakeRow("Panel.NextCMF", Vehicle, LOCTEXT("NextCMF", "Next CMF"), ECXMRTunableKind::Action);
		Row.Invoke = [CXMR] { if (UCXMRSubsystem* S = CXMR()) { S->RequestViewerAction(ECXMRViewerAction::NextCMF); } };
		AddRow(Rows, MoveTemp(Row));
	}

	const FText Human = LOCTEXT("CatHuman", "Human factors");
	{
		FCXMRTunable Row = MakeRow("Panel.Manikin", Human, LOCTEXT("Manikin", "Manikin"), ECXMRTunableKind::Stepper);
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
