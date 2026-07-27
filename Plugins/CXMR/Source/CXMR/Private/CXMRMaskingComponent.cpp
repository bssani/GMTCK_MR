// Copyright GMTCK CX.

#include "CXMRMaskingComponent.h"
#include "CXMRSubsystem.h"

#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/GameInstance.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRMask, Log, All);

UCXMRMaskingComponent::UCXMRMaskingComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Resolved in C++ for the same reason the pawn's panel class is: a Blueprint class default can
	// silently revert to null when PIE reinstances the Blueprint and rebuilds its CDO, and masking
	// failing that way is invisible — the toggle flips, the state broadcasts, nothing renders.
	// PP_MRParameters ships inside /CXMR/, so this is plugin content referencing itself.
	static ConstructorHelpers::FObjectFinder<UMaterialParameterCollection>
		MaskParamsFinder(TEXT("/CXMR/Core/Materials/PP_MRParameters"));
	if (MaskParamsFinder.Succeeded())
	{
		MaskParameters = MaskParamsFinder.Object;
	}
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
		// Silent absence is the failure mode that costs a headset session: the toggle flips, the panel
		// says ON, and nothing happens. Say it out loud, once per attempt.
		UE_LOG(LogCXMRMask, Warning,
			TEXT("Masking cannot be applied on %s: MaskParameters is unset, so the '%s' scalar never "
			     "reaches PP_MR. The toggle will keep reporting its state while nothing renders."),
			*GetNameSafe(GetOwner()), *MaskParameterName.ToString());
		return;
	}

	UKismetMaterialLibrary::SetScalarParameterValue(this, MaskParameters, MaskParameterName, bEnabled ? 1.0f : 0.0f);
}
