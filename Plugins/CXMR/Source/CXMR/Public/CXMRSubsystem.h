// Copyright GMTCK CX.
//
// UCXMRSubsystem — the single control surface for Varjo MR features.
//   * Every Varjo plugin call funnels through here (single façade).
//   * Feature state is OWNED here (not read back from the plugin each time).
//   * Every change broadcasts a delegate so UI / gameplay react.
//   * Marker events are re-exposed here — subscribe to the SUBSYSTEM, never the plugin.
//
// This is the ONLY CXMR class that includes VarjoOpenXR headers (kept in the .cpp).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CXMRTypes.h"
#include "CXMRSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCXMROnBoolChanged, bool, bNewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCXMROnFloatChanged, float, NewValue);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(FCXMROnMarkerPose, int32, MarkerId, FVector, Position, FRotator, Rotation, FVector2D, Size);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCXMROnMarkerId, int32, MarkerId);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FCXMROnRequest);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCXMROnViewerAction, ECXMRViewerAction, Action);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCXMROnIntChanged, int32, Value);

UCLASS(DisplayName = "CXMR Subsystem")
class CXMR_API UCXMRSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

	// ---------- Diagnostics ----------
	//
	// Console: CXMR.DumpMRState
	//
	// Exists because "the screen is black" is not a diagnosis. Passthrough needs alpha 0, and the
	// things that can force alpha to 1 live in four different places — the blend mode, the alpha
	// propagation setting, the scene colour format, and the post-process chain. Reading them one CVar
	// at a time in a headset is slow and misses the one comparison the console cannot show at all:
	// what the Varjo plugin thinks MR is doing versus what CXMR has cached.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Diagnostics") void DumpMRState() const;

	// ---------- Support queries (pass-through; valid once the XR session is up) ----------
	UFUNCTION(BlueprintPure, Category = "CXMR|MR")        bool IsMixedRealitySupported() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Markers")   bool IsMarkerTrackingSupported() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Depth")     bool IsEnvironmentDepthEstimationSupported() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Foveation") bool IsFoveatedRenderingSupported() const;

	// ---------- Mixed Reality (VR <-> MR) ----------
	// Toggles the alpha-blend environment blend mode via xr.OpenXREnvironmentBlendMode (3=MR, 1=VR).
	UFUNCTION(BlueprintCallable, Category = "CXMR|MR") void SetMixedReality(bool bEnable);
	UFUNCTION(BlueprintCallable, Category = "CXMR|MR") void ToggleMixedReality();
	UFUNCTION(BlueprintPure,     Category = "CXMR|MR") bool IsMixedRealityOn() const { return bMixedRealityOn; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|MR") FCXMROnBoolChanged OnMixedRealityChanged;

	// ---------- VR background (the virtual room: sky, walls, floor) ----------
	// Bound to IA_Varjo_MRBackgroundToggle. Defaults to VISIBLE so the editor / PIE looks like plain VR;
	// turning MR on is what calls for hiding it, and that stays an explicit act.
	// Objects hide THEMSELVES via UCXMRSceneObjectComponent (Role = VROnly) listening to the delegate —
	// there is deliberately no central list of actors here.
	UFUNCTION(BlueprintCallable, Category = "CXMR|MR") void SetVRBackgroundVisible(bool bVisible);
	UFUNCTION(BlueprintCallable, Category = "CXMR|MR") void ToggleVRBackground();
	UFUNCTION(BlueprintPure,     Category = "CXMR|MR") bool IsVRBackgroundVisible() const { return bVRBackgroundVisible; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|MR") FCXMROnBoolChanged OnVRBackgroundChanged;

	/** MR on hides the virtual room, MR off restores it; B still overrides afterwards. Off = the two
	 *  toggles are fully independent, which lets a tester reach "VR with the room hidden" — a state
	 *  that renders black and means nothing, because VR ignores alpha entirely. */
	UPROPERTY(BlueprintReadWrite, Category = "CXMR|MR") bool bCoupleVRBackgroundToMR = true;

	// ---------- Camera render position / View offset (0 = eye, 1 = passthrough camera) ----------
	UFUNCTION(BlueprintCallable, Category = "CXMR|MR") void  SetViewOffset(float Offset);
	UFUNCTION(BlueprintCallable, Category = "CXMR|MR") void  ToggleViewOffset(); // flips between 0 (eye) and 1 (camera)
	UFUNCTION(BlueprintPure,     Category = "CXMR|MR") float GetViewOffset() const { return ViewOffset; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|MR") FCXMROnFloatChanged OnViewOffsetChanged;

	// ---------- Depth occlusion ----------
	UFUNCTION(BlueprintCallable, Category = "CXMR|Depth") void SetDepthTest(bool bEnable);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Depth") void ToggleDepthTest();
	UFUNCTION(BlueprintPure,     Category = "CXMR|Depth") bool IsDepthTestOn() const { return bDepthTestOn; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Depth") FCXMROnBoolChanged OnDepthTestChanged;

	// ---------- Depth test RANGE ----------
	//
	// This is not a refinement of the depth test — it is the difference between a usable depth test and
	// an unusable one. With the range DISABLED the plugin submits farZ = HUGE_VALF
	// (DepthPlugin.cpp:58-59), so the compositor depth-tests the ENTIRE room at unlimited distance
	// against an estimated video depth that Varjo's own guidance calls unreliable on reflective, dark
	// and distant surfaces. That is what makes virtual geometry flicker. A bounded range is the fix.
	//
	// The plugin's struct carries FarZ = 0.75f, but it never reaches the compositor while the range is
	// disabled — so "the default is 0..0.75m" was never true in practice. CXMR now enables the range
	// itself and keeps it applied, which is what finally makes that statement correct.
	//
	// NOTE: BEYOND [NearZ, FarZ] the compositor ignores depth completely and VR content overwrites the
	// video stream — the real world disappears out there. Narrowing the range is not the safe direction.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Depth") void SetDepthTestRange(bool bEnable, float NearZ = 0.0f, float FarZ = 0.75f);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Depth") void ToggleDepthTestRange();

	/** Nudges the bounds (metres) and re-applies. Near is kept below Far; both stay >= 0. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Depth") void AdjustDepthTestRange(float NearDelta, float FarDelta);

	UFUNCTION(BlueprintPure, Category = "CXMR|Depth") bool  IsDepthTestRangeOn() const   { return bDepthRangeOn; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Depth") float GetDepthTestRangeNearZ() const { return DepthRangeNearZ; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Depth") float GetDepthTestRangeFarZ() const  { return DepthRangeFarZ; }

	/** Fires on enable/disable AND on every bound change — the panel readout needs both. */
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Depth") FCXMROnBoolChanged OnDepthTestRangeChanged;

	UFUNCTION(BlueprintCallable, Category = "CXMR|Depth") void SetEnvironmentDepthEstimation(bool bEnable);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Depth") void ToggleEnvironmentDepthEstimation();
	UFUNCTION(BlueprintPure,     Category = "CXMR|Depth") bool IsEnvironmentDepthEstimationOn() const { return bEnvDepthOn; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Depth") FCXMROnBoolChanged OnEnvironmentDepthEstimationChanged;

	// ---------- Masking (Custom-Depth mask) ----------
	// Subsystem owns the on/off state + broadcasts. The PP_MR material-parameter-collection scalar
	// is applied by a listener that holds the MPC asset ref (kept out of here to preserve portability).
	UFUNCTION(BlueprintCallable, Category = "CXMR|Masking") void SetMasking(bool bEnable);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Masking") void ToggleMasking();
	UFUNCTION(BlueprintPure,     Category = "CXMR|Masking") bool IsMaskingOn() const { return bMaskingOn; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Masking") FCXMROnBoolChanged OnMaskingChanged;

	// ---------- Hand visualization (verification instrument) ----------
	// Purely ours — no plugin call. UCXMRHandDebugComponent draws the engine's hand-tracking joints
	// while this is on. Bound to IA_Varjo_HandVisualizationToggle, which shipped in the Varjo example
	// but had never been wired to anything.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Hands") void SetHandVisualization(bool bEnable);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Hands") void ToggleHandVisualization();
	UFUNCTION(BlueprintPure,     Category = "CXMR|Hands") bool IsHandVisualizationOn() const { return bHandVisualizationOn; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Hands") FCXMROnBoolChanged OnHandVisualizationChanged;

	// ---------- Varjo Markers ----------
	UFUNCTION(BlueprintCallable, Category = "CXMR|Markers") bool SetMarkerTracking(bool bEnable);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Markers") void ToggleMarkerTracking();
	UFUNCTION(BlueprintPure,     Category = "CXMR|Markers") bool IsMarkerTrackingOn() const { return bMarkerTrackingOn; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Markers") FCXMROnBoolChanged OnMarkerTrackingChanged;

	// Re-exposed marker events — subscribe to the SUBSYSTEM, never the plugin directly.
	// Position/Rotation are already world-space (WorldToMeters + TrackingToWorld applied by the plugin).
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Markers") FCXMROnMarkerPose OnMarkerDetected; // fires once per ID per session
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Markers") FCXMROnMarkerPose OnMarkerMoved;    // re-acquire after loss fires THIS, not Detected
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Markers") FCXMROnMarkerId   OnMarkerLost;

	// Per-marker config. CAVEAT (plugin): only succeeds AFTER the marker has been detected (call in OnMarkerDetected).
	UFUNCTION(BlueprintCallable, Category = "CXMR|Markers") bool SetMarkerTimeout(int32 MarkerId, float Seconds);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Markers") bool SetMarkerTrackingMode(int32 MarkerId, ECXMRMarkerTrackingMode Mode);

	// ---------- Placement request relay (input/UI -> placement component, decoupled) ----------
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void RequestRecalibrate();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void RequestPlaceInFront();
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Placement") FCXMROnRequest OnRecalibrateRequested;
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Placement") FCXMROnRequest OnPlaceRequested;

	// ---------- Viewer relay (input/UI -> vehicle actor: turntable, vehicle/trim cycling) ----------
	// Same rendezvous as placement: the pawn holds the input, the vehicle holds the turntable.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Viewer") void RequestViewerAction(ECXMRViewerAction Action);
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Viewer") FCXMROnViewerAction OnViewerAction;

	/** Held-stick turntable rotation. Raw axis; the turntable applies deadzone and speed. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Viewer") void RequestTurntableAxis(float AxisValue);
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Viewer") FCXMROnFloatChanged OnTurntableAxis;

	// ---------- Viewer status mirror (vehicle actor -> UI, decoupled) ----------
	// The loader lives on the vehicle actor; the panel only ever knows the subsystem. The loader
	// reports its current selection here on every load / trim / CMF change, and the panel reads it
	// back — so the display stays live without the widget ever holding a loader reference.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Viewer")
	void ReportVehicleStatus(FText VehicleName, int32 VehicleIndex, int32 VehicleCount,
	                         FText TrimName, int32 TrimIndex, int32 TrimCount, int32 CMFIndex);

	UFUNCTION(BlueprintPure, Category = "CXMR|Viewer") FText GetVehicleName() const  { return VehicleName; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Viewer") int32 GetVehicleIndex() const { return VehicleIndex; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Viewer") int32 GetVehicleCount() const { return VehicleCount; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Viewer") FText GetTrimName() const     { return TrimName; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Viewer") int32 GetTrimIndex() const    { return TrimIndex; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Viewer") int32 GetTrimCount() const    { return TrimCount; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Viewer") int32 GetCMFIndex() const     { return CMFIndex; }

	/** Fires on every vehicle status change — the panel refreshes off this like any other. */
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Viewer") FCXMROnRequest OnVehicleStatusChanged;

	// ---------- Ergonomics relay (input/UI -> ergonomics component: percentile eye/hip snap) ----------
	// Same rendezvous again: input on the pawn, the ergonomics component on the vehicle actor.
	// Step +1 / -1 cycles the manikin; the component decides VR viewpoint vs MR vehicle move.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Ergonomics") void RequestErgonomicsStep(int32 Step);
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Ergonomics") FCXMROnIntChanged OnErgonomicsStepRequested;

	// Status mirror (ergonomics component -> UI), same shape as the vehicle mirror.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Ergonomics")
	void ReportManikin(FText Name, int32 Index, int32 Count);
	UFUNCTION(BlueprintPure, Category = "CXMR|Ergonomics") FText GetManikinName() const  { return ManikinName; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Ergonomics") int32 GetManikinIndex() const { return ManikinIndex; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Ergonomics") int32 GetManikinCount() const { return ManikinCount; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Ergonomics") FCXMROnRequest OnManikinChanged;

private:
	// ============================================================================
	//  STATE — split into two buckets for future networking (single-player-first,
	//  network-aware). SHARED would replicate later; LOCAL never does.
	// ============================================================================

	// --- SHARED / SESSION (replicate later) ---
	UPROPERTY(Transient) bool bMixedRealityOn   = false;
	UPROPERTY(Transient) bool bDepthTestOn      = false;
	UPROPERTY(Transient) bool bEnvDepthOn       = false;
	UPROPERTY(Transient) bool bMaskingOn        = false;
	UPROPERTY(Transient) bool bMarkerTrackingOn = false;
	UPROPERTY(Transient) bool bVRBackgroundVisible = true;
	UPROPERTY(Transient) bool bHandVisualizationOn = false;

	// Depth test range. ON by default, and that default is the point: leaving it off hands the
	// compositor farZ = HUGE_VALF (see the header comment above) and the room flickers.
	UPROPERTY(Transient) bool  bDepthRangeOn    = true;
	UPROPERTY(Transient) float DepthRangeNearZ  = 0.0f;
	UPROPERTY(Transient) float DepthRangeFarZ   = 0.75f;   // metres — Varjo's "good for hand occlusion"

	UPROPERTY(Transient) FText VehicleName;
	UPROPERTY(Transient) int32 VehicleIndex = 0;
	UPROPERTY(Transient) int32 VehicleCount = 0;
	UPROPERTY(Transient) FText TrimName;
	UPROPERTY(Transient) int32 TrimIndex = 0;
	UPROPERTY(Transient) int32 TrimCount = 0;
	UPROPERTY(Transient) int32 CMFIndex  = 0;

	UPROPERTY(Transient) FText ManikinName;
	UPROPERTY(Transient) int32 ManikinIndex = 0;
	UPROPERTY(Transient) int32 ManikinCount = 0;

	// --- LOCAL / CLIENT (never replicate — per-headset preference) ---
	UPROPERTY(Transient) float ViewOffset = 1.0f;

	/** Pushes the cached range to the plugin. Called on change AND whenever the depth test is enabled,
	 *  because the plugin resets its whole state struct on session creation (DepthPlugin.cpp
	 *  PostCreateSession: State = {}) and would otherwise fall back to the unbounded default. */
	void ApplyDepthTestRange();

	// Bridge handlers bound to the plugin's static marker delegates (see .cpp).
	void HandleMarkerDetected(int32 MarkerId, const FVector& Position, const FRotator& Rotation, const FVector2D& Size);
	void HandleMarkerMoved(int32 MarkerId, const FVector& Position, const FRotator& Rotation, const FVector2D& Size);
	void HandleMarkerLost(int32 MarkerId);

	FDelegateHandle DetectedHandle;
	FDelegateHandle MovedHandle;
	FDelegateHandle LostHandle;
};
