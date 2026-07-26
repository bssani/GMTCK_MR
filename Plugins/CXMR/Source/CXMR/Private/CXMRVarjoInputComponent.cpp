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
#include "UObject/ConstructorHelpers.h"

UCXMRVarjoInputComponent::UCXMRVarjoInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Only this one is defaulted in C++. The rest are assigned in the BP, per the convention above —
	// but IA_Varjo_HandVisualizationToggle is plugin content that nothing has ever pointed at, so
	// defaulting it here is what makes the H key work on a fresh clone with no BP wiring.
	static ConstructorHelpers::FObjectFinder<UInputAction>
		HandVisFinder(TEXT("/CXMR/Core/Input/Actions/IA_Varjo_HandVisualizationToggle"));
	if (HandVisFinder.Succeeded())
	{
		HandVisualizationToggleAction = HandVisFinder.Object;
	}
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
						AddedToInputSystem = InputSys;   // remembered so EndPlay can undo it
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
	if (HandVisualizationToggleAction) { EIC->BindAction(HandVisualizationToggleAction, ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnHandVisualizationToggle); }
	if (RecalibrateAction)      { EIC->BindAction(RecalibrateAction,      ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnRecalibrate); }
	if (PlaceVehicleAction)     { EIC->BindAction(PlaceVehicleAction,     ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnPlaceVehicle); }

	// Turntable: Triggered fires every frame the stick is held, which is what the rotation wants.
	if (TurntableAxisAction)   { EIC->BindAction(TurntableAxisAction,   ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnTurntableAxis); }
	if (SpinLeftToggleAction)  { EIC->BindAction(SpinLeftToggleAction,  ETriggerEvent::Started,   this, &UCXMRVarjoInputComponent::OnSpinLeftToggle); }
	if (SpinRightToggleAction) { EIC->BindAction(SpinRightToggleAction, ETriggerEvent::Started,   this, &UCXMRVarjoInputComponent::OnSpinRightToggle); }

	// Cycling: Triggered drives the flick, Completed clears the latch when the stick returns to rest.
	if (CycleTrimAction)
	{
		EIC->BindAction(CycleTrimAction, ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnCycleTrim);
		EIC->BindAction(CycleTrimAction, ETriggerEvent::Completed, this, &UCXMRVarjoInputComponent::OnCycleTrimReleased);
	}
	if (CycleVehicleAction)
	{
		EIC->BindAction(CycleVehicleAction, ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnCycleVehicle);
		EIC->BindAction(CycleVehicleAction, ETriggerEvent::Completed, this, &UCXMRVarjoInputComponent::OnCycleVehicleReleased);
	}
}

void UCXMRVarjoInputComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (AddedToInputSystem && MappingContext)
	{
		AddedToInputSystem->RemoveMappingContext(MappingContext);
	}
	AddedToInputSystem = nullptr;

	Super::EndPlay(Reason);
}

void UCXMRVarjoInputComponent::OnMRToggle(const FInputActionValue&)         { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleMixedReality(); } }
void UCXMRVarjoInputComponent::OnVRBackgroundToggle(const FInputActionValue&) { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleVRBackground(); } }
void UCXMRVarjoInputComponent::OnViewOffsetToggle(const FInputActionValue&) { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleViewOffset(); } }
void UCXMRVarjoInputComponent::OnDepthTestToggle(const FInputActionValue&)  { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleDepthTest(); } }
void UCXMRVarjoInputComponent::OnEnvDepthToggle(const FInputActionValue&)   { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleEnvironmentDepthEstimation(); } }
void UCXMRVarjoInputComponent::OnMaskToggle(const FInputActionValue&)       { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleMasking(); } }
void UCXMRVarjoInputComponent::OnMarkerToggle(const FInputActionValue&)     { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleMarkerTracking(); } }
void UCXMRVarjoInputComponent::OnHandVisualizationToggle(const FInputActionValue&) { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleHandVisualization(); } }
void UCXMRVarjoInputComponent::OnRecalibrate(const FInputActionValue&)      { if (UCXMRSubsystem* S = GetCXMR()) { S->RequestRecalibrate(); } }
void UCXMRVarjoInputComponent::OnPlaceVehicle(const FInputActionValue&)     { if (UCXMRSubsystem* S = GetCXMR()) { S->RequestPlaceInFront(); } }

// ---------- Viewer: turntable + cycling ----------

void UCXMRVarjoInputComponent::OnTurntableAxis(const FInputActionValue& Value)
{
	if (UCXMRSubsystem* S = GetCXMR()) { S->RequestTurntableAxis(Value.Get<float>()); }
}

void UCXMRVarjoInputComponent::OnSpinLeftToggle(const FInputActionValue&)
{
	if (UCXMRSubsystem* S = GetCXMR()) { S->RequestViewerAction(ECXMRViewerAction::SpinLeft); }
}

void UCXMRVarjoInputComponent::OnSpinRightToggle(const FInputActionValue&)
{
	if (UCXMRSubsystem* S = GetCXMR()) { S->RequestViewerAction(ECXMRViewerAction::SpinRight); }
}

void UCXMRVarjoInputComponent::StepOnFlick(float AxisValue, bool& bLatched, ECXMRViewerAction Positive, ECXMRViewerAction Negative)
{
	const float Magnitude = FMath::Abs(AxisValue);

	if (!bLatched && Magnitude >= CycleThreshold)
	{
		bLatched = true;
		if (UCXMRSubsystem* S = GetCXMR())
		{
			S->RequestViewerAction(AxisValue > 0.0f ? Positive : Negative);
		}
	}
	else if (bLatched && Magnitude <= CycleReleaseThreshold)
	{
		bLatched = false;
	}
}

void UCXMRVarjoInputComponent::OnCycleTrim(const FInputActionValue& Value)
{
	StepOnFlick(Value.Get<float>(), bTrimLatched, ECXMRViewerAction::NextTrim, ECXMRViewerAction::PreviousTrim);
}

void UCXMRVarjoInputComponent::OnCycleVehicle(const FInputActionValue& Value)
{
	StepOnFlick(Value.Get<float>(), bVehicleLatched, ECXMRViewerAction::NextVehicle, ECXMRViewerAction::PreviousVehicle);
}

void UCXMRVarjoInputComponent::OnCycleTrimReleased(const FInputActionValue&)    { bTrimLatched = false; }
void UCXMRVarjoInputComponent::OnCycleVehicleReleased(const FInputActionValue&) { bVehicleLatched = false; }
