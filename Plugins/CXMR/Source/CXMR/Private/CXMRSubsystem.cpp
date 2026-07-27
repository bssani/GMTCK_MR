// Copyright GMTCK CX.

#include "CXMRSubsystem.h"

// The ONLY place CXMR touches the Varjo plugin.
#include "VarjoOpenXR.h"        // UVarjoOpenXRFunctionLibrary, EMarkerTrackingMode
#include "VarjoMarkersEvent.h"  // UVarjoMarkerDelegates (static C++ multicast delegates)
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMR, Log, All);

/** Metres. The extension requires NearZ < FarZ; a collapsed range would be rejected silently. */
static constexpr float MinDepthRangeSpan = 0.05f;

void UCXMRSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Bind to the plugin's global marker delegates and re-broadcast through our own.
	DetectedHandle = UVarjoMarkerDelegates::NewVarjoMarkerDetected.AddUObject(this, &UCXMRSubsystem::HandleMarkerDetected);
	MovedHandle    = UVarjoMarkerDelegates::VarjoMarkerMoved.AddUObject(this, &UCXMRSubsystem::HandleMarkerMoved);
	LostHandle     = UVarjoMarkerDelegates::VarjoMarkerLost.AddUObject(this, &UCXMRSubsystem::HandleMarkerLost);
}

void UCXMRSubsystem::Deinitialize()
{
	UVarjoMarkerDelegates::NewVarjoMarkerDetected.Remove(DetectedHandle);
	UVarjoMarkerDelegates::VarjoMarkerMoved.Remove(MovedHandle);
	UVarjoMarkerDelegates::VarjoMarkerLost.Remove(LostHandle);

	Super::Deinitialize();
}

// ---------- Support ----------

bool UCXMRSubsystem::IsMixedRealitySupported() const
{
	return UVarjoOpenXRFunctionLibrary::IsMixedRealitySupported();
}

bool UCXMRSubsystem::IsMarkerTrackingSupported() const
{
	return UVarjoOpenXRFunctionLibrary::IsVarjoMarkersSupported();
}

bool UCXMRSubsystem::IsEnvironmentDepthEstimationSupported() const
{
	return UVarjoOpenXRFunctionLibrary::IsEnvironmentDepthEstimationSupported();
}

bool UCXMRSubsystem::IsFoveatedRenderingSupported() const
{
	return UVarjoOpenXRFunctionLibrary::IsFoveatedRenderingSupported();
}

// ---------- Mixed Reality ----------

void UCXMRSubsystem::SetMixedReality(bool bEnable)
{
	if (bMixedRealityOn == bEnable)
	{
		return;
	}

	// 3 = Alpha Blend (MR / passthrough), 1 = Opaque (VR). The plugin reads this same CVar.
	IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("xr.OpenXREnvironmentBlendMode"));
	if (!CVar)
	{
		// Report honestly, like every other toggle here: if the switch could not be thrown, the state
		// does not change and the panel keeps saying OFF. Claiming MR is on while the headset still
		// renders VR is the most expensive lie this class could tell.
		UE_LOG(LogCXMR, Warning,
			TEXT("Mixed reality toggle ignored: console variable xr.OpenXREnvironmentBlendMode not found."));
		return;
	}

	CVar->Set(bEnable ? 3 : 1);

	bMixedRealityOn = bEnable;
	OnMixedRealityChanged.Broadcast(bMixedRealityOn);
}

void UCXMRSubsystem::ToggleMixedReality()
{
	SetMixedReality(!bMixedRealityOn);
}

// ---------- VR background ----------

void UCXMRSubsystem::SetVRBackgroundVisible(bool bVisible)
{
	if (bVRBackgroundVisible == bVisible)
	{
		return;
	}
	bVRBackgroundVisible = bVisible;
	// UCXMRSceneObjectComponent (Role = VROnly) instances hide/show their own owner in response.
	OnVRBackgroundChanged.Broadcast(bVRBackgroundVisible);
}

void UCXMRSubsystem::ToggleVRBackground()
{
	SetVRBackgroundVisible(!bVRBackgroundVisible);
}

void UCXMRSubsystem::SetViewOffset(float Offset)
{
	Offset = FMath::Clamp(Offset, 0.0f, 1.0f);
	if (UVarjoOpenXRFunctionLibrary::SetViewOffset(Offset))
	{
		ViewOffset = Offset;
		OnViewOffsetChanged.Broadcast(ViewOffset);
	}
}

void UCXMRSubsystem::ToggleViewOffset()
{
	// 1.0 = passthrough camera position (stable in environment) <-> 0.0 = eye position (close inspection).
	SetViewOffset(ViewOffset > 0.5f ? 0.0f : 1.0f);
}

// ---------- Depth ----------

void UCXMRSubsystem::SetDepthTest(bool bEnable)
{
	if (bDepthTestOn == bEnable)
	{
		return;
	}
	UVarjoOpenXRFunctionLibrary::SetDepthTestEnabled(bEnable);
	bDepthTestOn = bEnable;

	// Re-assert the range every time the test comes on. The plugin wipes its state struct on session
	// creation, and a depth test running with the unbounded default is the flicker we are fixing.
	if (bDepthTestOn)
	{
		ApplyDepthTestRange();
	}

	OnDepthTestChanged.Broadcast(bDepthTestOn);
}

void UCXMRSubsystem::ToggleDepthTest()
{
	SetDepthTest(!bDepthTestOn);
}

// ---------- Depth test range ----------

void UCXMRSubsystem::ApplyDepthTestRange()
{
	UVarjoOpenXRFunctionLibrary::SetDepthTestRange(bDepthRangeOn, DepthRangeNearZ, DepthRangeFarZ);
}

void UCXMRSubsystem::SetDepthTestRange(bool bEnable, float NearZ, float FarZ)
{
	// Metres, non-negative, and Near strictly below Far — the extension rejects the alternative and
	// the plugin passes our numbers straight through without checking them.
	NearZ = FMath::Max(NearZ, 0.0f);
	FarZ  = FMath::Max(FarZ, NearZ + MinDepthRangeSpan);

	const bool bWasOn = bDepthRangeOn;

	bDepthRangeOn   = bEnable;
	DepthRangeNearZ = NearZ;
	DepthRangeFarZ  = FarZ;

	ApplyDepthTestRange();

	// Only the on/off transition is worth a line. The bounds move every frame while a key is held, so
	// logging those at Log level would bury the session log at frame rate.
	if (bWasOn != bDepthRangeOn)
	{
		UE_LOG(LogCXMR, Log, TEXT("Depth test range %s (near=%.2fm far=%.2fm)"),
			bDepthRangeOn ? TEXT("ON") : TEXT("OFF - compositor falls back to 0..infinity, which depth-tests the whole room"),
			DepthRangeNearZ, DepthRangeFarZ);
	}
	else
	{
		UE_LOG(LogCXMR, Verbose, TEXT("Depth test range: near=%.2fm far=%.2fm"), DepthRangeNearZ, DepthRangeFarZ);
	}

	OnDepthTestRangeChanged.Broadcast(bDepthRangeOn);
}

void UCXMRSubsystem::ToggleDepthTestRange()
{
	SetDepthTestRange(!bDepthRangeOn, DepthRangeNearZ, DepthRangeFarZ);
}

void UCXMRSubsystem::AdjustDepthTestRange(float NearDelta, float FarDelta)
{
	SetDepthTestRange(bDepthRangeOn, DepthRangeNearZ + NearDelta, DepthRangeFarZ + FarDelta);
}

void UCXMRSubsystem::SetEnvironmentDepthEstimation(bool bEnable)
{
	if (bEnvDepthOn == bEnable)
	{
		return;
	}
	UVarjoOpenXRFunctionLibrary::SetEnvironmentDepthEstimationEnabled(bEnable);
	bEnvDepthOn = bEnable;
	OnEnvironmentDepthEstimationChanged.Broadcast(bEnvDepthOn);
}

void UCXMRSubsystem::ToggleEnvironmentDepthEstimation()
{
	SetEnvironmentDepthEstimation(!bEnvDepthOn);
}

// ---------- Masking ----------

void UCXMRSubsystem::SetMasking(bool bEnable)
{
	if (bMaskingOn == bEnable)
	{
		return;
	}
	bMaskingOn = bEnable;
	// PP_MR MPC scalar 'MRMask' is applied by a listener bound to OnMaskingChanged
	// (that listener owns the MPC asset ref; the subsystem stays asset-agnostic / portable).
	OnMaskingChanged.Broadcast(bMaskingOn);
}

void UCXMRSubsystem::ToggleMasking()
{
	SetMasking(!bMaskingOn);
}

// ---------- Hand visualization ----------

void UCXMRSubsystem::SetHandVisualization(bool bEnable)
{
	if (bHandVisualizationOn == bEnable)
	{
		return;
	}
	bHandVisualizationOn = bEnable;
	OnHandVisualizationChanged.Broadcast(bHandVisualizationOn);
}

void UCXMRSubsystem::ToggleHandVisualization()
{
	SetHandVisualization(!bHandVisualizationOn);
}

// ---------- Markers ----------

bool UCXMRSubsystem::SetMarkerTracking(bool bEnable)
{
	// The plugin returns "is tracking now on", not "did the call succeed" — it computes
	// XR_ENSURE(...) && Enabled (VarjoMarkersPlugin.cpp:123-136). So a false here after asking for
	// true means the headset refused, and the only honest thing to do is say so rather than let the
	// panel sit at OFF with no explanation. Same posture as SetMixedReality below.
	const bool bResult = UVarjoOpenXRFunctionLibrary::SetVarjoMarkersEnabled(bEnable);

	if (bEnable && !bResult)
	{
		UE_LOG(LogCXMR, Warning,
			TEXT("Marker tracking could not be enabled. IsMarkerTrackingSupported()=%s — on a headset "
			     "that supports markers this usually means the XR session is not up yet."),
			IsMarkerTrackingSupported() ? TEXT("true") : TEXT("false"));
	}

	if (bMarkerTrackingOn != bResult)
	{
		bMarkerTrackingOn = bResult;
		OnMarkerTrackingChanged.Broadcast(bMarkerTrackingOn);
	}
	return bResult;
}

void UCXMRSubsystem::ToggleMarkerTracking()
{
	SetMarkerTracking(!bMarkerTrackingOn);
}

bool UCXMRSubsystem::SetMarkerTimeout(int32 MarkerId, float Seconds)
{
	return UVarjoOpenXRFunctionLibrary::SetMarkerTimeout(MarkerId, Seconds);
}

bool UCXMRSubsystem::SetMarkerTrackingMode(int32 MarkerId, ECXMRMarkerTrackingMode Mode)
{
	const EMarkerTrackingMode VarjoMode =
		(Mode == ECXMRMarkerTrackingMode::Dynamic) ? EMarkerTrackingMode::Dynamic : EMarkerTrackingMode::Stationary;
	return UVarjoOpenXRFunctionLibrary::SetMarkerTrackingMode(MarkerId, VarjoMode);
}

void UCXMRSubsystem::HandleMarkerDetected(int32 MarkerId, const FVector& Position, const FRotator& Rotation, const FVector2D& Size)
{
	OnMarkerDetected.Broadcast(MarkerId, Position, Rotation, Size);
}

void UCXMRSubsystem::HandleMarkerMoved(int32 MarkerId, const FVector& Position, const FRotator& Rotation, const FVector2D& Size)
{
	OnMarkerMoved.Broadcast(MarkerId, Position, Rotation, Size);
}

void UCXMRSubsystem::HandleMarkerLost(int32 MarkerId)
{
	OnMarkerLost.Broadcast(MarkerId);
}

// ---------- Placement request relay ----------

void UCXMRSubsystem::RequestRecalibrate()
{
	OnRecalibrateRequested.Broadcast();
}

void UCXMRSubsystem::RequestPlaceInFront()
{
	OnPlaceRequested.Broadcast();
}

// ---------- Viewer relay ----------

void UCXMRSubsystem::RequestViewerAction(ECXMRViewerAction Action)
{
	OnViewerAction.Broadcast(Action);
}

void UCXMRSubsystem::RequestTurntableAxis(float AxisValue)
{
	OnTurntableAxis.Broadcast(AxisValue);
}

void UCXMRSubsystem::ReportVehicleStatus(FText InVehicleName, int32 InVehicleIndex, int32 InVehicleCount,
                                         FText InTrimName, int32 InTrimIndex, int32 InTrimCount, int32 InCMFIndex)
{
	VehicleName  = InVehicleName;
	VehicleIndex = InVehicleIndex;
	VehicleCount = InVehicleCount;
	TrimName     = InTrimName;
	TrimIndex    = InTrimIndex;
	TrimCount    = InTrimCount;
	CMFIndex     = InCMFIndex;
	OnVehicleStatusChanged.Broadcast();
}

// ---------- Ergonomics relay ----------

void UCXMRSubsystem::RequestErgonomicsStep(int32 Step)
{
	OnErgonomicsStepRequested.Broadcast(Step);
}

void UCXMRSubsystem::ReportManikin(FText Name, int32 Index, int32 Count)
{
	ManikinName  = Name;
	ManikinIndex = Index;
	ManikinCount = Count;
	OnManikinChanged.Broadcast();
}
