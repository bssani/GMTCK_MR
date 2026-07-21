// Copyright GMTCK CX.

#include "CXMRSceneObjectComponent.h"
#include "CXMRSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/GameInstance.h"

UCXMRSceneObjectComponent::UCXMRSceneObjectComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UCXMRSubsystem* UCXMRSceneObjectComponent::GetCXMR() const
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

void UCXMRSceneObjectComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = GetCXMR();

	if (Role == ECXMRSceneRole::VROnly && Subsystem)
	{
		Subsystem->OnVRBackgroundChanged.AddDynamic(this, &UCXMRSceneObjectComponent::HandleVRBackgroundChanged);
	}

	// The subsystem is the source of truth, so a level loaded while MR is already on comes up correct.
	ApplyRole();
}

void UCXMRSceneObjectComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnVRBackgroundChanged.RemoveDynamic(this, &UCXMRSceneObjectComponent::HandleVRBackgroundChanged);
	}
	Super::EndPlay(Reason);
}

void UCXMRSceneObjectComponent::HandleVRBackgroundChanged(bool bVisible)
{
	if (AActor* Owner = GetOwner())
	{
		Owner->SetActorHiddenInGame(!bVisible);
	}
}

void UCXMRSceneObjectComponent::ApplyRole()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	switch (Role)
	{
	case ECXMRSceneRole::VROnly:
		// Visible unless the subsystem says the virtual room is currently hidden.
		Owner->SetActorHiddenInGame(Subsystem ? !Subsystem->IsVRBackgroundVisible() : false);
		break;

	case ECXMRSceneRole::MaskMesh:
		ApplyMaskRenderFlags();
		break;
	}
}

void UCXMRSceneObjectComponent::ApplyMaskRenderFlags()
{
	TArray<UPrimitiveComponent*> Primitives;
	GetOwner()->GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		// Written into the Custom-Depth buffer, which is all PP_MR reads...
		Primitive->SetRenderCustomDepth(true);

		// ...and absent from every other pass, so the mask itself is never seen or lit.
		// Missing bRenderInMainPass is the one that silently leaves the mask geometry visible.
		Primitive->SetRenderInMainPass(false);
		Primitive->SetRenderInDepthPass(false);
		Primitive->SetCastShadow(false);
		Primitive->SetVisibleInRayTracing(false);
		Primitive->SetReceivesDecals(false);

		// No setters for these two — assign and dirty the render state below.
		Primitive->bVisibleInReflectionCaptures  = false;
		Primitive->bVisibleInRealTimeSkyCaptures = false;

		Primitive->MarkRenderStateDirty();
	}
}
