// Copyright GMTCK CX.
//
// UCXMRVehicleLoaderComponent (Viewer) — loads / swaps the vehicle under the calibrated anchor.
//
// The alignment guarantee lives here: the vehicle is spawned as a CHILD of the anchor, and swapping
// only ever destroys and respawns that child. The anchor's transform — the thing calibration wrote —
// is never touched, so changing vehicle or trim never costs a recalibration.
//
// Trim and CMF address parts by component TAG (see FCXMRTrim). Untagged components are always
// visible, so shared body geometry needs no marking.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRTypes.h"
#include "CXMRVehicleLoaderComponent.generated.h"

class UCXMRVehicleProfile;
class UCXMRVehicleCatalog;
class UMaterialInterface;
class UCXMRSubsystem;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCXMROnVehicleLoaded, UCXMRVehicleProfile*, Profile);

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Vehicle Loader")
class CXMR_API UCXMRVehicleLoaderComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRVehicleLoaderComponent();

	/** Vehicle to load. Project asset (/Game/Vehicle/[program]/). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Vehicle") TObjectPtr<UCXMRVehicleProfile> Profile;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Vehicle") bool bLoadOnBeginPlay = true;

	/** Vehicles this program offers. Drives NextVehicle / PreviousVehicle. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Vehicle") TObjectPtr<UCXMRVehicleCatalog> Catalog;

	/** Spawn parent. Defaults to the owner's root — on ACXMRVehicleRoot that is the calibrated anchor. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "CXMR|Vehicle") TObjectPtr<USceneComponent> AttachTarget;

	/** Swaps the vehicle in place. The anchor transform is untouched, so calibration survives. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void LoadVehicle(UCXMRVehicleProfile* NewProfile);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void UnloadVehicle();

	/** Applies a trim (part visibility) and its default CMF. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void SetTrim(int32 TrimIndex);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Vehicle") void SetCMF(int32 CMFIndex);

	// Cycling wraps in both directions (3 -> 1 forward, 1 -> 3 back) so a stick flick never dead-ends.
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

	/** Pushes the marker profile from the vehicle profile onto the placement component, if present. */
	void SyncMarkerProfile();

	void ApplyTrim();
	void ApplyCMF();

	/** Pushes the current selection to the subsystem so the control panel can display it. */
	void ReportStatus();

	UPROPERTY(Transient) TObjectPtr<AActor> SpawnedVehicle;
	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;

	UFUNCTION() void HandleViewerAction(ECXMRViewerAction Action);

	void CycleVehicle(int32 Step);
	void CycleTrim(int32 Step);

	UPROPERTY(Transient) int32 VehicleIndex = 0;
	UPROPERTY(Transient) int32 TrimIndex = 0;
	UPROPERTY(Transient) int32 CMFIndex  = 0;
};
