// Copyright GMTCK CX.
//
// UCXMRMaskingComponent — applies the masking state to the PP_MR material.
// The subsystem owns the on/off state; this component owns the ASSET reference (Material Parameter
// Collection), keeping the subsystem asset-agnostic / portable.
//
// Masking = the Custom-Depth "window into the real world". It does NOT use depth estimation, so it is
// immune to the reflective / dark-interior problems that make depth occlusion unusable in a car.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRMaskingComponent.generated.h"

class UCXMRSubsystem;
class UMaterialParameterCollection;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Masking")
class CXMR_API UCXMRMaskingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRMaskingComponent();

	/** The PP_MRParameters collection that PP_MR reads (CXMR content: /CXMR/Core/Materials/). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Masking") TObjectPtr<UMaterialParameterCollection> MaskParameters;

	/** Scalar parameter name inside that collection. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Masking") FName MaskParameterName = TEXT("MRMask");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;

	UFUNCTION() void HandleMaskingChanged(bool bEnabled);
	void ApplyMasking(bool bEnabled);

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
};
