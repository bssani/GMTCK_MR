// Copyright GMTCK CX.

#include "CXMRPlacementComponent.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRMarkerProfile.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRAlignment, Log, All);

#define LOCTEXT_NAMESPACE "CXMRAlignment"

bool UCXMRPlacementComponent::GetDriverEyeWorld(FTransform& EyeWorld) const
{
	const UCXMRVehicleLoaderComponent* Loader = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	const UCXMRVehicleProfile* Profile = Loader ? Loader->Profile.Get() : nullptr;
	const AActor* Vehicle = Loader ? Loader->GetSpawnedVehicle() : nullptr;
	if (!IsValid(Vehicle) || !Profile || !Profile->bHasDriverEyeReference || Profile->DriverEyeReference.ContainsNaN()
		|| !Profile->DriverEyeReference.GetRotation().IsNormalized()) { return false; }
	EyeWorld = Profile->DriverEyeReference * Vehicle->GetActorTransform();
	return !EyeWorld.ContainsNaN();
}

bool UCXMRPlacementComponent::AlignToDriverEyePose(FTransform HeadWorld)
{
	AActor* Root = ResolveVehicleRoot();
	FTransform EyeWorld;
	if (!IsValid(Root) || Mode != ECXMRPlacementMode::MarkerAnchor || !GetDriverEyeWorld(EyeWorld)
		|| HeadWorld.ContainsNaN() || !HeadWorld.GetRotation().IsNormalized()) { return false; }
	const FTransform EyeRelative = EyeWorld.GetRelativeTransform(Root->GetActorTransform());
	const float HeadYaw = HeadWorld.Rotator().Yaw;
	const FQuat Rotation = FRotator(0, HeadYaw - EyeRelative.Rotator().Yaw, 0).Quaternion();
	// 메시 오프셋은 유지하고 앵커만 눈 위치에 맞춤.
	const FVector Location = HeadWorld.GetLocation() - Rotation.RotateVector(EyeRelative.GetLocation());
	BaseVehicleTransform = FTransform(Rotation, Location, FVector::OneVector);
	bHaveBasePose = true;
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	bManualAlignment = true;
	bAlignmentPoseLocked = true;
	bRestoreNeedsConfirmation = false;
	bCalibrated = false;
	CancelAlignmentCapture();
	DetectedCalib.Reset();
	SeenMarkers.Reset();
	MarkerLastObserved.Reset();
	MarkerSamples.Reset();
	LayoutFitError = -1.f;
	NudgePivot = ECXMRNudgePivot::DriverEyeReference;
	LatchNudgeHeading(HeadYaw);
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(SettleTimer); }
	PublishOffset();
	ApplyPlacement();
	return true;
}

void UCXMRPlacementComponent::CancelAlignmentCapture()
{
	bCapturingAlignment = false;
	AlignmentSaveMessage = FText::GetEmpty();
	CapturedAlignmentMarkers.Reset();
	CaptureMarkerIds.Reset();
}

int32 UCXMRPlacementComponent::GetRequiredAlignmentMarkerCount() const
{
	TSet<int32> Ids;
	if (MarkerProfile)
	{
		for (const FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
		{
			if (Entry.Role == ECXMRMarkerRole::Calibration && Entry.MarkerId > 0) { Ids.Add(Entry.MarkerId); }
		}
	}
	return Ids.Num();
}

bool UCXMRPlacementComponent::BeginAlignmentCapture()
{
	CancelAlignmentCapture();
	AActor* Root = ResolveVehicleRoot();
	const UCXMRVehicleLoaderComponent* Loader = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	if (!IsValid(Root) || !Loader || !IsValid(Loader->GetSpawnedVehicle()) || !Loader->Profile)
	{
		AlignmentSaveMessage = LOCTEXT("VehicleRequired", "Not saved. Load a vehicle before confirming alignment.");
		return false;
	}
	if (!MarkerProfile || GetRequiredAlignmentMarkerCount() == 0)
	{
		AlignmentSaveMessage = LOCTEXT("MarkersRequired", "Not saved. Configure calibration marker IDs in the vehicle marker profile.");
		return false;
	}
	if (!bManualAlignment && !bCalibrated)
	{
		AlignmentSaveMessage = LOCTEXT("AlignmentRequired", "Not saved. Align the vehicle before confirming its position.");
		return false;
	}
	const AActor* Vehicle = Loader->GetSpawnedVehicle();
	if (Vehicle->GetClass() != Loader->Profile->VehicleActor.Get() || !Vehicle->GetRootComponent()
		|| !Vehicle->GetRootComponent()->GetRelativeTransform().Equals(Loader->Profile->VehicleRootOffset, 0.001f))
	{
		AlignmentSaveMessage = LOCTEXT("ModelChanged", "Not saved. Model settings changed; reload the vehicle and confirm alignment again.");
		return false;
	}
	CaptureModelOffset = Loader->Profile->VehicleRootOffset;
	CaptureModelClass = Loader->Profile->VehicleActor.ToSoftObjectPath();
	CapturePose = Root->GetActorTransform();
	CaptureVehicle = Loader->GetSpawnedVehicle();
	CaptureVehicleProfile = Loader->Profile;
	CaptureMarkerProfile = MarkerProfile;
	CaptureVehicleRelativePose = CaptureVehicle->GetActorTransform().GetRelativeTransform(CapturePose);
	CaptureEyeReference = Loader->Profile->DriverEyeReference;
	bCaptureHasEyeReference = Loader->Profile->bHasDriverEyeReference;
	for (const FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
	{
		if (Entry.Role == ECXMRMarkerRole::Calibration && Entry.MarkerId > 0) { CaptureMarkerIds.Add(Entry.MarkerId); }
	}
	bCapturingAlignment = true;
	bAlignmentPoseLocked = true;
	return true;
}

bool UCXMRPlacementComponent::IsCapturePoseCurrent() const
{
	const AActor* Root = VehicleRoot ? VehicleRoot.Get() : GetOwner();
	const UCXMRVehicleLoaderComponent* Loader = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	if (!bCapturingAlignment || !IsValid(Root) || !Loader || !Loader->Profile || !CaptureVehicle.IsValid()
		|| !TWeakObjectPtr<AActor>(Loader->GetSpawnedVehicle()).HasSameIndexAndSerialNumber(CaptureVehicle)
		|| !TWeakObjectPtr<UCXMRVehicleProfile>(Loader->Profile.Get()).HasSameIndexAndSerialNumber(CaptureVehicleProfile)
		|| MarkerProfile.Get() != CaptureMarkerProfile.Get() || !Root->GetActorTransform().Equals(CapturePose, 0.001f)
		|| !CaptureVehicle->GetActorTransform().GetRelativeTransform(CapturePose).Equals(CaptureVehicleRelativePose, 0.001f)
		|| !Loader->Profile->VehicleRootOffset.Equals(CaptureModelOffset, 0.001f)
		|| Loader->Profile->VehicleActor.ToSoftObjectPath() != CaptureModelClass
		|| !Loader->Profile->DriverEyeReference.Equals(CaptureEyeReference, 0.001f)
		|| Loader->Profile->bHasDriverEyeReference != bCaptureHasEyeReference
		|| GetRequiredAlignmentMarkerCount() != CaptureMarkerIds.Num()) { return false; }
	for (int32 Id : CaptureMarkerIds)
	{
		FCXMRMarkerEntry Entry;
		if (!MarkerProfile->FindEntry(Id, Entry) || Entry.Role != ECXMRMarkerRole::Calibration) { return false; }
	}
	return true;
}

int32 UCXMRPlacementComponent::GetCapturedAlignmentMarkerCount() const
{
	return IsCapturePoseCurrent() ? CapturedAlignmentMarkers.Num() : 0;
}

bool UCXMRPlacementComponent::CanSaveAlignment() const
{
	return IsCapturePoseCurrent() && CaptureMarkerIds.Num() > 0 && CapturedAlignmentMarkers.Num() == CaptureMarkerIds.Num();
}

bool UCXMRPlacementComponent::SaveAlignment()
{
	bLastCalibrationSaveSucceeded = false;
	if (!CanSaveAlignment())
	{
		AlignmentSaveMessage = IsCapturePoseCurrent()
			? FText::FromString(FString::Printf(TEXT("Not saved. %d / %d markers captured. Observe the remaining markers, then press Enter or select Save Alignment."), GetCapturedAlignmentMarkerCount(), GetRequiredAlignmentMarkerCount()))
			: LOCTEXT("ConfirmAgain", "Not saved. Confirm alignment again to capture fresh marker observations.");
		return false;
	}
	TArray<FCXMRMarkerEntry> Entries = MarkerProfile->Markers;
	for (FCXMRMarkerEntry& Entry : Entries)
	{
		if (const FTransform* Relative = CapturedAlignmentMarkers.Find(Entry.MarkerId)) { Entry.LocalOffset = *Relative; }
	}
	// 파일 저장에 성공한 뒤 프로필과 기준 위치를 확정함.
	if (!WriteCalibrationData(Entries, CaptureMarkerIds))
	{
		AlignmentSaveMessage = LOCTEXT("SaveFailed", "Save failed. Previous alignment kept. Check the save location, then retry.");
		return false;
	}
	MarkerProfile->Markers = MoveTemp(Entries);
	SavedAlignmentMarkers = CaptureMarkerIds;
	TArray<int32> Ids = SavedAlignmentMarkers.Array();
	Ids.Sort();
	SavedPrimaryMarker = Ids[0];
	CancelAlignmentCapture();
	RebaseToCurrentTransform();
	bManualAlignment = false;
	bAlignmentPoseLocked = true;
	bRestoreNeedsConfirmation = false;
	bCalibrated = true;
	AlignmentSaveMessage = LOCTEXT("Saved", "Alignment saved.");
	return true;
}

void UCXMRPlacementComponent::ReportAlignmentSaveMessage() const
{
	UE_LOG(LogCXMRAlignment, Log, TEXT("%s"), *AlignmentSaveMessage.ToString());
	if (GEngine && GetWorld() && GetWorld()->IsGameWorld())
	{
		GEngine->AddOnScreenDebugMessage(static_cast<uint64>(GetUniqueID()), 8.f, bLastCalibrationSaveSucceeded ? FColor::Green : FColor::Yellow, AlignmentSaveMessage.ToString());
	}
}

bool UCXMRPlacementComponent::RequestAlignmentSave()
{
	bLastCalibrationSaveSucceeded = false;
	if (!HasAlignmentCapture())
	{
		if (BeginAlignmentCapture())
		{
			AlignmentSaveMessage = LOCTEXT("CaptureStarted", "Alignment confirmed. Observe all configured markers, then press Enter again to save.");
		}
		ReportAlignmentSaveMessage();
		return false;
	}
	const bool bSaved = SaveAlignment();
	ReportAlignmentSaveMessage();
	return bSaved;
}

bool UCXMRPlacementComponent::ComputeSavedAlignmentPose(FTransform& Out, bool& bReady)
{
	bReady = false;
	TArray<int32> Ids;
	for (int32 Id : SavedAlignmentMarkers)
	{
		if (DetectedCalib.Contains(Id) && IsMarkerObservationCurrent(Id)) { Ids.Add(Id); }
	}
	if (Ids.IsEmpty()) { return false; }
	Ids.Sort();
	const int32 Chosen = Ids.Contains(SavedPrimaryMarker) ? SavedPrimaryMarker : Ids[0];
	FCXMRMarkerEntry Reference;
	if (!MarkerProfile->FindEntry(Chosen, Reference)) { return false; }
	Out = Reference.LocalOffset.Inverse() * DetectedCalib.FindChecked(Chosen);
	if (bKeepLevel) { Out.SetRotation(FRotator(0, Out.Rotator().Yaw, 0).Quaternion()); }
	LayoutFitError = -1.f;
	if (Ids.Num() > 1)
	{
		LayoutFitError = 0.f;
		for (int32 Id : Ids)
		{
			FCXMRMarkerEntry Entry;
			if (!MarkerProfile->FindEntry(Id, Entry)) { return false; }
			const FTransform Candidate = Entry.LocalOffset.Inverse() * DetectedCalib.FindChecked(Id);
			LayoutFitError = FMath::Max(LayoutFitError, static_cast<float>(FVector::Distance(Out.GetLocation(), Candidate.GetLocation())));
			const float Angle = FMath::Abs(FMath::FindDeltaAngleDegrees(Out.Rotator().Yaw, Candidate.Rotator().Yaw));
			if (Angle > 5.f || LayoutFitError > FMath::Max(0.f, MaxCalibrationFitError)) { return false; }
		}
		FTransform Fit;
		float Residual = 0.f;
		if (!ComputeMultiMarkerTransform(Fit, Residual) || Residual > FMath::Max(0.f, MaxCalibrationFitError)) { return false; }
		// 위치로 계산한 최종 방향도 각 마커 방향과 맞아야 함.
		for (int32 Id : Ids)
		{
			FCXMRMarkerEntry Entry;
			if (!MarkerProfile->FindEntry(Id, Entry)) { return false; }
			const FTransform Candidate = Entry.LocalOffset.Inverse() * DetectedCalib.FindChecked(Id);
			if (FMath::Abs(FMath::FindDeltaAngleDegrees(Fit.Rotator().Yaw, Candidate.Rotator().Yaw)) > 5.f) { return false; }
		}
		Out = Fit;
		LayoutFitError = Residual;
	}
	bReady = !Out.ContainsNaN();
	return bReady;
}

bool UCXMRPlacementComponent::ConfirmRestoredAlignment()
{
	if (!bRestoreNeedsConfirmation || !bAlignmentPoseLocked) { return false; }
	bRestoreNeedsConfirmation = false;
	bCalibrated = true;
	return true;
}

#undef LOCTEXT_NAMESPACE
