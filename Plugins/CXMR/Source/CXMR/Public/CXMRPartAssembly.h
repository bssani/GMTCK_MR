#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CXMRPartAssembly.generated.h"
UCLASS(Blueprintable, DisplayName = "CXMR Part Assembly")
class CXMR_API ACXMRPartAssembly : public AActor
{
	GENERATED_BODY()
public:
	ACXMRPartAssembly();
	bool PrepareForReview(FString& OutError);
	void SetAssemblyActive(bool bActive);
	void DestroyOwnedChildren();
	UFUNCTION(BlueprintPure, Category = "CXMR|Parts")
	bool IsAssemblyActive() const
	{
		return bAssemblyActive;
	}

protected:
	virtual void PostInitializeComponents() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void CollectOwnedActors(TArray<AActor*>& OutActors) const;
	struct FActorActivity
	{
		TWeakObjectPtr<AActor> Actor;
		bool bHidden = false;
		bool bCollision = true;
		bool bTick = false;
		bool bContactEnabled = true;
	};
	struct FComponentActivity
	{
		TWeakObjectPtr<UActorComponent> Component;
		bool bTick = false;
	};
	TArray<FActorActivity> Activity;
	TArray<FComponentActivity> ComponentActivity;
	bool bAssemblyActive = false;
};
