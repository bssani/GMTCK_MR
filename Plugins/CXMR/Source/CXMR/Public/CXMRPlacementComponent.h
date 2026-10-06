// Copyright GMTCK CX.

// 마커나 사용자 기준으로 차량 배치함.
// 마커 이벤트는 Subsystem에서 받음.
// VehicleWorld = LocalOffset^-1 * MarkerWorld.
// 차량 메시가 바뀌어도 앵커 정렬은 유지함.

// MarkerAnchor는 마커 기준, PawnRelative는 사용자 앞에 배치함.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRTypes.h"
#include "Engine/TimerHandle.h"
#include "CXMRPlacementComponent.generated.h"

class UCXMRSubsystem;
class UCXMRMarkerProfile;
class UCXMRVehicleProfile;
struct FCXMRMarkerEntry;

/** 수동 회전의 중심 선택함. */
UENUM(BlueprintType, meta=(ToolTip="Pivot used for manual yaw adjustments."))
enum class ECXMRNudgePivot : uint8
{
	/*~ 보정 마커의 중심 사용함. 마커가 없으면 사용자 위치 사용함. */
	Markers UMETA(ToolTip="Calibration marker centre; falls back to the viewer if no marker is available."),
	/*~ 사용자 머리 위치를 회전 중심으로 사용함. */
	Viewer UMETA(ToolTip="Rotate around the viewer's head."),
	/*~ 차량 액터의 원점 사용함. CAD 원점은 형상과 멀 수 있음. */
	VehicleOrigin UMETA(ToolTip="Rotate around the vehicle origin, which may be far from CAD geometry."),
	/*~ 차량에 지정한 눈 기준점 사용함. */
	DriverEyeReference UMETA(ToolTip="Rotate around the vehicle's authored driver eye reference.")
};

/** 조정 버튼의 앞 방향 기준. */
UENUM(BlueprintType, meta=(ToolTip="Heading used by adjustment keys and buttons."))
enum class ECXMRNudgeFrame : uint8
{
	/*~ 첫 조정 때 시선 방향을 고정함. 이후 고개를 돌려도 축은 유지함. */
	ViewerLatched UMETA(ToolTip="Hold the heading captured at the first adjustment until it is reset."),
	/*~ 현재 시선 방향 사용함. */
	ViewerLive UMETA(ToolTip="Use the viewer's current heading."),
	/*~ 차량 방향을 수평으로 펴서 사용함. */
	Vehicle UMETA(ToolTip="Use the vehicle's horizontal forward and right axes.")
};

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Placement")
class CXMR_API UCXMRPlacementComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRPlacementComponent();

	/** 런타임 프로필 변경은 SetMarkerProfile로 처리함. 직접 대입하면 보정 전환을 건너뜀. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta=(ToolTip="Marker profile for this vehicle. Change it through SetMarkerProfile at runtime.")) TObjectPtr<UCXMRMarkerProfile> MarkerProfile;

	/** 이전 원본 복원 후 새 프로필의 저장값 적용함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Restore the outgoing profile and apply the incoming profile's saved calibration.")) void SetMarkerProfile(UCXMRMarkerProfile* NewProfile, bool bForceReload = false);

	/** 차량 오프셋을 포함한 눈 기준점 구함. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Alignment", meta=(ToolTip="Resolve the configured driver eye reference through the loaded vehicle transform.")) bool GetDriverEyeWorld(FTransform& EyeWorld) const;
	/** 추적한 머리 위치와 yaw로 한 번 배치함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Alignment", meta=(ToolTip="Align the driver eye reference to a valid world head pose, preserving a level vehicle. The caller must validate HMD tracking.")) bool AlignToDriverEyePose(FTransform HeadWorld);
	/** 확인한 위치를 고정하고 새 마커 관측 모음. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Alignment", meta=(ToolTip="Confirm the final vehicle pose and collect fresh observations of every configured calibration marker.")) bool BeginAlignmentCapture();
	UFUNCTION(BlueprintPure, Category = "CXMR|Alignment") bool CanSaveAlignment() const;
	UFUNCTION(BlueprintCallable, Category = "CXMR|Alignment") bool SaveAlignment();
	UFUNCTION(BlueprintPure, Category = "CXMR|Alignment") bool IsManualAlignment() const { return bManualAlignment; }
	bool HasAlignmentCapture() const { return IsCapturePoseCurrent(); }
	UFUNCTION(BlueprintPure, Category = "CXMR|Alignment") bool NeedsRestoreConfirmation() const { return bRestoreNeedsConfirmation; }
	bool HasSavedAlignment() const { return !SavedAlignmentMarkers.IsEmpty(); }
	UFUNCTION(BlueprintCallable, Category = "CXMR|Alignment") bool ConfirmRestoredAlignment();
	UFUNCTION(BlueprintPure, Category = "CXMR|Alignment") int32 GetCapturedAlignmentMarkerCount() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Alignment") int32 GetRequiredAlignmentMarkerCount() const;
	void CancelAlignmentCapture();

	/** 보정 기준 액터. 지정하지 않으면 소유자 사용함. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "CXMR|Placement", meta=(ToolTip="Calibrated anchor actor. Defaults to the component owner.")) TObjectPtr<AActor> VehicleRoot;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement") ECXMRPlacementMode Mode = ECXMRPlacementMode::MarkerAnchor;

	/** 보정 완료 후 이 컴포넌트의 배치만 고정함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta=(ToolTip="Freeze vehicle placement after calibration; keep headset marker tracking active.")) bool bFreezeAfterCalibration = true;

	/** 완료 후 전체 마커 추적도 끔. 동적 마커까지 멈추므로 기본값은 꺼둠. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta=(ToolTip="Stop all marker tracking after calibration, including dynamic objects. Disabled by default.")) bool bStopMarkerTrackingWhenCalibrated = false;

	/** XR 세션이 준비되면 마커 추적 시작함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta=(ToolTip="Start marker tracking once the XR session is ready.")) bool bStartMarkerTrackingOnBeginPlay = true;

	/** 위치 고정 전 표본을 더 받을 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ToolTip="Seconds to accumulate samples before freezing placement.", ClampMin = "0.0")) float CalibrationSettleSeconds = 1.5f;

	/** 완료에 필요한 마커 수. 2개 이상이면 위치로 수평 정렬함. 그 전에는 임시 배치함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ToolTip="Markers required for completion. Two or more use a horizontal fit; fewer give provisional placement.", ClampMin = "1")) int32 MinMarkersToCalibrate = 2;

	/** 최근 관측한 마커만 학습과 완료 판정에 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ToolTip="Maximum observation age used for learning and calibration completion, in seconds.", ClampMin = "0.01")) float MarkerObservationMaxAge = 0.5f;

	/** 완료에 허용할 최대 RMS 오차(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ToolTip="Maximum RMS error allowed for multi-marker calibration, in cm.", ClampMin = "0.0")) float MaxCalibrationFitError = 2.0f;

	/** 단일 마커도 차량을 수평으로 유지함. 실제 차량이 기울어진 경우에만 끔. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta=(ToolTip="Keep single-marker placement level. Disable only for a physically tilted vehicle.")) bool bKeepLevel = true;

	/** 수동 yaw 회전의 중심. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta=(ToolTip="What the manual yaw adjustment rotates the vehicle around.")) ECXMRNudgePivot NudgePivot = ECXMRNudgePivot::Markers;

	/** 조정 앞 방향 기준. 기본값은 첫 시선 방향 고정함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta=(ToolTip="Adjustment heading reference. Defaults to the first viewer heading.")) ECXMRNudgeFrame NudgeFrame = ECXMRNudgeFrame::ViewerLatched;

	/** 이동 버튼 한 번의 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ToolTip="Movement per adjustment button press, in cm.", ClampMin = "0.1")) float NudgeMoveStep = 1.0f;

	/** 회전 버튼 한 번의 각도(deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ToolTip="Rotation per adjustment button press, in degrees.", ClampMin = "0.1")) float NudgeYawStep = 1.0f;

	/** 사용자 앞에 배치할 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ToolTip="Pawn Relative: distance in front of the pawn (cm).", ClampMin = "0.0")) float PawnRelativeDistance = 350.0f;

	/** 추적 중 재배치에 필요한 최소 이동 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta = (ToolTip="Minimum movement (cm) before a Moved update re-runs calibration. Marker poses jitter every frame; without this the vehicle would be re-placed continuously and read as unstable.", ClampMin = "0.0"))
	float MarkerUpdateThreshold = 0.5f;

	/** 보정 중 표본을 평균 내서 관측 흔들림을 줄임. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Placement", meta=(ToolTip="Average every marker sample taken while calibrating, rather than placing from the latest one. One frame of marker pose carries a few millimetres of noise, and whichever frame happened to arrive last decided where the car stood — differently every session, which is what made a carefully aligned car come back somewhere else.")) bool bAverageMarkerSamples = true;

	UPROPERTY(BlueprintReadOnly, Category = "CXMR|Placement") bool bCalibrated = false;

	/** 보정 상태를 비우고 마커 추적 다시 시작함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Re-run calibration: clears the flag and cycles marker tracking so Detected fires again.")) void Recalibrate();

	/** 사용자 앞 바닥에 놓고 사용자를 바라보게 함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Pawn Relative: place the vehicle in front of the local player, on the floor, facing them.")) void PlaceInFrontOfPawn();

	/** X는 앞, Y는 오른쪽, Z는 위. yaw 양수는 시계 방향. 결과는 저장용 오프셋에 반영함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Move along the selected adjustment frame. X is forward, Y is right, Z is up. Positive yaw is clockwise. The result is kept as a calibration offset.")) void NudgeVehicle(FVector ViewerDelta, float YawDelta);

	/** 시선 방향과 사용자 위치를 직접 받아 조정함. NudgeFrame에 따라 고정 방향이나 차량 방향을 사용함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Adjust using the supplied heading and viewer position, subject to NudgeFrame."))
	void NudgeVehicleInFrame(FVector ViewerDelta, float YawDelta, float HeadingYaw, FVector ViewerLocation);

	/** 현재 조정의 앞 방향 yaw 구함. 고정값이 없으면 ViewerYaw 사용함. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement", meta=(ToolTip="The world yaw the adjust keys treat as \"away from me\", for the current NudgeFrame. ViewerYaw is where the viewer is looking now; it is what comes back when nothing is latched and the frame is not the vehicle's.")) float ResolveNudgeYaw(float ViewerYaw) const;

	/** 조정 앞 방향을 HeadingYaw로 고정함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Hold HeadingYaw as \"away from me\" from now on (ViewerLatched).")) void LatchNudgeHeading(float HeadingYaw);

	/** 현재 시선 방향으로 조정 축 다시 잡음. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Latch the way the viewer is facing right now — use it when the axes ended up crooked.")) void RetakeNudgeHeading();

	/** 방향 고정 여부. false면 다음 조정에서 잡음. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement", meta=(ToolTip="True once a heading is held. False = the next adjustment takes one.")) bool HasNudgeHeading() const { return bHaveNudgeHeading; }

	/** 월드 위치를 조정 오프셋에 반영함. 저장과 학습에도 같은 위치 사용함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Put the vehicle at this world pose, kept as the manual offset like a nudge — so Save adjustment and Learn work on it unchanged. Used by alignments that compute a whole pose at once (touched box corners).")) void MoveVehicleTo(FTransform VehicleWorld);

	/** 차량 기준 임시 오프셋 조정함. 이동은 cm, 회전은 deg. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Adjust the offset in the VEHICLE's own frame (temporary). X/Y/Z cm + rotation deg. Kept for scripts; the adjust keys go through NudgeVehicle.")) void AdjustMarkerOffset(FVector DeltaLocation, FRotator DeltaRotation);

	/** 조정값을 마커 프로필과 파일에 저장함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Save adjusted offset to the marker profile (permanent).")) void SaveMarkerOffsetToProfile();

	/** 임시 조정값 비움. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Reset temporary offset adjustments.")) void ResetMarkerOffset();

	/** 현재 위치를 새 기준으로 잡음. 외부에서 차량을 옮긴 뒤 호출해야 함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Adopt the current pose as the adjustment base after an external move.")) void RebaseToCurrentTransform();

	// 현장 보정
	// 차량을 실물에 맞춘 뒤 마커의 상대 좌표를 기록함.

	/** 최근 관측한 마커를 차량 기준으로 학습하고 저장함. 미등록 마커도 추가함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Record every marker in view relative to the vehicle as it stands right now, and save. Markers the profile does not list are added to the layout (and the saved file), so a site needs no marker ids typed in beforehand.")) void LearnMarkerLayout();

	/** 현재 마커 배치를 Saved/CXMR에 저장함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Write the current marker layout to Saved/CXMR. A cooked build cannot save its data assets, so without this every restart loses the calibration.")) bool SaveCalibrationToDisk();
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement") bool WasLastCalibrationSaveSuccessful() const { return bLastCalibrationSaveSucceeded; }

	/** 이 프로필의 저장값이 있으면 적용함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Apply a previously saved layout, if one exists for this profile.")) bool LoadCalibrationFromDisk();

	/** 저장 파일을 지우고 프로필 원본 복원함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Placement", meta=(ToolTip="Delete the saved file and restore the offsets the profile shipped with.")) void ResetCalibrationToAuthored();

	/** 현재 보정 파일 경로. 프로필이 없으면 빈 값. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement", meta=(ToolTip="Where the calibration for the current profile is written. Empty if no profile is set.")) FString GetCalibrationFilePath() const;

	/** 임시 조정값. 패널은 Subsystem에 게시한 값을 읽음. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement", meta=(ToolTip="Live temporary offset. NOTE: the control panel must NOT read these — this component does not live on the pawn, so widgets cannot find it. It publishes the same values to the subsystem; UI reads UCXMRSubsystem::GetMarkerLocationOffset() instead.")) FVector GetMarkerLocationOffset() const { return TempMarkerLocationOffset; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Placement") FRotator GetMarkerRotationOffset() const { return TempMarkerRotationOffset; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	AActor* ResolveVehicleRoot();

	// 최초 감지 후 갱신과 재관측은 Moved로 받음.

	UFUNCTION() void HandleMarkerDetected(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size);
	UFUNCTION() void HandleMarkerMoved(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size);
	UFUNCTION() void HandleMarkerLost(int32 MarkerId);
	UFUNCTION() void HandleMarkerTrackingChanged(bool bEnabled);
	UFUNCTION() void HandleRecalibrateRequest();
	UFUNCTION() void HandlePlaceRequest();

	/** Detected와 Moved에서 현재 프로필 설정 적용함. */
	void HandleMarkerPose(int32 MarkerId, const FVector& Position, const FRotator& Rotation, bool bIsFirstSighting);
	bool IsMarkerObservationCurrent(int32 MarkerId) const;
	int32 CurrentMarkerCount() const;
	/** 임시 배치와 보정 완료 조건 확인함. */
	bool ComputeCalibrationPose(FTransform& Out, bool& bOutReady);

	/** 누적 마커로 기준 위치 다시 계산함. */
	void RecomputeCalibration();
	/** 마커 2개 이상으로 수평 이동·yaw 계산함. OutResidual은 RMS 오차(cm). */
	bool ComputeMultiMarkerTransform(FTransform& Out, float& OutResidual) const;

	/** 위치는 좌표, 방향은 축 벡터로 평균 냄. 각도 경계 문제를 피함. */
	struct FMarkerSamples
	{
		FVector PositionSum = FVector::ZeroVector;
		FVector ForwardSum  = FVector::ZeroVector;
		FVector UpSum       = FVector::ZeroVector;
		double  SquaredSum  = 0.0;
		int32   Count       = 0;
		/** 평균 주변 표본 흔들림(cm). */
		float   Scatter     = 0.0f;

		FTransform Mean() const;
	};

	/** 마커별 표본. 재보정 시 비움. */
	TMap<int32, FMarkerSamples> MarkerSamples;

	/** 표본 하나를 추가하고 평균 자세 반환함. */
	FTransform AccumulateSample(int32 MarkerId, const FVector& Position, const FRotator& Rotation);

	/** 현재 보정에서 가장 큰 흔들림(cm). */
	float WorstScatter() const;

	/** 배치의 RMS 오차(cm). 다중 계산 전에는 음수. */
	float LayoutFitError = -1.0f;

	/** 배치 조작을 통합 창에 등록함. */
	void RegisterTunables();

	/** NudgePivot에 따른 월드 회전 중심. */
	FVector ResolveNudgePivot(const FTransform& Vehicle, const FVector& ViewerLocation) const;

	/** 배치에 사용한 마커 ID와 월드 좌표. */
	TMap<int32, FTransform> DetectedCalib;

	/** 과거 좌표는 보관하고 학습에는 최근 관측만 사용함. */
	TMap<int32, FTransform> SeenMarkers;
	TMap<int32, double> MarkerLastObserved;
	/** Timeout과 Tracking Mode를 적용한 ID. 실패하면 다음 이벤트에서 재시도함. */
	TSet<int32> ConfiguredMarkerIds;

	FTimerHandle SettleTimer;
	FTimerHandle TrackingStartTimer;
	int32 TrackingStartAttempts = 0;

	/** 안정화 시간이 끝나면 위치 고정함. */
	void FinishCalibration();

	/** 세션 지원 확인 후 추적 시작함. 일정 시간 재시도함. */
	void TryStartMarkerTracking();

	/** 미세조정용 임시 이동(cm)과 회전(deg). */
	FVector TempMarkerLocationOffset = FVector::ZeroVector;
	FRotator TempMarkerRotationOffset = FRotator::ZeroRotator;

	/** 오프셋 적용 전 기준 위치. 마커가 안 보여도 조정할 수 있게 보관함. */
	FTransform BaseVehicleTransform = FTransform::Identity;
	bool bHaveBasePose = false;

	/** 고정한 앞 방향 yaw와 고정 여부. */
	float LatchedNudgeYaw = 0.0f;
	bool bHaveNudgeHeading = false;
	bool bLastCalibrationSaveSucceeded = false;
	// 수동 위치는 새 마커가 잡혀도 유지함.
	bool bManualAlignment = false;
	bool bAlignmentPoseLocked = false;
	bool bRestoreNeedsConfirmation = false;
	bool bCapturingAlignment = false;
	FTransform CapturePose;
	FTransform CaptureVehicleRelativePose;
	FTransform CaptureModelOffset;
	FSoftObjectPath CaptureModelClass;
	FTransform CaptureEyeReference;
	TWeakObjectPtr<AActor> CaptureVehicle;
	TWeakObjectPtr<UCXMRVehicleProfile> CaptureVehicleProfile;
	TWeakObjectPtr<UCXMRMarkerProfile> CaptureMarkerProfile;
	TMap<int32, FTransform> CapturedAlignmentMarkers;
	TSet<int32> CaptureMarkerIds;
	bool bCaptureHasEyeReference = false;
	TSet<int32> SavedAlignmentMarkers;
	int32 SavedPrimaryMarker = 0;
	bool IsCapturePoseCurrent() const;
	bool WriteCalibrationData(const TArray<FCXMRMarkerEntry>& Entries, const TSet<int32>& ValidatedIds);
	bool ComputeSavedAlignmentPose(FTransform& Out, bool& bReady);

	/** 기준 위치에 임시 오프셋 적용함. */
	void ApplyPlacement();

	/** 패널에서 읽을 수 있게 오프셋을 Subsystem에 게시함. */
	void PublishOffset();

	/** 종료와 초기화에 사용할 프로필 원본 보관함. */
	void CaptureAuthoredOffsets();
	void RestoreAuthoredOffsets();

	/** 프로필별 저장값은 한 번만 적용함. BeginPlay와 프로필 변경 순서에 영향받지 않게 함. */
	void EnsureCalibrationLoaded();

	/** 마커 ID별 원본 오프셋. */
	TMap<int32, FTransform> AuthoredOffsets;
	TWeakObjectPtr<UCXMRMarkerProfile> CapturedProfile;
	bool bCalibrationLoaded = false;

	/** 이번 세션에서 시작 백업을 만든 파일. */
	TSet<FString> StartupBackedUp;

	// 입력과 UI 요청은 Subsystem에서 전달받음.
	UFUNCTION() void HandleAdjustOffsetRequest(FVector DeltaLocation, FRotator DeltaRotation);
	UFUNCTION() void HandleSaveOffsetRequest();
	UFUNCTION() void HandleResetOffsetRequest();

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
};
