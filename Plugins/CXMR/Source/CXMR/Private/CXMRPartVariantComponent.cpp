#include "CXMRPartVariantComponent.h"
#include "CXMRDesignOption.h"
#include "CXMRPartAssembly.h"
#include "CXMRPartSlotComponent.h"
#include "Engine/World.h"
#include "Templates/UnrealTemplate.h"

UCXMRPartVariantComponent::UCXMRPartVariantComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}
void UCXMRPartVariantComponent::OnRegister()
{
	Super::OnRegister();
	if (IsValid(GetOwner()))
	{
		GetOwner()->OnDestroyed.AddUniqueDynamic(this, &UCXMRPartVariantComponent::OnOwnerDestroyed);
	}
}
void UCXMRPartVariantComponent::OnOwnerDestroyed(AActor* DestroyedOwner)
{
	ClearAll();
}
void UCXMRPartVariantComponent::InitializeOptions(const TArray<TSoftObjectPtr<UCXMRDesignOption>>& InOptions)
{
	ClearAll();
	Options = InOptions;
	RemovedSlots.Reset();
	LastError.Reset();
}
bool UCXMRPartVariantComponent::Fail(const FString& Message)
{
	LastError = Message;
	UE_LOG(LogTemp, Warning, TEXT("CXMR part option rejected: %s"), *Message);
	return false;
}
UCXMRPartSlotComponent* UCXMRPartVariantComponent::ResolveSlot(FName SlotId)
{
	if (!IsValid(GetOwner()) || SlotId.IsNone() || RemovedSlots.Contains(SlotId))
	{
		Fail(TEXT("Part slot is missing or removed."));
		return nullptr;
	}
	TArray<UCXMRPartSlotComponent*> Slots;
	GetOwner()->GetComponents(Slots);
	TSet<FName> Ids;
	UCXMRPartSlotComponent* Found = nullptr;
	for (UCXMRPartSlotComponent* Slot : Slots)
	{
		if (!IsValid(Slot) || !Slot->IsRegistered() || Slot->SlotId.IsNone() || Ids.Contains(Slot->SlotId))
		{
			Fail(TEXT("Vehicle has an invalid or duplicate part slot ID."));
			return nullptr;
		}
		Ids.Add(Slot->SlotId);
		if (Slot->SlotId == SlotId)
		{
			Found = Slot;
		}
	}
	if (!Found || !Found->GetComponentTransform().IsValid() ||
		Found->GetComponentTransform().GetScale3D().GetAbsMin() <= SMALL_NUMBER)
	{
		Fail(TEXT("Requested part slot is missing or has an invalid transform."));
		return nullptr;
	}
	return Found;
}
bool UCXMRPartVariantComponent::SelectOption(UCXMRDesignOption* Option)
{
	if (bSelecting)
	{
		return Fail(TEXT("Part selection is already in progress."));
	}
	TGuardValue<bool> Guard(bSelecting, true);
	FString Error;
	if (!IsValid(Option))
	{
		return Fail(TEXT("Design option is missing."));
	}
	if (!Option->Validate(Error))
	{
		return Fail(Error);
	}
	TSet<FName> OptionIds;
	bool bListed = false;
	for (const TSoftObjectPtr<UCXMRDesignOption>& Reference : Options)
	{
		UCXMRDesignOption* Entry = Reference.LoadSynchronous();
		if (!IsValid(Entry) || !Entry->Validate(Error))
		{
			return Fail(IsValid(Entry) ? Error : TEXT("Vehicle has an unavailable design option."));
		}
		if (OptionIds.Contains(Entry->OptionId))
		{
			return Fail(TEXT("Vehicle has duplicate design option IDs."));
		}
		OptionIds.Add(Entry->OptionId);
		bListed |= Entry == Option;
	}
	if (!bListed)
	{
		return Fail(TEXT("Design option is not in this vehicle profile."));
	}
	UCXMRPartSlotComponent* Slot = ResolveSlot(Option->SlotId);
	if (!Slot)
	{
		return false;
	}
	UClass* AssemblyClass = Option->AssemblyActor.LoadSynchronous();
	if (!AssemblyClass || !AssemblyClass->IsChildOf(ACXMRPartAssembly::StaticClass()) ||
		AssemblyClass->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
	{
		return Fail(TEXT("Design option assembly class cannot be spawned."));
	}
	UWorld* World = GetWorld();
	if (!World)
	{
		return Fail(TEXT("Vehicle has no world."));
	}
	const FTransform WorldTransform = Option->SlotLocalOffset * Slot->GetComponentTransform();
	ACXMRPartAssembly* Candidate = World->SpawnActorDeferred<ACXMRPartAssembly>(
		AssemblyClass, WorldTransform, GetOwner(), nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Candidate)
	{
		return Fail(TEXT("Design option assembly spawn failed."));
	}

	Candidate->FinishSpawning(WorldTransform);
	if (!IsValid(Candidate) || !Candidate->AttachToComponent(Slot, FAttachmentTransformRules::KeepWorldTransform))
	{
		if (IsValid(Candidate))
		{
			Candidate->DestroyOwnedChildren();
			Candidate->Destroy();
		}
		return Fail(TEXT("Design option assembly attachment failed."));
	}
	Candidate->SetActorRelativeTransform(Option->SlotLocalOffset);
	if (!Candidate->PrepareForReview(Error))
	{
		Candidate->DestroyOwnedChildren();
		Candidate->Destroy();
		return Fail(Error);
	}
	// 검증된 후보만 교체함. 차량·앵커 오프셋은 수정하지 않음.
	ClearSlot(Option->SlotId);
	ActiveAssemblies.Add(Option->SlotId, Candidate);
	ActiveOptions.Add(Option->SlotId, Option);
	Candidate->SetAssemblyActive(true);
	LastError.Reset();
	return true;
}
AActor* UCXMRPartVariantComponent::GetActiveAssembly(FName SlotId) const
{
	const TObjectPtr<ACXMRPartAssembly>* Found = ActiveAssemblies.Find(SlotId);
	return Found && IsValid(Found->Get()) ? Found->Get() : nullptr;
}
UCXMRDesignOption* UCXMRPartVariantComponent::GetActiveOption(FName SlotId) const
{
	const TObjectPtr<UCXMRDesignOption>* Found = ActiveOptions.Find(SlotId);
	return GetActiveAssembly(SlotId) && Found ? Found->Get() : nullptr;
}
void UCXMRPartVariantComponent::ClearSlot(FName SlotId)
{
	if (ACXMRPartAssembly* Assembly = Cast<ACXMRPartAssembly>(GetActiveAssembly(SlotId)))
	{
		Assembly->SetAssemblyActive(false);
		Assembly->DestroyOwnedChildren();
		Assembly->Destroy();
	}
	ActiveAssemblies.Remove(SlotId);
	ActiveOptions.Remove(SlotId);
}
bool UCXMRPartVariantComponent::RemoveSlot(FName SlotId)
{
	if (!ResolveSlot(SlotId))
	{
		return false;
	}
	ClearSlot(SlotId);
	RemovedSlots.Add(SlotId);
	LastError.Reset();
	return true;
}
void UCXMRPartVariantComponent::ClearAll()
{
	TArray<FName> Keys;
	ActiveAssemblies.GetKeys(Keys);
	for (FName Id : Keys)
	{
		ClearSlot(Id);
	}
}
void UCXMRPartVariantComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	ClearAll();
	Super::EndPlay(Reason);
}
void UCXMRPartVariantComponent::OnComponentDestroyed(bool bDestroyingHierarchy)
{
	ClearAll();
	Super::OnComponentDestroyed(bDestroyingHierarchy);
}
