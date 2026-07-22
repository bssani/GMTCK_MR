// Copyright GMTCK CX.
//
// UCXMRVehicleProfile — one vehicle, authored per program.
// UCXMRVehicleCatalog — the list of vehicles a project offers.
//
// TYPE lives in the plugin; INSTANCES live in the project (/Game/Vehicle/[program]/), same split as
// UCXMRMarkerProfile. The plugin never references a project asset, which is the whole of portability:
// the vehicle is something CXMR LOADS, not something CXMR knows.
//
// The profile links its own marker profile, so switching vehicle switches calibration config with it.

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

	/** Actor holding the vehicle geometry. Spawned under the calibrated anchor, never at world root. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TSoftClassPtr<AActor> VehicleActor;

	/** Calibration config for THIS vehicle (marker IDs / offsets differ per program). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TSoftObjectPtr<UCXMRMarkerProfile> MarkerProfile;

	/** Corrects the authored pivot so the calibrated anchor lands where the marker offsets assume. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") FTransform VehicleRootOffset = FTransform::Identity;

	/** Trim levels. Empty = single-configuration vehicle, which is a valid and common case. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TArray<FCXMRTrim> Trims;

	/** Percentile seating references (HF). Swaps with the vehicle so eye/hip points follow the car. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TSoftObjectPtr<UCXMRErgonomicsProfile> Ergonomics;

	/** Every part tag mentioned by any trim — these are the components trim switching manages. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") TSet<FName> GetManagedPartTags() const;

	UFUNCTION(BlueprintPure, Category = "CXMR|Vehicle") bool IsValidTrim(int32 TrimIndex) const;
};

UCLASS(BlueprintType, DisplayName = "CXMR Vehicle Catalog")
class CXMR_API UCXMRVehicleCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Vehicle") TArray<TSoftObjectPtr<UCXMRVehicleProfile>> Vehicles;
};
