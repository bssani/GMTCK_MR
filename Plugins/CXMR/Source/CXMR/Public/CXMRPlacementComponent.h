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
#include "Engine/TimerHandle.h"
#include "CXMRPlacementComponent.generated.h"

class UCXMRSubsystem;
class UCXMRMarkerProfile;

/** What manual yaw adjustments rotate the vehicle around. */
UENUM(BlueprintType)
enum class ECXMRNudgePivot : uint8
{
	/** Centre of the calibration markers seen this session (the viewer if none yet). Whatever was lined up
	 *  next to the markers stays put while the rest of the car swings into place. */
	Markers,
	/** The viewer's head. The car turns around the person sitting in it. */
	Viewer,
	/** The vehicle actor's own origin — often far from anything visible for CAD-origin vehicles. */
	VehicleOrigin
};

/**
 * Which way the adjust keys and steppers call "away from me".
 *
 * The frame used to be read from the headset on every press. Aligning a car means looking around constantly — at the
 * A-pillar, then the console, then down at the sill — so each press went a different way, and holding a key while
 * turning the head dragged the car along a curve. Worst of all a key and its opposite no longer cancelled.
 */
UENUM(BlueprintType)
enum class ECXMRNudgeFrame : uint8
{
	/** The way the viewer faced at the FIRST adjustment, held until re-taken. Turning your head afterwards does not
	 *  turn the axes, so a key and its opposite cancel exactly. */
	ViewerLatched,
	/** Wherever the viewer is looking at this instant — what it did before 2026-09-21. */
	ViewerLive,
	/** The vehicle's own forward and right, flattened level: "away" is the way the CAR faces, whatever you look at. */
	Vehicle
};

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

	/** Marker Anchor: turn the headset's marker tracking on at start, retrying until the XR session is up. Tracking
	 *  starts off, so without this a restarted session sat uncalibrated — the saved layout unused — until V. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") bool bStartMarkerTrackingOnBeginPlay = true;

	/** Once enough markers are seen, keep refining from their updates for this long, then freeze. A marker's first
	 *  sighting is its noisiest sample, and freezing on it locked that noise into the placement. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ClampMin = "0.0")) float CalibrationSettleSeconds = 1.5f;

	/** Markers needed before freezing. 1 = single marker. 2+ = baseline yaw from marker POSITIONS + floor
	 *  assumption (roll/pitch=0), robust against single-marker angle noise. A single marker still gives a
	 *  provisional placement until more arrive. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ClampMin = "1")) int32 MinMarkersToCalibrate = 2;

	/** Keep the vehicle level on the single-marker path as well. The multi-marker solve always assumes a
	 *  level floor; letting one marker's own tilt through laid the car at whatever angle the marker was
	 *  stuck on, put the adjust keys on tilted axes, and made the pose jump when a second marker switched
	 *  the solve to the level one. Turn off only for a vehicle that really sits tilted. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") bool bKeepLevel = true;

	/** What the manual yaw adjustment rotates the vehicle around. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") ECXMRNudgePivot NudgePivot = ECXMRNudgePivot::Markers;

	/** Which way the adjust keys call "away from me". Latched by default, so the axes hold still while you look around. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") ECXMRNudgeFrame NudgeFrame = ECXMRNudgeFrame::ViewerLatched;

	/** Distance one [-]/[+] press in the tuning window moves the vehicle, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ClampMin = "0.1")) float NudgeMoveStep = 1.0f;

	/** Angle one [-]/[+] press in the tuning window turns the vehicle, degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ClampMin = "0.1")) float NudgeYawStep = 1.0f;

	/** Pawn Relative: distance in front of the pawn (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ClampMin = "0.0")) float PawnRelativeDistance = 350.0f;

	/** Minimum movement (cm) before a Moved update re-runs calibration. Marker poses jitter every
	 *  frame; without this the vehicle would be re-placed continuously and read as unstable. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ClampMin = "0.0"))
	float MarkerUpdateThreshold = 0.5f;

	/** Average every marker sample taken while calibrating, rather than placing from the latest one. One frame of
	 *  marker pose carries a few millimetres of noise, and whichever frame happened to arrive last decided where the
	 *  car stood — differently every session, which is what made a carefully aligned car come back somewhere else. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") bool bAverageMarkerSamples = true;

	UPROPERTY(BlueprintReadOnly, Category = "CXMR|Placement") bool bCalibrated = false;

	/** Re-run calibration: clears the flag and cycles marker tracking so Detected fires again. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void Recalibrate();

	/** Pawn Relative: place the vehicle in front of the local player, on the floor, facing them. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void PlaceInFrontOfPawn();

	/**
	 * Move the vehicle in the VIEWER's frame, whichever way the car faces: X away from the viewer (head
	 * direction flattened), Y to the viewer's right, Z world up; yaw about world up around NudgePivot,
	 * positive = clockwise seen from above. The result is kept as the offset, so saving and learning work
	 * exactly as before.
	 */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void NudgeVehicle(FVector ViewerDelta, float YawDelta);

	/**
	 * NudgeVehicle with the frame handed in rather than read from the headset: HeadingYaw is what "away from me"
	 * means and ViewerLocation is where the Viewer pivot sits. For scripts, and for tests that have no camera.
	 * NudgeFrame still decides whether HeadingYaw is used or overridden by the latch / the vehicle's own facing.
	 */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement")
	void NudgeVehicleInFrame(FVector ViewerDelta, float YawDelta, float HeadingYaw, FVector ViewerLocation);

	/**
	 * The world yaw the adjust keys treat as "away from me", for the current NudgeFrame. ViewerYaw is where the
	 * viewer is looking now; it is what comes back when nothing is latched and the frame is not the vehicle's.
	 */
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement") float ResolveNudgeYaw(float ViewerYaw) const;

	/** Hold HeadingYaw as "away from me" from now on (ViewerLatched). */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void LatchNudgeHeading(float HeadingYaw);

	/** Latch the way the viewer is facing right now — use it when the axes ended up crooked. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void RetakeNudgeHeading();

	/** True once a heading is held. False = the next adjustment takes one. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement") bool HasNudgeHeading() const { return bHaveNudgeHeading; }

	/** Put the vehicle at this world pose, kept as the manual offset like a nudge — so Save adjustment and Learn work
	 *  on it unchanged. Used by alignments that compute a whole pose at once (touched box corners). */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement") void MoveVehicleTo(FTransform VehicleWorld);

	/** Adjust the offset in the VEHICLE's own frame (temporary). X/Y/Z cm + rotation deg. Kept for scripts;
	 *  the adjust keys go through NudgeVehicle. */
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

	/** Record every marker in view relative to the vehicle as it stands right now, and save. Markers the profile does
	 *  not list are added to the layout (and the saved file), so a site needs no marker ids typed in beforehand. */
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
	/** Rigid fit (yaw about Z + translation, roll/pitch = 0) from >=2 marker positions.
	 *  OutResidual = RMS distance in cm between the measured markers and the saved layout after the fit. */
	bool ComputeMultiMarkerTransform(FTransform& Out, float& OutResidual) const;

	/** One marker's samples for this calibration. Positions are averaged directly; the orientation is averaged as
	 *  axis vectors, because averaging angles wraps around. */
	struct FMarkerSamples
	{
		FVector PositionSum = FVector::ZeroVector;
		FVector ForwardSum  = FVector::ZeroVector;
		FVector UpSum       = FVector::ZeroVector;
		double  SquaredSum  = 0.0;
		int32   Count       = 0;
		/** Spread of the samples around their own mean, cm — how steady the tracking was. */
		float   Scatter     = 0.0f;

		FTransform Mean() const;
	};

	/** Samples per marker, cleared by Recalibrate. */
	TMap<int32, FMarkerSamples> MarkerSamples;

	/** Adds one sample and returns that marker's averaged pose. */
	FTransform AccumulateSample(int32 MarkerId, const FVector& Position, const FRotator& Rotation);

	/** The worst marker scatter this calibration, cm. */
	float WorstScatter() const;

	/** RMS fit error to the saved layout, cm. Negative until a multi-marker fit has run. */
	float LayoutFitError = -1.0f;

	/** Adds this component's rows (steps, nudges, pivot, save / learn) to the tuning window. */
	void RegisterTunables();

	/** World point NudgeVehicle turns the vehicle around, per NudgePivot. */
	FVector ResolveNudgePivot(const FTransform& Vehicle, const FVector& ViewerLocation) const;

	/** Markers of the layout used for placement this session: id -> marker world transform. */
	TMap<int32, FTransform> DetectedCalib;

	/** Every marker seen this session, listed in the profile or not, at its latest pose. Learn records from these. */
	TMap<int32, FTransform> SeenMarkers;

	FTimerHandle SettleTimer;
	FTimerHandle TrackingStartTimer;
	int32 TrackingStartAttempts = 0;

	/** End of the settle window: freeze (and optionally stop tracking). */
	void FinishCalibration();

	/** Turns marker tracking on once the session supports it; retries for a while, then says so. */
	void TryStartMarkerTracking();

	/** Temporary offset adjustment (cm + deg) for fine-tuning marker calibration. */
	FVector TempMarkerLocationOffset = FVector::ZeroVector;
	FRotator TempMarkerRotationOffset = FRotator::ZeroRotator;

	/** The pose the manual offset is measured from — whatever markers (or a manual placement) last
	 *  produced, before the offset is applied. Cached so the offset can still be nudged when no
	 *  marker is in view: without it, looking away from the markers froze the adjustment keys. */
	FTransform BaseVehicleTransform = FTransform::Identity;
	bool bHaveBasePose = false;

	/** The yaw the adjust keys hold as "away from me" (ViewerLatched), and whether one has been taken yet. */
	float LatchedNudgeYaw = 0.0f;
	bool bHaveNudgeHeading = false;

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
