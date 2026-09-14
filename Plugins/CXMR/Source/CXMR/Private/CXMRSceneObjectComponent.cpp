// Copyright GMTCK CX.

#include "CXMRSceneObjectComponent.h"
#include "CXMRSubsystem.h"

#include "Components/ExponentialHeightFogComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/VolumetricCloudComponent.h"
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
	ApplyVROnlyVisibility(bVisible);
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
		ApplyVROnlyVisibility(Subsystem ? Subsystem->IsVRBackgroundVisible() : true);
		break;

	case ECXMRSceneRole::MaskMesh:
		ApplyMaskRenderFlags();
		break;
	}
}

void UCXMRSceneObjectComponent::ApplyVROnlyVisibility(bool bVisible)
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	// Take the virtual room out of the MAIN VIEW only. This used to hide the whole actor, which also took it
	// out of every lighting input: a real-time sky light re-captured a scene with no sky, ambient fell to
	// zero, and every mesh went dark the moment MR came on. With main pass off the sky, fog, clouds and room
	// still feed the sky light, reflections and shadows, but draw no pixels of their own — those pixels stay
	// empty, and empty is what lets passthrough show.
	// (Varjo's sample gets away with hiding because its lighting is baked and its sky light never re-captures.)
	TInlineComponentArray<UActorComponent*> Components(Owner);
	for (UActorComponent* Component : Components)
	{
		if (USkyAtmosphereComponent* Sky = Cast<USkyAtmosphereComponent>(Component))
		{
			Sky->SetRenderInMainPass(bVisible);
		}
		else if (UExponentialHeightFogComponent* Fog = Cast<UExponentialHeightFogComponent>(Component))
		{
			Fog->SetRenderInMainPass(bVisible);
		}
		else if (UVolumetricCloudComponent* Cloud = Cast<UVolumetricCloudComponent>(Component))
		{
			Cloud->SetRenderInMainPass(bVisible);
		}
		else if (UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component))
		{
			Primitive->SetRenderInMainPass(bVisible);
			// Depth as well: an unseen floor still writing depth would occlude real objects in Varjo's depth
			// test and hand PP_MR a SceneDepth the mask would have to beat.
			Primitive->SetRenderInDepthPass(bVisible);
		}
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
