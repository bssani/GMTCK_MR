// Copyright GMTCK CX.

#include "CXMRGrabbableComponent.h"

#include "Engine/World.h"

namespace
{
	/** Grabbables that have begun play, in any world; GetAll filters by world. A list instead of an actor iterator:
	 *  the hand looks for them every pinch, and there are a handful among thousands of actors. */
	TArray<TWeakObjectPtr<UCXMRGrabbableComponent>> GGrabbableRegistry;
}

UCXMRGrabbableComponent::UCXMRGrabbableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCXMRGrabbableComponent::BeginPlay()
{
	Super::BeginPlay();
	GGrabbableRegistry.AddUnique(this);
}

void UCXMRGrabbableComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	GGrabbableRegistry.RemoveAll([this](const TWeakObjectPtr<UCXMRGrabbableComponent>& Entry)
	{
		return !Entry.IsValid() || Entry.Get() == this;
	});
	Super::EndPlay(Reason);
}

TArray<UCXMRGrabbableComponent*> UCXMRGrabbableComponent::GetAll(const UWorld* World)
{
	TArray<UCXMRGrabbableComponent*> Result;
	for (const TWeakObjectPtr<UCXMRGrabbableComponent>& Entry : GGrabbableRegistry)
	{
		UCXMRGrabbableComponent* Grabbable = Entry.Get();
		if (Grabbable && Grabbable->GetWorld() == World)
		{
			Result.Add(Grabbable);
		}
	}
	return Result;
}

void UCXMRGrabbableComponent::NotifyGrabbed(EControllerHand Hand)
{
	bHeld = true;
	OnGrabbed.Broadcast(Hand);
}

void UCXMRGrabbableComponent::NotifyReleased(EControllerHand Hand)
{
	bHeld = false;
	OnReleased.Broadcast(Hand);
}
