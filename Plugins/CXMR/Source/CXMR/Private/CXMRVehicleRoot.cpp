// Copyright GMTCK CX.

#include "CXMRVehicleRoot.h"
#include "CXMRPlacementComponent.h"

ACXMRVehicleRoot::ACXMRVehicleRoot()
{
	PrimaryActorTick.bCanEverTick = false;

	VehicleAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("VehicleAnchor"));
	SetRootComponent(VehicleAnchor);

	// Placement targets this actor by default (its VehicleRoot property falls back to GetOwner()).
	Placement = CreateDefaultSubobject<UCXMRPlacementComponent>(TEXT("Placement"));
}
