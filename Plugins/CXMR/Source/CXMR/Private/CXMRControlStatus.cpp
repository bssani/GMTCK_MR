// Copyright GMTCK CX.
#include "CXMRControlStatus.h"
FString CXMRMissingMarkerNames(const FCXMRControlStatus& State)
{
	TArray<FString> Names;
	for (int32 Id : State.MissingMarkers) { Names.Add(FString::FromInt(Id)); }
	return FString::Join(Names, TEXT(" and "));
}
FCXMRControlStatusText CXMRDescribeControlStatus(const FCXMRControlStatus& S)
{
	switch (S.Stage)
	{
	case ECXMRControlStage::Starting: return {TEXT("Starting the headset session"), TEXT("MR, the depth test and real-world depth switch on as soon as the headset is ready.")};
	case ECXMRControlStage::NoHeadset: return {TEXT("Headset not running"), TEXT("MR and markers need a running headset session. Showing VR until it starts.")};
	case ECXMRControlStage::Waiting: return {FString::Printf(TEXT("Waiting for markers · %d of %d in view"), S.RequiredMarkers.Num()-S.MissingMarkers.Num(), S.RequiredMarkers.Num()), FString::Printf(TEXT("Show marker %s to the headset. The car stays where it is until every marker is in view."), *CXMRMissingMarkerNames(S))};
	case ECXMRControlStage::Settling: return {TEXT("Reading markers"), FString::Printf(TEXT("Hold the view steady. Averaging every marker for %.1f s."), S.SettleSeconds)};
	case ECXMRControlStage::Saving: return {TEXT("Saving alignment"), TEXT("Averaging every marker. Hold the view steady.")};
	case ECXMRControlStage::Adjusted: return {TEXT("Adjusted · not saved"), S.MissingMarkers.IsEmpty() ? TEXT("Save alignment to reuse this position next session.") : FString::Printf(TEXT("Show marker %s to save."), *CXMRMissingMarkerNames(S))};
	case ECXMRControlStage::Aligned:
		switch (S.Restore)
		{
		case ECXMRControlRestore::World: return {TEXT("Aligned · saved position reused"), FString::Printf(TEXT("Tracking origin unchanged. Largest marker shift %.2f cm."), S.OriginMovement)};
		case ECXMRControlRestore::Markers: return {TEXT("Aligned · recalculated from markers"), FString::Printf(TEXT("Tracking origin moved %.2f cm since the save, so the position came from %d markers."), S.OriginMovement, S.RequiredMarkers.Num())};
		case ECXMRControlRestore::Saved: return {TEXT("Aligned · saved"), TEXT("Saved just now. The next session reuses this position.")};
		default: return {TEXT("Aligned from markers"), TEXT("Nothing saved for this vehicle yet. Fine-adjust if needed, then save.")};
		}
	default: return {TEXT("Not calibrated"), TEXT("")};
	}
}
