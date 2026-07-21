// Copyright GMTCK CX.
//
// ACXMRVehicleRoot — the placed vehicle origin.
//   * VehicleAnchor (root) receives the calibrated transform; the vehicle mesh attaches under it,
//     so swapping the mesh / trim keeps the alignment.
//   * Placement component (pre-attached) targets this actor by default (VehicleRoot = owner).
//   * Later hosts the vehicle-load / trim / CMF swap logic (from UCXMRVehicleProfile).
//
// Usage: make a project BP subclass per program (/Game/Vehicles/[program]/), add the vehicle mesh
// under VehicleAnchor, and set Placement->MarkerProfile.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CXMRVehicleRoot.generated.h"

class UCXMRPlacementComponent;

UCLASS()
class CXMR_API ACXMRVehicleRoot : public AActor
{
	GENERATED_BODY()

public:
	ACXMRVehicleRoot();

	/** Calibrated origin. Attach the vehicle mesh under this (BP subclass, or spawned by the vehicle system). */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TObjectPtr<USceneComponent> VehicleAnchor;

	/** Places this actor from Calibration markers or the pawn. Targets this actor by default. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TObjectPtr<UCXMRPlacementComponent> Placement;
};
