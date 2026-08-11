// Copyright GMTCK CX.

#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRMarkerProfile.h"
#include "CXMRPlacementComponent.h"

#include "Components/PrimitiveComponent.h"
#include "Materials/MaterialInterface.h"
#include "GameFramework/Actor.h"
#include "CXMRSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRVehicle, Log, All);

UCXMRVehicleLoaderComponent::UCXMRVehicleLoaderComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UCXMRVehicleLoaderComponent::BeginPlay()
{
	Super::BeginPlay();

	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			Subsystem = GI->GetSubsystem<UCXMRSubsystem>();
		}
	}
	if (Subsystem)
	{
		Subsystem->OnViewerAction.AddDynamic(this, &UCXMRVehicleLoaderComponent::HandleViewerAction);
	}

	if (bLoadOnBeginPlay && Profile)
	{
		LoadVehicle(Profile);
	}
}

void UCXMRVehicleLoaderComponent::HandleViewerAction(ECXMRViewerAction Action)
{
	switch (Action)
	{
	case ECXMRViewerAction::NextVehicle:     NextVehicle();     break;
	case ECXMRViewerAction::PreviousVehicle: PreviousVehicle(); break;
	case ECXMRViewerAction::NextTrim:        NextTrim();        break;
	case ECXMRViewerAction::PreviousTrim:    PreviousTrim();    break;
	case ECXMRViewerAction::NextCMF:         NextCMF();         break;
	default: break;   // rotation belongs to the turntable
	}
}

void UCXMRVehicleLoaderComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnViewerAction.RemoveDynamic(this, &UCXMRVehicleLoaderComponent::HandleViewerAction);
	}
	UnloadVehicle();
	Super::EndPlay(Reason);
}

USceneComponent* UCXMRVehicleLoaderComponent::ResolveAttachTarget()
{
	if (AttachTarget)
	{
		return AttachTarget;
	}
	// On ACXMRVehicleRoot the root component IS VehicleAnchor, the transform calibration writes to.
	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}

void UCXMRVehicleLoaderComponent::LoadVehicle(UCXMRVehicleProfile* NewProfile)
{
	UnloadVehicle();

	Profile = NewProfile;
	if (!Profile)
	{
		return;
	}

	UClass* VehicleClass = Profile->VehicleActor.LoadSynchronous();
	if (!VehicleClass)
	{
		UE_LOG(LogCXMRVehicle, Warning, TEXT("Vehicle profile '%s' has no VehicleActor set."), *Profile->GetName());
		return;
	}

	USceneComponent* Target = ResolveAttachTarget();
	UWorld* World = GetWorld();
	if (!Target || !World)
	{
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	SpawnedVehicle = World->SpawnActor<AActor>(VehicleClass, Target->GetComponentTransform(), Params);
	if (!SpawnedVehicle)
	{
		return;
	}

	// Attached to the anchor, then offset locally — the anchor's own transform stays exactly as
	// calibration left it, which is what makes swapping free of recalibration.
	SpawnedVehicle->AttachToComponent(Target, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	SpawnedVehicle->SetActorRelativeTransform(Profile->VehicleRootOffset);

	SyncMarkerProfile();
	SyncVehicleIndex();

	TrimIndex = 0;
	CMFIndex  = ResolveDefaultCMF(0);
	ApplyTrim();
	ApplyCMF();

	ReportStatus();
	OnVehicleLoaded.Broadcast(Profile);
}

void UCXMRVehicleLoaderComponent::UnloadVehicle()
{
	if (SpawnedVehicle)
	{
		SpawnedVehicle->Destroy();
		SpawnedVehicle = nullptr;
	}
}

void UCXMRVehicleLoaderComponent::SyncMarkerProfile()
{
	// The vehicle carries its own calibration config, so switching vehicle switches marker IDs with it.
	if (Profile->MarkerProfile.IsNull())
	{
		return;
	}
	if (AActor* Owner = GetOwner())
	{
		if (UCXMRPlacementComponent* Placement = Owner->FindComponentByClass<UCXMRPlacementComponent>())
		{
			Placement->MarkerProfile = Profile->MarkerProfile.LoadSynchronous();
		}
	}
}

void UCXMRVehicleLoaderComponent::SyncVehicleIndex()
{
	if (!Catalog || !Profile)
	{
		return;
	}

	// Compare by path so an unloaded catalog entry still matches the loaded profile.
	const FSoftObjectPath Wanted(Profile);
	const int32 Found = Catalog->Vehicles.IndexOfByPredicate(
		[&Wanted](const TSoftObjectPtr<UCXMRVehicleProfile>& Entry) { return Entry.ToSoftObjectPath() == Wanted; });

	if (Found != INDEX_NONE)
	{
		VehicleIndex = Found;
	}
}

int32 UCXMRVehicleLoaderComponent::ResolveDefaultCMF(int32 InTrimIndex) const
{
	if (!Profile || !Profile->IsValidTrim(InTrimIndex))
	{
		return 0;
	}
	const FCXMRTrim& Trim = Profile->Trims[InTrimIndex];
	return Trim.CMFOptions.IsValidIndex(Trim.DefaultCMF) ? Trim.DefaultCMF : 0;
}

void UCXMRVehicleLoaderComponent::SetTrim(int32 InTrimIndex)
{
	if (!Profile || !Profile->IsValidTrim(InTrimIndex))
	{
		return;
	}
	TrimIndex = InTrimIndex;
	CMFIndex  = ResolveDefaultCMF(TrimIndex);
	ApplyTrim();
	ApplyCMF();
	ReportStatus();
}

void UCXMRVehicleLoaderComponent::SetCMF(int32 InCMFIndex)
{
	if (!Profile || !Profile->IsValidTrim(TrimIndex))
	{
		return;
	}
	if (!Profile->Trims[TrimIndex].CMFOptions.IsValidIndex(InCMFIndex))
	{
		return;
	}
	CMFIndex = InCMFIndex;
	ApplyCMF();
	ReportStatus();
}

void UCXMRVehicleLoaderComponent::ReportStatus()
{
	if (!Subsystem)
	{
		return;
	}

	const int32 VehicleCount = Catalog ? Catalog->Vehicles.Num() : 0;
	const int32 TrimCount    = Profile ? Profile->Trims.Num() : 0;

	FText TrimName = FText::GetEmpty();
	if (Profile && Profile->IsValidTrim(TrimIndex))
	{
		TrimName = FText::FromName(Profile->Trims[TrimIndex].Name);
	}

	const FText VehicleName = Profile ? Profile->DisplayName : FText::GetEmpty();

	Subsystem->ReportVehicleStatus(VehicleName, VehicleIndex, VehicleCount,
		TrimName, TrimIndex, TrimCount, CMFIndex);
}

// ---------- Cycling ----------
//
// Wrapping is what makes a stick flick usable: the list has no ends to get stuck against.

void UCXMRVehicleLoaderComponent::CycleVehicle(int32 Step)
{
	if (!Catalog || Catalog->Vehicles.Num() == 0)
	{
		return;
	}

	const int32 Count = Catalog->Vehicles.Num();
	VehicleIndex = ((VehicleIndex + Step) % Count + Count) % Count;   // negative-safe modulo

	if (UCXMRVehicleProfile* Next = Catalog->Vehicles[VehicleIndex].LoadSynchronous())
	{
		LoadVehicle(Next);
	}
}

void UCXMRVehicleLoaderComponent::CycleTrim(int32 Step)
{
	if (!Profile || Profile->Trims.Num() == 0)
	{
		return;
	}

	const int32 Count = Profile->Trims.Num();
	SetTrim(((TrimIndex + Step) % Count + Count) % Count);
}

void UCXMRVehicleLoaderComponent::NextVehicle()     { CycleVehicle(1); }
void UCXMRVehicleLoaderComponent::PreviousVehicle() { CycleVehicle(-1); }
void UCXMRVehicleLoaderComponent::NextTrim()        { CycleTrim(1); }
void UCXMRVehicleLoaderComponent::PreviousTrim()    { CycleTrim(-1); }

void UCXMRVehicleLoaderComponent::NextCMF()
{
	if (!Profile || !Profile->IsValidTrim(TrimIndex))
	{
		return;
	}
	const int32 Count = Profile->Trims[TrimIndex].CMFOptions.Num();
	if (Count > 0)
	{
		SetCMF((CMFIndex + 1) % Count);
	}
}

void UCXMRVehicleLoaderComponent::ApplyTrim()
{
	if (!SpawnedVehicle || !Profile || !Profile->IsValidTrim(TrimIndex))
	{
		return;
	}

	// Only tags some trim mentions are managed; anything untagged is shared geometry and stays visible.
	const TSet<FName> Managed = Profile->GetManagedPartTags();
	const TArray<FName>& Visible = Profile->Trims[TrimIndex].VisibleParts;

	TArray<UPrimitiveComponent*> Primitives;
	SpawnedVehicle->GetComponents<UPrimitiveComponent>(Primitives);

	for (UPrimitiveComponent* Primitive : Primitives)
	{
		bool bManaged = false;
		bool bShow    = false;

		for (const FName& Tag : Primitive->ComponentTags)
		{
			if (Managed.Contains(Tag))
			{
				bManaged = true;
				if (Visible.Contains(Tag))
				{
					bShow = true;
					break;
				}
			}
		}

		if (bManaged)
		{
			Primitive->SetHiddenInGame(!bShow);
			Primitive->SetVisibility(bShow, /*bPropagateToChildren*/ false);
		}
	}
}

void UCXMRVehicleLoaderComponent::ApplyCMF()
{
	if (!SpawnedVehicle || !Profile || !Profile->IsValidTrim(TrimIndex))
	{
		return;
	}

	const FCXMRTrim& Trim = Profile->Trims[TrimIndex];
	if (!Trim.CMFOptions.IsValidIndex(CMFIndex))
	{
		return;
	}

	TArray<UPrimitiveComponent*> Primitives;
	SpawnedVehicle->GetComponents<UPrimitiveComponent>(Primitives);

	for (const FCXMRMaterialOverride& Override : Trim.CMFOptions[CMFIndex].Materials)
	{
		UMaterialInterface* Material = Override.Material.LoadSynchronous();
		if (!Material)
		{
			continue;
		}

		for (UPrimitiveComponent* Primitive : Primitives)
		{
			// A None tag means "the whole vehicle" — the common case for paint.
			if (Override.PartTag.IsNone() || Primitive->ComponentTags.Contains(Override.PartTag))
			{
				Primitive->SetMaterial(Override.MaterialSlot, Material);
			}
		}
	}
}
