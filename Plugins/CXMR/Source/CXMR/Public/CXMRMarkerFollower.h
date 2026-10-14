// Copyright GMTCK CX.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CXMRTypes.h"
#include "CXMRMarkerFollower.generated.h"

class UCXMRSubsystem;
class UTextRenderComponent;
struct FCXMRMarkerEntry;

UENUM(BlueprintType)
enum class ECXMRMarkerWhenLost : uint8
{
	Hold,
	Hide
};

UCLASS(Blueprintable, meta=(DisplayName="CXMR Marker Follower"))
class CXMR_API ACXMRMarkerFollower : public AActor
{
	GENERATED_BODY()

public:
	ACXMRMarkerFollower();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CXMR|Marker Follower", meta=(ToolTip="Marker number printed on the physical marker. Zero leaves this actor inactive.", ClampMin="0"))
	int32 MarkerId = 0;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CXMR|Marker Follower", meta=(ToolTip="Requested tracking mode. Dynamic follows moving objects; Stationary favors fixed objects."))
	ECXMRMarkerTrackingMode TrackingMode = ECXMRMarkerTrackingMode::Dynamic;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="CXMR|Marker Follower", meta=(ToolTip="Follow marker poses. Turn off to hold the current actor transform."))
	bool bFollow = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CXMR|Marker Follower", meta=(ToolTip="Actor origin relative to the marker, in centimeters and degrees. Applied before the marker world pose."))
	FTransform MarkerToActor = FTransform::Identity;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CXMR|Marker Follower", meta=(ToolTip="Hold the last pose or hide until the marker is observed again."))
	ECXMRMarkerWhenLost WhenLost = ECXMRMarkerWhenLost::Hold;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="CXMR|Marker Follower", meta=(ToolTip="Show the marker number and runtime tracking mode above the actor."))
	bool bShowLabel = true;

	UFUNCTION(BlueprintCallable, Category="CXMR|Marker Follower", meta=(ToolTip="Resume on the next marker pose, or hold the current actor transform."))
	void SetFollow(bool bEnable);
	UFUNCTION(BlueprintCallable, Category="CXMR|Marker Follower", meta=(ToolTip="Request a tracking mode after this marker has been detected. Failed requests retry on the next pose."))
	void SetTrackingMode(ECXMRMarkerTrackingMode Mode);
	UFUNCTION(BlueprintPure, Category="CXMR|Marker Follower", meta=(ToolTip="Whether this actor has observed its marker without a subsequent loss."))
	bool IsMarkerTracked() const { return bTracked; }
	UFUNCTION(BlueprintImplementableEvent, Category="CXMR|Marker Follower")
	void OnMarkerFound();
	UFUNCTION(BlueprintImplementableEvent, Category="CXMR|Marker Follower")
	void OnMarkerLost();

	virtual void Tick(float DeltaSeconds) override;
#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
	UPROPERTY(VisibleAnywhere, Category="CXMR|Marker Follower") TObjectPtr<UTextRenderComponent> Label;
	bool bTracked = false;
	bool bObserved = false;
	bool bCanConfigure = false;
	bool bModePending = true;
	bool bModeApplied = false;
	bool bWarnedTrackingOff = false;

	UFUNCTION() void HandleMarkerPose(int32 Id, FVector Position, FRotator Rotation, FVector2D Size);
	UFUNCTION() void HandleMarkerLost(int32 Id);
	UFUNCTION() void HandleTrackingChanged(bool bEnabled);
	void TryApplyTrackingMode();
	void UpdateLabel();
	bool FindCalibrationEntry(FCXMRMarkerEntry& OutEntry) const;
};
