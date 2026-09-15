// Copyright GMTCK CX.
//
// UCXMRHandDebugComponent — headset verification instrument for hand tracking.
//
// Draws the tracked hand skeleton so a session can answer "does the headset see my hands, and how
// well" without any interaction feature existing yet. Reads the engine's IHandTracker modular
// feature (XR_EXT_hand_tracking via OpenXRHandTracking), NOT the Varjo plugin — hand JOINTS are an
// engine-level capability; the Varjo module only adds aim/grip interaction poses on top.
//
// Visibility follows the subsystem's hand-visualization state (bound to IA_Varjo_HandVisualization),
// so it is a toggle in the headset rather than a console command.
//
// It also owns the hand-alignment rows of the tuning window: where the index fingertip is relative to the head,
// how far it is from the nearest marker, and the correction CXMRHands applies to every hand consumer. Touch a
// marker's centre with the index fingertip and press the snap button — the error becomes the correction.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "CXMRHandDebugComponent.generated.h"

class UCXMRSubsystem;
class UCXMRTuningSubsystem;
class IHandTracker;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Hand Debug")
class CXMR_API UCXMRHandDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRHandDebugComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True when a hand tracker is present AND currently reporting valid state. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Hands") bool IsHandTrackingAvailable() const;

	/** Change the hand correction so a fingertip at TipWorld would land on TargetWorld. Saved like a tuning edit. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Hands") bool CalibrateHandOffset(FVector TipWorld, FVector TargetWorld);

	/** CalibrateHandOffset with the right (else left) index tip and the marker nearest to it. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Hands") bool CalibrateToNearestMarker();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands") FLinearColor LeftColor  = FLinearColor(0.20f, 0.75f, 1.00f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands") FLinearColor RightColor = FLinearColor(1.00f, 0.65f, 0.15f, 1.0f);

	/** Joints are drawn at the tracker's own reported radius; this scales it for legibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float RadiusScale = 1.0f;

	/** Connect the joints of each finger. Off = joint spheres only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands") bool bDrawBones = true;

	/** Log a line whenever either hand gains or loses tracking — the headset answer we are after. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands") bool bLogTrackingChanges = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	UCXMRTuningSubsystem* GetTuning() const;
	IHandTracker* GetHandTracker() const;

	/** Returns true if this hand was drawn (i.e. is actually being tracked right now). */
	bool DrawHand(EControllerHand Hand, const FLinearColor& Color);

	void RegisterTunables();
	FText DescribeTip(EControllerHand Hand) const;
	FText DescribeTipToMarker() const;

	/** Right (else left) index tip, and the known marker nearest to it. */
	bool FindTipAndMarker(FVector& OutTip, int32& OutMarkerId, FVector& OutMarker) const;

	UFUNCTION() void HandleVisualizationChanged(bool bOn);
	UFUNCTION() void HandleMarkerPose(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size);
	UFUNCTION() void HandleMarkerLost(int32 MarkerId);

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;

	/** Marker centres in world space — points whose real position is known, for the alignment readout. */
	TMap<int32, FVector> MarkerPositions;

	// Previous tracked state per hand, so the log fires on transitions only.
	bool bLeftWasTracked  = false;
	bool bRightWasTracked = false;

	/** When the visualization was switched on, and whether the "no hand data" warning has been given since. */
	double VisualizationOnSeconds = 0.0;
	bool bWarnedNoHandData = false;
};
