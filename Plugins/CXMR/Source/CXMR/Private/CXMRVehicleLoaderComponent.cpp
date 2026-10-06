// Copyright GMTCK CX.

#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRMarkerProfile.h"
#include "CXMRPlacementComponent.h"
#include "CXMRErgonomicsComponent.h"

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
	default: break;   // 회전은 턴테이블에서 처리함.
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
	// VehicleRoot의 루트가 보정 기준 앵커임.
	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}

void UCXMRVehicleLoaderComponent::LoadVehicle(UCXMRVehicleProfile* NewProfile)
{
	if (!NewProfile)
	{
		UE_LOG(LogCXMRVehicle, Warning, TEXT("Cannot replace vehicle: no profile supplied."));
		return;
	}

	UClass* VehicleClass = NewProfile->VehicleActor.LoadSynchronous();
	if (!VehicleClass)
	{
		UE_LOG(LogCXMRVehicle, Warning, TEXT("Vehicle profile '%s' has no loadable VehicleActor. Keeping current vehicle."), *NewProfile->GetName());
		return;
	}

	USceneComponent* Target = ResolveAttachTarget();
	UWorld* World = GetWorld();
	if (!Target || !World)
	{
		UE_LOG(LogCXMRVehicle, Warning, TEXT("Cannot load vehicle '%s': missing world or attach target."), *NewProfile->GetName());
		return;
	}

	FActorSpawnParameters Params;
	Params.Owner = GetOwner();
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	AActor* Candidate = World->SpawnActor<AActor>(VehicleClass, Target->GetComponentTransform(), Params);
	if (!IsValid(Candidate))
	{
		UE_LOG(LogCXMRVehicle, Warning, TEXT("Cannot spawn vehicle '%s'. Keeping current vehicle."), *NewProfile->GetName());
		return;
	}

	// 차량은 앵커 아래에 붙이고 로컬 오프셋만 적용함.

	if (!Candidate->AttachToComponent(Target, FAttachmentTransformRules::SnapToTargetNotIncludingScale))
	{
		Candidate->Destroy();
		UE_LOG(LogCXMRVehicle, Warning, TEXT("Cannot attach vehicle '%s'. Keeping current vehicle."), *NewProfile->GetName());
		return;
	}
	Candidate->SetActorRelativeTransform(NewProfile->VehicleRootOffset);

	// 새 차량 준비 후 기존 차량과 선택 상태 교체함.
	UnloadVehicle();
	SpawnedVehicle = Candidate;
	Profile = NewProfile;

	SyncMarkerProfile();
	SyncVehicleIndex();

	TrimIndex = 0;
	CMFIndex  = ResolveDefaultCMF(0);
	ApplyTrim();
	ApplyCMF();

	// 차량 교체 시 착좌 프로필과 순번도 갱신함.

	if (AActor* Owner = GetOwner())
	{
		if (UCXMRErgonomicsComponent* Ergonomics = Owner->FindComponentByClass<UCXMRErgonomicsComponent>())
		{
			Ergonomics->RefreshForNewVehicle();
		}
	}

	ReportStatus();
	OnVehicleLoaded.Broadcast(Profile);
}

void UCXMRVehicleLoaderComponent::UnloadVehicle()
{
	OriginalMaterials.Reset();
	if (SpawnedVehicle)
	{
		SpawnedVehicle->Destroy();
		SpawnedVehicle = nullptr;
	}
}

void UCXMRVehicleLoaderComponent::SyncMarkerProfile()
{
	// 차량의 마커 프로필로 전환함.
	if (AActor* Owner = GetOwner())
	{
		if (UCXMRPlacementComponent* Placement = Owner->FindComponentByClass<UCXMRPlacementComponent>())
		{
			// Setter로 이전 원본 복원과 새 보정 파일 적용을 함께 처리함.

			Placement->SetMarkerProfile(Profile->MarkerProfile.LoadSynchronous(), true);
		}
	}
}

void UCXMRVehicleLoaderComponent::SyncVehicleIndex()
{
	if (!Catalog || !Profile)
	{
		return;
	}

	// 로드 전 카탈로그 항목도 찾을 수 있게 경로로 비교함.
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

// 차량·트림 순환

// 목록 끝에서는 처음 항목으로 돌아감.

void UCXMRVehicleLoaderComponent::CycleVehicle(int32 Step)
{
	if (!Catalog || Catalog->Vehicles.Num() == 0)
	{
		return;
	}

	const int32 Count = Catalog->Vehicles.Num();
	const int32 NextIndex = ((VehicleIndex + Step) % Count + Count) % Count;

	if (UCXMRVehicleProfile* Next = Catalog->Vehicles[NextIndex].LoadSynchronous())
	{
		LoadVehicle(Next);
	}
	else
	{
		UE_LOG(LogCXMRVehicle, Warning, TEXT("Cannot load vehicle catalog entry %d. Keeping current selection."), NextIndex);
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

	// 트림에서 사용하는 태그만 관리함. 공용 형상은 유지함.
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
	// 새 옵션에 없는 재질은 원래대로 복원함.
	for (const FCXMRCMFOriginalMaterial& Original : OriginalMaterials)
	{
		if (UPrimitiveComponent* Component = Original.Component.Get())
		{
			Component->SetMaterial(Original.Slot, Original.Material);
		}
	}
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
			// 태그가 없으면 차량 전체에 적용함.
			if (Override.PartTag.IsNone() || Primitive->ComponentTags.Contains(Override.PartTag))
			{
				if (Override.MaterialSlot < 0)
				{
					continue;
				}
				if (!OriginalMaterials.ContainsByPredicate([Primitive, &Override](const FCXMRCMFOriginalMaterial& Original)
					{ return Original.Component.Get() == Primitive && Original.Slot == Override.MaterialSlot; }))
				{
					FCXMRCMFOriginalMaterial& Original = OriginalMaterials.AddDefaulted_GetRef();
					Original.Component = Primitive;
					Original.Slot = Override.MaterialSlot;
					Original.Material = Primitive->GetMaterial(Override.MaterialSlot);
				}
				Primitive->SetMaterial(Override.MaterialSlot, Material);
			}
		}
	}
}
