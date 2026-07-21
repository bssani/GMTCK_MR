// Copyright GMTCK CX.
//
// ACXMRVehicleRoot — the placed vehicle origin.
//   * VehicleAnchor (root) receives the calibrated transform; the vehicle mesh attaches under it,
//     so swapping the mesh / trim keeps the alignment.
//   * Placement component (pre-attached) targets this actor by default (VehicleRoot = owner).
//   * Hierarchy: VehicleAnchor (calibrated) -> Turntable (exterior yaw) -> spawned vehicle.
//     Each transform has exactly one writer, so spinning never disturbs the calibration and
//     recalibrating never resets the viewing angle.
//
// Usage: make a project BP subclass per program (/Game/Vehicle/[program]/) and set Loader->Profile.
// The vehicle profile carries its own marker profile, so Placement is configured from it on load.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CXMRVehicleRoot.generated.h"

class UCXMRPlacementComponent;
class UCXMRVehicleLoaderComponent;
class UCXMRTurntableComponent;

UCLASS()
class CXMR_API ACXMRVehicleRoot : public AActor
{
	GENERATED_BODY()

public:
	ACXMRVehicleRoot();

	/** Calibrated origin. ONLY calibration writes this — never rotate it. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TObjectPtr<USceneComponent> VehicleAnchor;

	/** Exterior turntable yaw, applied under the anchor so calibration is left intact. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TObjectPtr<USceneComponent> Turntable;

	/** Places this actor from Calibration markers or the pawn. Targets this actor by default. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TObjectPtr<UCXMRPlacementComponent> Placement;

	/** Spawns the vehicle under VehicleAnchor and swaps trim / CMF. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TObjectPtr<UCXMRVehicleLoaderComponent> Loader;

	/** Exterior rotation (held stick + A/B continuous spin). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TObjectPtr<UCXMRTurntableComponent> TurntableControl;
};
