// Copyright GMTCK CX.
// 앵커 아래 차량을 교체함. 앵커 위치는 유지함.
// 트림 태그가 있는 부품만 변경하고 공용 형상은 유지함.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRTypes.h"
#include "CXMRVehicleLoaderComponent.generated.h"

class UCXMRVehicleProfile;
class UCXMRVehicleCatalog;
class UMaterialInterface;
class UCXMRSubsystem;
class UPrimitiveComponent;

/** CMF 변경 전 재질을 보관함. */
USTRUCT(meta=(ToolTip="Original materials retained while a CMF is applied."))
struct FCXMRCMFOriginalMaterial
{
	GENERATED_BODY()

	UPROPERTY() TWeakObjectPtr<UPrimitiveComponent> Component;
	UPROPERTY() int32 Slot = 0;
	UPROPERTY() TObjectPtr<UMaterialInterface> Material;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCXMROnVehicleLoaded, UCXMRVehicleProfile*, Profile);

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Vehicle Loader")
class CXMR_API UCXMRVehicleLoaderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRVehicleLoaderComponent();

	/** 로드할 차량 프로필. 프로젝트 애셋 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Vehicle", meta=(ToolTip="Use the vehicle's horizontal forward and right axes.")) TObjectPtr<UCXMRVehicleProfile> Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Vehicle") bool bLoadOnBeginPlay = true;

	/** 차량 순환에 사용할 목록. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Vehicle", meta=(ToolTip="Use the vehicle's horizontal forward and right axes.")) TObjectPtr<UCXMRVehicleCatalog> Catalog;

	/** 차량 부착 대상. 비우면 소유자의 루트 사용함. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "CXMR|Vehicle", meta=(ToolTip="Use the vehicle's horizontal forward and right axes.")) TObjectPtr<USceneComponent> AttachTarget;

	/** 새 차량 준비 후 교체함. 앵커는 유지함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle", meta=(ToolTip="Use the vehicle's horizontal forward and right axes.")) void LoadVehicle(UCXMRVehicleProfile* NewProfile);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void UnloadVehicle();

	/** 트림과 기본 CMF 적용함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle", meta=(ToolTip="Use the vehicle's horizontal forward and right axes.")) void SetTrim(int32 TrimIndex);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void SetCMF(int32 CMFIndex);

	// 목록 끝에서는 처음 항목으로 돌아감.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void NextVehicle();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void PreviousVehicle();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void NextTrim();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void PreviousTrim();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void NextCMF();

	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") AActor* GetSpawnedVehicle() const { return SpawnedVehicle; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") int32 GetTrimIndex() const { return TrimIndex; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") int32 GetCMFIndex() const { return CMFIndex; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") int32 GetVehicleIndex() const { return VehicleIndex; }

	UPROPERTY(BlueprintAssignable, Category = "CXMR|Vehicle") FCXMROnVehicleLoaded OnVehicleLoaded;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	USceneComponent* ResolveAttachTarget();

	/** 새 마커 프로필 적용함. 없으면 이전 값도 비움. */
	void SyncMarkerProfile();

	/** 로드한 프로필에 맞춰 목록 순번 갱신함. */
	void SyncVehicleIndex();

	/** 유효한 기본 CMF 반환함. 범위를 벗어나면 0 사용함. */
	int32 ResolveDefaultCMF(int32 InTrimIndex) const;

	void ApplyTrim();
	void ApplyCMF();

	/** 현재 선택을 Subsystem에 게시함. */
	void ReportStatus();

	UPROPERTY(Transient) TObjectPtr<AActor> SpawnedVehicle;
	UPROPERTY(Transient) TArray<FCXMRCMFOriginalMaterial> OriginalMaterials;
	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;

	UFUNCTION() void HandleViewerAction(ECXMRViewerAction Action);

	void CycleVehicle(int32 Step);
	void CycleTrim(int32 Step);

	UPROPERTY(Transient) int32 VehicleIndex = 0;
	UPROPERTY(Transient) int32 TrimIndex = 0;
	UPROPERTY(Transient) int32 CMFIndex  = 0;
};
