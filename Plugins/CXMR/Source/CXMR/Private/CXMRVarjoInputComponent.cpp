// Copyright GMTCK CX.

#include "CXMRVarjoInputComponent.h"
#include "CXMRSubsystem.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputAction.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"

UCXMRVarjoInputComponent::UCXMRVarjoInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UCXMRSubsystem* UCXMRVarjoInputComponent::GetCXMR() const
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

void UCXMRVarjoInputComponent::SetupInput(UEnhancedInputComponent* EIC)
{
	if (!EIC)
	{
		UE_LOG(LogTemp, Warning, TEXT("CXMRVarjoInputComponent::SetupInput called with null EnhancedInputComponent."));
		return;
	}

	// Add the mapping context via the owning pawn's local player.
	if (MappingContext)
	{
		if (const APawn* Pawn = Cast<APawn>(GetOwner()))
		{
			if (const APlayerController* PC = Cast<APlayerController>(Pawn->GetController()))
			{
				if (ULocalPlayer* LP = PC->GetLocalPlayer())
				{
					if (UEnhancedInputLocalPlayerSubsystem* InputSys = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
					{
						InputSys->AddMappingContext(MappingContext, MappingPriority);
					}
				}
			}
		}
	}

	// Bind toggles. ETriggerEvent::Started = fire once on press.
	if (MRToggleAction)            { EIC->BindAction(MRToggleAction,            ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnMRToggle); }
	if (VRBackgroundToggleAction)  { EIC->BindAction(VRBackgroundToggleAction,  ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnVRBackgroundToggle); }
	if (ViewOffsetToggleAction)    { EIC->BindAction(ViewOffsetToggleAction,    ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnViewOffsetToggle); }
	if (DepthTestToggleAction)  { EIC->BindAction(DepthTestToggleAction,  ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnDepthTestToggle); }
	if (EnvDepthToggleAction)   { EIC->BindAction(EnvDepthToggleAction,   ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnEnvDepthToggle); }
	if (MaskToggleAction)       { EIC->BindAction(MaskToggleAction,       ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnMaskToggle); }
	if (MarkerToggleAction)     { EIC->BindAction(MarkerToggleAction,     ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnMarkerToggle); }
	if (RecalibrateAction)      { EIC->BindAction(RecalibrateAction,      ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnRecalibrate); }
	if (PlaceVehicleAction)     { EIC->BindAction(PlaceVehicleAction,     ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnPlaceVehicle); }
}

void UCXMRVarjoInputComponent::OnMRToggle(const FInputActionValue&)         { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleMixedReality(); } }
void UCXMRVarjoInputComponent::OnVRBackgroundToggle(const FInputActionValue&) { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleVRBackground(); } }
void UCXMRVarjoInputComponent::OnViewOffsetToggle(const FInputActionValue&) { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleViewOffset(); } }
void UCXMRVarjoInputComponent::OnDepthTestToggle(const FInputActionValue&)  { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleDepthTest(); } }
void UCXMRVarjoInputComponent::OnEnvDepthToggle(const FInputActionValue&)   { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleEnvironmentDepthEstimation(); } }
void UCXMRVarjoInputComponent::OnMaskToggle(const FInputActionValue&)       { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleMasking(); } }
void UCXMRVarjoInputComponent::OnMarkerToggle(const FInputActionValue&)     { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleMarkerTracking(); } }
void UCXMRVarjoInputComponent::OnRecalibrate(const FInputActionValue&)      { if (UCXMRSubsystem* S = GetCXMR()) { S->RequestRecalibrate(); } }
void UCXMRVarjoInputComponent::OnPlaceVehicle(const FInputActionValue&)     { if (UCXMRSubsystem* S = GetCXMR()) { S->RequestPlaceInFront(); } }
