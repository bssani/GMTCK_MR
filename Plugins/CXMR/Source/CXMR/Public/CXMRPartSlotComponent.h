#pragma once
#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "CXMRPartSlotComponent.generated.h"
UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Part Slot")
class CXMR_API UCXMRPartSlotComponent : public USceneComponent
{
	GENERATED_BODY()
public:
	UCXMRPartSlotComponent();
	UPROPERTY(
		EditAnywhere, BlueprintReadOnly, Category = "CXMR|Parts",
		meta = (ToolTip = "Stable unique mount ID within this vehicle. Author the mount in the vehicle Blueprint."))
	FName SlotId;
};
