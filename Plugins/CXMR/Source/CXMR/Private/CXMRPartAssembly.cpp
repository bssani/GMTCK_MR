#include "CXMRPartAssembly.h"
#include "CXMRUsbPortTarget.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/ChildActorComponent.h"
#include "EngineUtils.h"

namespace
{
	bool IsOwnedDescendant(const AActor* Actor, const AActor* Assembly)
	{
		TSet<const AActor*> Visited;
		TArray<const AActor*> Pending;
		if (Actor->GetOwner())
		{
			Pending.Add(Actor->GetOwner());
		}
		if (Actor->GetParentActor())
		{
			Pending.Add(Actor->GetParentActor());
		}
		while (!Pending.IsEmpty())
		{
			const AActor* Parent = Pending.Pop(EAllowShrinking::No);
			if (Parent == Assembly)
			{
				return true;
			}
			if (Visited.Contains(Parent))
			{
				continue;
			}
			Visited.Add(Parent);
			if (Parent->GetOwner())
			{
				Pending.Add(Parent->GetOwner());
			}
			if (Parent->GetParentActor())
			{
				Pending.Add(Parent->GetParentActor());
			}
		}
		return false;
	}
} // namespace

ACXMRPartAssembly::ACXMRPartAssembly()
{
	PrimaryActorTick.bCanEverTick = false;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("AssemblyRoot")));
}

void ACXMRPartAssembly::CollectOwnedActors(TArray<AActor*>& OutActors) const
{
	OutActors.Reset();
	if (!GetWorld())
	{
		return;
	}
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (*It != this && IsValid(*It) && IsOwnedDescendant(*It, this))
		{
			OutActors.Add(*It);
		}
	}
}

void ACXMRPartAssembly::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	SetAssemblyActive(false);
}

void ACXMRPartAssembly::SetAssemblyActive(bool bActive)
{
	TArray<AActor*> Actors;
	CollectOwnedActors(Actors);
	Actors.Insert(this, 0);
	for (AActor* Actor : Actors)
	{
		if (!IsValid(Actor))
		{
			continue;
		}
		if (!Activity.ContainsByPredicate([Actor](const FActorActivity& State) { return State.Actor.Get() == Actor; }))
		{
			FActorActivity State;
			State.Actor = Actor;
			State.bHidden = Actor->IsHidden();
			State.bCollision = Actor->GetActorEnableCollision();
			State.bTick = Actor->IsActorTickEnabled();
			if (const ACXMRUsbPortTarget* Port = Cast<ACXMRUsbPortTarget>(Actor))
			{
				State.bContactEnabled = Port->IsContactEnabled();
			}
			Activity.Add(State);
		}
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			if (!ComponentActivity.ContainsByPredicate(
					[Component](const FComponentActivity& State) { return State.Component.Get() == Component; }))
			{
				ComponentActivity.Add({Component, Component->IsComponentTickEnabled()});
			}
		}
	}
	// 준비 중에는 접촉·충돌·피드백을 모두 차단함.
	for (const FActorActivity& State : Activity)
	{
		AActor* Actor = State.Actor.Get();
		if (!IsValid(Actor) || (Actor != this && !IsOwnedDescendant(Actor, this)))
		{
			continue;
		}
		Actor->SetActorHiddenInGame(!bActive || State.bHidden);
		Actor->SetActorEnableCollision(bActive && State.bCollision);

		if (ACXMRUsbPortTarget* Port = Cast<ACXMRUsbPortTarget>(Actor))
		{
			Port->SetContactEnabled(bActive && State.bContactEnabled && !State.bHidden);
		}
	}
	for (const FActorActivity& State : Activity)
	{
		if (AActor* Actor = State.Actor.Get())
		{
			Actor->SetActorTickEnabled(bActive && State.bTick);
		}
	}
	for (const FComponentActivity& State : ComponentActivity)
	{
		if (UActorComponent* Component = State.Component.Get())
		{
			Component->SetComponentTickEnabled(bActive && State.bTick);
		}
	}
	bAssemblyActive = bActive;
}

bool ACXMRPartAssembly::PrepareForReview(FString& OutError)
{
	SetAssemblyActive(false);
	if (!GetRootComponent())
	{
		OutError = TEXT("Assembly has no root component.");
		return false;
	}
	TArray<AActor*> Attached;
	GetAttachedActors(Attached, true, true);
	for (AActor* Actor : Attached)
	{
		if (!IsValid(Actor) || !IsOwnedDescendant(Actor, this))
		{
			OutError = TEXT("Assembly children must be owned by the assembly hierarchy.");
			return false;
		}
	}
	bool bHaveGeometry = false;
	for (const FActorActivity& State : Activity)
	{
		AActor* Actor = State.Actor.Get();
		if (!IsValid(Actor) || !Actor->GetActorTransform().IsValid())
		{
			OutError = TEXT("Assembly contains an invalid actor or transform.");
			return false;
		}

		TArray<USceneComponent*> Scenes;
		Actor->GetComponents(Scenes);
		for (USceneComponent* Scene : Scenes)
		{
			if (!Scene->GetRelativeTransform().IsValid())
			{
				OutError = TEXT("Assembly contains an invalid component transform.");
				return false;
			}
			if (!Actor->IsA<ACXMRUsbPortTarget>())
			{
				if (const UStaticMeshComponent* Mesh = Cast<UStaticMeshComponent>(Scene))
				{
					bHaveGeometry |= Mesh->GetStaticMesh() != nullptr;
				}
				if (const USkeletalMeshComponent* Mesh = Cast<USkeletalMeshComponent>(Scene))
				{
					bHaveGeometry |= Mesh->GetSkeletalMeshAsset() != nullptr;
				}
			}
			if (UChildActorComponent* Child = Cast<UChildActorComponent>(Scene))
			{
				if (Child->GetChildActorClass() && !IsValid(Child->GetChildActor()))
				{
					OutError = TEXT("Assembly child actor failed to spawn.");
					return false;
				}
			}
		}
	}
	if (!bHaveGeometry)
	{
		OutError = TEXT("Assembly has no loaded geometry.");
		return false;
	}
	OutError.Reset();
	return true;
}

void ACXMRPartAssembly::DestroyOwnedChildren()
{
	SetAssemblyActive(false);
	TArray<AActor*> OwnedChildren;
	CollectOwnedActors(OwnedChildren);
	for (int32 Index = OwnedChildren.Num() - 1; Index >= 0; --Index)
	{
		if (IsValid(OwnedChildren[Index]))
		{
			OwnedChildren[Index]->SetActorHiddenInGame(true);
			OwnedChildren[Index]->SetActorEnableCollision(false);
			OwnedChildren[Index]->SetActorTickEnabled(false);
			OwnedChildren[Index]->Destroy();
		}
	}
}

void ACXMRPartAssembly::EndPlay(const EEndPlayReason::Type Reason)
{
	DestroyOwnedChildren();
	Super::EndPlay(Reason);
}
