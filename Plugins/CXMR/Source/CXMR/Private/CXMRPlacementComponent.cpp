// Copyright GMTCK CX.

#include "CXMRPlacementComponent.h"
#include "CXMRSubsystem.h"
#include "CXMRMarkerProfile.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRLevelFit.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
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

	RegisterTunables();

	// A saved layout only helps while markers are tracked, and tracking starts off.
	if (Mode == ECXMRPlacementMode::MarkerAnchor && bStartMarkerTrackingOnBeginPlay)
	{
		TryStartMarkerTracking();
	}
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
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearAllTimersForObject(this);
	}

	RestoreAuthoredOffsets();

	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	if (UCXMRTuningSubsystem* Tuning = GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr)
	{
		Tuning->UnregisterOwner(this);
	}

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

	// Remember every marker, listed or not. Learn records the layout from these — which is how a site whose marker
	// ids were never typed into the profile gets calibrated at all. While calibrating this is the AVERAGE of the
	// samples so far, so a layout is never learned from one noisy frame either.
	const bool bAveraging = bAverageMarkerSamples && !bCalibrated;
	const FTransform Pose = bAveraging
		? AccumulateSample(MarkerId, Position, Rotation)
		: FTransform(Rotation, Position, FVector::OneVector);
	if (MarkerId != 0)
	{
		SeenMarkers.Add(MarkerId, Pose);
	}

	FCXMRMarkerEntry Entry;
	if (!MarkerProfile->FindEntry(MarkerId, Entry) || Entry.Role != ECXMRMarkerRole::Calibration)
	{
		return; // not in the layout yet (Learn adds it); DynamicObject markers belong to another component
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

	// While calibrating, every sample counts: the mean of a settle window's worth of them is what makes a restarted
	// session land where the last one did. The movement threshold used to drop samples here, which kept whichever
	// early, noisy frame arrived first and stopped the pose improving at all.
	//
	// Once calibrated and following markers (freeze off), the threshold earns its keep: jitter would otherwise
	// re-place the car every frame and read as an unstable vehicle.
	if (bCalibrated && !bIsFirstSighting)
	{
		const FTransform* Existing = DetectedCalib.Find(MarkerId);
		if (Existing && FVector::Dist(Existing->GetLocation(), Pose.GetLocation()) < MarkerUpdateThreshold)
		{
			return;
		}
	}

	DetectedCalib.Add(MarkerId, Pose);
	RecomputeCalibration();
}

/** Same position, rotation reduced to heading. The heading comes from whichever horizontal axis survives:
 *  a marker lying flat can point its X straight up, where FRotator::Yaw means nothing. */
static FTransform LevelTransform(const FTransform& In)
{
	const FQuat Rotation = In.GetRotation();
	FVector Heading = Rotation.GetForwardVector();
	Heading.Z = 0.0;
	if (!Heading.Normalize())
	{
		FVector Right = Rotation.GetRightVector();
		Right.Z = 0.0;
		Heading = Right.GetSafeNormal() ^ FVector::UpVector;   // right x up = forward
	}
	return FTransform(FRotationMatrix::MakeFromX(Heading).ToQuat(), In.GetLocation(), In.GetScale3D());
}

FTransform UCXMRPlacementComponent::FMarkerSamples::Mean() const
{
	if (Count <= 0)
	{
		return FTransform::Identity;
	}
	const FVector Position = PositionSum / Count;
	const FVector Forward = ForwardSum.GetSafeNormal();
	const FVector Up = UpSum.GetSafeNormal();
	if (Forward.IsNearlyZero() || Up.IsNearlyZero())
	{
		return FTransform(FQuat::Identity, Position, FVector::OneVector);
	}
	return FTransform(FRotationMatrix::MakeFromXZ(Forward, Up).ToQuat(), Position, FVector::OneVector);
}

FTransform UCXMRPlacementComponent::AccumulateSample(int32 MarkerId, const FVector& Position, const FRotator& Rotation)
{
	FMarkerSamples& Samples = MarkerSamples.FindOrAdd(MarkerId);
	const FQuat Q = Rotation.Quaternion();
	Samples.PositionSum += Position;
	Samples.ForwardSum  += Q.GetForwardVector();
	Samples.UpSum       += Q.GetUpVector();
	Samples.SquaredSum  += Position.SizeSquared();
	++Samples.Count;

	// Spread around the mean: sqrt(E[|p|^2] - |E[p]|^2). Reported to the operator, who otherwise has no way to tell
	// a marker the headset sees well from one it is guessing at.
	const FVector MeanPosition = Samples.PositionSum / Samples.Count;
	const double Variance = Samples.SquaredSum / Samples.Count - MeanPosition.SizeSquared();
	Samples.Scatter = static_cast<float>(FMath::Sqrt(FMath::Max(0.0, Variance)));

	return Samples.Mean();
}

float UCXMRPlacementComponent::WorstScatter() const
{
	float Worst = 0.0f;
	for (const TPair<int32, FMarkerSamples>& Pair : MarkerSamples)
	{
		Worst = FMath::Max(Worst, Pair.Value.Scatter);
	}
	return Worst;
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
	LayoutFitError = -1.0f;
	if (DetectedCalib.Num() >= 2)
	{
		bHavePose = ComputeMultiMarkerTransform(VehicleWorld, LayoutFitError);
	}
	if (!bHavePose && DetectedCalib.Num() > 0 && MarkerProfile)
	{
		const TPair<int32, FTransform>& First = *DetectedCalib.CreateConstIterator();
		FCXMRMarkerEntry Entry;
		if (MarkerProfile->FindEntry(First.Key, Entry))
		{
			// VehicleWorld = Inverse(LocalOffset) * MarkerWorld (UE compose A*B = apply A then B).
			VehicleWorld = Entry.LocalOffset.Inverse() * First.Value;
			if (bKeepLevel)
			{
				// Match the multi-marker solve, which is always level. Otherwise the car takes the tilt of
				// whatever surface the marker sits on, and jumps when a second marker arrives.
				VehicleWorld = LevelTransform(VehicleWorld);
			}
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
	if (DetectedCalib.Num() >= MinMarkersToCalibrate && !bCalibrated)
	{
		UWorld* World = GetWorld();
		if (CalibrationSettleSeconds <= 0.0f || !World)
		{
			FinishCalibration();
		}
		else if (!World->GetTimerManager().IsTimerActive(SettleTimer))
		{
			// Keep taking marker updates for a moment before freezing: a first sighting is a marker's noisiest sample.
			World->GetTimerManager().SetTimer(SettleTimer, this, &UCXMRPlacementComponent::FinishCalibration, CalibrationSettleSeconds, false);
		}
	}
}

void UCXMRPlacementComponent::FinishCalibration()
{
	if (bCalibrated || DetectedCalib.Num() < MinMarkersToCalibrate)
	{
		return;
	}
	bCalibrated = true;
	UE_LOG(LogCXMRPlacement, Log, TEXT("Calibrated from %d marker(s)%s."), DetectedCalib.Num(),
		bFreezeAfterCalibration ? TEXT(" - placement frozen until Recalibrate") : TEXT(""));
	if (bStopMarkerTrackingWhenCalibrated && Subsystem)
	{
		Subsystem->SetMarkerTracking(false);
	}
}

void UCXMRPlacementComponent::TryStartMarkerTracking()
{
	if (!Subsystem || Subsystem->IsMarkerTrackingOn())
	{
		return;
	}

	// Support is only reported once the XR session is up, which can be seconds after BeginPlay. Asking earlier makes
	// the plugin refuse and log a warning every time, so wait quietly instead.
	if (Subsystem->IsMarkerTrackingSupported() && Subsystem->SetMarkerTracking(true))
	{
		UE_LOG(LogCXMRPlacement, Log, TEXT("Marker tracking started for calibration."));
		return;
	}

	const int32 MaxAttempts = 40;   // every 0.5 s -> 20 s
	if (++TrackingStartAttempts < MaxAttempts)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(TrackingStartTimer, this, &UCXMRPlacementComponent::TryStartMarkerTracking, 0.5f, false);
		}
		return;
	}
	UE_LOG(LogCXMRPlacement, Warning,
		TEXT("Marker tracking could not be started within 20 s (no headset session, or markers unsupported). "
		     "Press V once the headset is running."));
}

bool UCXMRPlacementComponent::ComputeMultiMarkerTransform(FTransform& Out, float& OutResidual) const
{
	// Correspondences: marker position in vehicle-local space vs measured world position. Uses marker POSITIONS only —
	// the baseline between markers gives a far more stable yaw than any single marker's own (noisy) orientation.
	//
	// OutResidual: how far the measured markers end up from the saved layout, RMS cm. A layout learned in one session
	// and markers measured in another disagree by exactly this much — and that disagreement is what moves the car
	// between sessions, so it is worth a number on screen rather than a surprise the next morning.
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
	return CXMRLevelFit::Solve(P, Q, Out, OutResidual, 0.0);
}

void UCXMRPlacementComponent::Recalibrate()
{
	bCalibrated = false;
	DetectedCalib.Reset();
	SeenMarkers.Reset();
	MarkerSamples.Reset();
	LayoutFitError = -1.0f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettleTimer);
	}

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

	// Markers Learn or a saved file added at runtime were never part of the asset.
	Profile->Markers.RemoveAll([this](const FCXMRMarkerEntry& Entry) { return !AuthoredOffsets.Contains(Entry.MarkerId); });

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
	if (!Root || !MarkerProfile || SeenMarkers.Num() == 0)
	{
		UE_LOG(LogCXMRPlacement, Warning,
			TEXT("Cannot learn marker layout: need a vehicle, a profile and at least one marker in view "
			     "(vehicle=%s profile=%s markers seen=%d)."),
			Root ? TEXT("ok") : TEXT("MISSING"), MarkerProfile ? TEXT("ok") : TEXT("MISSING"), SeenMarkers.Num());
		return;
	}

	// Where the vehicle stands right now is the truth we are recording against — the operator has
	// just lined it up with the physical model by eye.
	const FTransform VehicleWorld = Root->GetActorTransform();

	int32 Updated = 0;
	int32 Added = 0;
	for (const TPair<int32, FTransform>& Seen : SeenMarkers)
	{
		// LocalOffset * VehicleWorld == MarkerWorld, which is what GetRelativeTransform solves.
		const FTransform Local = Seen.Value.GetRelativeTransform(VehicleWorld);
		if (FCXMRMarkerEntry* Entry = MarkerProfile->GetEntryMutable(Seen.Key))
		{
			if (Entry->Role != ECXMRMarkerRole::Calibration)
			{
				continue;   // a marker driving a door or a prop is not part of the vehicle's layout
			}
			Entry->LocalOffset = Local;
			++Updated;
		}
		else
		{
			// Not listed: add it. The layout lives in the saved file, so the profile need not know a site's marker
			// ids in advance. These used to be ignored, and Learn then learned nothing at all.
			FCXMRMarkerEntry NewEntry;
			NewEntry.MarkerId = Seen.Key;
			NewEntry.Role = ECXMRMarkerRole::Calibration;
			NewEntry.LocalOffset = Local;
			NewEntry.Label = FName(*FString::Printf(TEXT("Learned_%d"), Seen.Key));
			MarkerProfile->Markers.Add(NewEntry);
			++Added;
		}
		DetectedCalib.Add(Seen.Key, Seen.Value);   // learned markers take part in placement straight away
	}

	// Listed markers that were not in view keep offsets from an earlier setup; if that setup differs they pull the fit
	// the next time they are seen, so name them.
	for (const FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
	{
		if (Entry.Role == ECXMRMarkerRole::Calibration && Entry.MarkerId != 0 && !SeenMarkers.Contains(Entry.MarkerId))
		{
			UE_LOG(LogCXMRPlacement, Warning,
				TEXT("Marker %d is in the layout but was not in view while learning: its offset is from an earlier setup."),
				Entry.MarkerId);
		}
	}

	if (Updated + Added == 0)
	{
		UE_LOG(LogCXMRPlacement, Warning, TEXT("Marker layout not learned: no calibration marker in view."));
		return;
	}

	// The learned layout already reproduces this pose, so the manual offset starts clean.
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	BaseVehicleTransform = VehicleWorld;
	bHaveBasePose = true;
	PublishOffset();

	UE_LOG(LogCXMRPlacement, Log, TEXT("Learned marker layout from the current vehicle pose: %d updated, %d added."), Updated, Added);

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

		const int32 MarkerIdValue = static_cast<int32>(Id);
		FCXMRMarkerEntry* Entry = MarkerProfile->GetEntryMutable(MarkerIdValue);
		if (!Entry)
		{
			// Learned on site: the profile never listed it. Put it back into the layout.
			FCXMRMarkerEntry NewEntry;
			NewEntry.MarkerId = MarkerIdValue;
			NewEntry.Role = ECXMRMarkerRole::Calibration;
			FString Label;
			if ((*Obj)->TryGetStringField(TEXT("label"), Label) && !Label.IsEmpty() && Label != TEXT("None"))
			{
				NewEntry.Label = FName(*Label);
			}
			MarkerProfile->Markers.Add(NewEntry);
			Entry = &MarkerProfile->Markers.Last();
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
	// The request carries the key layout's signs: the left key of each NumPad pair is positive (7 / 1 / 4 / 0).
	// In the viewer's frame those keys mean away / left / turn left / up, so Y and yaw flip into
	// NudgeVehicle's convention (Y+ right, yaw+ clockwise from above).
	NudgeVehicle(FVector(DeltaLocation.X, -DeltaLocation.Y, DeltaLocation.Z), -DeltaRotation.Yaw);
}

FVector UCXMRPlacementComponent::ResolveNudgePivot(const FTransform& Vehicle, const FVector& ViewerLocation) const
{
	switch (NudgePivot)
	{
	case ECXMRNudgePivot::Markers:
		if (DetectedCalib.Num() > 0)
		{
			FVector Sum = FVector::ZeroVector;
			for (const TPair<int32, FTransform>& Pair : DetectedCalib)
			{
				Sum += Pair.Value.GetLocation();
			}
			return Sum / DetectedCalib.Num();
		}
		return ViewerLocation;   // nothing seen yet
	case ECXMRNudgePivot::Viewer:
		return ViewerLocation;
	default:
		return Vehicle.GetLocation();
	}
}

void UCXMRPlacementComponent::RegisterTunables()
{
	const UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
	UCXMRTuningSubsystem* Tuning = GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
	if (!Tuning)
	{
		return;
	}

	const FText Category = NSLOCTEXT("CXMRPlacement", "CatVehicle", "Vehicle placement");
	auto Make = [this, &Category](FName Id, const FText& Label, ECXMRTunableKind Kind)
	{
		FCXMRTunable Tunable;
		Tunable.Id = Id;
		Tunable.Category = Category;
		Tunable.Label = Label;
		Tunable.Kind = Kind;
		Tunable.Owner = this;
		return Tunable;
	};

	{
		FCXMRTunable T = Make("Vehicle.Pose", NSLOCTEXT("CXMRPlacement", "Pose", "Vehicle (world)"), ECXMRTunableKind::Readout);
		T.Text = [this]
		{
			const AActor* Root = ResolveVehicleRoot();
			if (!Root)
			{
				return FText::GetEmpty();
			}
			const FVector L = Root->GetActorLocation();
			const FRotator R = Root->GetActorRotation();
			FString Text = FString::Printf(TEXT("X %.1f  Y %.1f  Z %.1f  Yaw %.1f"), L.X, L.Y, L.Z, R.Yaw);
			if (FMath::Abs(R.Pitch) > 0.5f || FMath::Abs(R.Roll) > 0.5f)
			{
				// Yaw alone hid an upside-down test car for weeks.
				Text += FString::Printf(TEXT("  TILTED P %.0f R %.0f"), R.Pitch, R.Roll);
			}
			return FText::FromString(Text);
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.Markers", NSLOCTEXT("CXMRPlacement", "Markers", "Calibration markers"), ECXMRTunableKind::Readout);
		T.Text = [this]
		{
			const bool bSettling = GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(SettleTimer);
			return FText::FromString(FString::Printf(TEXT("%d in view, %d used - %s"), SeenMarkers.Num(), DetectedCalib.Num(),
				bCalibrated ? TEXT("calibrated") : (bSettling ? TEXT("settling") : TEXT("not calibrated"))));
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.Steadiness", NSLOCTEXT("CXMRPlacement", "Steadiness", "Marker steadiness"), ECXMRTunableKind::Readout);
		T.Text = [this]
		{
			if (MarkerSamples.Num() == 0)
			{
				return NSLOCTEXT("CXMRPlacement", "NoSamples", "no marker sampled yet");
			}
			int32 Samples = 0;
			for (const TPair<int32, FMarkerSamples>& Pair : MarkerSamples)
			{
				Samples = FMath::Max(Samples, Pair.Value.Count);
			}
			return FText::FromString(FString::Printf(TEXT("%.1f mm wobble, %d samples averaged"), WorstScatter() * 10.0f, Samples));
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.LayoutFit", NSLOCTEXT("CXMRPlacement", "LayoutFit", "Fit to saved layout"), ECXMRTunableKind::Readout);
		T.Text = [this]
		{
			if (LayoutFitError < 0.0f)
			{
				return NSLOCTEXT("CXMRPlacement", "NoFit", "one marker - nothing to cross-check");
			}
			return FText::FromString(FString::Printf(TEXT("%.2f cm off the saved layout"), LayoutFitError));
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.MoveStep", NSLOCTEXT("CXMRPlacement", "MoveStep", "Move per press"), ECXMRTunableKind::Float);
		T.Unit = NSLOCTEXT("CXMRPlacement", "cm", "cm"); T.Min = 0.1f; T.Max = 100.0f; T.Delta = 0.5f; T.Default = 1.0f; T.bPersist = true;
		T.Get = [this] { return NudgeMoveStep; };
		T.Set = [this](float V) { NudgeMoveStep = V; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.YawStep", NSLOCTEXT("CXMRPlacement", "YawStep", "Turn per press"), ECXMRTunableKind::Float);
		T.Unit = NSLOCTEXT("CXMRPlacement", "deg", "deg"); T.Min = 0.1f; T.Max = 45.0f; T.Delta = 0.1f; T.Default = 1.0f; T.bPersist = true;
		T.Get = [this] { return NudgeYawStep; };
		T.Set = [this](float V) { NudgeYawStep = V; };
		Tuning->Register(MoveTemp(T));
	}
	// Steppers go through NudgeVehicle, so they move in the viewer's frame exactly like the NumPad keys.
	{
		FCXMRTunable T = Make("Vehicle.Away", NSLOCTEXT("CXMRPlacement", "Away", "Away from me (+) / toward (-)"), ECXMRTunableKind::Stepper);
		T.Step = [this](float Direction) { NudgeVehicle(FVector(Direction * NudgeMoveStep, 0.0, 0.0), 0.0f); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.Right", NSLOCTEXT("CXMRPlacement", "Right", "To my right (+) / left (-)"), ECXMRTunableKind::Stepper);
		T.Step = [this](float Direction) { NudgeVehicle(FVector(0.0, Direction * NudgeMoveStep, 0.0), 0.0f); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.Up", NSLOCTEXT("CXMRPlacement", "Up", "Up (+) / down (-)"), ECXMRTunableKind::Stepper);
		T.Step = [this](float Direction) { NudgeVehicle(FVector(0.0, 0.0, Direction * NudgeMoveStep), 0.0f); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.Turn", NSLOCTEXT("CXMRPlacement", "Turn", "Turn clockwise (+) / counter (-)"), ECXMRTunableKind::Stepper);
		T.Step = [this](float Direction) { NudgeVehicle(FVector::ZeroVector, Direction * NudgeYawStep); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.Pivot", NSLOCTEXT("CXMRPlacement", "Pivot", "Turn around"), ECXMRTunableKind::Choice);
		T.Options = {
			NSLOCTEXT("CXMRPlacement", "PivotMarkers", "Markers"),
			NSLOCTEXT("CXMRPlacement", "PivotViewer", "Viewer"),
			NSLOCTEXT("CXMRPlacement", "PivotOrigin", "Vehicle origin") };
		T.Get = [this] { return static_cast<float>(static_cast<uint8>(NudgePivot)); };
		T.Set = [this](float V) { NudgePivot = static_cast<ECXMRNudgePivot>(FMath::Clamp(FMath::RoundToInt(V), 0, 2)); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.KeepLevel", NSLOCTEXT("CXMRPlacement", "KeepLevel", "Keep level"), ECXMRTunableKind::Bool);
		T.Default = 1.0f;
		T.Get = [this] { return bKeepLevel ? 1.0f : 0.0f; };
		T.Set = [this](float V) { bKeepLevel = V > 0.5f; RecomputeCalibration(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.SaveOffset", NSLOCTEXT("CXMRPlacement", "SaveOffset", "Save adjustment into the marker layout"), ECXMRTunableKind::Action);
		T.Invoke = [this] { SaveMarkerOffsetToProfile(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.LearnMarkers", NSLOCTEXT("CXMRPlacement", "LearnMarkers", "Learn marker layout from this pose (and save)"), ECXMRTunableKind::Action);
		T.Invoke = [this] { LearnMarkerLayout(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.Recalibrate", NSLOCTEXT("CXMRPlacement", "Recalibrate", "Re-read markers"), ECXMRTunableKind::Action);
		T.Invoke = [this] { Recalibrate(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.ResetOffset", NSLOCTEXT("CXMRPlacement", "ResetOffset", "Discard unsaved adjustment"), ECXMRTunableKind::Action);
		T.Invoke = [this] { ResetMarkerOffset(); };
		Tuning->Register(MoveTemp(T));
	}
}

void UCXMRPlacementComponent::NudgeVehicle(FVector ViewerDelta, float YawDelta)
{
	AActor* Root = ResolveVehicleRoot();
	APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (!Root || !CamMgr)
	{
		return;
	}
	if (!bHaveBasePose)
	{
		BaseVehicleTransform = Root->GetActorTransform();
		bHaveBasePose = true;
	}

	// The viewer's level frame: forward is where the head points, flattened; up is the world's. The keys used
	// to move along the vehicle's own axes — backwards, because the offset was defined on the marker side — so
	// the same key went a different way depending on how the car happened to sit.
	const FRotator Heading(0.0f, CamMgr->GetCameraRotation().Yaw, 0.0f);
	const FVector WorldDelta = Heading.RotateVector(ViewerDelta);

	const FTransform Current = Root->GetActorTransform();
	const FVector Pivot = ResolveNudgePivot(Current, CamMgr->GetCameraLocation());

	// Turn about the pivot, then slide: T(-P) * R * T(P + delta), applied after the current pose.
	const FTransform Nudge = FTransform(-Pivot) * FTransform(FRotator(0.0f, YawDelta, 0.0f)) * FTransform(Pivot + WorldDelta);
	MoveVehicleTo(Current * Nudge);
}

void UCXMRPlacementComponent::MoveVehicleTo(FTransform VehicleWorld)
{
	AActor* Root = ResolveVehicleRoot();
	if (!Root)
	{
		return;
	}
	if (!bHaveBasePose)
	{
		BaseVehicleTransform = Root->GetActorTransform();
		bHaveBasePose = true;
	}
	VehicleWorld.SetScale3D(FVector::OneVector);

	// Store the result as the vehicle-frame offset the rest of this component already speaks
	// (ApplyPlacement: Vehicle = Adjust^-1 * Base  =>  Adjust = Base * Desired^-1), so saving the offset
	// and learning the marker layout keep working unchanged.
	const FTransform Adjust = BaseVehicleTransform * VehicleWorld.Inverse();
	TempMarkerLocationOffset = Adjust.GetLocation();
	TempMarkerRotationOffset = Adjust.Rotator();

	PublishOffset();
	RecomputeCalibration();
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
	AActor* Root = ResolveVehicleRoot();
	if (!Root || !MarkerProfile || DetectedCalib.Num() == 0)
	{
		UE_LOG(LogCXMRPlacement, Warning, TEXT("Cannot save marker offset: no vehicle, no profile, or no layout marker in view"));
		return;
	}

	if (TempMarkerLocationOffset.IsNearlyZero() && TempMarkerRotationOffset.IsNearlyZero())
	{
		UE_LOG(LogCXMRPlacement, Log, TEXT("Nothing to save: marker offset is zero"));
		return;
	}

	// What is on screen is what the next session has to reproduce.
	//  * Markers in view are recorded against that pose directly. Shifting their old offsets by the adjustment is only
	//    exact when the solve reproduces the markers exactly; a single marker on a tilted surface is levelled, so the
	//    saved car came back somewhere else.
	//  * Markers out of view get the same rigid shift, so the layout stays one piece. Updating only the markers in view
	//    mixed new offsets with old ones and skewed the fit whenever the next session saw a different set.
	const FTransform Shown = Root->GetActorTransform();
	const FTransform Adjust(TempMarkerRotationOffset, TempMarkerLocationOffset);
	int32 FromView = 0;
	int32 Shifted = 0;
	for (FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
	{
		if (Entry.Role != ECXMRMarkerRole::Calibration || Entry.MarkerId == 0)
		{
			continue;
		}
		if (const FTransform* InView = DetectedCalib.Find(Entry.MarkerId))
		{
			Entry.LocalOffset = InView->GetRelativeTransform(Shown);
			++FromView;
		}
		else
		{
			Entry.LocalOffset = Entry.LocalOffset * Adjust;
			++Shifted;
		}
	}

	UE_LOG(LogCXMRPlacement, Log,
		TEXT("Adjustment saved into the marker layout: %d marker(s) from view, %d shifted. Loc=(%.1f, %.1f, %.1f) cm, Yaw=%.1f deg"),
		FromView, Shifted, TempMarkerLocationOffset.X, TempMarkerLocationOffset.Y, TempMarkerLocationOffset.Z,
		TempMarkerRotationOffset.Yaw);

	// Saved/CXMR is where calibration survives: a cooked build cannot write its assets, and in the editor the asset is
	// handed back unmodified when play ends (so it is no longer marked dirty here).
	SaveCalibrationToDisk();

	// The pose is now baked into the layout; clearing the temporary offset leaves the vehicle where it is.
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
