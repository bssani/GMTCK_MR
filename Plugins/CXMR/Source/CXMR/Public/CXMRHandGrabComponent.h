// Copyright GMTCK CX.
//
// UCXMRHandGrabComponent — pick things up with a tracked hand.
//
// Pinch (thumb tip to index tip) next to an actor that has UCXMRGrabbableComponent and it follows the pinch until the
// fingers open. Joints come through CXMRHands, so the hand-alignment correction moves the grab point exactly as it
// moves the skeleton. No controller and no input action: this is the bare hand.
//
// A pinch closes below one gap and opens above a larger one, so fingertips resting near the threshold cannot grab and
// drop every frame. A held actor also survives a short tracking dropout instead of falling at the first lost frame.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "CXMRHandGrabComponent.generated.h"

class UCXMRGrabbableComponent;
class UCXMRTuningSubsystem;
class UStaticMesh;

/** One hand's pinch and what it holds. Plain C++: never saved, never seen by Blueprints. */
struct FCXMRGrabHand
{
	bool bTracked = false;
	bool bPinching = false;
	/** Thumb tip to index tip (cm); negative while unknown or simulated. */
	float PinchGap = -1.0f;
	/** Between the two fingertips, oriented like the palm. */
	FTransform Frame;
	FVector Velocity = FVector::ZeroVector;
	float UntrackedSeconds = 0.0f;

	TWeakObjectPtr<UCXMRGrabbableComponent> Held;
	/** The held actor relative to Frame, fixed at the moment it was picked up. */
	FTransform HeldOffset;
	bool bHeldWasSimulating = false;

	bool bSimulated = false;
	bool bSimulatedPinching = false;
	FTransform SimulatedFrame;
};

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Hand Grab")
class CXMR_API UCXMRHandGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRHandGrabComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Grab") bool bHandGrabEnabled = true;

	/** Fingertips closer than this start a pinch (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Grab", meta = (ClampMin = "0.5", ClampMax = "10.0"))
	float PinchCloseDistance = 2.0f;

	/** ...and must open past this to end it (cm). Keep it above the close distance. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Grab", meta = (ClampMin = "0.5", ClampMax = "15.0"))
	float PinchOpenDistance = 3.5f;

	/** How far an actor's bounds may be from the pinch point and still be picked up (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Grab", meta = (ClampMin = "0.0", ClampMax = "50.0"))
	float GrabRadius = 5.0f;

	/** Seconds a held actor stays in the hand after tracking drops out. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Grab", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float LostTrackingGraceSeconds = 0.25f;

	/** Mesh for SpawnTestCube. A soft reference, so cooking includes it without loading it into every level. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Grab") TSoftObjectPtr<UStaticMesh> TestCubeMesh;

	UFUNCTION(BlueprintPure, Category = "CXMR|Grab") bool IsPinching(EControllerHand Hand) const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Grab") AActor* GetHeldActor(EControllerHand Hand) const;

	/** Lets go of whatever the hand holds, restoring its physics. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Grab") void ReleaseHand(EControllerHand Hand);

	/** Drives a hand without tracking: whether it pinches and where the pinch is. For tests, and for grabbing from
	 *  another source. In effect until ClearSimulatedPinch. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Grab") void SetSimulatedPinch(EControllerHand Hand, bool bPinching, FTransform PinchFrame);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Grab") void ClearSimulatedPinch(EControllerHand Hand);

	/** An 8 cm grabbable cube 40 cm in front of the viewer. Console: CXMR.SpawnGrabCube [1 = with physics] */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Grab") AActor* SpawnTestCube(bool bSimulatePhysics = false);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRTuningSubsystem* GetTuning() const;

	FCXMRGrabHand& StateFor(EControllerHand Hand) { return Hand == EControllerHand::Left ? LeftHand : RightHand; }
	const FCXMRGrabHand& StateFor(EControllerHand Hand) const { return Hand == EControllerHand::Left ? LeftHand : RightHand; }

	void UpdateHand(EControllerHand Hand, float DeltaTime);
	UCXMRGrabbableComponent* FindGrabbable(const FVector& Point) const;
	void Grab(EControllerHand Hand, UCXMRGrabbableComponent* Target);
	void Release(EControllerHand Hand, bool bRestorePhysics);

	void RegisterTunables();
	FText DescribeHand(EControllerHand Hand) const;

	FCXMRGrabHand LeftHand;
	FCXMRGrabHand RightHand;
};
