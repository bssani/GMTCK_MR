#pragma once
#include "GameFramework/Actor.h"
#include "CXMRVehicleTestTypes.generated.h"
// 제거된 옵션 fixture에서 일반 차량·수명 검사만 남김.
UCLASS(Transient, NotBlueprintable)
class ACXMRVehicleTestActor : public AActor
{
	GENERATED_BODY()
public:
	ACXMRVehicleTestActor();
};
UCLASS(Transient, NotBlueprintable)
class ACXMRVehicleActivityTestActor : public ACXMRVehicleTestActor
{
	GENERATED_BODY()
public:
	ACXMRVehicleActivityTestActor();
	virtual void OnConstruction(const FTransform& Transform) override;
};
