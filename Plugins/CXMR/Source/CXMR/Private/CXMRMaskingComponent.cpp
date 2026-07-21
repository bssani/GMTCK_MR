// Copyright GMTCK CX.

#include "CXMRMaskingComponent.h"
#include "CXMRSubsystem.h"

#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/GameInstance.h"

UCXMRMaskingComponent::UCXMRMaskingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UCXMRSubsystem* UCXMRMaskingComponent::GetCXMR() const
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

void UCXMRMaskingComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = GetCXMR();
	if (Subsystem)
	{
		Subsystem->OnMaskingChanged.AddDynamic(this, &UCXMRMaskingComponent::HandleMaskingChanged);

		// Re-apply the current state — the subsystem is the source of truth, so masking survives
		// pawn swaps / level-local recreation of this component.
		ApplyMasking(Subsystem->IsMaskingOn());
	}
}

void UCXMRMaskingComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnMaskingChanged.RemoveDynamic(this, &UCXMRMaskingComponent::HandleMaskingChanged);
	}
	Super::EndPlay(Reason);
}

void UCXMRMaskingComponent::HandleMaskingChanged(bool bEnabled)
{
	ApplyMasking(bEnabled);
}

void UCXMRMaskingComponent::ApplyMasking(bool bEnabled)
{
	if (!MaskParameters)
	{
		return;
	}

	UKismetMaterialLibrary::SetScalarParameterValue(this, MaskParameters, MaskParameterName, bEnabled ? 1.0f : 0.0f);
}
