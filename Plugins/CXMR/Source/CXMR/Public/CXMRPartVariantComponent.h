#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRPartVariantComponent.generated.h"
class UCXMRDesignOption;
class UCXMRPartSlotComponent;
class ACXMRPartAssembly;
UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Part Variants")
class CXMR_API UCXMRPartVariantComponent : public UActorComponent
{
	GENERATED_BODY()
public:
	UCXMRPartVariantComponent();
	void InitializeOptions(const TArray<TSoftObjectPtr<UCXMRDesignOption>>& InOptions);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Parts")
	bool SelectOption(UCXMRDesignOption* Option);
	UFUNCTION(BlueprintPure, Category = "CXMR|Parts")
	AActor* GetActiveAssembly(FName SlotId) const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Parts")
	UCXMRDesignOption* GetActiveOption(FName SlotId) const;
	UFUNCTION(BlueprintCallable, Category = "CXMR|Parts")
	void ClearSlot(FName SlotId);
	UFUNCTION(
		BlueprintCallable, Category = "CXMR|Parts",
		meta = (ToolTip =
					"Remove this slot from review for the current vehicle instance without moving its authored mount."))
	bool RemoveSlot(FName SlotId);
	UFUNCTION(BlueprintPure, Category = "CXMR|Parts")
	FString GetLastFailureReason() const
	{
		return LastError;
	}
	UPROPERTY(BlueprintReadOnly, Transient, Category = "CXMR|Parts")
	FString LastError;

protected:
	virtual void OnRegister() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	virtual void OnComponentDestroyed(bool bDestroyingHierarchy) override;

private:
	UFUNCTION()
	void OnOwnerDestroyed(AActor* DestroyedOwner);
	UCXMRPartSlotComponent* ResolveSlot(FName SlotId);
	void ClearAll();
	bool Fail(const FString& Message);
	UPROPERTY(Transient)
	TArray<TSoftObjectPtr<UCXMRDesignOption>> Options;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<ACXMRPartAssembly>> ActiveAssemblies;
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UCXMRDesignOption>> ActiveOptions;
	TSet<FName> RemovedSlots;
	bool bSelecting = false;
};
