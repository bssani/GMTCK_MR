// Copyright GMTCK CX.
//
// UCXMRMarkerProfile — per-program marker configuration (which IDs, roles, offsets).
// The TYPE lives in the plugin (C++); INSTANCES are authored per project in /Game/Vehicles/[program]/.
// The placement/calibration component reads this to know what each detected marker means.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CXMRTypes.h"
#include "CXMRMarkerProfile.generated.h"

UCLASS(BlueprintType)
class CXMR_API UCXMRMarkerProfile : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Marker Profile", meta = (TitleProperty = "Label"))
	TArray<FCXMRMarkerEntry> Markers;

	/** Find the entry for a marker id. Returns false if not in the profile. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Marker Profile")
	bool FindEntry(int32 MarkerId, FCXMRMarkerEntry& OutEntry) const;

	/** Find and modify the entry for a marker id (mutable). Returns nullptr if not found. */
	FCXMRMarkerEntry* GetEntryMutable(int32 MarkerId);

	/** Role of a marker id (Reserved if not in the profile). */
	UFUNCTION(BlueprintPure, Category = "CXMR|Marker Profile")
	ECXMRMarkerRole GetRole(int32 MarkerId) const;
};
