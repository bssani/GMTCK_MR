// Copyright GMTCK CX.

#include "CXMRVehicleRoot.h"
#include "CXMRPlacementComponent.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRTurntableComponent.h"
#include "CXMRErgonomicsComponent.h"

ACXMRVehicleRoot::ACXMRVehicleRoot()
{
	PrimaryActorTick.bCanEverTick = false;

	VehicleAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("VehicleAnchor"));
	SetRootComponent(VehicleAnchor);

	// Placement targets this actor by default (its VehicleRoot property falls back to GetOwner()).
	Placement = CreateDefaultSubobject<UCXMRPlacementComponent>(TEXT("Placement"));

	// Turntable sits BETWEEN the calibrated anchor and the vehicle: one writer per transform.
	Turntable = CreateDefaultSubobject<USceneComponent>(TEXT("Turntable"));
	Turntable->SetupAttachment(VehicleAnchor);

	Loader = CreateDefaultSubobject<UCXMRVehicleLoaderComponent>(TEXT("Loader"));
	Loader->AttachTarget = Turntable;

	TurntableControl = CreateDefaultSubobject<UCXMRTurntableComponent>(TEXT("TurntableControl"));
	TurntableControl->TurntableTarget = Turntable;

	// HF eye/hip snapping. Reads the loaded vehicle's ergonomics profile via the sibling loader.
	Ergonomics = CreateDefaultSubobject<UCXMRErgonomicsComponent>(TEXT("Ergonomics"));
}
