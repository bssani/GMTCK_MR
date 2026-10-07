// Copyright GMTCK CX.
//
// 차량별 모델과 보정 기준 보관함.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CXMRTypes.h"
#include "CXMRVehicleProfile.generated.h"

class AActor;
class UCXMRMarkerProfile;
class UCXMRErgonomicsProfile;
class UCXMRDesignOption;

UCLASS(BlueprintType, DisplayName = "CXMR Vehicle Profile")
class CXMR_API UCXMRVehicleProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") FText DisplayName;

	/** 앵커 아래에 생성할 차량 클래스. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle", meta=(ToolTip="Vehicle geometry class spawned under the calibrated anchor.")) TSoftClassPtr<AActor> VehicleActor;

	/** 차량별 마커 설정. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle", meta=(ToolTip="Calibration marker IDs and offsets for this vehicle.")) TSoftObjectPtr<UCXMRMarkerProfile> MarkerProfile;

	/** 원본 차량 전체에 적용할 로컬 오프셋. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle", meta=(ToolTip="Local transform applied once to the entire imported vehicle.")) FTransform VehicleRootOffset = FTransform::Identity;

	/** 그룹이 같으면 저장된 앵커 정렬 공유함. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Alignment", meta=(ToolTip="Vehicles with the same group share one saved anchor alignment and calibration marker IDs. None keeps vehicle-specific saves. Model offsets and driver eye references remain per vehicle.")) FName AlignmentGroup = NAME_None;

	/** 눈 기준점을 지정한 차량만 초기 정렬 허용함. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Alignment", meta=(ToolTip="Enable after authoring the driver eye reference for this vehicle.")) bool bHasDriverEyeReference = false;

	/** 원본 차량 로컬 좌표. +X는 운전자 정면 방향. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Alignment", meta=(EditCondition="bHasDriverEyeReference", ToolTip="Driver eye position and forward direction in the imported vehicle actor's local space. Do not apply VehicleRootOffset twice.")) FTransform DriverEyeReference = FTransform::Identity;

	/** 고정 부품 장착점에서 비교할 설계 옵션. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TArray<TSoftObjectPtr<UCXMRDesignOption>> DesignOptions;

	/** HF 평가용 착좌 프로필. 초기 정렬과 별도로 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle", meta=(ToolTip="Optional HF seating references, separate from initial driver eye alignment.")) TSoftObjectPtr<UCXMRErgonomicsProfile> Ergonomics;

};

UCLASS(BlueprintType, DisplayName = "CXMR Vehicle Catalog")
class CXMR_API UCXMRVehicleCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TArray<TSoftObjectPtr<UCXMRVehicleProfile>> Vehicles;
};
