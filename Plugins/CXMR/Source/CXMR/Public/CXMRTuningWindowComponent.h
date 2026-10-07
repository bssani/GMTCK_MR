// Copyright GMTCK CX.

// 작업 메뉴별로 통합 창 표시함.
// 기능에서 등록한 설정으로 행을 만듦.
// 다른 입력으로 바뀐 값도 현재 상태를 읽어 표시함.
// MR·깊이·노출·속도는 여기서 등록함.
// CXMR.Tuning으로 열고 닫음.
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "CXMRTuningWindowComponent.generated.h"

class SWidget;
class SWindow;
class UCXMRTuningSubsystem;
class UCXMRControlPanelWidget;
class UCXMRPlacementComponent;
class UCXMRMarkerProfile;
class UCXMRVehicleProfile;
class UCXMRVehicleLoaderComponent;
class APostProcessVolume;

UENUM(BlueprintType)
enum class ECXMRControlPage : uint8
{
	Vehicle, View, Calibration, Display, USB, Diagnostics
};

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Tuning Window")
class CXMR_API UCXMRTuningWindowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRTuningWindowComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Tuning Window") bool bOpenOnBeginPlay = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Tuning Window") FVector2D WindowSize = FVector2D(900.f, 760.f);
	/** 통합 창 시작 위치(픽셀). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Tuning Window", meta=(ToolTip="Initial desktop position of the unified control window, in pixels.")) FVector2D WindowPosition = FVector2D(40.f, 60.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Tuning Window") FText WindowTitle;

	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning Window") void OpenWindow();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning Window") void CloseWindow();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning Window") void ToggleWindow();
	UFUNCTION(BlueprintPure,     Category = "CXMR|Tuning Window") bool IsWindowOpen() const;
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void SelectPage(ECXMRControlPage Page);
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") ECXMRControlPage GetActivePage() const { return ActivePage; }
	void StartCalibration();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI", meta=(ToolTip="Click twice within five seconds to delete the loaded group alignment. Vehicle-specific saves remain unchanged.")) void ResetSharedAlignment();
	bool IsSharedAlignmentResetArmed() const;
	FText GetSharedAlignmentResetLabel() const;
	void StartInitialAlignment();
	void CancelInitialAlignment();
	bool IsInitialAlignmentPending() const;
	bool CanStartInitialAlignment() const;
	bool AcceptRestoredAlignment();
	void PlaceManually();
	bool ConfirmCalibration();
	bool SaveCalibration();
	void RequestAlignmentSave();
	bool CanConfirmCalibration() const;
	bool CanSaveCalibration() const;
	int32 GetCalibrationPhase() const;
	FText GetCalibrationMessage() const;
	FText GetAlignmentStatus(const UCXMRVehicleLoaderComponent* Loader) const;
	UCXMRPlacementComponent* FindPlacement() const;
	bool HasCalibrationError() const { return bAlignmentSaveFailed; }

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void FinishInitialAlignment();
	void CaptureSessionIdentity(UCXMRPlacementComponent* Placement);
	UFUNCTION() void HandleDisplayModeChanged(bool bMixedReality);
	void CaptureExposureBaseline();
	float GetExposurePreset(bool bMR) const;
	void SetExposurePreset(bool bMR, float Value);
	TWeakObjectPtr<APostProcessVolume> ExposureVolume;
	float BaselineExposure = 0.f;
	bool bBaselineExposureOverride = false;
	bool bHasMRExposure = false;
	bool bHasVRExposure = false;
	float MRExposure = 0.f;
	float VRExposure = 0.f;
	bool bExposureModeMR = false;
	FTimerHandle InitialAlignmentTimer;
	FTransform InitialEyeReference;
	FTransform InitialModelOffset;
	FTransform InitialVehicleRelativePose;
	FSoftObjectPath InitialModelClass;
	UFUNCTION() void HandleManualPlacementRequest();
	UCXMRTuningSubsystem* GetTuning() const;
	void RegisterCoreTunables();
	void RebuildContent();
	TSharedRef<SWidget> BuildPanel();

	/** Slate 창은 EndPlay에서 직접 닫음. */
	TSharedPtr<SWindow> Window;
	FDelegateHandle TunablesChangedHandle;
	ECXMRControlPage ActivePage = ECXMRControlPage::Vehicle;
	TWeakObjectPtr<UCXMRPlacementComponent> ResetPlacement;
	FName ResetGroup;
	double ResetDeadline = 0.0;
	// 기준이 바뀌면 확인 무효화함. 옛 위치로 돌아와도 복구하지 않음.
	mutable int32 CalibrationPhase = 0;
	mutable FText CalibrationMessage;
	bool bAlignmentSaveFailed = false;
	TWeakObjectPtr<UCXMRPlacementComponent> CalibrationPlacement;
	TWeakObjectPtr<UCXMRMarkerProfile> CalibrationProfile;
	TWeakObjectPtr<UCXMRVehicleProfile> CalibrationVehicleProfile;
	TWeakObjectPtr<AActor> CalibrationVehicle;
	FTransform ConfirmedPose;
	UPROPERTY(Transient) TObjectPtr<UCXMRControlPanelWidget> ControlWidget;
};
