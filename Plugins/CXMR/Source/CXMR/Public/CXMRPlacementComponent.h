// Copyright GMTCK CX.
//
// UCXMRPlacementComponent (Viewer, world-side) — places the vehicle.
//   * Subscribes to the SUBSYSTEM's marker events (not the plugin — single façade).
//   * On a Calibration-role marker: computes VehicleWorld = Inverse(LocalOffset) * MarkerWorld
//     and sets it on VehicleRoot (its children = the vehicle mesh, so swapping the mesh keeps alignment).
//   * Boundary #7: Core computes/relays marker data; THIS (Viewer) knows the vehicle.
//
// Strategies: MarkerAnchor (interior one-shot freeze / exterior stand marker) and PawnRelative
// (exterior turntable — place in front of the pawn, no marker). Multi-marker layers on top later.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRTypes.h"
#include "CXMRPlacementComponent.generated.h"

class UCXMRSubsystem;
class UCXMRMarkerProfile;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Placement")
class CXMR_API UCXMRPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRPlacementComponent();

	/** Per-program marker config (project asset, /Game/Vehicles/[program]/).
	 *  ⚠ At runtime assign through SetMarkerProfile() — a plain write skips the calibration swap
	 *  and leaves the previous profile holding field values. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") TObjectPtr<UCXMRMarkerProfile> MarkerProfile;

	/** Swap the profile and bring its saved calibration with it. Restores the outgoing profile to
	 *  the values it shipped with, so switching vehicles never leaves an asset edited. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void SetMarkerProfile(UCXMRMarkerProfile* NewProfile);

	/** Actor placed at the calibrated vehicle origin (children = vehicle mesh). Defaults to this component's owner. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "CXMR|Placement") TObjectPtr<AActor> VehicleRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") ECXMRPlacementMode Mode = ECXMRPlacementMode::MarkerAnchor;

	/** Marker Anchor: once calibrated, THIS component stops re-placing the vehicle (one-shot freeze —
	 *  interior). Local to placement; the headset keeps tracking markers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") bool bFreezeAfterCalibration = true;

	/** ALSO stop the headset's marker tracking once calibrated. Off by default, and deliberately
	 *  separate from the freeze above: marker tracking is GLOBAL, so turning it off here kills
	 *  DynamicObject markers (doors, props, cups) along with the calibration ones. Only enable it for
	 *  programs that track nothing but calibration markers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") bool bStopMarkerTrackingWhenCalibrated = false;

	/** Markers needed before freezing. 1 = single-marker (uses marker orientation). 2+ = baseline yaw
	 *  from marker POSITIONS + floor assumption (roll/pitch=0), robust against single-marker angle noise.
	 *  A single marker still gives a provisional placement until more arrive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ClampMin = "1")) int32 MinMarkersToCalibrate = 2;

	/** Pawn Relative: distance in front of the pawn (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ClampMin = "0.0")) float PawnRelativeDistance = 350.0f;

	/** Minimum movement (cm) before a Moved update re-runs calibration. Marker poses jitter every
	 *  frame; without this the vehicle would be re-placed continuously and read as unstable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ClampMin = "0.0"))
	float MarkerUpdateThreshold = 0.5f;

	UPROPERTY(BlueprintReadOnly, Category = "CXMR|Placement") bool bCalibrated = false;

	/** Re-run calibration: clears the flag and cycles marker tracking so Detected fires again. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void Recalibrate();

	/** Pawn Relative: place the vehicle in front of the local player, on the floor, facing them. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void PlaceInFrontOfPawn();

	/** Adjust marker offset (temporary). X/Y/Z cm + rotation deg. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void AdjustMarkerOffset(FVector DeltaLocation, FRotator DeltaRotation);

	/** Save adjusted offset to the marker profile (permanent). */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void SaveMarkerOffsetToProfile();

	/** Reset temporary offset adjustments. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void ResetMarkerOffset();

	/** Adopt the vehicle's current transform as the pose the offset is measured from. Call this after
	 *  moving the vehicle by any other means (ergonomics, a script) — otherwise the next offset nudge
	 *  snaps it back to whatever calibration last computed. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void RebaseToCurrentTransform();

	// ---- Field calibration -------------------------------------------------------------------
	// Markers get stuck onto a clay model by hand and nobody measures where they ended up, so the
	// authored LocalOffsets mean nothing. These turn that around: place the vehicle by eye, then
	// record where the markers are RELATIVE to it. From then on seeing a marker restores that pose.

	/** Record every detected marker's pose relative to the vehicle as it stands right now. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void LearnMarkerLayout();

	/** Write the current marker layout to Saved/CXMR. A cooked build cannot save its data assets,
	 *  so without this every restart loses the calibration. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") bool SaveCalibrationToDisk();

	/** Apply a previously saved layout, if one exists for this profile. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") bool LoadCalibrationFromDisk();

	/** Delete the saved file and restore the offsets the profile shipped with. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void ResetCalibrationToAuthored();

	/** Where the calibration for the current profile is written. Empty if no profile is set. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement") FString GetCalibrationFilePath() const;

	/** Live temporary offset. NOTE: the control panel must NOT read these — this component does not
	 *  live on the pawn, so widgets cannot find it. It publishes the same values to the subsystem;
	 *  UI reads UCXMRSubsystem::GetMarkerLocationOffset() instead. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement") FVector GetMarkerLocationOffset() const { return TempMarkerLocationOffset; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement") FRotator GetMarkerRotationOffset() const { return TempMarkerRotationOffset; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	AActor* ResolveVehicleRoot();

	// Detected fires once per ID per session and carries the FIRST, noisiest pose; every refinement
	// after that — and every re-acquisition following a loss — arrives as Moved. Listening only to
	// Detected locked calibration to one bad sample and made multi-marker impossible, because a second
	// marker seen after the first would never reach DetectedCalib.
	UFUNCTION() void HandleMarkerDetected(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size);
	UFUNCTION() void HandleMarkerMoved(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size);
	UFUNCTION() void HandleRecalibrateRequest();
	UFUNCTION() void HandlePlaceRequest();

	/** Shared body of the two handlers above. bIsFirstSighting drives the one-shot per-marker config. */
	void HandleMarkerPose(int32 MarkerId, const FVector& Position, const FRotator& Rotation, bool bIsFirstSighting);

	/** Recompute + apply the vehicle transform from the accumulated calibration markers. */
	void RecomputeCalibration();
	/** Rigid fit (yaw about Z + translation, roll/pitch = 0) from >=2 marker positions. */
	bool ComputeMultiMarkerTransform(FTransform& Out) const;

	/** Detected Calibration markers this session: id -> marker world transform. */
	TMap<int32, FTransform> DetectedCalib;

	/** Temporary offset adjustment (cm + deg) for fine-tuning marker calibration. */
	FVector TempMarkerLocationOffset = FVector::ZeroVector;
	FRotator TempMarkerRotationOffset = FRotator::ZeroRotator;

	/** The pose the manual offset is measured from — whatever markers (or a manual placement) last
	 *  produced, before the offset is applied. Cached so the offset can still be nudged when no
	 *  marker is in view: without it, looking away from the markers froze the adjustment keys. */
	FTransform BaseVehicleTransform = FTransform::Identity;
	bool bHaveBasePose = false;

	/** Writes BaseVehicleTransform + the temporary offset onto the vehicle. */
	void ApplyPlacement();

	/** Mirrors the live offset onto the subsystem so the control panel can display it. */
	void PublishOffset();

	/** Snapshots the profile's shipped offsets so ResetCalibrationToAuthored can undo field edits,
	 *  and so EndPlay can hand the asset back unmodified (PIE would otherwise leave it dirty). */
	void CaptureAuthoredOffsets();
	void RestoreAuthoredOffsets();

	/** Idempotent: snapshot the current profile and apply its saved calibration, once per profile.
	 *  Called from BeginPlay AND from SetMarkerProfile because component BeginPlay order is not
	 *  guaranteed — whichever runs first wins and the other becomes a no-op. */
	void EnsureCalibrationLoaded();

	/** Offsets exactly as the profile shipped them, keyed by marker id. */
	TMap<int32, FTransform> AuthoredOffsets;
	TWeakObjectPtr<UCXMRMarkerProfile> CapturedProfile;
	bool bCalibrationLoaded = false;

	/** Calibration files already copied to their .startup backup this session. */
	TSet<FString> StartupBackedUp;

	// Relayed from the subsystem — input and UI cannot reach this component directly.
	UFUNCTION() void HandleAdjustOffsetRequest(FVector DeltaLocation, FRotator DeltaRotation);
	UFUNCTION() void HandleSaveOffsetRequest();
	UFUNCTION() void HandleResetOffsetRequest();

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
};
