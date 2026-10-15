// Copyright GMTCK CX.
#include "CXMRAdvancedSettings.h"
#include "CXMRTuningSubsystem.h"

namespace
{
	// 등록 메타데이터의 문구 원본. 시안 SETTINGS 순서를 유지함.
	const FCXMRAdvancedRow CXMRAdvancedRows[] = {
		{TEXT("MR.StartInMR"), TEXT("MR"), TEXT("Startup"), TEXT("Start in MR"), TEXT("Switch to MR as soon as the headset session is running."), TEXT(""), TEXT("toggle"), TEXT("")},
		{TEXT("Depth.Test"), TEXT("MR"), TEXT("Real objects"), TEXT("Real objects in front of the car"), TEXT("Hands and nearby real objects cover the virtual car instead of disappearing behind it."), TEXT(""), TEXT("toggle"), TEXT("")},
		{TEXT("Depth.EnvEstimation"), TEXT("MR"), TEXT("Real objects"), TEXT("Estimate real-world depth"), TEXT("Cleaner hand edges. Works only with the setting above."), TEXT(""), TEXT("toggle"), TEXT("Depth.Test")},
		{TEXT("Depth.FarZ"), TEXT("MR"), TEXT("Real objects"), TEXT("Depth range, far edge"), TEXT("Real objects closer than this cover the car; beyond it the car is always on top. A wider range can flicker where real and virtual surfaces meet, such as a physical steering wheel."), TEXT("m"), TEXT("number"), TEXT("Depth.Test")},
		{TEXT("Depth.NearZ"), TEXT("MR"), TEXT("Real objects"), TEXT("Depth range, near edge"), TEXT("Usually 0. Raise it only if something right in front of the headset should not cover the car."), TEXT("m"), TEXT("number"), TEXT("Depth.Test")},
		{TEXT("Display.MRExposure"), TEXT("MR"), TEXT("Picture"), TEXT("Virtual exposure"), TEXT("Brightness of the virtual car over the camera image. Camera exposure itself is set in Varjo Base."), TEXT("EV"), TEXT("range"), TEXT("")},
		{TEXT("MR.ViewOffset"), TEXT("MR"), TEXT("Picture"), TEXT("Render from"), TEXT("Cameras matches the camera image best for objects within reach. Eyes matches your real eye position."), TEXT(""), TEXT("seg"), TEXT("")},
		{TEXT("MR.ViewOffsetGlide"), TEXT("MR"), TEXT("Picture"), TEXT("Render-from change time"), TEXT("How long the switch between Eyes and Cameras takes. 0 switches at once."), TEXT("s"), TEXT("number"), TEXT("")},
		{TEXT("MR.Masking"), TEXT("MR"), TEXT("Picture"), TEXT("Scene masks"), TEXT("Lets marked real objects show through the virtual scene."), TEXT(""), TEXT("toggle"), TEXT("")},
		{TEXT("Display.VRExposure"), TEXT("VR"), TEXT("Picture"), TEXT("Virtual exposure"), TEXT("Brightness of the virtual scene in VR. Kept separately from MR."), TEXT("EV"), TEXT("range"), TEXT("")},
		{TEXT("MR.VRBackground"), TEXT("VR"), TEXT("Picture"), TEXT("Virtual background"), TEXT("Show the virtual room around the car."), TEXT(""), TEXT("toggle"), TEXT("")},
		{TEXT("Vehicle.Markers"), TEXT("Calibration"), TEXT("Live readings"), TEXT("Calibration markers"), TEXT(""), TEXT(""), TEXT("readout"), TEXT("")},
		{TEXT("Vehicle.Steadiness"), TEXT("Calibration"), TEXT("Live readings"), TEXT("Marker steadiness"), TEXT("How much the marker readings shook while averaging. Large values mean poor light, distance or viewing angle."), TEXT(""), TEXT("readout"), TEXT("")},
		{TEXT("Vehicle.LayoutFit"), TEXT("Calibration"), TEXT("Live readings"), TEXT("Fit to saved layout"), TEXT("How far the markers in view are from the saved layout. This is the size of the session-to-session error."), TEXT(""), TEXT("readout"), TEXT("")},
		{TEXT("Vehicle.Pose"), TEXT("Calibration"), TEXT("Live readings"), TEXT("Vehicle position"), TEXT(""), TEXT(""), TEXT("readout"), TEXT("")},
		{TEXT("Vehicle.KeepLevel"), TEXT("Calibration"), TEXT("Placement"), TEXT("Keep the car level"), TEXT("Follow the markers' heading only and ignore their tilt."), TEXT(""), TEXT("toggle"), TEXT("")},
		{TEXT("Vehicle.CalibrationSettleSeconds"), TEXT("Calibration"), TEXT("Placement"), TEXT("Marker averaging time"), TEXT("How long marker readings are averaged before the car is placed or saved."), TEXT("s"), TEXT("number"), TEXT("")},
		{TEXT("Vehicle.OriginReuseTolerance"), TEXT("Calibration"), TEXT("Placement"), TEXT("Reuse the saved position within"), TEXT("If every marker is at most this far from where it was when saved, the saved position is used as is. Otherwise it is recalculated from the markers."), TEXT("cm"), TEXT("number"), TEXT("")},
		{TEXT("Vehicle.OriginReuseYawTolerance"), TEXT("Calibration"), TEXT("Placement"), TEXT("Heading tolerance, one-marker profiles"), TEXT("With a single marker, its heading must also match within this angle before the saved position is reused."), TEXT("°"), TEXT("number"), TEXT("")},
		{TEXT("Vehicle.YawStep"), TEXT("Calibration"), TEXT("Fine-adjust keys"), TEXT("Turn per press"), TEXT("Rotation for each Turn button or numpad press."), TEXT("°"), TEXT("number"), TEXT("")},
		{TEXT("Vehicle.NudgeFrame"), TEXT("Calibration"), TEXT("Fine-adjust keys"), TEXT("Keys move along"), TEXT("Where I first faced keeps Away fixed even when you turn your head."), TEXT(""), TEXT("seg"), TEXT("")},
		{TEXT("Vehicle.NudgeHeading"), TEXT("Calibration"), TEXT("Fine-adjust keys"), TEXT("Away from me is"), TEXT(""), TEXT(""), TEXT("readout"), TEXT("")},
		{TEXT("Vehicle.RetakeHeading"), TEXT("Calibration"), TEXT("Fine-adjust keys"), TEXT("Use the way I face now"), TEXT(""), TEXT(""), TEXT("action"), TEXT("")},
		{TEXT("Vehicle.Pivot"), TEXT("Calibration"), TEXT("Fine-adjust keys"), TEXT("Turn around"), TEXT("The point the car rotates around when you press Turn."), TEXT(""), TEXT("seg"), TEXT("")},
		{TEXT("Input.OffsetAdjustSpeed"), TEXT("Calibration"), TEXT("Fine-adjust keys"), TEXT("Numpad speed"), TEXT("Movement while a numpad key is held, per second (cm or °)."), TEXT("/s"), TEXT("number"), TEXT("")},
		{TEXT("Vehicle.LearnMarkers"), TEXT("Calibration"), TEXT("Marker layout"), TEXT("Learn marker layout"), TEXT("Records where each marker sits on the car, from the car's current position. Needs every marker in view."), TEXT(""), TEXT("action"), TEXT("")},
		{TEXT("Vehicle.PlaceInFront"), TEXT("Calibration"), TEXT("Marker layout"), TEXT("Place car in front of me"), TEXT("For a walk-around without markers. The car is placed 2.5 m ahead, facing you."), TEXT(""), TEXT("action"), TEXT("")},
		{TEXT("Vehicle.ResetGroupAlignment"), TEXT("Calibration"), TEXT("Marker layout"), TEXT("Delete saved alignment for {0}"), TEXT("Affects every vehicle in the group. The second click must come within 5 seconds."), TEXT(""), TEXT("action"), TEXT("")},
		{TEXT("Diagnostics.HandSkeleton"), TEXT("Diagnostics"), TEXT("Overlays"), TEXT("Hand skeleton"), TEXT("Draw tracked hand joints."), TEXT(""), TEXT("toggle"), TEXT("")},
		{TEXT("Diagnostics.MarkerLabels"), TEXT("Diagnostics"), TEXT("Overlays"), TEXT("Marker axes and labels"), TEXT("Draw each marker's axes and ID where the headset sees it."), TEXT(""), TEXT("toggle"), TEXT("")},
		{TEXT("Hands.RightTip"), TEXT("Diagnostics"), TEXT("Hand alignment"), TEXT("Right index tip"), TEXT(""), TEXT(""), TEXT("readout"), TEXT("")},
		{TEXT("Hands.LeftTip"), TEXT("Diagnostics"), TEXT("Hand alignment"), TEXT("Left index tip"), TEXT(""), TEXT(""), TEXT("readout"), TEXT("")},
		{TEXT("Hands.TipToMarker"), TEXT("Diagnostics"), TEXT("Hand alignment"), TEXT("Tip minus marker"), TEXT(""), TEXT(""), TEXT("readout"), TEXT("")},
		{TEXT("Hands.SnapToMarker"), TEXT("Diagnostics"), TEXT("Hand alignment"), TEXT("Snap hand to marker"), TEXT("Touch a marker's centre with your index tip, then press. Corrects the hand offset."), TEXT(""), TEXT("action"), TEXT("")},
		{TEXT("Spectator.Mode"), TEXT("Diagnostics"), TEXT("Monitor"), TEXT("Monitor shows"), TEXT(""), TEXT(""), TEXT("seg"), TEXT("")},
		{TEXT("Spectator.FieldOfView"), TEXT("Diagnostics"), TEXT("Monitor"), TEXT("Monitor field of view"), TEXT(""), TEXT("°"), TEXT("number"), TEXT("")},
		{TEXT("Spectator.FollowSpeed"), TEXT("Diagnostics"), TEXT("Monitor"), TEXT("Smoothing"), TEXT("Lower is calmer."), TEXT("/s"), TEXT("number"), TEXT("")},
		{TEXT("Spectator.OrbitDistance"), TEXT("Diagnostics"), TEXT("Monitor"), TEXT("Orbit distance"), TEXT(""), TEXT("cm"), TEXT("number"), TEXT("")},
		{TEXT("Spectator.OrbitSpeed"), TEXT("Diagnostics"), TEXT("Monitor"), TEXT("Orbit speed"), TEXT(""), TEXT("°/s"), TEXT("number"), TEXT("")},
		{TEXT("Spectator.Output"), TEXT("Diagnostics"), TEXT("Monitor"), TEXT("Monitor picture"), TEXT(""), TEXT(""), TEXT("readout"), TEXT("")},
		{TEXT("Diagnostics.MarkerTracking"), TEXT("Diagnostics"), TEXT("Tracking"), TEXT("Marker tracking"), TEXT("Leave on. Turning it off stops calibration and restore."), TEXT(""), TEXT("toggle"), TEXT("")},
		{TEXT("Diagnostics.DynamicMarkers"), TEXT("Diagnostics"), TEXT("Tracking"), TEXT("Dynamic tracking for all markers"), TEXT("For checking moving markers only. Markers follow movement faster but jitter more, so calibration is less accurate while this is on. Marker labels show each marker's mode. Not saved."), TEXT(""), TEXT("toggle"), TEXT("")},
		{TEXT("Diagnostics.DumpMarkers"), TEXT("Diagnostics"), TEXT("Logs"), TEXT("Write marker positions to log"), TEXT(""), TEXT(""), TEXT("action"), TEXT("")},
		{TEXT("Diagnostics.MRState"), TEXT("Diagnostics"), TEXT("Logs"), TEXT("Write MR state to log"), TEXT(""), TEXT(""), TEXT("action"), TEXT("")},
	};
}
TConstArrayView<FCXMRAdvancedRow> CXMRAdvancedSettings::Rows() { return MakeArrayView(CXMRAdvancedRows); }
void CXMRAdvancedSettings::Describe(FCXMRTunable& Row)
{
	for (const FCXMRAdvancedRow& Spec : Rows())
	{
		if (Row.Id != Spec.Id) { continue; }
		Row.Label = FText::FromString(Spec.Label);
		Row.Help = FText::FromString(Spec.Help);
		Row.Unit = FText::FromString(Spec.Unit);
		Row.Category = FText::FromString(Spec.Group);
		if (Row.Id == "MR.ViewOffset") { Row.Options = { FText::FromString(TEXT("Eyes")), FText::FromString(TEXT("Cameras")) }; }
		return;
	}
}
int32 CXMRAdvancedSettings::ChangedCount(const UCXMRTuningSubsystem* Tuning, const FString& Tab)
{
	if (!Tuning) { return 0; }
	int32 Count = 0;
	for (const FCXMRTunable* Row : Tuning->GetTunables())
	{
		if (!Row->Get || Row->Kind == ECXMRTunableKind::Readout || Row->Kind == ECXMRTunableKind::Action) { continue; }
		for (const FCXMRAdvancedRow& Spec : Rows())
		{ if (Row->Id == Spec.Id && (Tab.IsEmpty() || Tab == Spec.Tab) && !FMath::IsNearlyEqual(Row->Get(), Row->Default)) { ++Count; break; } }
	}
	return Count;
}
void CXMRAdvancedSettings::ResetTab(UCXMRTuningSubsystem* Tuning, const FString& Tab)
{
	if (!Tuning) { return; }
	for (const FCXMRAdvancedRow& Spec : Rows())
	{ if (Tab == Spec.Tab && FString(Spec.Type) != TEXT("readout") && FString(Spec.Type) != TEXT("action")) { Tuning->ResetToDefault(Spec.Id); } }
}
