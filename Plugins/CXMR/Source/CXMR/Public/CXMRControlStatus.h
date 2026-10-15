// Copyright GMTCK CX.
#pragma once
#include "CoreMinimal.h"
enum class ECXMRControlStage : uint8 { Starting, NoHeadset, Waiting, Settling, Aligned, Adjusted, Saving, Idle };
enum class ECXMRControlRestore : uint8 { World, Markers, New, Saved };
struct FCXMRControlStatus
{
	ECXMRControlStage Stage = ECXMRControlStage::Idle;
	ECXMRControlRestore Restore = ECXMRControlRestore::New;
	TArray<int32> RequiredMarkers;
	TArray<int32> MissingMarkers;
	float OriginMovement = 0.f;
	float Progress = 0.f;
	float SettleSeconds = 1.5f;
};
struct FCXMRControlStatusText
{
	FString Title;
	FString Detail;
};
CXMR_API FCXMRControlStatusText CXMRDescribeControlStatus(const FCXMRControlStatus& State);
CXMR_API FString CXMRMissingMarkerNames(const FCXMRControlStatus& State);
