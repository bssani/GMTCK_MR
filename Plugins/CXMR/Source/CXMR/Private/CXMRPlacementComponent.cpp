// Copyright GMTCK CX.

#include "CXMRPlacementComponent.h"
#include "CXMRSubsystem.h"
#include "CXMRMarkerProfile.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRTuningWindowComponent.h"
#include "EngineUtils.h"
#include "CXMRLevelFit.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Kismet/GameplayStatics.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Pawn.h"

#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "UObject/UObjectIterator.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRPlacement, Log, All);

namespace
{
	/** 월드의 배치 컴포넌트에 콘솔 명령 전달함. */
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
		// 마커 이벤트와 배치 요청은 Subsystem에서 받음.
		Subsystem->OnMarkerDetected.AddDynamic(this, &UCXMRPlacementComponent::HandleMarkerDetected);
		Subsystem->OnMarkerMoved.AddDynamic(this, &UCXMRPlacementComponent::HandleMarkerMoved);
		Subsystem->OnMarkerLost.AddDynamic(this, &UCXMRPlacementComponent::HandleMarkerLost);
		Subsystem->OnMarkerTrackingChanged.AddDynamic(this, &UCXMRPlacementComponent::HandleMarkerTrackingChanged);
		Subsystem->OnRecalibrateRequested.AddDynamic(this, &UCXMRPlacementComponent::HandleRecalibrateRequest);
		Subsystem->OnPlaceRequested.AddDynamic(this, &UCXMRPlacementComponent::HandlePlaceRequest);
		Subsystem->OnAdjustMarkerOffsetRequested.AddDynamic(this, &UCXMRPlacementComponent::HandleAdjustOffsetRequest);
		Subsystem->OnSaveMarkerOffsetRequested.AddDynamic(this, &UCXMRPlacementComponent::HandleSaveOffsetRequest);
		Subsystem->OnResetMarkerOffsetRequested.AddDynamic(this, &UCXMRPlacementComponent::HandleResetOffsetRequest);
		PublishOffset();
	}

	// 원본 값을 보관한 뒤 현장 보정 파일을 적용함.

	// 프로필이 먼저 바뀌어도 중복 적용하지 않음.
	EnsureCalibrationLoaded();

	RegisterTunables();

	// 저장한 배치를 쓰려면 마커 추적이 필요함.
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
		Subsystem->OnMarkerLost.RemoveDynamic(this, &UCXMRPlacementComponent::HandleMarkerLost);
		Subsystem->OnMarkerTrackingChanged.RemoveDynamic(this, &UCXMRPlacementComponent::HandleMarkerTrackingChanged);
		Subsystem->OnRecalibrateRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandleRecalibrateRequest);
		Subsystem->OnPlaceRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandlePlaceRequest);
		Subsystem->OnAdjustMarkerOffsetRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandleAdjustOffsetRequest);
		Subsystem->OnSaveMarkerOffsetRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandleSaveOffsetRequest);
		Subsystem->OnResetMarkerOffsetRequested.RemoveDynamic(this, &UCXMRPlacementComponent::HandleResetOffsetRequest);
	}

	// 종료 시 원본 값을 복원함. 현장 보정은 파일에만 남김.

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

bool UCXMRPlacementComponent::IsMarkerObservationCurrent(int32 MarkerId) const
{
	const double* LastObserved = MarkerLastObserved.Find(MarkerId);
	const UWorld* World = GetWorld();
	if (!LastObserved || !World || (Subsystem && !Subsystem->IsMarkerTrackingOn()))
	{
		return false;
	}
	const double Age = World->GetRealTimeSeconds() - *LastObserved;
	return Age >= 0.0 && Age <= FMath::Max(0.01f, MarkerObservationMaxAge);
}

int32 UCXMRPlacementComponent::CurrentMarkerCount() const
{
	int32 Count = 0;
	for (const TPair<int32, FTransform>& Seen : SeenMarkers)
	{
		Count += IsMarkerObservationCurrent(Seen.Key) ? 1 : 0;
	}
	return Count;
}

void UCXMRPlacementComponent::HandleMarkerLost(int32 MarkerId)
{
	MarkerLastObserved.Remove(MarkerId);
	const bool bWasCalibrationMarker = DetectedCalib.Remove(MarkerId) > 0;
	MarkerSamples.Remove(MarkerId);
	// 재관측 시 이전 평균을 버림. 안 보이는 동안 옮겨졌을 수 있음.
	if (bWasCalibrationMarker && !bCalibrated)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SettleTimer);
		}
	}
}

void UCXMRPlacementComponent::HandleMarkerTrackingChanged(bool bEnabled)
{
	if (!bEnabled)
	{
		CancelAlignmentCapture();
		MarkerLastObserved.Reset();
		DetectedCalib.Reset();
		MarkerSamples.Reset();
		ConfiguredMarkerIds.Reset();
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SettleTimer);
		}
	}
}

void UCXMRPlacementComponent::HandleMarkerPose(int32 MarkerId, const FVector& Position, const FRotator& Rotation, bool bIsFirstSighting)
{
	if (Mode != ECXMRPlacementMode::MarkerAnchor || !MarkerProfile || MarkerId == 0 || Position.ContainsNaN() || Rotation.ContainsNaN())
	{
		return;
	}
	if (!IsMarkerObservationCurrent(MarkerId))
	{
		MarkerSamples.Remove(MarkerId);
		DetectedCalib.Remove(MarkerId);
	}
	if (const UWorld* World = GetWorld())
	{
		MarkerLastObserved.Add(MarkerId, World->GetRealTimeSeconds());
	}

	// 미등록 마커도 보관함. 학습에는 보정 중 누적한 평균 좌표를 사용함.

	const bool bAveraging = bAverageMarkerSamples && !bCalibrated && !bAlignmentPoseLocked;
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
		return; // 미등록 마커는 학습 때 추가함. 동적 마커는 다른 컴포넌트에서 처리함.
	}

	// 프로필이 바뀌어도 플러그인이 아는 ID는 Moved로 들어옴.
	if (Subsystem && !ConfiguredMarkerIds.Contains(MarkerId))
	{
		const bool bTimeoutSet = Subsystem->SetMarkerTimeout(MarkerId, Entry.Timeout);
		const bool bModeSet = Subsystem->SetMarkerTrackingMode(MarkerId, Entry.TrackingMode);
		if (bTimeoutSet && bModeSet)
		{
			ConfiguredMarkerIds.Add(MarkerId);
		}
	}

	if (bCapturingAlignment)
	{
		if (!IsCapturePoseCurrent()) { CancelAlignmentCapture(); }
		else if (CaptureMarkerIds.Contains(MarkerId))
		{
			CapturedAlignmentMarkers.Add(MarkerId, Pose.GetRelativeTransform(CapturePose));
		}
	}
	if (bAlignmentPoseLocked) { return; }
	if (!SavedAlignmentMarkers.IsEmpty() && !SavedAlignmentMarkers.Contains(MarkerId)) { return; }
	if (bFreezeAfterCalibration && bCalibrated)
	{
		return; // 보정 후 위치 고정함.
	}

	// 보정 중에는 모든 표본을 평균에 반영함.

	// 마커를 따라갈 때만 이동 임계값을 적용해서 흔들림을 줄임.

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

/** 높이와 위치는 유지하고 수평 축에서 yaw를 구함. */
static FTransform LevelTransform(const FTransform& In)
{
	const FQuat Rotation = In.GetRotation();
	FVector Heading = Rotation.GetForwardVector();
	Heading.Z = 0.0;
	if (!Heading.Normalize())
	{
		FVector Right = Rotation.GetRightVector();
		Right.Z = 0.0;
		Heading = Right.GetSafeNormal() ^ FVector::UpVector;   // 오른쪽 축과 위 축으로 앞 방향 구함.
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

	// 표본의 평균 주변 분산으로 추적 흔들림을 표시함.

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

bool UCXMRPlacementComponent::ComputeCalibrationPose(FTransform& VehicleWorld, bool& bOutReady)
{
	bOutReady = false;
	for (auto It = DetectedCalib.CreateIterator(); It; ++It)
	{
		FCXMRMarkerEntry Entry;
		if (!IsMarkerObservationCurrent(It.Key()) || !MarkerProfile ||
			!MarkerProfile->FindEntry(It.Key(), Entry) || Entry.Role != ECXMRMarkerRole::Calibration)
		{
			It.RemoveCurrent();
		}
	}

	bool bHavePose = false;
	bool bMultiMarkerSolved = false;
	if (!SavedAlignmentMarkers.IsEmpty()) { return ComputeSavedAlignmentPose(VehicleWorld, bOutReady); }

	// 다중 마커는 위치로 계산하고 단일 마커는 방향까지 사용함.
	LayoutFitError = -1.0f;
	if (DetectedCalib.Num() >= 2)
	{
		bMultiMarkerSolved = ComputeMultiMarkerTransform(VehicleWorld, LayoutFitError);
		bHavePose = bMultiMarkerSolved;
	}
	if (!bHavePose && DetectedCalib.Num() > 0 && MarkerProfile)
	{
		const TPair<int32, FTransform>& First = *DetectedCalib.CreateConstIterator();
		FCXMRMarkerEntry Entry;
		if (MarkerProfile->FindEntry(First.Key, Entry))
		{
			// UE는 A*B에서 A를 먼저 적용함. VehicleWorld = LocalOffset^-1 * MarkerWorld.
			VehicleWorld = Entry.LocalOffset.Inverse() * First.Value;
			if (bKeepLevel)
			{
				// 단일 마커도 수평을 유지해서 마커가 추가될 때 기울기가 바뀌지 않게 함.

				VehicleWorld = LevelTransform(VehicleWorld);
			}
			bHavePose = true;
		}
	}

	const bool bFitAcceptable = bMultiMarkerSolved && FMath::IsFinite(LayoutFitError) &&
		LayoutFitError <= FMath::Max(0.f, MaxCalibrationFitError);
	bOutReady = bHavePose && !VehicleWorld.ContainsNaN() &&
		DetectedCalib.Num() >= FMath::Max(1, MinMarkersToCalibrate) &&
		(MinMarkersToCalibrate <= 1 || bFitAcceptable);
	return bHavePose && !VehicleWorld.ContainsNaN();
}

void UCXMRPlacementComponent::RecomputeCalibration()
{
	if (bAlignmentPoseLocked) { return; }
	AActor* Root = ResolveVehicleRoot();
	if (!Root)
	{
		return;
	}
	FTransform VehicleWorld;
	bool bReady = false;
	const bool bHavePose = ComputeCalibrationPose(VehicleWorld, bReady);

	// 단일·다중 마커 모두 같은 경로에서 수동 오프셋을 적용함.

	if (bHavePose)
	{
		BaseVehicleTransform = VehicleWorld;
		bHaveBasePose = true;
	}
	else if (!bHaveBasePose)
	{
		// 마커와 기준 위치가 없으면 현재 차량 위치를 기준으로 조정함.
		BaseVehicleTransform = Root->GetActorTransform();
		bHaveBasePose = true;
	}

	ApplyPlacement();

	// 위치 고정은 이 컴포넌트에만 적용함. 전체 추적을 끄면 동적 마커도 멈춤.

	if (bReady && !bCalibrated)
	{
		UWorld* World = GetWorld();
		if (CalibrationSettleSeconds <= 0.0f || !World)
		{
			FinishCalibration();
		}
		else if (!World->GetTimerManager().IsTimerActive(SettleTimer))
		{
			// 첫 관측의 흔들림을 줄이려고 잠시 표본을 더 받음.
			World->GetTimerManager().SetTimer(SettleTimer, this, &UCXMRPlacementComponent::FinishCalibration, CalibrationSettleSeconds, false);
		}
	}
	else if (!bReady)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(SettleTimer);
		}
	}
}

void UCXMRPlacementComponent::FinishCalibration()
{
	if (bCalibrated)
	{
		return;
	}
	FTransform VehicleWorld;
	bool bReady = false;
	if (!ComputeCalibrationPose(VehicleWorld, bReady) || !bReady)
	{
		return;
	}
	// 타이머 종료 시 관측 수와 계산 품질을 다시 확인함.
	BaseVehicleTransform = VehicleWorld;
	bHaveBasePose = true;
	ApplyPlacement();
	bRestoreNeedsConfirmation = !SavedAlignmentMarkers.IsEmpty() && DetectedCalib.Num() < 2;
	bCalibrated = !bRestoreNeedsConfirmation;
	if (!SavedAlignmentMarkers.IsEmpty()) { bAlignmentPoseLocked = true; }
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

	// XR 세션이 준비된 뒤 추적을 시작함.

	if (Subsystem->IsMarkerTrackingSupported() && Subsystem->SetMarkerTracking(true))
	{
		UE_LOG(LogCXMRPlacement, Log, TEXT("Marker tracking started for calibration."));
		return;
	}

	const int32 MaxAttempts = 40;   // 0.5초 간격으로 최대 20초 재시도함.
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
	// 마커 위치로 차량의 이동과 yaw를 계산함. 마커 방향은 사용하지 않음.

	// OutResidual은 저장된 배치와 관측 좌표의 RMS 오차(cm).

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
	return CXMRLevelFit::Solve(P, Q, Out, OutResidual);
}

void UCXMRPlacementComponent::Recalibrate()
{
	CancelAlignmentCapture();
	bManualAlignment = false;
	bAlignmentPoseLocked = false;
	bRestoreNeedsConfirmation = false;
	bCalibrated = false;
	DetectedCalib.Reset();
	SeenMarkers.Reset();
	MarkerLastObserved.Reset();
	ConfiguredMarkerIds.Reset();
	MarkerSamples.Reset();
	LayoutFitError = -1.0f;
	// 다시 보정하면 기존 조정 방향도 비움.

	bHaveNudgeHeading = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettleTimer);
	}

	// 추적을 껐다 켜서 플러그인의 감지 이력을 초기화함.
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

	// 카메라의 수평 시선 방향 사용함.
	FRotator LookRot = CamMgr->GetCameraRotation();
	LookRot.Pitch = 0.0f;
	LookRot.Roll = 0.0f;
	const FVector Forward = LookRot.Vector();

	// 사용자 앞에 놓고 높이는 pawn의 바닥 기준을 사용함.
	FVector Target = CamMgr->GetCameraLocation() + Forward * PawnRelativeDistance;
	Target.Z = Pawn->GetActorLocation().Z;

	// 사용자를 바라보도록 yaw를 180도 돌림.
	const FRotator FaceRot(0.0f, LookRot.Yaw + 180.0f, 0.0f);

	// 수동 배치를 새 기준으로 사용함. 다음 조정 때 옛 마커 위치로 돌아가지 않게 함.

	BaseVehicleTransform = FTransform(FaceRot, Target, FVector::OneVector);
	CancelAlignmentCapture();
	bAlignmentPoseLocked = true;
	bManualAlignment = true;
	bRestoreNeedsConfirmation = false;
	bHaveBasePose = true;
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	PublishOffset();   // 버린 오프셋을 패널에도 반영함.
	ApplyPlacement();

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettleTimer);
	}
	bCalibrated = true;
}

// 현장 보정

void UCXMRPlacementComponent::EnsureCalibrationLoaded()
{
	if (bCalibrationLoaded && CapturedProfile == MarkerProfile)
	{
		return;
	}

	// 프로필 교체 전 원본 값을 복원함.

	RestoreAuthoredOffsets();

	CaptureAuthoredOffsets();
	LoadCalibrationFromDisk();
	bCalibrationLoaded = true;
}

void UCXMRPlacementComponent::SetMarkerProfile(UCXMRMarkerProfile* NewProfile, bool bForceReload)
{
	if (MarkerProfile == NewProfile && !bForceReload)
	{
		return;
	}
	// 같은 저장 레이아웃으로 차량만 바꾸면 확인한 앵커 유지함.
	const bool bKeepSharedPose = !LoadedAlignmentGroup.IsNone() && LoadedAlignmentGroup == GetAlignmentGroup()
		&& bCalibrated && !bManualAlignment && bAlignmentPoseLocked && HasSavedAlignment();
	TMap<int32, FTransform> PreviousSharedOffsets;
	if (bKeepSharedPose && MarkerProfile)
	{
		for (const FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
		{
			if (SavedAlignmentMarkers.Contains(Entry.MarkerId)) { PreviousSharedOffsets.Add(Entry.MarkerId, Entry.LocalOffset); }
		}
	}
	CancelAlignmentCapture();
	bManualAlignment = false;
	bAlignmentPoseLocked = false;
	bRestoreNeedsConfirmation = false;
	SavedAlignmentMarkers.Reset();
	SavedPrimaryMarker = 0;
	LoadedAlignmentGroup = NAME_None;
	if (bForceReload) { bCalibrationLoaded = false; }
	MarkerProfile = NewProfile;
	bHaveNudgeHeading = false;   // 차량별 조정 방향 초기화함.
	EnsureCalibrationLoaded();

	// 이전 프로필의 관측과 보정 상태를 버림.

	DetectedCalib.Reset();
	bCalibrated = false;
	SeenMarkers.Reset();
	MarkerLastObserved.Reset();
	ConfiguredMarkerIds.Reset();
	MarkerSamples.Reset();
	LayoutFitError = -1.0f;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettleTimer);
	}
	// 차량 위치는 유지하고 이전 미세조정 값만 비움.
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	bHaveBasePose = false;
	RebaseToCurrentTransform();
	if (!bHaveBasePose)
	{
		PublishOffset();
	}
	if (bKeepSharedPose && LoadedAlignmentGroup == GetAlignmentGroup() && HasSavedAlignment()
		&& PreviousSharedOffsets.Num() == SavedAlignmentMarkers.Num())
	{
		bool bSameLayout = true;
		for (int32 Id : SavedAlignmentMarkers)
		{
			FCXMRMarkerEntry Entry;
			const FTransform* Previous = PreviousSharedOffsets.Find(Id);
			if (!Previous || !MarkerProfile->FindEntry(Id, Entry) || !Previous->Equals(Entry.LocalOffset, 0.001f)) { bSameLayout = false; break; }
		}
		if (bSameLayout) { bCalibrated = true; bAlignmentPoseLocked = true; }
	}
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

	// 학습이나 파일에서 추가한 마커는 원본에 남기지 않음.
	Profile->Markers.RemoveAll([this](const FCXMRMarkerEntry& Entry) { return !AuthoredOffsets.Contains(Entry.MarkerId); });

	for (FCXMRMarkerEntry& Entry : Profile->Markers)
	{
		if (const FTransform* Authored = AuthoredOffsets.Find(Entry.MarkerId))
		{
			Entry.LocalOffset = *Authored;
		}
	}
}

FName UCXMRPlacementComponent::GetAlignmentGroup() const
{
	const UCXMRVehicleLoaderComponent* Loader = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	return Loader && Loader->Profile ? Loader->Profile->AlignmentGroup : NAME_None;
}

FString UCXMRPlacementComponent::GetCalibrationFilePath() const
{
	if (!MarkerProfile)
	{
		return FString();
	}
	if (!GetAlignmentGroup().IsNone())
	{
		const FTCHARToUTF8 GroupName(*GetAlignmentGroup().ToString().ToLower());
		const FString Key = FMD5::HashBytes(reinterpret_cast<const uint8*>(GroupName.Get()), GroupName.Length());
		return FPaths::ProjectSavedDir() / TEXT("CXMR") / FString::Printf(TEXT("AlignmentGroup_%s.json"), *Key);
	}
	const UCXMRVehicleLoaderComponent* Loader = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	FString VehicleKey;
	if (Loader && Loader->Profile)
	{
		const FTCHARToUTF8 AssetPath(*Loader->Profile->GetPathName());
		VehicleKey = TEXT("_") + FMD5::HashBytes(reinterpret_cast<const uint8*>(AssetPath.Get()), AssetPath.Length());
	}
	return FPaths::ProjectSavedDir() / TEXT("CXMR")
		/ FString::Printf(TEXT("MarkerCalib_%s%s.json"), *MarkerProfile->GetCalibrationId(), *VehicleKey);
}

void UCXMRPlacementComponent::LearnMarkerLayout()
{
	if (!GetAlignmentGroup().IsNone())
	{
		// 그룹 정렬은 일부 마커 학습 대신 전체 확인·캡처로 저장함.
		bLastCalibrationSaveSucceeded = false;
		AlignmentStorageMessage = NSLOCTEXT("CXMRAlignmentStorage", "GroupLearnDisabled", "Shared alignment uses Confirm Alignment and Save Alignment. Individual marker learning is disabled for groups.");
		AlignmentSaveMessage = AlignmentStorageMessage;
		ReportAlignmentSaveMessage();
		return;
	}
	AActor* Root = ResolveVehicleRoot();
	if (!Root || !MarkerProfile || SeenMarkers.Num() == 0)
	{
		UE_LOG(LogCXMRPlacement, Warning,
			TEXT("Cannot learn marker layout: need a vehicle, a profile and at least one marker in view "
			     "(vehicle=%s profile=%s markers seen=%d)."),
			Root ? TEXT("ok") : TEXT("MISSING"), MarkerProfile ? TEXT("ok") : TEXT("MISSING"), SeenMarkers.Num());
		return;
	}

	// 현재 차량 위치를 기준으로 마커 배치를 기록함.

	const FTransform VehicleWorld = Root->GetActorTransform();

	int32 Updated = 0;
	int32 Added = 0;
	TMap<int32, FTransform> LearnedMarkers;
	for (const TPair<int32, FTransform>& Seen : SeenMarkers)
	{
		if (!IsMarkerObservationCurrent(Seen.Key))
		{
			continue;
		}
		// LocalOffset * VehicleWorld = MarkerWorld.
		const FTransform Local = Seen.Value.GetRelativeTransform(VehicleWorld);
		if (FCXMRMarkerEntry* Entry = MarkerProfile->GetEntryMutable(Seen.Key))
		{
			if (Entry->Role != ECXMRMarkerRole::Calibration)
			{
				continue;   // 동적 마커는 차량 배치에 사용하지 않음.
			}
			Entry->LocalOffset = Local;
			++Updated;
		}
		else
		{
			// 학습한 미등록 마커는 프로필과 저장 파일에 추가함.

			FCXMRMarkerEntry NewEntry;
			NewEntry.MarkerId = Seen.Key;
			NewEntry.Role = ECXMRMarkerRole::Calibration;
			NewEntry.LocalOffset = Local;
			NewEntry.Label = FName(*FString::Printf(TEXT("Learned_%d"), Seen.Key));
			MarkerProfile->Markers.Add(NewEntry);
			++Added;
		}
		LearnedMarkers.Add(Seen.Key, Seen.Value);
	}

	// 안 보이는 마커는 이전 보정값을 유지하므로 따로 알림.

	for (const FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
	{
		if (Entry.Role == ECXMRMarkerRole::Calibration && Entry.MarkerId != 0 && !LearnedMarkers.Contains(Entry.MarkerId))
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

	// 학습한 배치가 현재 위치를 재현하므로 임시 오프셋을 비움.
	DetectedCalib = MoveTemp(LearnedMarkers);
	MarkerSamples.Reset();
	bCalibrated = false;
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SettleTimer);
	}
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	BaseVehicleTransform = VehicleWorld;
	bHaveBasePose = true;
	PublishOffset();

	UE_LOG(LogCXMRPlacement, Log, TEXT("Learned marker layout from the current vehicle pose: %d updated, %d added."), Updated, Added);

	// 학습 결과로 다시 배치해서 차량 위치가 유지되는지 확인함.

	RecomputeCalibration();

	SaveCalibrationToDisk();
}

void UCXMRPlacementComponent::ResetCalibrationToAuthored()
{
	TryResetCalibrationToAuthored();
}

bool UCXMRPlacementComponent::TryResetCalibrationToAuthored()
{
	const FString Path = GetCalibrationFilePath();
	const bool bHasFile = !Path.IsEmpty() && FPaths::FileExists(Path);
	// 삭제 성공 전에는 캡처와 런타임 정렬을 버리지 않음.
	if (Path.IsEmpty() || (bHasFile && !IFileManager::Get().Delete(*Path)))
	{
		bLastCalibrationSaveSucceeded = false;
		AlignmentStorageMessage = NSLOCTEXT("CXMRAlignmentStorage", "ResetFailed", "Reset failed. Previous alignment kept. Check the save file's access permissions, then retry.");
		AlignmentSaveMessage = AlignmentStorageMessage;
		ReportAlignmentSaveMessage();
		return false;
	}
	CancelAlignmentCapture();
	bManualAlignment = false;
	bAlignmentPoseLocked = false;
	bRestoreNeedsConfirmation = false;
	SavedAlignmentMarkers.Reset();
	SavedPrimaryMarker = 0;
	LoadedAlignmentGroup = NAME_None;
	AlignmentStorageMessage = FText::GetEmpty();
	if (bHasFile)
	{
		UE_LOG(LogCXMRPlacement, Log, TEXT("Deleted saved calibration: %s"), *Path);
	}

	RestoreAuthoredOffsets();
	// 차량별 원본 저장으로 이전 공용 파일이 다시 적용되지 않게 함.
	const UCXMRVehicleLoaderComponent* Loader = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	if (Loader && Loader->Profile && MarkerProfile && GetAlignmentGroup().IsNone()) { SaveCalibrationToDisk(); }
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;
	PublishOffset();
	RecomputeCalibration();

	UE_LOG(LogCXMRPlacement, Log, TEXT("Marker calibration reset to the values the profile shipped with."));
	return true;
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
	// 넘버패드 부호를 조정 좌표계에 맞춤. Y와 yaw는 반전함.

	NudgeVehicle(FVector(DeltaLocation.X, -DeltaLocation.Y, DeltaLocation.Z), -DeltaRotation.Yaw);
}

FVector UCXMRPlacementComponent::ResolveNudgePivot(const FTransform& Vehicle, const FVector& ViewerLocation) const
{
	switch (NudgePivot)
	{
	case ECXMRNudgePivot::DriverEyeReference:
	{
		FTransform Eye;
		return GetDriverEyeWorld(Eye) ? Eye.GetLocation() : ViewerLocation;
	}
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
		return ViewerLocation;   // 아직 관측된 마커 없음.
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
				// yaw뿐 아니라 전체 회전을 표시함.
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
			return FText::FromString(FString::Printf(TEXT("%d in view, %d used - %s"), CurrentMarkerCount(), DetectedCalib.Num(),
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
			if (bManualAlignment) { return NSLOCTEXT("CXMRPlacement", "ManualFit", "Manual alignment - check against the physical vehicle"); }
			if (LayoutFitError < 0.0f)
			{
				return DetectedCalib.IsEmpty() ? NSLOCTEXT("CXMRPlacement", "NoObservations", "Waiting for marker observations")
					: NSLOCTEXT("CXMRPlacement", "NoFit", "One marker - cross-check unavailable");
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
	// 버튼과 넘버패드 모두 같은 조정 방향 사용함.
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
		FCXMRTunable T = Make("Vehicle.NudgeFrame", NSLOCTEXT("CXMRPlacement", "NudgeFrame", "Keys move along"), ECXMRTunableKind::Choice);
		T.Options = {
			NSLOCTEXT("CXMRPlacement", "FrameLatched", "Where I first faced"),
			NSLOCTEXT("CXMRPlacement", "FrameLive", "Where I look now"),
			NSLOCTEXT("CXMRPlacement", "FrameVehicle", "The car's own axes") };
		T.bPersist = true;
		T.Get = [this] { return static_cast<float>(static_cast<uint8>(NudgeFrame)); };
		T.Set = [this](float V)
		{
			NudgeFrame = static_cast<ECXMRNudgeFrame>(FMath::Clamp(FMath::RoundToInt(V), 0, 2));
			bHaveNudgeHeading = false;   // 다음 조정에서 방향을 다시 잡음.
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.NudgeHeading", NSLOCTEXT("CXMRPlacement", "NudgeHeading", "\"Away from me\" is"), ECXMRTunableKind::Readout);
		T.Text = [this]
		{
			const APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
			if (!CamMgr)
			{
				return NSLOCTEXT("CXMRPlacement", "HeadingNoCam", "no viewer yet");
			}
			const float ViewerYaw = CamMgr->GetCameraRotation().Yaw;
			if (NudgeFrame == ECXMRNudgeFrame::ViewerLive)
			{
				return NSLOCTEXT("CXMRPlacement", "HeadingLive", "wherever you look - turning your head turns the axes");
			}
			// 조정 방향과 현재 시선의 차이를 표시함.
			const float Off = FMath::Abs(FMath::FindDeltaAngleDegrees(ViewerYaw, ResolveNudgeYaw(ViewerYaw)));
			if (NudgeFrame == ECXMRNudgeFrame::Vehicle)
			{
				return FText::FromString(FString::Printf(TEXT("the car's front, %.0f\u00B0 off your view"), Off));
			}
			return bHaveNudgeHeading
				? FText::FromString(FString::Printf(TEXT("held, %.0f\u00B0 off your view"), Off))
				: NSLOCTEXT("CXMRPlacement", "HeadingUnset", "not held yet - the next press takes it");
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.RetakeHeading", NSLOCTEXT("CXMRPlacement", "RetakeHeading", "Use the way I face now"), ECXMRTunableKind::Action);
		T.Invoke = [this] { RetakeNudgeHeading(); };
		T.IsEnabled = [this] { return NudgeFrame == ECXMRNudgeFrame::ViewerLatched; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Vehicle.Pivot", NSLOCTEXT("CXMRPlacement", "Pivot", "Turn around"), ECXMRTunableKind::Choice);
		T.Options = {
			NSLOCTEXT("CXMRPlacement", "PivotMarkers", "Markers"),
			NSLOCTEXT("CXMRPlacement", "PivotViewer", "Viewer"),
			NSLOCTEXT("CXMRPlacement", "PivotOrigin", "Vehicle origin"),
			NSLOCTEXT("CXMRPlacement", "PivotEye", "Driver eye reference") };
		T.Get = [this] { return static_cast<float>(static_cast<uint8>(NudgePivot)); };
		T.Set = [this](float V) { NudgePivot = static_cast<ECXMRNudgePivot>(FMath::Clamp(FMath::RoundToInt(V), 0, 3)); };
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

float UCXMRPlacementComponent::ResolveNudgeYaw(float ViewerYaw) const
{
	if (NudgeFrame == ECXMRNudgeFrame::Vehicle)
	{
		// 차량 방향은 수평으로 펴서 사용함.
		if (const AActor* Root = VehicleRoot ? ToRawPtr(VehicleRoot) : GetOwner())
		{
			return LevelTransform(Root->GetActorTransform()).GetRotation().Rotator().Yaw;
		}
		return ViewerYaw;
	}
	if (NudgeFrame == ECXMRNudgeFrame::ViewerLatched && bHaveNudgeHeading)
	{
		return LatchedNudgeYaw;
	}
	return ViewerYaw;
}

void UCXMRPlacementComponent::LatchNudgeHeading(float HeadingYaw)
{
	LatchedNudgeYaw = HeadingYaw;
	bHaveNudgeHeading = true;
	UE_LOG(LogCXMRPlacement, Log, TEXT("Adjust keys now treat yaw %.1f as 'away from me'"), HeadingYaw);
}

void UCXMRPlacementComponent::RetakeNudgeHeading()
{
	if (const APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0))
	{
		LatchNudgeHeading(CamMgr->GetCameraRotation().Yaw);
	}
}

void UCXMRPlacementComponent::NudgeVehicle(FVector ViewerDelta, float YawDelta)
{
	const APlayerCameraManager* CamMgr = UGameplayStatics::GetPlayerCameraManager(GetWorld(), 0);
	if (!CamMgr)
	{
		return;
	}
	const float ViewerYaw = CamMgr->GetCameraRotation().Yaw;
	NudgeVehicleInFrame(ViewerDelta, YawDelta, ViewerYaw, CamMgr->GetCameraLocation());
}

void UCXMRPlacementComponent::NudgeVehicleInFrame(FVector ViewerDelta, float YawDelta, float HeadingYaw, FVector ViewerLocation)
{
	AActor* Root = ResolveVehicleRoot();
	if (!Root)
	{
		return;
	}
	// 첫 조정 때 방향 고정함.
	if (NudgeFrame == ECXMRNudgeFrame::ViewerLatched && !bHaveNudgeHeading)
	{
		LatchNudgeHeading(HeadingYaw);
	}
	if (!bHaveBasePose)
	{
		BaseVehicleTransform = Root->GetActorTransform();
		bHaveBasePose = true;
	}

	// 앞 방향은 조정 기준, 위 방향은 월드 기준 사용함.

	const FRotator Heading(0.0f, ResolveNudgeYaw(HeadingYaw), 0.0f);
	const FVector WorldDelta = Heading.RotateVector(ViewerDelta);

	const FTransform Current = Root->GetActorTransform();
	FTransform DriverEye;
	if (Mode == ECXMRPlacementMode::MarkerAnchor && GetDriverEyeWorld(DriverEye))
	{
		NudgePivot = ECXMRNudgePivot::DriverEyeReference;
	}
	const FVector Pivot = ResolveNudgePivot(Current, ViewerLocation);

	// 현재 위치에서 T(-P) * R * T(P + delta) 순서로 회전 후 이동함.
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
	CancelAlignmentCapture();
	bManualAlignment = true;
	bAlignmentPoseLocked = true;
	bRestoreNeedsConfirmation = false;
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(SettleTimer); }

	// 저장과 학습에 쓰도록 결과를 차량 기준 오프셋으로 변환함. Adjust = Base * Desired^-1.

	const FTransform Adjust = BaseVehicleTransform * VehicleWorld.Inverse();
	TempMarkerLocationOffset = Adjust.GetLocation();
	TempMarkerRotationOffset = Adjust.Rotator();

	PublishOffset();
	// 현재 기준에 수동 오프셋만 적용함.
	ApplyPlacement();
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

	// 차량 기준 오프셋을 먼저 적용함. 마커 +X 조정은 차량 -X 이동임.

	const FTransform Adjust(TempMarkerRotationOffset, TempMarkerLocationOffset);
	Root->SetActorTransform(Adjust.Inverse() * BaseVehicleTransform);
}

void UCXMRPlacementComponent::AdjustMarkerOffset(FVector DeltaLocation, FRotator DeltaRotation)
{
	if (!bHaveBasePose)
	{
		RebaseToCurrentTransform();
	}
	if (!GetAlignmentGroup().IsNone() && (!DeltaLocation.IsNearlyZero() || !DeltaRotation.IsNearlyZero()))
	{
		CancelAlignmentCapture();
		bManualAlignment = true;
		bAlignmentPoseLocked = true;
		bRestoreNeedsConfirmation = false;
		if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(SettleTimer); }
	}
	TempMarkerLocationOffset += DeltaLocation;
	TempMarkerRotationOffset += DeltaRotation;

	UE_LOG(LogCXMRPlacement, Log,
		TEXT("Marker offset adjusted: Loc=(%.1f, %.1f, %.1f) cm, Rot=(%.1f, %.1f, %.1f) deg"),
		TempMarkerLocationOffset.X, TempMarkerLocationOffset.Y, TempMarkerLocationOffset.Z,
		TempMarkerRotationOffset.Pitch, TempMarkerRotationOffset.Yaw, TempMarkerRotationOffset.Roll);

	// 현재 기준에 조정값만 적용함.
	PublishOffset();
	ApplyPlacement();
}

void UCXMRPlacementComponent::SaveMarkerOffsetToProfile()
{
	if (bManualAlignment || bCapturingAlignment || (!GetAlignmentGroup().IsNone() && !HasSavedAlignment()))
	{
		// 패널 상태도 단축키와 같은 저장 절차로 갱신함.
		if (GetWorld())
		{
			for (TActorIterator<AActor> It(GetWorld()); It; ++It)
			{
				if (It->IsActorBeingDestroyed()) { continue; }
				UCXMRTuningWindowComponent* Control = It->FindComponentByClass<UCXMRTuningWindowComponent>();
				if (Control && Control->IsRegistered() && Control->FindPlacement() == this)
				{
					Control->RequestAlignmentSave();
					return;
				}
			}
		}
		RequestAlignmentSave();
		return;
	}
	bLastCalibrationSaveSucceeded = false;
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

	// 다음 실행에서도 현재 차량 위치를 재현해야 함.
	// 보이는 마커는 현재 차량 위치에서 다시 기록함.

	// 안 보이는 마커도 같은 변환으로 옮겨서 배치를 유지함.

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

	SaveCalibrationToDisk();
	// 저장한 위치를 새 기준으로 사용함.
	RebaseToCurrentTransform();
}

void UCXMRPlacementComponent::ResetMarkerOffset()
{
	CancelAlignmentCapture();
	TempMarkerLocationOffset = FVector::ZeroVector;
	TempMarkerRotationOffset = FRotator::ZeroRotator;

	UE_LOG(LogCXMRPlacement, Log, TEXT("Marker offset adjustments reset"));

	// 오프셋만 비우고 현재 기준으로 되돌림.
	PublishOffset();
	ApplyPlacement();
}
