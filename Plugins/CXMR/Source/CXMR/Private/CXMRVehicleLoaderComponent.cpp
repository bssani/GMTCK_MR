// Copyright GMTCK CX.

#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRMarkerProfile.h"
#include "CXMRPlacementComponent.h"
#include "CXMRErgonomicsComponent.h"

#include "CXMRPartVariantComponent.h"
#include "CXMRPartAssembly.h"
#include "CXMRDesignOption.h"
#include "CXMRUsbPortTarget.h"
#include "Components/SceneComponent.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "CXMRSubsystem.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRVehicle, Log, All);

namespace
{
	bool IsOwnedVehicleActor(const AActor* Actor, const AActor* Vehicle)
	{
		if (Actor == Vehicle) { return true; }
		TSet<const AActor*> Seen;
		TArray<const AActor*> Pending { Actor->GetOwner(), Actor->GetParentActor() };
		while (!Pending.IsEmpty())
		{
			const AActor* Next = Pending.Pop(EAllowShrinking::No);
			if (!Next || Seen.Contains(Next)) { continue; }
			if (Next == Vehicle) { return true; }
			Seen.Add(Next);
			Pending.Add(Next->GetOwner());
			Pending.Add(Next->GetParentActor());
		}
		return false;
	}

	TArray<AActor*> GetOwnedVehicleActors(AActor* Vehicle)
	{
		TArray<AActor*> Actors;
		if (!Vehicle || !Vehicle->GetWorld()) { return Actors; }
		if (IsValid(Vehicle)) { Actors.Add(Vehicle); }
		for (TActorIterator<AActor> It(Vehicle->GetWorld()); It; ++It)
		{
			if (*It != Vehicle && IsValid(*It) && IsOwnedVehicleActor(*It, Vehicle)) { Actors.Add(*It); }
		}
		return Actors;
	}

	void DestroyOwnedVehicle(AActor* Vehicle)
	{
		// 외부에서 붙인 액터는 소유하지 않으므로 보존함.
		TArray<AActor*> Actors = GetOwnedVehicleActors(Vehicle);
		for (AActor* Actor : Actors)
		{
			TArray<AActor*> Attached;
			Actor->GetAttachedActors(Attached, true, false);
			for (AActor* Child : Attached)
			{
				if (IsValid(Child) && !IsOwnedVehicleActor(Child, Vehicle)) { Child->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform); }
			}
			Actor->SetActorHiddenInGame(true);
			Actor->SetActorEnableCollision(false);
			Actor->SetActorTickEnabled(false);
			if (ACXMRUsbPortTarget* Port = Cast<ACXMRUsbPortTarget>(Actor)) { Port->SetContactEnabled(false); }
		}
		for (int32 Index = Actors.Num() - 1; Index >= 0; --Index)
		{
			if (IsValid(Actors[Index])) { Actors[Index]->Destroy(); }
		}
	}

	struct FVehicleActivity
	{
		TWeakObjectPtr<AActor> Actor;
		bool bHidden;
		bool bCollision;
		bool bTick;
		bool bContact;
	};

	struct FVehicleComponentActivity
	{
		TWeakObjectPtr<UActorComponent> Component;
		bool bTick;
	};

	FVehicleActivity CaptureActivity(AActor* Actor)
	{
		ACXMRUsbPortTarget* Port = Cast<ACXMRUsbPortTarget>(Actor);
		return {Actor, Actor->IsHidden(), Actor->GetActorEnableCollision(), Actor->IsActorTickEnabled(), Port && Port->IsContactEnabled()};
	}

	void SetVehicleActivity(const FVehicleActivity& State, bool bActive)
	{
		if (AActor* Actor = State.Actor.Get())
		{
			Actor->SetActorHiddenInGame(!bActive || State.bHidden);
			Actor->SetActorEnableCollision(bActive && State.bCollision);
			Actor->SetActorTickEnabled(bActive && State.bTick);
			if (ACXMRUsbPortTarget* Port = Cast<ACXMRUsbPortTarget>(Actor)) { Port->SetContactEnabled(bActive && State.bContact); }
		}
	}

	bool IsUsableVehicleTransform(const FTransform& Transform)
	{
		const FVector Scale = Transform.GetScale3D();
		return Transform.IsValid() && FMath::Abs(Scale.X) > SMALL_NUMBER && FMath::Abs(Scale.Y) > SMALL_NUMBER && FMath::Abs(Scale.Z) > SMALL_NUMBER;
	}
}

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
	default: break;
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
	TryLoadVehicle(NewProfile);
}

bool UCXMRVehicleLoaderComponent::Fail(const FString& Reason)
{
	LastFailureReason = Reason;
	UE_LOG(LogCXMRVehicle, Warning, TEXT("%s"), *Reason);
	return false;
}

bool UCXMRVehicleLoaderComponent::TryLoadVehicle(UCXMRVehicleProfile* NewProfile)
{
	if (!IsValid(NewProfile))
	{
		return Fail(TEXT("No vehicle profile supplied."));
	}

	UClass* VehicleClass = NewProfile->VehicleActor.LoadSynchronous();
	if (!VehicleClass || !VehicleClass->IsChildOf(AActor::StaticClass()) || VehicleClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return Fail(FString::Printf(TEXT("Vehicle profile '%s' has no loadable vehicle actor."), *NewProfile->GetName()));
	}

	if (!IsUsableVehicleTransform(NewProfile->VehicleRootOffset))
	{
		return Fail(TEXT("Vehicle root offset must be finite with normalized rotation and nonzero scale."));
	}

	USceneComponent* Target = ResolveAttachTarget();
	UWorld* World = GetWorld();
	if (!IsValid(Target) || !World || !IsUsableVehicleTransform(Target->GetComponentTransform()))
	{
		return Fail(TEXT("Vehicle world or anchor is unavailable."));
	}

	const FTransform CandidateTransform = NewProfile->VehicleRootOffset * Target->GetComponentTransform();
	if (!IsUsableVehicleTransform(CandidateTransform)) { return Fail(TEXT("Vehicle offset and anchor produce an invalid transform.")); }
	AActor* Candidate = World->SpawnActorDeferred<AActor>(VehicleClass, CandidateTransform, GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!IsValid(Candidate)) { return Fail(TEXT("Vehicle candidate could not be spawned.")); }

	// 동기 생성이 끝난 실제 설정을 캡처함. 준비와 교체 사이에는 프레임을 넘기지 않음.
	Candidate->FinishSpawning(CandidateTransform);
	if (!IsValid(Candidate))
	{
		DestroyOwnedVehicle(Candidate);
		return Fail(TEXT("Vehicle candidate was destroyed during construction."));
	}

	if (ACXMRPartAssembly* Assembly = Cast<ACXMRPartAssembly>(Candidate)) { Assembly->SetAssemblyActive(true); }

	TArray<FVehicleActivity> Activity;
	TArray<FVehicleComponentActivity> ComponentActivity;
	for (AActor* Actor : GetOwnedVehicleActors(Candidate))
	{
		const FVehicleActivity State = CaptureActivity(Actor);
		Activity.Add(State);
		SetVehicleActivity(State, false);
		TArray<UActorComponent*> Components;
		Actor->GetComponents(Components);
		for (UActorComponent* Component : Components)
		{
			ComponentActivity.Add({Component, Component->IsComponentTickEnabled()});
			Component->SetComponentTickEnabled(false);
		}
	}

	if (!Candidate->GetRootComponent() || !Candidate->AttachToComponent(Target, FAttachmentTransformRules::KeepWorldTransform))
	{
		DestroyOwnedVehicle(Candidate);
		return Fail(TEXT("Vehicle candidate has no attachable root component."));
	}
	Candidate->SetActorRelativeTransform(NewProfile->VehicleRootOffset);
	for (const FVehicleActivity& State : Activity)
	{
		AActor* Actor = State.Actor.Get();
		if (!IsValid(Actor) || !IsUsableVehicleTransform(Actor->GetActorTransform()))
		{
			DestroyOwnedVehicle(Candidate);
			return Fail(TEXT("Vehicle contains an invalid actor transform."));
		}
		TArray<USceneComponent*> Scenes;
		Actor->GetComponents(Scenes);
		for (USceneComponent* Scene : Scenes)
		{
			if (!IsUsableVehicleTransform(Scene->GetRelativeTransform()))
			{
				DestroyOwnedVehicle(Candidate);
				return Fail(TEXT("Vehicle contains an invalid component transform."));
			}
		}
	}

	UCXMRPartVariantComponent* CandidateVariants = Candidate->FindComponentByClass<UCXMRPartVariantComponent>();
	if (!CandidateVariants)
	{
		CandidateVariants = NewObject<UCXMRPartVariantComponent>(Candidate);
		Candidate->AddInstanceComponent(CandidateVariants);
		CandidateVariants->RegisterComponent();
	}
	if (!IsValid(CandidateVariants) || !CandidateVariants->IsRegistered())
	{
		DestroyOwnedVehicle(Candidate);
		return Fail(TEXT("Vehicle part controller could not be registered."));
	}
	CandidateVariants->InitializeOptions(NewProfile->DesignOptions);

	// 새 차량 준비 후 기존 차량과 선택 상태 교체함.
	UnloadVehicle();
	SpawnedVehicle = Candidate;
	PartVariants = CandidateVariants;
	Profile = NewProfile;
	for (const FVehicleActivity& State : Activity) { SetVehicleActivity(State, true); }
	for (const FVehicleComponentActivity& State : ComponentActivity)
	{
		if (UActorComponent* Component = State.Component.Get()) { Component->SetComponentTickEnabled(State.bTick); }
	}

	SyncMarkerProfile();
	SyncVehicleIndex();


	// 차량 교체 시 착좌 프로필과 순번도 갱신함.

	if (AActor* Owner = GetOwner())
	{
		if (UCXMRErgonomicsComponent* Ergonomics = Owner->FindComponentByClass<UCXMRErgonomicsComponent>())
		{
			Ergonomics->RefreshForNewVehicle();
		}
	}

	LastFailureReason.Reset();
	ReportStatus();
	OnVehicleLoaded.Broadcast(Profile);
	return true;
}

void UCXMRVehicleLoaderComponent::UnloadVehicle()
{
	PartVariants = nullptr;
	if (SpawnedVehicle)
	{
		DestroyOwnedVehicle(SpawnedVehicle);
		SpawnedVehicle = nullptr;
	}
	ReportStatus();
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

void UCXMRVehicleLoaderComponent::ReportStatus()
{
	if (!Subsystem)
	{
		return;
	}

	Subsystem->ReportVehicleStatus(IsValid(SpawnedVehicle) && Profile ? Profile->DisplayName : FText::GetEmpty(), IsValid(SpawnedVehicle) ? VehicleIndex : INDEX_NONE,
		Catalog ? Catalog->Vehicles.Num() : 0);
}

bool UCXMRVehicleLoaderComponent::SelectVehicle(int32 Index)
{
	if (!Catalog || !Catalog->Vehicles.IsValidIndex(Index))
	{
		return Fail(TEXT("Vehicle catalog index is invalid."));
	}
	UCXMRVehicleProfile* Selected = Catalog->Vehicles[Index].LoadSynchronous();
	if (!Selected) { return Fail(TEXT("Vehicle catalog profile cannot be loaded.")); }
	if (!TryLoadVehicle(Selected)) { return false; }
	VehicleIndex = Index;
	ReportStatus();
	return true;
}

bool UCXMRVehicleLoaderComponent::SelectDesignOption(UCXMRDesignOption* Option)
{
	if (!IsValid(SpawnedVehicle) || !IsValid(PartVariants))
	{
		return Fail(TEXT("Load a vehicle before selecting a design option."));
	}
	if (!PartVariants->SelectOption(Option))
	{
		return Fail(PartVariants->GetLastFailureReason());
	}
	LastFailureReason.Reset();
	return true;
}

// 차량 순환

// 목록 끝에서는 처음 항목으로 돌아감.

void UCXMRVehicleLoaderComponent::CycleVehicle(int32 Step)
{
	if (!Catalog || Catalog->Vehicles.Num() == 0)
	{
		return;
	}

	const int32 Count = Catalog->Vehicles.Num();
	const int32 NextIndex = ((VehicleIndex + Step) % Count + Count) % Count;

	SelectVehicle(NextIndex);
}

void UCXMRVehicleLoaderComponent::NextVehicle() { CycleVehicle(1); }
void UCXMRVehicleLoaderComponent::PreviousVehicle() { CycleVehicle(-1); }
