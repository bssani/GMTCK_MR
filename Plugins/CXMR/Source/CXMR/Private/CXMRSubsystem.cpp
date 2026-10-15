// Copyright GMTCK CX.

#include "CXMRSubsystem.h"

// The ONLY place CXMR touches the Varjo plugin.
#include "VarjoOpenXR.h"        // UVarjoOpenXRFunctionLibrary, EMarkerTrackingMode
#include "VarjoMarkersEvent.h"  // UVarjoMarkerDelegates (static C++ multicast delegates)
#include "HAL/IConsoleManager.h"

#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "IOpenXRHMD.h"
#include "EngineUtils.h"
#include "CXMRPlacementComponent.h"
#include "CXMRMarkerProfile.h"
#include "CXMRMarkerFollower.h"
#include "IXRTrackingSystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMR, Log, All);

/** Metres. The extension requires NearZ < FarZ; a collapsed range would be rejected silently. */
static constexpr float MinDepthRangeSpan = 0.05f;

namespace
{
	/** Reads an int CVar, reporting absence rather than pretending it is zero — "not registered" and
	 *  "set to 0" mean completely different things when you are hunting a black screen. */
	FString DescribeIntCVar(const TCHAR* Name)
	{
		if (const IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(Name))
		{
			return FString::Printf(TEXT("%d"), CVar->GetInt());
		}
		return TEXT("<not registered>");
	}

	/**
	 * The Varjo plugin passes its cached session handle straight to the runtime — MixedRealityPlugin.cpp SetViewOffset,
	 * DepthPlugin.cpp SetEnvironmentDepthEstimationEnabled, VarjoMarkersPlugin.cpp — without checking that a session
	 * exists. Before xrBeginSession that handle is null (after a session ends, stale) and the Varjo runtime dereferences
	 * it: the tuning window restoring a saved View offset at BeginPlay took the editor down on the headset PC (2026-09-15).
	 * A PC without the Varjo runtime never shows it — the extension function is missing there and the plugin returns early.
	 * Every call here that reaches the runtime asks this first.
	 */
	bool IsXRSessionRunning()
	{
		if (!GEngine || !GEngine->XRSystem.IsValid())
		{
			return false;
		}
		const IOpenXRHMD* OpenXR = GEngine->XRSystem->GetIOpenXRHMD();
		return OpenXR && OpenXR->IsRunning();
	}
}

// Console entry point. WithWorld so it can reach the game instance subsystem that owns the state.
static FAutoConsoleCommandWithWorld GCXMRDumpMRState(
	TEXT("CXMR.DumpMRState"),
	TEXT("Dump the mixed-reality state that decides whether passthrough can appear at all."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (World)
		{
			if (const UGameInstance* GI = World->GetGameInstance())
			{
				if (const UCXMRSubsystem* CXMR = GI->GetSubsystem<UCXMRSubsystem>())
				{
					CXMR->DumpMRState();
					return;
				}
			}
		}
		UE_LOG(LogCXMR, Warning, TEXT("CXMR.DumpMRState: no CXMR subsystem on this world."));
	}));

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

	StopViewOffsetTransition();
	StopPendingViewOffset();
	if (SessionTicker.IsValid()) { FTSTicker::RemoveTicker(SessionTicker); SessionTicker.Reset(); }

	Super::Deinitialize();
}

// ---------- Diagnostics ----------

void UCXMRSubsystem::DumpMRState() const
{
	const bool bSupported = IsMixedRealitySupported();

	// The plugin derives this independently of us — from the blend-mode CVar and the XR system's IsAR
	// flag (MixedRealityPlugin.cpp:80-96). If it disagrees with our cached bMixedRealityOn, the toggle
	// is lying and nothing downstream is worth reading.
	// Guarded: the plugin dereferences the CVar without a null check once past the support test.
	const FString PluginEnabled = bSupported
		? (UVarjoOpenXRFunctionLibrary::IsMixedRealityEnabled() ? TEXT("true") : TEXT("false"))
		: TEXT("n/a (MR unsupported)");

	UE_LOG(LogCXMR, Warning, TEXT("===== CXMR MR state ====="));

	UE_LOG(LogCXMR, Warning, TEXT("  MR supported=%s | plugin says enabled=%s | CXMR cached=%s"),
		bSupported ? TEXT("true") : TEXT("false"),
		*PluginEnabled,
		bMixedRealityOn ? TEXT("true") : TEXT("false"));

	// Passthrough is composited by the OpenXR runtime, not by us: 3 = alpha blend, 1 = opaque.
	const FString BlendMode = DescribeIntCVar(TEXT("xr.OpenXREnvironmentBlendMode"));
	UE_LOG(LogCXMR, Warning, TEXT("  xr.OpenXREnvironmentBlendMode=%s   (3=alpha blend/MR, 1=opaque/VR)"), *BlendMode);

	// Everything below decides whether a pixel can carry alpha 0 at all. Varjo shows the video stream
	// only where RGBA is (0,0,0,0), so any one of these silently forcing alpha to 1 produces a black
	// screen that looks exactly like "MR is not working".
	UE_LOG(LogCXMR, Warning, TEXT("  --- alpha path (passthrough needs alpha 0) ---"));
	UE_LOG(LogCXMR, Warning, TEXT("  r.PostProcessing.PropagateAlpha=%s   (0 here means no pixel can ever be transparent)"),
		*DescribeIntCVar(TEXT("r.PostProcessing.PropagateAlpha")));
	UE_LOG(LogCXMR, Warning, TEXT("  r.SceneColorFormat=%s"), *DescribeIntCVar(TEXT("r.SceneColorFormat")));
	UE_LOG(LogCXMR, Warning, TEXT("  r.AntiAliasingMethod=%s   (3=TSR; TSR must preserve alpha or the background resolves opaque)"),
		*DescribeIntCVar(TEXT("r.AntiAliasingMethod")));
	UE_LOG(LogCXMR, Warning, TEXT("  r.TSR.AlphaChannel=%s"), *DescribeIntCVar(TEXT("r.TSR.AlphaChannel")));
	UE_LOG(LogCXMR, Warning, TEXT("  r.DefaultBackBufferPixelFormat=%s"), *DescribeIntCVar(TEXT("r.DefaultBackBufferPixelFormat")));
	UE_LOG(LogCXMR, Warning, TEXT("  r.ForwardShading=%s  r.CustomDepth=%s"),
		*DescribeIntCVar(TEXT("r.ForwardShading")), *DescribeIntCVar(TEXT("r.CustomDepth")));

	UE_LOG(LogCXMR, Warning, TEXT("  --- CXMR feature state ---"));
	UE_LOG(LogCXMR, Warning, TEXT("  VR background visible=%s   (hiding it is what should reveal passthrough)"),
		bVRBackgroundVisible ? TEXT("true") : TEXT("false"));
	UE_LOG(LogCXMR, Warning, TEXT("  masking=%s  markers=%s  hands=%s  viewOffset=%.2f"),
		bMaskingOn ? TEXT("on") : TEXT("off"),
		bMarkerTrackingOn ? TEXT("on") : TEXT("off"),
		bHandVisualizationOn ? TEXT("on") : TEXT("off"),
		ViewOffset);
	UE_LOG(LogCXMR, Warning, TEXT("  depthTest=%s  range=%s near=%.2fm far=%.2fm  envDepth=%s"),
		bDepthTestOn ? TEXT("on") : TEXT("off"),
		bDepthRangeOn ? TEXT("on") : TEXT("OFF -> compositor uses 0..infinity"),
		DepthRangeNearZ, DepthRangeFarZ,
		bEnvDepthOn ? TEXT("on") : TEXT("off"));

	// Call out the contradictions rather than leaving them to be spotted in a wall of numbers.
	if (bMixedRealityOn && BlendMode != TEXT("3"))
	{
		UE_LOG(LogCXMR, Error,
			TEXT("  !! CXMR believes MR is ON but the blend mode is not 3. The compositor is still in VR; "
			     "this is not an alpha problem."));
	}
	if (bMixedRealityOn && !bVRBackgroundVisible && DescribeIntCVar(TEXT("r.PostProcessing.PropagateAlpha")) == TEXT("0"))
	{
		UE_LOG(LogCXMR, Error,
			TEXT("  !! Alpha propagation is off. The background cannot be transparent, so hiding the VR "
			     "background can only ever produce black."));
	}

	UE_LOG(LogCXMR, Warning, TEXT("========================="));
}

// ---------- Support ----------

bool UCXMRSubsystem::IsMixedRealitySupported() const
{
	return UVarjoOpenXRFunctionLibrary::IsMixedRealitySupported();
}

bool UCXMRSubsystem::IsMarkerTrackingSupported() const
{
	// The plugin's flag outlives the session it was learned in; with no session running nothing can be switched on.
	return IsXRSessionRunning() && UVarjoOpenXRFunctionLibrary::IsVarjoMarkersSupported();
}

bool UCXMRSubsystem::IsEnvironmentDepthEstimationSupported() const
{
	return UVarjoOpenXRFunctionLibrary::IsEnvironmentDepthEstimationSupported();
}

bool UCXMRSubsystem::IsFoveatedRenderingSupported() const
{
	return UVarjoOpenXRFunctionLibrary::IsFoveatedRenderingSupported();
}

bool UCXMRSubsystem::IsFoveatedRenderingEnabled() const
{
	return UVarjoOpenXRFunctionLibrary::IsFoveatedRenderingEnabled();
}

// ---------- Mixed Reality ----------

bool UCXMRSubsystem::HasRunningSession() const
{
#if WITH_DEV_AUTOMATION_TESTS
	if (SessionRunningForTest.IsSet()) { return SessionRunningForTest.GetValue(); }
#endif
	return IsXRSessionRunning();
}

void UCXMRSubsystem::StartHeadsetSessionWait()
{
	SessionWaitSeconds = 0.f;
	bMRRequested = bStartInMR;
	bSessionWasRunning = false;
	if (!SessionTicker.IsValid())
	{
		SessionTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCXMRSubsystem::TickHeadsetSession));
	}
	TickHeadsetSession(0.f);
}

bool UCXMRSubsystem::TickHeadsetSession(float DeltaTime)
{
	const bool bRunning = HasRunningSession();
	if (!bRunning) { SessionWaitSeconds += DeltaTime; }
	if (bRunning && !bSessionWasRunning)
	{
		ApplyRequestedMode();
		if (bRequestedMarkerTracking) { SetMarkerTracking(true); }
	}
	bSessionWasRunning = bRunning;
	return true; // 20초 뒤 시작하는 세션도 적용함.
}

void UCXMRSubsystem::ApplyRuntimeCall(FName Call, TFunction<void()> Invoke)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (SessionRunningForTest.IsSet()) { if (RuntimeCallForTest) { RuntimeCallForTest(Call); } return; }
#endif
	Invoke();
}

void UCXMRSubsystem::ApplyRequestedMode()
{
	if (!HasRunningSession()) { return; }
	const bool bEnable = bMRRequested;
	ApplyRuntimeCall("MR", [bEnable]
	{
		if (IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(TEXT("xr.OpenXREnvironmentBlendMode"))) { CVar->Set(bEnable ? 3 : 1); }
	});
	const bool bChanged = bMixedRealityOn != bEnable;
	bMixedRealityOn = bEnable;
	if (bChanged) { OnMixedRealityChanged.Broadcast(bEnable); }
	if (bCoupleVRBackgroundToMR) { SetVRBackgroundVisible(bEnable ? false : bRequestedVRBackground); }
	if (bEnable)
	{
		ApplyRuntimeCall("Depth.Test", [this] { UVarjoOpenXRFunctionLibrary::SetDepthTestEnabled(bRequestedDepthTest); });
		bDepthTestOn = bRequestedDepthTest;
		OnDepthTestChanged.Broadcast(bDepthTestOn);
		bDepthRangeOn = true;
		ApplyRuntimeCall("Depth.Range", [this] { ApplyDepthTestRange(); });
		if (bRequestedDepthTest)
		{
			ApplyRuntimeCall("Depth.EnvEstimation", [this] { UVarjoOpenXRFunctionLibrary::SetEnvironmentDepthEstimationEnabled(bRequestedEnvironmentDepth); });
			bEnvDepthOn = bRequestedEnvironmentDepth;
		}
		else if (bEnvDepthOn)
		{
			ApplyRuntimeCall("Depth.EnvEstimation", [] { UVarjoOpenXRFunctionLibrary::SetEnvironmentDepthEstimationEnabled(false); });
			bEnvDepthOn = false;
		}
		OnEnvironmentDepthEstimationChanged.Broadcast(bEnvDepthOn);
		SetViewOffset(RequestedViewOffset);
	}
	else { SyncViewOffsetWithMode(); }
}

void UCXMRSubsystem::SetMixedReality(bool bEnable)
{
	bMRRequested = bEnable;
	ApplyRequestedMode();
}

void UCXMRSubsystem::ToggleMixedReality()
{
	SetMixedReality(!bMRRequested);
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
	if (bMixedRealityOn) { SetVRBackgroundVisible(!bVRBackgroundVisible); }
	else { SetRequestedVRBackground(!bRequestedVRBackground); }
}

void UCXMRSubsystem::SetRequestedVRBackground(bool bVisible)
{
	bRequestedVRBackground = bVisible;
	if (!bMixedRealityOn) { SetVRBackgroundVisible(bVisible); }
}

void UCXMRSubsystem::SetViewOffset(float Offset)
{
	RequestedViewOffset = FMath::Clamp(Offset, 0.f, 1.f);
	if (!bMixedRealityOn) { return; }
	StopViewOffsetTransition();
	if (!IsXRSessionRunning()) { return; }
	StopPendingViewOffset();
	if (UVarjoOpenXRFunctionLibrary::SetViewOffset(RequestedViewOffset))
	{
		ViewOffset = RequestedViewOffset;
		bViewOffsetChosen = true;
		OnViewOffsetChanged.Broadcast(ViewOffset);
	}
}

void UCXMRSubsystem::SyncViewOffsetWithMode()
{
	if (bViewOffsetChosen)
	{
		// Someone picked a render position; switching MR must not quietly swap it for the new mode's default.
		if (IsXRSessionRunning())
		{
			UVarjoOpenXRFunctionLibrary::SetViewOffset(ViewOffset);
		}
		else if (!PendingViewOffsetTicker.IsValid())
		{
			DeferViewOffset(ViewOffset);
		}
		return;
	}

	// Nothing picked yet: the runtime renders from the cameras in MR and from the eyes in VR (the plugin's own
	// GetViewOffset assumes the same). Mirror that so the panel shows what the headset is actually doing —
	// it used to say "cameras" from the start while a VR session rendered from the eyes.
	const float RuntimeDefault = bMixedRealityOn ? 1.0f : 0.0f;
	if (!FMath::IsNearlyEqual(ViewOffset, RuntimeDefault))
	{
		ViewOffset = RuntimeDefault;
		OnViewOffsetChanged.Broadcast(ViewOffset);
	}
}

void UCXMRSubsystem::ToggleViewOffset()
{
	// 1.0 = passthrough camera position (stable in environment) <-> 0.0 = eye position (close inspection).
	// Pressed again mid-glide, it turns back from where the glide was heading.
	const float Target = RequestedViewOffset > 0.5f ? 0.f : 1.f;
	RequestedViewOffset = Target;
	if (bMixedRealityOn) { TransitionViewOffset(Target, ViewOffsetTransitionSeconds); }
}

void UCXMRSubsystem::TransitionViewOffset(float Target, float Seconds)
{
	Target = FMath::Clamp(Target, 0.0f, 1.0f);
	StopViewOffsetTransition();

	// With no session to glide on, SetViewOffset keeps the target for when one runs.
	if (Seconds <= KINDA_SMALL_NUMBER || FMath::IsNearlyEqual(ViewOffset, Target) || !IsXRSessionRunning())
	{
		SetViewOffset(Target);
		return;
	}

	ViewOffsetFrom = ViewOffset;
	ViewOffsetTo = Target;
	ViewOffsetElapsed = 0.0f;
	ViewOffsetDuration = Seconds;
	ViewOffsetTicker = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateUObject(this, &UCXMRSubsystem::TickViewOffsetTransition));
}

bool UCXMRSubsystem::TickViewOffsetTransition(float DeltaTime)
{
	// Capped per step: after a hitch one long frame would otherwise skip most of the glide and jump after all.
	ViewOffsetElapsed += FMath::Min(DeltaTime, 1.0f / 30.0f);
	const float Alpha = FMath::Clamp(ViewOffsetElapsed / ViewOffsetDuration, 0.0f, 1.0f);
	const float Value = FMath::Lerp(ViewOffsetFrom, ViewOffsetTo, FMath::InterpEaseInOut(0.0f, 1.0f, Alpha, 2.0f));

	if (!IsXRSessionRunning() || !UVarjoOpenXRFunctionLibrary::SetViewOffset(Value))
	{
		// No XR session (PIE on a monitor) or the runtime refused. Stop at the last value it accepted rather than
		// report a position the headset never reached.
		UE_LOG(LogCXMR, Warning, TEXT("View offset glide stopped at %.2f: the runtime did not accept %.2f."), ViewOffset, Value);
		ViewOffsetTicker.Reset();
		return false;
	}

	ViewOffset = Value;
	bViewOffsetChosen = true;
	OnViewOffsetChanged.Broadcast(ViewOffset);

	if (Alpha >= 1.0f)
	{
		ViewOffsetTicker.Reset();
		return false;
	}
	return true;
}

void UCXMRSubsystem::StopViewOffsetTransition()
{
	if (ViewOffsetTicker.IsValid())
	{
		FTSTicker::RemoveTicker(ViewOffsetTicker);
	}
	ViewOffsetTicker.Reset();
}

void UCXMRSubsystem::DeferViewOffset(float Offset)
{
	PendingViewOffset = Offset;
	if (!PendingViewOffsetTicker.IsValid())
	{
		UE_LOG(LogCXMR, Log, TEXT("View offset %.2f will be applied once the headset session is running."), Offset);
		PendingViewOffsetTicker = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UCXMRSubsystem::TickPendingViewOffset), 0.25f);
	}
}

bool UCXMRSubsystem::TickPendingViewOffset(float DeltaTime)
{
	if (!IsXRSessionRunning())
	{
		return true;
	}
	// Handle first: SetViewOffset stops a pending request, and this one is already finishing.
	PendingViewOffsetTicker.Reset();
	UE_LOG(LogCXMR, Log, TEXT("Headset session running: applying view offset %.2f."), PendingViewOffset);
	SetViewOffset(PendingViewOffset);
	return false;
}

void UCXMRSubsystem::StopPendingViewOffset()
{
	if (PendingViewOffsetTicker.IsValid())
	{
		FTSTicker::RemoveTicker(PendingViewOffsetTicker);
	}
	PendingViewOffsetTicker.Reset();
}

// ---------- Depth ----------

void UCXMRSubsystem::SetDepthTest(bool bEnable)
{
	bRequestedDepthTest = bEnable;
	if (bMixedRealityOn) { ApplyRequestedMode(); }
}

void UCXMRSubsystem::ToggleDepthTest()
{
	SetDepthTest(!bRequestedDepthTest);
}

// ---------- Depth test range ----------

void UCXMRSubsystem::ApplyDepthTestRange()
{
	if (IsXRSessionRunning()) { UVarjoOpenXRFunctionLibrary::SetDepthTestRange(bDepthRangeOn, DepthRangeNearZ, DepthRangeFarZ); }
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
	bRequestedEnvironmentDepth = bEnable;
	if (bMixedRealityOn) { ApplyRequestedMode(); }
}

void UCXMRSubsystem::ToggleEnvironmentDepthEstimation()
{
	SetEnvironmentDepthEstimation(!bRequestedEnvironmentDepth);
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
	bRequestedMarkerTracking = bEnable;
	if (!HasRunningSession()) { return false; }
	// The plugin returns "is tracking now on", not "did the call succeed" — it computes
	// XR_ENSURE(...) && Enabled (VarjoMarkersPlugin.cpp:123-136). So a false here after asking for
	// true means the headset refused, and the only honest thing to do is say so rather than let the
	// panel sit at OFF with no explanation. Same posture as SetMixedReality below.
	bool bResult = false;
	ApplyRuntimeCall("Diagnostics.MarkerTracking", [&bResult, bEnable] { bResult = UVarjoOpenXRFunctionLibrary::SetVarjoMarkersEnabled(bEnable); });
#if WITH_DEV_AUTOMATION_TESTS
	if (SessionRunningForTest.IsSet()) { bResult = bEnable; }
#endif

	if (bEnable && !bResult)
	{
		UE_LOG(LogCXMR, Warning,
			TEXT("Marker tracking could not be enabled. IsMarkerTrackingSupported()=%s — on a headset "
			     "that supports markers this usually means the XR session is not up yet."),
			IsMarkerTrackingSupported() ? TEXT("true") : TEXT("false"));
	}

	if (!bResult)
	{
		// Off (or refused) empties the plugin's marker table (VarjoMarkersPlugin.cpp SetVarjoMarkersEnabled: markers.Reset()),
		// and its mode getter then warns on every call for an id it no longer holds — marker labels ask every frame, which
		// flooded a headset session log with two warnings per frame after V / R. The ids come back with the next Detected.
		PluginMarkerIds.Reset();
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
	SetMarkerTracking(!bRequestedMarkerTracking);
}

bool UCXMRSubsystem::SetMarkerTimeout(int32 MarkerId, float Seconds)
{
	return IsXRSessionRunning() && UVarjoOpenXRFunctionLibrary::SetMarkerTimeout(MarkerId, Seconds);
}

bool UCXMRSubsystem::SetMarkerTrackingMode(int32 MarkerId, ECXMRMarkerTrackingMode Mode)
{
	if (!HasRunningSession()) { return false; }
	const EMarkerTrackingMode VarjoMode =
		((bDynamicMarkers ? ECXMRMarkerTrackingMode::Dynamic : Mode) == ECXMRMarkerTrackingMode::Dynamic) ? EMarkerTrackingMode::Dynamic : EMarkerTrackingMode::Stationary;
	bool bResult = false;
	ApplyRuntimeCall(VarjoMode == EMarkerTrackingMode::Dynamic ? "Markers.Dynamic" : "Markers.Stationary",
		[&bResult, MarkerId, VarjoMode] { bResult = UVarjoOpenXRFunctionLibrary::SetMarkerTrackingMode(MarkerId, VarjoMode); });
#if WITH_DEV_AUTOMATION_TESTS
	if (SessionRunningForTest.IsSet()) { bResult = true; }
#endif
	return bResult;
}

bool UCXMRSubsystem::GetMarkerTrackingMode(int32 MarkerId, ECXMRMarkerTrackingMode& OutMode) const
{
	if (!PluginMarkerIds.Contains(MarkerId))
	{
		return false;
	}

	EMarkerTrackingMode VarjoMode = EMarkerTrackingMode::Stationary;
	if (!UVarjoOpenXRFunctionLibrary::GetMarkerTrackingMode(MarkerId, VarjoMode))
	{
		return false;
	}
	OutMode = (VarjoMode == EMarkerTrackingMode::Dynamic) ? ECXMRMarkerTrackingMode::Dynamic : ECXMRMarkerTrackingMode::Stationary;
	return true;
}

void UCXMRSubsystem::HandleMarkerDetected(int32 MarkerId, const FVector& Position, const FRotator& Rotation, const FVector2D& Size)
{
	const bool bNewMarker = !PluginMarkerIds.Contains(MarkerId);
	PluginMarkerIds.Add(MarkerId);
	if (bDynamicMarkers && bNewMarker) { SetMarkerTrackingMode(MarkerId, ECXMRMarkerTrackingMode::Dynamic); }
	OnMarkerDetected.Broadcast(MarkerId, Position, Rotation, Size);
}

void UCXMRSubsystem::HandleMarkerMoved(int32 MarkerId, const FVector& Position, const FRotator& Rotation, const FVector2D& Size)
{
	// 프로필 전환 뒤 Moved로 처음 들어오는 ID도 한 번만 적용함.
	const bool bNewMarker = !PluginMarkerIds.Contains(MarkerId);
	PluginMarkerIds.Add(MarkerId);
	if (bDynamicMarkers && bNewMarker) { SetMarkerTrackingMode(MarkerId, ECXMRMarkerTrackingMode::Dynamic); }
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

void UCXMRSubsystem::RequestAdjustMarkerOffset(FVector DeltaLocation, FRotator DeltaRotation)
{
	OnAdjustMarkerOffsetRequested.Broadcast(DeltaLocation, DeltaRotation);
}

void UCXMRSubsystem::RequestSaveMarkerOffset()
{
	OnSaveMarkerOffsetRequested.Broadcast();
}

void UCXMRSubsystem::RequestResetMarkerOffset()
{
	OnResetMarkerOffsetRequested.Broadcast();
}

void UCXMRSubsystem::PublishMarkerOffset(FVector Location, FRotator Rotation)
{
	MarkerLocationOffset = Location;
	MarkerRotationOffset = Rotation;
}

// ---------- Viewer relay ----------

void UCXMRSubsystem::RequestViewerAction(ECXMRViewerAction Action)
{
	OnViewerAction.Broadcast(Action);
}

void UCXMRSubsystem::ReportVehicleStatus(FText InVehicleName, int32 InVehicleIndex, int32 InVehicleCount)
{
	VehicleName  = InVehicleName;
	VehicleIndex = InVehicleIndex;
	VehicleCount = InVehicleCount;
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

void UCXMRSubsystem::SetDynamicMarkers(bool bEnable)
{
	if (bDynamicMarkers == bEnable) { return; }
	bDynamicMarkers = bEnable;
	ApplyDynamicMarkerModes();
}

void UCXMRSubsystem::ApplyDynamicMarkerModes()
{
	for (int32 Id : PluginMarkerIds)
	{
		ECXMRMarkerTrackingMode Mode = ECXMRMarkerTrackingMode::Stationary;
		bool bConfigured = false;
		if (GetWorld())
		{
			for (TActorIterator<ACXMRMarkerFollower> It(GetWorld()); It; ++It)
			{ if (!It->IsActorBeingDestroyed() && It->MarkerId == Id) { Mode=It->TrackingMode; bConfigured=true; break; } }
			if (!bConfigured)
			{
				for (TActorIterator<AActor> It(GetWorld()); It; ++It)
				{
					const UCXMRPlacementComponent* Placement = It->FindComponentByClass<UCXMRPlacementComponent>();
					FCXMRMarkerEntry Entry;
					if (Placement && Placement->MarkerProfile && Placement->MarkerProfile->FindEntry(Id, Entry) && Entry.Role == ECXMRMarkerRole::Calibration)
					{ Mode=Entry.TrackingMode; break; }
				}
			}
		}
		SetMarkerTrackingMode(Id, Mode);
	}
}
