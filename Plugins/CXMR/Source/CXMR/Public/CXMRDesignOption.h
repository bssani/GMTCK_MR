#pragma once
#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CXMRDesignOption.generated.h"
class ACXMRPartAssembly;
UCLASS(BlueprintType, DisplayName = "CXMR Design Option")
class CXMR_API UCXMRDesignOption : public UPrimaryDataAsset
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Design")
	FName OptionId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Design")
	FText DisplayName;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Design")
	FName SlotId;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Design")
	TSoftClassPtr<ACXMRPartAssembly> AssemblyActor;
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Design",
			  meta = (ToolTip = "Assembly transform relative to the fixed part slot. Distances are in cm."))
	FTransform SlotLocalOffset = FTransform::Identity;
	bool Validate(FString& OutError) const;
};
