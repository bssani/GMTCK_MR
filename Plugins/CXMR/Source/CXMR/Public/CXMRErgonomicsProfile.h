// Copyright GMTCK CX.
//
// UCXMRErgonomicsProfile — the percentile seating references for one vehicle (Evaluation / HF).
//
// Package data, authored per program alongside the vehicle. Linked from UCXMRVehicleProfile so it
// swaps with the car. The eye/hip points are vehicle-local; the ergonomics component turns them into
// a viewpoint move (VR) or a vehicle move (MR).

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CXMRTypes.h"
#include "CXMRErgonomicsProfile.generated.h"

UCLASS(BlueprintType, DisplayName = "CXMR Ergonomics Profile")
class CXMR_API UCXMRErgonomicsProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	/** Ordered so cycling reads naturally, e.g. 5th → 50th → 95th. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Ergonomics") TArray<FCXMRManikinPosition> Positions;

	UFUNCTION(BlueprintPure, Category = "CXMR|Ergonomics") bool IsValidPosition(int32 Index) const
	{
		return Positions.IsValidIndex(Index);
	}
};
