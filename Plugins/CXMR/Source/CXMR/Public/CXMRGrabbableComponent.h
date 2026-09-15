// Copyright GMTCK CX.
//
// UCXMRGrabbableComponent — marks an actor the wearer can pick up with a tracked hand.
//
// Add it to any actor whose root is Movable. UCXMRHandGrabComponent (on the pawn) looks for grabbables next to a pinch;
// the actor then follows the pinch until the fingers open. Physics is paused while held and restored on release with
// the hand's last motion — the Varjo example's BP_FloatingObject and BP_PhysicsObject in one component.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "CXMRGrabbableComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCXMROnGrabChanged, EControllerHand, Hand);

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Grabbable")
class CXMR_API UCXMRGrabbableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRGrabbableComponent();

	/** Off = cannot be picked up for now; the component stays. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Grab") bool bGrabbable = true;

	/** A simulating actor keeps the hand's velocity when let go. Off = it drops straight down. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Grab") bool bThrowOnRelease = true;

	UPROPERTY(BlueprintAssignable, Category = "CXMR|Grab") FCXMROnGrabChanged OnGrabbed;
	UPROPERTY(BlueprintAssignable, Category = "CXMR|Grab") FCXMROnGrabChanged OnReleased;

	UFUNCTION(BlueprintPure, Category = "CXMR|Grab") bool IsHeld() const { return bHeld; }

	/** Every grabbable in World that has begun play. */
	static TArray<UCXMRGrabbableComponent*> GetAll(const UWorld* World);

	/** Called by UCXMRHandGrabComponent as it takes and lets go. */
	void NotifyGrabbed(EControllerHand Hand);
	void NotifyReleased(EControllerHand Hand);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	bool bHeld = false;
};
