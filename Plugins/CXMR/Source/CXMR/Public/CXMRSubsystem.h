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

UCLASS(DisplayName = "CXMR Subsystem")
class CXMR_API UCXMRSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	//~ Begin USubsystem
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	//~ End USubsystem

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

	// NOTE: range BEYOND [NearZ, FarZ] shows VR only (real world disappears). Default 0..0.75m = hand range.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Depth") void SetDepthTestRange(bool bEnable, float NearZ = 0.0f, float FarZ = 0.75f);
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

	// --- LOCAL / CLIENT (never replicate — per-headset preference) ---
	UPROPERTY(Transient) float ViewOffset = 1.0f;

	// Bridge handlers bound to the plugin's static marker delegates (see .cpp).
	void HandleMarkerDetected(int32 MarkerId, const FVector& Position, const FRotator& Rotation, const FVector2D& Size);
	void HandleMarkerMoved(int32 MarkerId, const FVector& Position, const FRotator& Rotation, const FVector2D& Size);
	void HandleMarkerLost(int32 MarkerId);

	FDelegateHandle DetectedHandle;
	FDelegateHandle MovedHandle;
	FDelegateHandle LostHandle;
};
