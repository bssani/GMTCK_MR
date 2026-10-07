// Copyright GMTCK CX.
// 앵커 아래 차량을 교체함. 앵커 위치는 유지함.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRTypes.h"
#include "CXMRVehicleLoaderComponent.generated.h"

class UCXMRVehicleProfile;
class UCXMRVehicleCatalog;
class UCXMRSubsystem;
class UCXMRDesignOption;
class UCXMRPartVariantComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCXMROnVehicleLoaded, UCXMRVehicleProfile*, Profile);

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Vehicle Loader")
class CXMR_API UCXMRVehicleLoaderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRVehicleLoaderComponent();

	/** 로드할 차량 프로필. 프로젝트 애셋 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Vehicle", meta=(ToolTip="Vehicle profile to load beneath the calibrated anchor.")) TObjectPtr<UCXMRVehicleProfile> Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Vehicle") bool bLoadOnBeginPlay = true;

	/** 차량 순환에 사용할 목록. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Vehicle", meta=(ToolTip="Available vehicles for direct selection or cycling.")) TObjectPtr<UCXMRVehicleCatalog> Catalog;

	/** 차량 부착 대상. 비우면 소유자의 루트 사용함. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "CXMR|Vehicle", meta=(ToolTip="Attachment anchor. Empty uses the owning actor root.")) TObjectPtr<USceneComponent> AttachTarget;

	/** 새 차량 준비 후 교체함. 앵커는 유지함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle", meta=(ToolTip="Prepare a replacement before changing the current vehicle. Failure preserves the accepted vehicle.")) void LoadVehicle(UCXMRVehicleProfile* NewProfile);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void UnloadVehicle();

	// 목록 끝에서는 처음 항목으로 돌아감.
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void NextVehicle();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void PreviousVehicle();

	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") AActor* GetSpawnedVehicle() const { return SpawnedVehicle; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") int32 GetVehicleIndex() const { return VehicleIndex; }

	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") bool SelectVehicle(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") bool SelectDesignOption(UCXMRDesignOption* Option);
	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") UCXMRPartVariantComponent* GetPartVariants() const { return PartVariants; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") FString GetLastFailureReason() const { return LastFailureReason; }
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Vehicle") FCXMROnVehicleLoaded OnVehicleLoaded;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool TryLoadVehicle(UCXMRVehicleProfile* NewProfile);
	bool Fail(const FString& Reason);
	USceneComponent* ResolveAttachTarget();

	/** 새 마커 프로필 적용함. 없으면 이전 값도 비움. */
	void SyncMarkerProfile();

	/** 로드한 프로필에 맞춰 목록 순번 갱신함. */
	void SyncVehicleIndex();

	/** 현재 선택을 Subsystem에 게시함. */
	void ReportStatus();

	UPROPERTY(Transient) TObjectPtr<AActor> SpawnedVehicle;
	UPROPERTY(Transient) TObjectPtr<UCXMRPartVariantComponent> PartVariants;
	UPROPERTY(Transient) FString LastFailureReason;
	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;

	UFUNCTION() void HandleViewerAction(ECXMRViewerAction Action);

	void CycleVehicle(int32 Step);

	UPROPERTY(Transient) int32 VehicleIndex = 0;
};
