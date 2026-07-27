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

	/** Per-program marker config (project asset, /Game/Vehicles/[program]/). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") TObjectPtr<UCXMRMarkerProfile> MarkerProfile;

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

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
};
