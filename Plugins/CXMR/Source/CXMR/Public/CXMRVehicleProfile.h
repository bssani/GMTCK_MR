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

	/** 눈 기준점을 지정한 차량만 초기 정렬 허용함. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Alignment", meta=(ToolTip="Enable after authoring the driver eye reference for this vehicle.")) bool bHasDriverEyeReference = false;

	/** 원본 차량 로컬 좌표. +X는 운전자 정면 방향. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Alignment", meta=(EditCondition="bHasDriverEyeReference", ToolTip="Driver eye position and forward direction in the imported vehicle actor's local space. Do not apply VehicleRootOffset twice.")) FTransform DriverEyeReference = FTransform::Identity;

	/** 트림이 없으면 기본 구성 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle", meta=(ToolTip="Optional trim configurations.")) TArray<FCXMRTrim> Trims;

	/** HF 평가용 착좌 프로필. 초기 정렬과 별도로 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle", meta=(ToolTip="Optional HF seating references, separate from initial driver eye alignment.")) TSoftObjectPtr<UCXMRErgonomicsProfile> Ergonomics;

	/** 트림에서 관리하는 부품 태그 모음. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle", meta=(ToolTip="All component tags controlled by the trim configurations.")) TSet<FName> GetManagedPartTags() const;

	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") bool IsValidTrim(int32 TrimIndex) const;
};

UCLASS(BlueprintType, DisplayName = "CXMR Vehicle Catalog")
class CXMR_API UCXMRVehicleCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TArray<TSoftObjectPtr<UCXMRVehicleProfile>> Vehicles;
};
