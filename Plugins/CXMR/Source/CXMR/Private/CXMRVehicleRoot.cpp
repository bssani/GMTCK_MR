// Copyright GMTCK CX.
#include "CXMRVehicleRoot.h"
#include "CXMRPlacementComponent.h"
#include "CXMRVehicleLoaderComponent.h"
ACXMRVehicleRoot::ACXMRVehicleRoot()
{
	PrimaryActorTick.bCanEverTick = false;
	VehicleAnchor = CreateDefaultSubobject<USceneComponent>(TEXT("VehicleAnchor"));
	SetRootComponent(VehicleAnchor);
	Placement = CreateDefaultSubobject<UCXMRPlacementComponent>(TEXT("Placement"));
	Loader = CreateDefaultSubobject<UCXMRVehicleLoaderComponent>(TEXT("Loader"));
	Loader->AttachTarget = VehicleAnchor;
}

void ACXMRVehicleRoot::PostLoad()
{
	Super::PostLoad();
	// 이전 턴테이블 참조를 고정 앵커로 옮김.
	if (Loader && (!Loader->AttachTarget || !GetComponents().Contains(Loader->AttachTarget.Get())))
	{
		Loader->AttachTarget = VehicleAnchor;
	}
}
