// Copyright GMTCK CX.

#include "CXMRSubsystem.h"

// The ONLY place CXMR touches the Varjo plugin.
#include "VarjoOpenXR.h"        // UVarjoOpenXRFunctionLibrary, EMarkerTrackingMode
#include "VarjoMarkersEvent.h"  // UVarjoMarkerDelegates (static C++ multicast delegates)
#include "HAL/IConsoleManager.h"

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
	if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("xr.OpenXREnvironmentBlendMode")))
	{
		CVar->Set(bEnable ? 3 : 1);
	}

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
	OnDepthTestChanged.Broadcast(bDepthTestOn);
}

void UCXMRSubsystem::ToggleDepthTest()
{
	SetDepthTest(!bDepthTestOn);
}

void UCXMRSubsystem::SetDepthTestRange(bool bEnable, float NearZ, float FarZ)
{
	UVarjoOpenXRFunctionLibrary::SetDepthTestRange(bEnable, NearZ, FarZ);
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

// ---------- Markers ----------

bool UCXMRSubsystem::SetMarkerTracking(bool bEnable)
{
	const bool bResult = UVarjoOpenXRFunctionLibrary::SetVarjoMarkersEnabled(bEnable);
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
