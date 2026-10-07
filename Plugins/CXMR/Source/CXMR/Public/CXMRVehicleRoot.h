// Copyright GMTCK CX.
// 차량 공용 정렬의 기준 액터.
// VehicleAnchor는 캘리브레이션만 움직임.
// Placement가 정렬을 관리하고 Loader가 모델 로컬 오프셋을 적용함.
// 차량·부품 선택은 앵커와 운전자 눈 기준을 바꾸지 않음.
#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CXMRVehicleRoot.generated.h"
class UCXMRPlacementComponent;
class UCXMRVehicleLoaderComponent;
UCLASS()
class CXMR_API ACXMRVehicleRoot : public AActor
{
	GENERATED_BODY()
public:
	ACXMRVehicleRoot();
	virtual void PostLoad() override;
	/** 캘리브레이션 전용 앵커. 선택은 모델 로컬 오프셋만 변경함. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CXMR|Vehicle", meta=(ToolTip="Fixed calibrated origin shared by vehicle and part selections.")) TObjectPtr<USceneComponent> VehicleAnchor;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CXMR|Vehicle", meta=(ToolTip="Calibrates the fixed vehicle anchor.")) TObjectPtr<UCXMRPlacementComponent> Placement;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="CXMR|Vehicle", meta=(ToolTip="Loads vehicles directly beneath the calibrated anchor.")) TObjectPtr<UCXMRVehicleLoaderComponent> Loader;
};
