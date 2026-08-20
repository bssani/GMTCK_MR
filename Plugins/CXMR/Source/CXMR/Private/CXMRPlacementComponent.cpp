// Copyright GMTCK CX.

#include "CXMRPlacementComponent.h"
#include "CXMRSubsystem.h"
#include "CXMRMarkerProfile.h"

#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRPlacement, Log, All);

namespace
{
	/** Runs an action on every placement component in the world. Console commands cannot reach a
	 *  component directly, and on site the operator is wearing a headset — typing is the fallback. */
	void ForEachPlacement(UWorld* World, TFunctionRef<void(UCXMRPlacementComponent&)> Action)
	{
		if (!World)
		{
			return;
		}
		for (TObjectIterator<UCXMRPlacementComponent> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				Action(**It);
			}
		}
	}
}

static FAutoConsoleCommandWithWorld GCXMRLearnMarkers(
	TEXT("CXMR.LearnMarkers"),
	TEXT("Record where the detected markers sit relative to the vehicle as it stands now, and save it."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ForEachPlacement(World, [](UCXMRPlacementComponent& P) { P.LearnMarkerLayout(); });
	}));

static FAutoConsoleCommandWithWorld GCXMRResetCalibration(
	TEXT("CXMR.ResetCalibration"),
	TEXT("Delete the saved marker calibration and restore the offsets the profile shipped with."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ForEachPlacement(World, [](UCXMRPlacementComponent& P) { P.ResetCalibrationToAuthored(); });
	}));

static FAutoConsoleCommandWithWorld GCXMRSaveCalibration(
	TEXT("CXMR.SaveCalibration"),
	TEXT("Write the current marker calibration to Saved/CXMR."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ForEachPlacement(World, [](UCXMRPlacementComponent& P)
		{
			UE_LOG(LogCXMRPlacement, Log, TEXT("Save requested -> %s"), *P.GetCalibrationFilePath());
			P.SaveCalibrationToDisk();
		});
	}));

UCXMRPlacementComponent::UCXMRPlacementComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UCXMRSubsystem* UCXMRPlacementComponent::GetCXMR() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UCXMRSubsystem>();
		}
	}
	return nullptr;
}

AActor* UCXMRPlacementComponent::ResolveVehicleRoot()
{
	return VehicleRoot ? VehicleRoot.Get() : GetOwner();
}

void UCXMRPlacementComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = GetCXMR();
	if (Subsystem)
	{
		// Subscribe to the subsystem (marker events + placement requests), never the plugin directly.
		Subsystem->OnMarkerDetected.AddDynamic(this, &UCXMRPlacementComponent::HandleMarkerDetected);
		Subsystem->OnMarkerMoved.AddDynamic(this, &UCXMRPlacementComponent::HandleMarkerMoved);
		Subsystem->OnRecalibrateRequested.AddDynamic(this, &UCXMRPlacementComponent::HandleRecalibrateRequest);
		Subsystem->OnPlaceRequested.AddDynamic(this, &UCXMRPlacementComponent::HandlePlaceRequest);
		Subsystem->OnAdjustMarkerOffsetRequested.AddDynamic(this, &UCXMRPlacementComponent::HandleAdjustOffsetRequest);
		Subsystem->OnSaveMarkerOffsetRequested.AddDynamic(this, &UCXMRPlacementComponent::HandleSaveOffsetRequest);
		Subsystem->OnResetMarkerOffsetRequested.AddDynamic(this, &UCXMRPlacementComponent::HandleResetOffsetRequest);
		PublishOffset();
	}

	// Keep the shipped values so field edits can be undone, then let a saved calibration win —
	// on site that file, not the asset, is what describes where the markers actually are.
	// The vehicle loader may have swapped the profile before we got here, so this is idempotent.
	EnsureCalibrationLoaded();
}

void UCXMRPlacementComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnMarkerDetected.RemoveDynamic(this, &UCXMRPlacementComponent::HandleMarkerDetected);
		Subsystem->OnMarkerMoved.RemoveDynamic(this, &UCXMRPlacementComponent::HandleMarkerMoved);
		Subsystem->OnRecalibrateRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandleRecalibrateRequest);
		Subsystem->OnPlaceRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandlePlaceRequest);
		Subsystem->OnAdjustMarkerOffsetRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandleAdjustOffsetRequest);
		Subsystem->OnSaveMarkerOffsetRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandleSaveOffsetRequest);
		Subsystem->OnResetMarkerOffsetRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandleResetOffsetRequest);
	}

	// Hand the data asset back exactly as it shipped. Field calibration lives in Saved/CXMR, so PIE
	// must not leave the asset dirty with values that were only ever meant for one physical setup.
	RestoreAuthoredOffsets();

	Super::EndPlay(Reason);
}

void UCXMRPlacementComponent::HandleMarkerDetected(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size)
{
	HandleMarkerPose(MarkerId, Position, Rotation, /*bIsFirstSighting*/ true);
}

void UCXMRPlacementComponent::HandleMarkerMoved(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size)
{
	HandleMarkerPose(MarkerId, Position, Rotation, /*bIsFirstSighting*/ false);
}

void UCXMRPlacementComponent::HandleMarkerPose(int32 MarkerId, const FVector& Position, const FRotator& Rotation, bool bIsFirstSighting)
{
	if (Mode != ECXMRPlacementMode::MarkerAnchor || !MarkerProfile)
	{
		return;
	}

	FCXMRMarkerEntry Entry;
	if (!MarkerProfile->FindEntry(MarkerId, Entry) || Entry.Role != ECXMRMarkerRole::Calibration)
	{
		return; // not a calibration marker (DynamicObject handled by a future component)
	}

	// Per-marker config — only valid AFTER detection (plugin caveat), and only worth doing once.
	if (bIsFirstSighting && Subsystem)
	{
		// The plugin reserves id 0 as its "invalid marker" sentinel and rejects both of these calls
		// outright (VarjoMarkersPlugin.cpp:150, :162), logging a warning that names no profile and so
		// tells the tester nothing. A profile authored with id 0 — the struct default — therefore looks
		// like a working setup right up until no physical marker ever matches it.
		if (MarkerId == 0)
		{
			UE_LOG(LogCXMRPlacement, Warning,
				TEXT("Marker profile '%s' contains id 0, which the Varjo plugin treats as its invalid-id "
				     "sentinel: timeout and tracking mode cannot be set for it. Physical markers carry "
				     "their own printed number — read it from the LogCXMRDebug 'DETECTED id=' line and "
				     "put that in the profile."),
				*MarkerProfile->GetName());
		}
		else
		{
			Subsystem->SetMarkerTimeout(MarkerId, Entry.Timeout);
			Subsystem->SetMarkerTrackingMode(MarkerId, Entry.TrackingMode);
		}
	}

	if (bFreezeAfterCalibration && bCalibrated)
	{
		return; // frozen
	}

	// Marker poses jitter every frame. Re-placing the vehicle on sub-millimetre noise would read as an
	// unstable car, so an update has to actually move before it earns a recompute. A first sighting
	// always counts — that is the sample that puts the marker on the board at all.
	const FTransform NewPose(Rotation, Position, FVector::OneVector);
	if (!bIsFirstSighting)
	{
		const FTransform* Existing = DetectedCalib.Find(MarkerId);
		if (Existing && FVector::Dist(Existing->GetLocation(), Position) < MarkerUpdateThreshold)
		{
			return;
		}
	}

	// Accumulate this calibration marker's world pose, then (re)compute the alignment.
	DetectedCalib.Add(MarkerId, NewPose);
	RecomputeCalibration();
}

void UCXMRPlacementComponent::RecomputeCalibration()
{
	AActor* Root = ResolveVehicleRoot();
	if (!Root)
	{
		return;
	}

	FTransform VehicleWorld;
	bool bHavePose = false;

	// 2+ markers: robust baseline yaw + floor. 1 marker: full-transform fallback (uses its orientation).
	if (DetectedCalib.Num() >= 2)
	{
		bHavePose = ComputeMultiMarkerTransform(VehicleWorld);
	}
	if (!bHavePose && DetectedCalib.Num() > 0 && MarkerProfile)
	{
		const TPair<int32, FTransform>& First = *DetectedCalib.CreateConstIterator();
		FCXMRMarkerEntry Entry;
		if (MarkerProfile->FindEntry(First.Key, Entry))
		{
			// VehicleWorld = Inverse(LocalOffset) * MarkerWorld (UE compose A*B = apply A then B).
			VehicleWorld = Entry.LocalOffset.Inverse() * First.Value;
			bHavePose = true;
		}
	}

	// The manual offset is applied in one place for both paths. It used to live inside the
	// single-marker fallback only, so with the usual 2+ markers the adjust keys did nothing.
	if (bHavePose)
	{
		BaseVehicleTransform = VehicleWorld;
		bHaveBasePose = true;
	}
	else if (!bHaveBasePose)
	{
		// No markers yet and nothing cached — nudge the vehicle from wherever it currently stands.
		BaseVehicleTransform = Root->GetActorTransform();
		bHaveBasePose = true;
	}

	ApplyPlacement();

	// Freeze once enough markers have contributed. The freeze itself is LOCAL — bCalibrated makes
	// HandleMarkerDetected stop re-placing. Killing the headset's marker tracking is global and would
	// take DynamicObject markers (doors, props) down with it, so it is opt-in and off by default.
	if (DetectedCalib.Num() >= MinMarkersToCalibrate)
	{
		bCalibrated = true;
		if (bStopMarkerTrackingWhenCalibrated && Subsystem)
		{
			Subsystem->SetMarkerTracking(false);
		}
	}
}

bool UCXMRPlacementComponent::ComputeMultiMarkerTransform(FTransform& Out) const
{
	// Correspondences: p = marker position in vehicle-local space, q = measured world position.
	// Solve yaw (about Z) + translation, roll/pitch = 0 (vehicle sits level on the floor).
	// Uses marker POSITIONS only — the baseline between markers gives a far more stable yaw than
	// any single marker's own (noisy) orientation.
	TArray<FVector> P, Q;
	for (const TPair<int32, FTransform>& Pair : DetectedCalib)
	{
		FCXMRMarkerEntry Entry;
		if (MarkerProfile->FindEntry(Pair.Key, Entry))
		{
			P.Add(Entry.LocalOffset.GetLocation());
			Q.Add(Pair.Value.GetLocation());
		}
	}
	if (P.Num() < 2)
	{
		return false;
	}

	FVector PBar = FVector::ZeroVector;
	FVector QBar = FVector::ZeroVector;
	for (int32 i = 0; i < P.Num(); ++i) { PBar += P[i]; QBar += Q[i]; }
	PBar /= P.Num();
	QBar /= P.Num();

	// Optimal yaw about Z (least squares): yaw = atan2( sum cross_xy, sum dot_xy ).
	double DotSum = 0.0;
	double CrossSum = 0.0;
	for (int32 i = 0; i < P.Num(); ++i)
	{
		const FVector dp = P[i] - PBar;
		const FVector dq = Q[i] - QBar;
		DotSum   += dp.X * dq.X + dp.Y * dq.Y;
		CrossSum += dp.X * dq.Y - dp.Y * dq.X;
	}

	const float YawDeg = FMath::RadiansToDegrees(static_cast<float>(FMath::Atan2(CrossSum, DotSum)));
	const FRotator Rot(0.0f, YawDeg, 0.0f);
	const FVector Trans = QBar - Rot.RotateVector(PBar);

	Out = FTransform(Rot, Trans, FVector::OneVector);
	return true;
}

void UCXMRPlacementComponent::Recalibrate()
{
	bCalibrated = false;
	DetectedCalib.Reset();

	// Cycle tracking so the plugin re-fires Detected (its marker map is only reset on disable).
	if (Subsystem)
	{
		Subsystem->SetMarkerTracking(false);
		Subsystem->SetMarkerTracking(true);
	}
}

void UCXMRPlacementComponent::HandleRecalibrateRequest()
{
	Recalibrate();
}

void UCXMRPlacementComponent::HandlePlaceRequest()
{
	PlaceInFrontOfPawn();
}

void UCXMRPlacementComponent::PlaceInFrontOfPawn()
{
	AActor* Root = ResolveVehicleRoot();
	if (!Root)
	{
		return;
	}

	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	APawn* Pawn = UGameplayStatics::GetPlayerPawn(GetWorld(), 0);
	if (!CamMgr || !Pawn)
	{
		return;
	}

	// Horizontal look direction from the (HMD-driven) camera.
	FRotator LookRot = CamMgr->GetCameraRotation();
	LookRot.Pitch = 0.0f;
	LookRot.Roll = 0.0f;
	const FVector Forward = LookRot.Vector();

	// In front of the camera, on the floor (pawn origin = VR floor when tracking origin is floor/stage).
	FVector Target = CamMgr->GetCameraLocation() + Forward * PawnRelativeDistance;
	Target.Z = Pawn->GetActorLocation().Z;

	// Face the user (yaw + 180 so the vehicle front points back toward them).
	const FRotator FaceRot(0.0f, LookRot.Yaw + 180.0f, 0.0f);

	// This placement becomes what the offset keys nudge from; otherwise the first nudge would snap
	// the vehicle back to whatever the markers last said.
	BaseVehicleTransform = FTransform(FaceRot, Target, FVector::OneVector);
	bHaveBasePose = true;
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	PublishOffset();   // the panel would otherwise keep showing the offset we just threw away
	ApplyPlacement();

	bCalibrated = true;
}

// ---------- Field calibration ----------

void UCXMRPlacementComponent::EnsureCalibrationLoaded()
{
	if (bCalibrationLoaded && CapturedProfile == MarkerProfile)
	{
		return;
	}

	// Whatever the previous profile is holding came from a file, not from its author. Give it back
	// before moving on, or that asset stays edited for the rest of the session.
	RestoreAuthoredOffsets();

	CaptureAuthoredOffsets();
	LoadCalibrationFromDisk();
	bCalibrationLoaded = true;
}

void UCXMRPlacementComponent::SetMarkerProfile(UCXMRMarkerProfile* NewProfile)
{
	if (MarkerProfile == NewProfile)
	{
		return;
	}
	MarkerProfile = NewProfile;
	EnsureCalibrationLoaded();

	// Everything detected so far was keyed by the OLD profile's marker ids. Keeping it means the
	// solve runs against markers the new vehicle never heard of, and bCalibrated would keep
	// HandleMarkerPose frozen so the new ones are ignored too — a swap that silently never aligns.
	DetectedCalib.Reset();
	bCalibrated = false;
}

void UCXMRPlacementComponent::CaptureAuthoredOffsets()
{
	AuthoredOffsets.Reset();
	CapturedProfile = MarkerProfile;
	if (!MarkerProfile)
	{
		return;
	}
	for (const FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
	{
		AuthoredOffsets.Add(Entry.MarkerId, Entry.LocalOffset);
	}
}

void UCXMRPlacementComponent::RestoreAuthoredOffsets()
{
	UCXMRMarkerProfile* Profile = CapturedProfile.Get();
	if (!Profile)
	{
		return;
	}
	for (FCXMRMarkerEntry& Entry : Profile->Markers)
	{
		if (const FTransform* Authored = AuthoredOffsets.Find(Entry.MarkerId))
		{
			Entry.LocalOffset = *Authored;
		}
	}
}

FString UCXMRPlacementComponent::GetCalibrationFilePath() const
{
	if (!MarkerProfile)
	{
		return FString();
	}
	return FPaths::ProjectSavedDir() / TEXT("CXMR")
		/ FString::Printf(TEXT("MarkerCalib_%s.json"), *MarkerProfile->GetCalibrationId());
}

/** Untouched copy of whatever was on disk when this session started. The live file is overwritten
 *  every save, so a run of bad saves would otherwise leave nothing to go back to. */
static FString StartupBackupPath(const FString& LivePath)
{
	return FPaths::ChangeExtension(LivePath, TEXT("startup.json"));
}

void UCXMRPlacementComponent::LearnMarkerLayout()
{
	AActor* Root = ResolveVehicleRoot();
	if (!Root || !MarkerProfile || DetectedCalib.Num() == 0)
	{
		UE_LOG(LogCXMRPlacement, Warning,
			TEXT("Cannot learn marker layout: need a vehicle, a profile and at least one detected marker "
			     "(vehicle=%s profile=%s detected=%d)."),
			Root ? TEXT("ok") : TEXT("MISSING"), MarkerProfile ? TEXT("ok") : TEXT("MISSING"),
			DetectedCalib.Num());
		return;
	}

	// Where the vehicle stands right now is the truth we are recording against — the operator has
	// just lined it up with the physical model by eye.
	const FTransform VehicleWorld = Root->GetActorTransform();

	int32 Learned = 0;
	for (const TPair<int32, FTransform>& Detected : DetectedCalib)
	{
		if (FCXMRMarkerEntry* Entry = MarkerProfile->GetEntryMutable(Detected.Key))
		{
			// LocalOffset * VehicleWorld == MarkerWorld, which is what GetRelativeTransform solves.
			Entry->LocalOffset = Detected.Value.GetRelativeTransform(VehicleWorld);
			++Learned;
		}
	}

	if (Learned == 0)
	{
		UE_LOG(LogCXMRPlacement, Warning,
			TEXT("Marker layout not learned: none of the %d detected markers is listed in profile '%s'."),
			DetectedCalib.Num(), *MarkerProfile->GetName());
		return;
	}

	// The learned layout already reproduces this pose, so the manual offset starts clean.
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	BaseVehicleTransform = VehicleWorld;
	bHaveBasePose = true;
	PublishOffset();

	UE_LOG(LogCXMRPlacement, Log, TEXT("Learned layout of %d marker(s) from the current vehicle pose."), Learned);

	// Re-place from what we just learned. If the maths is right the vehicle does not move; if it
	// jumps, the layout is wrong and the operator sees it immediately instead of hours later.
	RecomputeCalibration();

	SaveCalibrationToDisk();
}

bool UCXMRPlacementComponent::SaveCalibrationToDisk()
{
	const FString Path = GetCalibrationFilePath();
	if (Path.IsEmpty() || !MarkerProfile)
	{
		return false;
	}

	TArray<TSharedPtr<FJsonValue>> MarkerArray;
	for (const FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
	{
		const FVector  Loc = Entry.LocalOffset.GetLocation();
		const FRotator Rot = Entry.LocalOffset.Rotator();

		TSharedRef<FJsonObject> Obj = MakeShared<FJsonObject>();
		Obj->SetNumberField(TEXT("id"), Entry.MarkerId);
		Obj->SetStringField(TEXT("label"), Entry.Label.ToString());
		Obj->SetNumberField(TEXT("x"), Loc.X);
		Obj->SetNumberField(TEXT("y"), Loc.Y);
		Obj->SetNumberField(TEXT("z"), Loc.Z);
		Obj->SetNumberField(TEXT("pitch"), Rot.Pitch);
		Obj->SetNumberField(TEXT("yaw"),   Rot.Yaw);
		Obj->SetNumberField(TEXT("roll"),  Rot.Roll);
		MarkerArray.Add(MakeShared<FJsonValueObject>(Obj));
	}

	TSharedRef<FJsonObject> RootObj = MakeShared<FJsonObject>();
	RootObj->SetStringField(TEXT("profile"), MarkerProfile->GetName());
	RootObj->SetStringField(TEXT("savedAt"), FDateTime::Now().ToString());
	RootObj->SetStringField(TEXT("units"), TEXT("cm, degrees; marker pose in vehicle-local space"));
	RootObj->SetArrayField(TEXT("markers"), MarkerArray);

	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(RootObj, Writer))
	{
		UE_LOG(LogCXMRPlacement, Warning, TEXT("Failed to serialize marker calibration."));
		return false;
	}

	// SaveStringToFile does not create directories, and Saved/CXMR does not exist on a fresh install
	// — without this the very first save fails.
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree*/ true);

	if (!FFileHelper::SaveStringToFile(Text, *Path))
	{
		UE_LOG(LogCXMRPlacement, Warning, TEXT("Failed to write marker calibration to '%s'."), *Path);
		return false;
	}

	UE_LOG(LogCXMRPlacement, Log, TEXT("Marker calibration saved: %s"), *Path);
	return true;
}

bool UCXMRPlacementComponent::LoadCalibrationFromDisk()
{
	const FString Path = GetCalibrationFilePath();
	if (Path.IsEmpty() || !MarkerProfile || !FPaths::FileExists(Path))
	{
		return false;
	}

	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		UE_LOG(LogCXMRPlacement, Warning, TEXT("Could not read marker calibration '%s'."), *Path);
		return false;
	}

	TSharedPtr<FJsonObject> RootObj;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, RootObj) || !RootObj.IsValid())
	{
		UE_LOG(LogCXMRPlacement, Warning,
			TEXT("Marker calibration '%s' is not valid JSON — ignoring it and using the profile as authored."), *Path);
		return false;
	}

	// Two profiles sharing a CalibrationId would silently read each other's file. Say so.
	FString SavedFor;
	if (RootObj->TryGetStringField(TEXT("profile"), SavedFor) && SavedFor != MarkerProfile->GetName())
	{
		UE_LOG(LogCXMRPlacement, Warning,
			TEXT("Calibration '%s' was saved for profile '%s' but is being applied to '%s'. "
			     "Check for a duplicate Calibration Id."),
			*Path, *SavedFor, *MarkerProfile->GetName());
	}

	// One untouched copy per session, taken before anything can overwrite the live file. Keyed by
	// path, not a bool, so switching vehicles backs up each profile's file exactly once.
	if (!StartupBackedUp.Contains(Path))
	{
		IFileManager::Get().Copy(*StartupBackupPath(Path), *Path);
		StartupBackedUp.Add(Path);
	}

	const TArray<TSharedPtr<FJsonValue>>* MarkerArray = nullptr;
	if (!RootObj->TryGetArrayField(TEXT("markers"), MarkerArray) || !MarkerArray)
	{
		return false;
	}

	int32 Applied = 0;
	for (const TSharedPtr<FJsonValue>& Value : *MarkerArray)
	{
		const TSharedPtr<FJsonObject>* Obj = nullptr;
		if (!Value.IsValid() || !Value->TryGetObject(Obj) || !Obj)
		{
			continue;
		}

		double Id = 0.0;
		if (!(*Obj)->TryGetNumberField(TEXT("id"), Id))
		{
			continue;
		}

		FCXMRMarkerEntry* Entry = MarkerProfile->GetEntryMutable(static_cast<int32>(Id));
		if (!Entry)
		{
			UE_LOG(LogCXMRPlacement, Warning,
				TEXT("Saved calibration mentions marker %d, which profile '%s' does not list. Skipped."),
				static_cast<int32>(Id), *MarkerProfile->GetName());
			continue;
		}

		const FVector Loc(
			(*Obj)->GetNumberField(TEXT("x")),
			(*Obj)->GetNumberField(TEXT("y")),
			(*Obj)->GetNumberField(TEXT("z")));
		const FRotator Rot(
			(*Obj)->GetNumberField(TEXT("pitch")),
			(*Obj)->GetNumberField(TEXT("yaw")),
			(*Obj)->GetNumberField(TEXT("roll")));

		Entry->LocalOffset = FTransform(Rot, Loc, FVector::OneVector);
		++Applied;
	}

	UE_LOG(LogCXMRPlacement, Log, TEXT("Applied saved calibration for %d marker(s) from %s"), Applied, *Path);
	return Applied > 0;
}

void UCXMRPlacementComponent::ResetCalibrationToAuthored()
{
	const FString Path = GetCalibrationFilePath();
	if (!Path.IsEmpty() && FPaths::FileExists(Path))
	{
		IFileManager::Get().Delete(*Path);
		UE_LOG(LogCXMRPlacement, Log, TEXT("Deleted saved calibration: %s"), *Path);
	}

	RestoreAuthoredOffsets();
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	PublishOffset();
	RecomputeCalibration();

	UE_LOG(LogCXMRPlacement, Log, TEXT("Marker calibration reset to the values the profile shipped with."));
}

void UCXMRPlacementComponent::RebaseToCurrentTransform()
{
	AActor* Root = ResolveVehicleRoot();
	if (!Root)
	{
		return;
	}

	BaseVehicleTransform = Root->GetActorTransform();
	bHaveBasePose = true;
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	PublishOffset();
}

void UCXMRPlacementComponent::HandleAdjustOffsetRequest(FVector DeltaLocation, FRotator DeltaRotation)
{
	AdjustMarkerOffset(DeltaLocation, DeltaRotation);
}

void UCXMRPlacementComponent::HandleSaveOffsetRequest()  { SaveMarkerOffsetToProfile(); }
void UCXMRPlacementComponent::HandleResetOffsetRequest() { ResetMarkerOffset(); }

void UCXMRPlacementComponent::PublishOffset()
{
	if (Subsystem)
	{
		Subsystem->PublishMarkerOffset(TempMarkerLocationOffset, TempMarkerRotationOffset);
	}
}

void UCXMRPlacementComponent::ApplyPlacement()
{
	AActor* Root = ResolveVehicleRoot();
	if (!Root || !bHaveBasePose)
	{
		return;
	}

	// Adjust is expressed in the vehicle's own frame, so it is applied before the base pose
	// (UE compose A*B = apply A then B). Inverse: nudging the marker +X slides the vehicle -X.
	const FTransform Adjust(TempMarkerRotationOffset, TempMarkerLocationOffset);
	Root->SetActorTransform(Adjust.Inverse() * BaseVehicleTransform);
}

void UCXMRPlacementComponent::AdjustMarkerOffset(FVector DeltaLocation, FRotator DeltaRotation)
{
	TempMarkerLocationOffset += DeltaLocation;
	TempMarkerRotationOffset += DeltaRotation;

	UE_LOG(LogCXMRPlacement, Log,
		TEXT("Marker offset adjusted: Loc=(%.1f, %.1f, %.1f) cm, Rot=(%.1f, %.1f, %.1f) deg"),
		TempMarkerLocationOffset.X, TempMarkerLocationOffset.Y, TempMarkerLocationOffset.Z,
		TempMarkerRotationOffset.Pitch, TempMarkerRotationOffset.Yaw, TempMarkerRotationOffset.Roll);

	// Recompute placement with new offset.
	PublishOffset();
	RecomputeCalibration();
}

void UCXMRPlacementComponent::SaveMarkerOffsetToProfile()
{
	if (!MarkerProfile || DetectedCalib.Num() == 0)
	{
		UE_LOG(LogCXMRPlacement, Warning, TEXT("Cannot save marker offset: no profile or no detected markers"));
		return;
	}

	if (TempMarkerLocationOffset.IsNearlyZero() && TempMarkerRotationOffset.IsNearlyZero())
	{
		UE_LOG(LogCXMRPlacement, Log, TEXT("Nothing to save: marker offset is zero"));
		return;
	}

	// Every detected marker moves by the same amount. The multi-marker solve fits the vehicle to ALL
	// of their local positions, so baking the offset into just one would skew the fit instead of
	// shifting the vehicle. L' = L * Adjust reproduces the pose the offset is currently showing.
	const FTransform Adjust(TempMarkerRotationOffset, TempMarkerLocationOffset);
	int32 SavedCount = 0;
	for (const TPair<int32, FTransform>& Detected : DetectedCalib)
	{
		if (FCXMRMarkerEntry* EntryPtr = MarkerProfile->GetEntryMutable(Detected.Key))
		{
			EntryPtr->LocalOffset = EntryPtr->LocalOffset * Adjust;
			++SavedCount;
		}
		else
		{
			UE_LOG(LogCXMRPlacement, Warning, TEXT("Cannot find marker %d in profile"), Detected.Key);
		}
	}

	if (SavedCount == 0)
	{
		UE_LOG(LogCXMRPlacement, Warning, TEXT("Marker offset not saved: no detected marker is in the profile"));
		return;
	}

	UE_LOG(LogCXMRPlacement, Log,
		TEXT("Marker offset saved onto %d marker(s): Loc=(%.1f, %.1f, %.1f) cm, Yaw=%.1f deg"),
		SavedCount, TempMarkerLocationOffset.X, TempMarkerLocationOffset.Y, TempMarkerLocationOffset.Z,
		TempMarkerRotationOffset.Yaw);

	// Persist to Saved/CXMR. MarkPackageDirty only means anything in the editor — a cooked build
	// cannot write its own assets, so without the file the calibration dies with the process.
	MarkerProfile->MarkPackageDirty();
	SaveCalibrationToDisk();

	// Clear temporary adjustments — the pose is now baked into the profile, so re-applying would double it.
	ResetMarkerOffset();
}

void UCXMRPlacementComponent::ResetMarkerOffset()
{
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;

	UE_LOG(LogCXMRPlacement, Log, TEXT("Marker offset adjustments reset"));

	// Recompute with clean offset.
	PublishOffset();
	RecomputeCalibration();
}
