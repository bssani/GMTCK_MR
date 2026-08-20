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

	/** File-name stem for the field calibration saved to Saved/CXMR. Empty = use the asset name.
	 *  Keep it short — this is the name someone reads out over the phone from a remote site.
	 *  ⚠ Two profiles sharing an id overwrite each other's calibration. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Marker Profile")
	FString CalibrationId;

	/** CalibrationId if set, otherwise the asset name. Sanitised for use in a file name. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Marker Profile")
	FString GetCalibrationId() const;

	/** Find the entry for a marker id. Returns false if not in the profile. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Marker Profile")
	bool FindEntry(int32 MarkerId, FCXMRMarkerEntry& OutEntry) const;

	/** Find and modify the entry for a marker id (mutable). Returns nullptr if not found. */
	FCXMRMarkerEntry* GetEntryMutable(int32 MarkerId);

	/** Role of a marker id (Reserved if not in the profile). */
	UFUNCTION(BlueprintPure, Category = "CXMR|Marker Profile")
	ECXMRMarkerRole GetRole(int32 MarkerId) const;
};
