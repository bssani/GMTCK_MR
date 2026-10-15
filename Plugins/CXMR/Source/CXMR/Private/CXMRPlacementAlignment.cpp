// Copyright GMTCK CX.

#include "CXMRPlacementComponent.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRMarkerProfile.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Components/SceneComponent.h"
#include "TimerManager.h"
#include "EngineUtils.h"
#include "CXMRMarkerFollower.h"

DEFINE_LOG_CATEGORY(LogCXMRAlignment);

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
	CancelAlignmentCapture();
	bManualAlignment = true;
	bAlignmentPoseLocked = true;
	bCalibrated = false;
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
	if (bCapturingAlignment) { bAlignmentPoseLocked = bCapturePreviousLock; }
	bCapturingAlignment = false;
	bAutoSaveCapture = false;
	AlignmentSaveMessage = FText::GetEmpty();
	CapturedAlignmentMarkers.Reset();
	CaptureSamples.Reset();
	CaptureMarkerIds.Reset();
	if (GetWorld())
	{
		GetWorld()->GetTimerManager().ClearTimer(CaptureCheckTimer);
		GetWorld()->GetTimerManager().ClearTimer(CaptureTimeoutTimer);
	}
}

bool UCXMRPlacementComponent::IsFollowerMarker(int32 Id) const
{
	if (!GetWorld()) { return false; }
	for (TActorIterator<ACXMRMarkerFollower> It(GetWorld()); It; ++It)
	{
		if (!It->IsActorBeingDestroyed() && It->MarkerId == Id) { return true; }
	}
	return false;
}

TSet<int32> UCXMRPlacementComponent::RequiredAlignmentMarkerIds() const
{
	TSet<int32> Ids;
	if (MarkerProfile)
	{
		for (const FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
		{
			if (Entry.Role == ECXMRMarkerRole::Calibration && Entry.MarkerId > 0 && !IsFollowerMarker(Entry.MarkerId)) { Ids.Add(Entry.MarkerId); }
		}
	}
	return Ids;
}

int32 UCXMRPlacementComponent::GetRequiredAlignmentMarkerCount() const { return RequiredAlignmentMarkerIds().Num(); }

FString UCXMRPlacementComponent::GetMissingAlignmentMarkerIds() const
{
	TSet<int32> Required = RequiredAlignmentMarkerIds();
	for (int32 Id : SavedAlignmentMarkers) { if (!IsFollowerMarker(Id)) { Required.Add(Id); } }
	TArray<int32> Ids = Required.Array();
	Ids.Sort();
	TArray<FString> Missing;
	for (int32 Id : Ids) { if (!IsMarkerObservationCurrent(Id)) { Missing.Add(FString::FromInt(Id)); } }
	return FString::Join(Missing, TEXT(", "));
}

bool UCXMRPlacementComponent::HaveAllAlignmentMarkers() const
{
	return GetRequiredAlignmentMarkerCount() > 0 && GetMissingAlignmentMarkerIds().IsEmpty();
}

bool UCXMRPlacementComponent::IsAlignmentSettling() const
{
	return GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(SettleTimer) && HaveAllAlignmentMarkers();
}

FText UCXMRPlacementComponent::GetAlignmentProgress() const
{
	if (bCapturingAlignment)
	{
		int32 Samples = 0;
		for (int32 Id : CaptureMarkerIds) { const FMarkerSamples* Found = CaptureSamples.Find(Id); Samples += Found ? FMath::Min(Found->Count, FMath::Max(1, AlignmentCaptureMinSamples)) : 0; }
		return FText::FromString(FString::Printf(TEXT("Saving alignment: %d / %d samples"), Samples, CaptureMarkerIds.Num() * FMath::Max(1, AlignmentCaptureMinSamples)));
	}
	if (!AlignmentSaveMessage.IsEmpty()) { return AlignmentSaveMessage; }
	if (!AlignmentStorageMessage.IsEmpty()) { return AlignmentStorageMessage; }
	if (bCalibrated) { return LOCTEXT("Aligned", "Aligned"); }
	const FString Missing = GetMissingAlignmentMarkerIds();
	if (GetRequiredAlignmentMarkerCount() == 0) { return LOCTEXT("NoMarkers", "Configure calibration marker IDs."); }
	if (!Missing.IsEmpty()) { return FText::FromString(FString::Printf(TEXT("Waiting for markers: %d of %d in view (missing %s)"), [this] { int32 Count = 0; for (int32 Id : RequiredAlignmentMarkerIds()) { Count += IsMarkerObservationCurrent(Id) ? 1 : 0; } return Count; }(), GetRequiredAlignmentMarkerCount(), *Missing)); }
	return IsAlignmentSettling() ? LOCTEXT("Settling", "Settling markers") : LOCTEXT("FitPending", "Waiting for a consistent marker fit");
}

void UCXMRPlacementComponent::FailAlignmentCapture(const FString& Reason)
{
	CancelAlignmentCapture();
	bLastCalibrationSaveSucceeded = false;
	AlignmentSaveMessage = FText::FromString(Reason);
	ReportAlignmentSaveMessage();
}

void UCXMRPlacementComponent::CheckAlignmentCapture()
{
	if (!bCapturingAlignment) { return; }
	if (!IsCapturePoseCurrent()) { FailAlignmentCapture(TEXT("Not saved. Placement or vehicle changed; request saving again.")); return; }
	const FString Missing = GetMissingAlignmentMarkerIds();
	if (!Missing.IsEmpty()) { FailAlignmentCapture(TEXT("Not saved. Missing markers: ") + Missing + TEXT(". Request saving again.")); return; }
	if (bAutoSaveCapture && CanSaveAlignment())
	{
		if (!SaveAlignment()) { const FString Reason = AlignmentSaveMessage.ToString(); FailAlignmentCapture(Reason); }
		else { ReportAlignmentSaveMessage(); }
	}
}

void UCXMRPlacementComponent::TimeoutAlignmentCapture()
{
	if (!bCapturingAlignment) { return; }
	TArray<int32> Ids = CaptureMarkerIds.Array(); Ids.Sort();
	TArray<FString> Short;
	for (int32 Id : Ids)
	{
		const FMarkerSamples* Samples = CaptureSamples.Find(Id);
		const int32 Needed = FMath::Max(1, AlignmentCaptureMinSamples) - (Samples ? Samples->Count : 0);
		if (Needed > 0) { Short.Add(FString::Printf(TEXT("%d (%d more samples)"), Id, Needed)); }
	}
	FailAlignmentCapture(TEXT("Not saved. Capture timed out; markers: ") + FString::Join(Short, TEXT(", ")) + TEXT(". Request saving again."));
}

bool UCXMRPlacementComponent::BeginAlignmentCapture()
{
	CancelAlignmentCapture();
	bLastCalibrationSaveSucceeded = false;
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
	CaptureAlignmentGroup = GetAlignmentGroup();
	CapturePose = Root->GetActorTransform();
	CaptureVehicle = Loader->GetSpawnedVehicle();
	CaptureVehicleProfile = Loader->Profile;
	CaptureMarkerProfile = MarkerProfile;
	CaptureVehicleRelativePose = CaptureVehicle->GetActorTransform().GetRelativeTransform(CapturePose);
	CaptureEyeReference = Loader->Profile->DriverEyeReference;
	bCaptureHasEyeReference = Loader->Profile->bHasDriverEyeReference;
	if (!HaveAllAlignmentMarkers())
	{
		AlignmentSaveMessage = FText::FromString(TEXT("Not saved. Missing markers: ") + GetMissingAlignmentMarkerIds());
		return false;
	}
	CaptureMarkerIds = RequiredAlignmentMarkerIds();
	bCapturePreviousLock = bAlignmentPoseLocked;
	bCapturingAlignment = true;
	bAlignmentPoseLocked = true;
	GetWorld()->GetTimerManager().SetTimer(CaptureCheckTimer, this, &UCXMRPlacementComponent::CheckAlignmentCapture, 0.05f, true);
	GetWorld()->GetTimerManager().SetTimer(CaptureTimeoutTimer, this, &UCXMRPlacementComponent::TimeoutAlignmentCapture, FMath::Max(0.1f, AlignmentCaptureTimeout), false);
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
		|| GetAlignmentGroup() != CaptureAlignmentGroup
		|| !Loader->Profile->DriverEyeReference.Equals(CaptureEyeReference, 0.001f)
		|| Loader->Profile->bHasDriverEyeReference != bCaptureHasEyeReference
		|| RequiredAlignmentMarkerIds().Num() != CaptureMarkerIds.Num() || !RequiredAlignmentMarkerIds().Includes(CaptureMarkerIds)) { return false; }
	for (int32 Id : CaptureMarkerIds)
	{
		FCXMRMarkerEntry Entry;
		if (!MarkerProfile->FindEntry(Id, Entry) || Entry.Role != ECXMRMarkerRole::Calibration) { return false; }
	}
	return true;
}

int32 UCXMRPlacementComponent::GetCapturedAlignmentMarkerCount() const
{
	if (!IsCapturePoseCurrent()) { return 0; }
	int32 Count = 0;
	for (int32 Id : CaptureMarkerIds) { const FMarkerSamples* Samples = CaptureSamples.Find(Id); Count += Samples && Samples->Count >= FMath::Max(1, AlignmentCaptureMinSamples) ? 1 : 0; }
	return Count;
}

bool UCXMRPlacementComponent::CanSaveAlignment() const
{
	return IsCapturePoseCurrent() && HaveAllAlignmentMarkers() && CaptureMarkerIds.Num() > 0 && GetCapturedAlignmentMarkerCount() == CaptureMarkerIds.Num();
}

bool UCXMRPlacementComponent::SaveAlignment()
{
	bLastCalibrationSaveSucceeded = false;
	if (!CanSaveAlignment())
	{
		AlignmentSaveMessage = IsCapturePoseCurrent()
			? FText::FromString(FString::Printf(TEXT("Not saved. %d / %d markers have enough samples. Keep all markers visible; saving is automatic."), GetCapturedAlignmentMarkerCount(), GetRequiredAlignmentMarkerCount()))
			: LOCTEXT("ConfirmAgain", "Not saved. Confirm alignment again to capture fresh marker observations.");
		return false;
	}
	TArray<FCXMRMarkerEntry> Entries = MarkerProfile->Markers;
	for (FCXMRMarkerEntry& Entry : Entries)
	{
		if (const FTransform* Relative = CapturedAlignmentMarkers.Find(Entry.MarkerId)) { Entry.LocalOffset = *Relative; }
	}
	// 파일 저장에 성공한 뒤 프로필과 기준 위치를 확정함.
	TMap<int32, FTransform> WorldMarkers;
	for (int32 Id : CaptureMarkerIds) { WorldMarkers.Add(Id, CaptureSamples.FindChecked(Id).Mean()); }
	if (!WriteCalibrationData(Entries, CaptureMarkerIds, 0, &WorldMarkers))
	{
		AlignmentSaveMessage = AlignmentStorageMessage.IsEmpty()
			? LOCTEXT("SaveFailed", "Save failed. Previous alignment kept. Check the save location, then retry.") : AlignmentStorageMessage;
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
			bAutoSaveCapture = true;
			AlignmentSaveMessage = LOCTEXT("CaptureStarted", "Saving alignment. Keep all configured markers visible.");
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
	if (Ids.IsEmpty() || Ids.Num() != SavedAlignmentMarkers.Num()) { return false; }
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

void UCXMRPlacementComponent::ChooseRestoredWorldPose(FTransform& Pose)
{
	bUsedSavedWorldPose = false;
	OriginMovement = -1.f;
	if (SavedAlignmentMarkers.IsEmpty()) { return; }
	bool bReuse = bHaveSavedWorldPose && SavedMarkerWorld.Num() == SavedAlignmentMarkers.Num();
	float MaxDistance = 0.f;
	for (int32 Id : SavedAlignmentMarkers)
	{
		const FTransform* Previous = SavedMarkerWorld.Find(Id);
		const FTransform* Current = DetectedCalib.Find(Id);
		if (!Previous || !Current) { bReuse = false; continue; }
		MaxDistance = FMath::Max(MaxDistance, static_cast<float>(FVector::Distance(Previous->GetLocation(), Current->GetLocation())));
		if (SavedAlignmentMarkers.Num() == 1 && FMath::Abs(FMath::FindDeltaAngleDegrees(Previous->Rotator().Yaw, Current->Rotator().Yaw)) > OriginReuseYawTolerance) { bReuse = false; }
	}
	if (bHaveSavedWorldPose && SavedMarkerWorld.Num() == SavedAlignmentMarkers.Num()) { OriginMovement = MaxDistance; }
	bUsedSavedWorldPose = bReuse && MaxDistance <= OriginReuseTolerance;
	if (bUsedSavedWorldPose) { Pose = SavedWorldPose; }
	AlignmentStorageMessage = OriginMovement < 0.f ? LOCTEXT("LegacyRestore", "Restored from markers (no saved origin reference)")
		: FText::FromString(bUsedSavedWorldPose
			? FString::Printf(TEXT("Restored saved world pose (origin unchanged, max %.2f cm)"), OriginMovement)
			: FString::Printf(TEXT("Restored from markers (origin moved %.2f cm)"), OriginMovement));
	UE_LOG(LogCXMRAlignment, Log, TEXT("%s"), *AlignmentStorageMessage.ToString());
	LogAlignmentMarkers(TEXT("Restore"), DetectedCalib, MarkerProfile->Markers, Pose);
}

void UCXMRPlacementComponent::LogAlignmentMarkers(const TCHAR* Action, const TMap<int32, FTransform>& Observations, const TArray<FCXMRMarkerEntry>& Entries, const FTransform& Pose) const
{
	TArray<int32> Ids; Observations.GetKeys(Ids); Ids.Sort();
	for (int32 Id : Ids)
	{
		const FCXMRMarkerEntry* Entry = Entries.FindByPredicate([Id](const FCXMRMarkerEntry& Item) { return Item.MarkerId == Id; });
		if (!Entry || Entry->Role != ECXMRMarkerRole::Calibration || IsFollowerMarker(Id)) { continue; }
		const FTransform& Observed = Observations.FindChecked(Id);
		const FMarkerSamples* Samples = bCapturingAlignment ? CaptureSamples.Find(Id) : MarkerSamples.Find(Id);
		const FVector World = Observed.GetLocation(), Local = Entry->LocalOffset.GetLocation();
		const FRotator Rotation = Observed.Rotator();
		const FVector Candidate = (Entry->LocalOffset.Inverse() * Observed).GetLocation();
		UE_LOG(LogCXMRAlignment, Log, TEXT("%s marker id=%d samples=%d scatter=%.2f cm world=(%.2f,%.2f,%.2f) cm rotation=(%.2f,%.2f,%.2f) deg local=(%.2f,%.2f,%.2f) cm candidate=(%.2f,%.2f,%.2f) cm root=(%.2f,%.2f,%.2f) cm yaw=%.2f deg fit=%.2f cm"),
			Action, Id, Samples ? Samples->Count : 1, Samples ? Samples->Scatter : 0.f, World.X, World.Y, World.Z, Rotation.Pitch, Rotation.Yaw, Rotation.Roll,
			Local.X, Local.Y, Local.Z, Candidate.X, Candidate.Y, Candidate.Z, Pose.GetLocation().X, Pose.GetLocation().Y, Pose.GetLocation().Z, Pose.Rotator().Yaw, LayoutFitError);
	}
}

#undef LOCTEXT_NAMESPACE
