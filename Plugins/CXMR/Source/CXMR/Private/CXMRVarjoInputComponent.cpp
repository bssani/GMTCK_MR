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
#include "Engine/World.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRInput, Log, All);

UCXMRVarjoInputComponent::UCXMRVarjoInputComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// These are defaulted in C++, the rest are assigned in the BP per the convention above. What they
	// have in common: every one is plugin content that shipped inside /CXMR/, is already mapped in
	// IMC_Varjo, and that no Blueprint has ever pointed at — so defaulting here is what makes the key
	// work on a fresh clone with no BP wiring. A project can still override any of them on its own BP.
	static ConstructorHelpers::FObjectFinder<UInputAction>
		HandVisFinder(TEXT("/CXMR/Core/Input/Actions/IA_Varjo_HandVisualizationToggle"));
	if (HandVisFinder.Succeeded())
	{
		HandVisualizationToggleAction = HandVisFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction>
		GazeVisFinder(TEXT("/CXMR/Core/Input/Actions/IA_Varjo_GazeVisualizationToggle"));
	if (GazeVisFinder.Succeeded())
	{
		GazeVisualizationToggleAction = GazeVisFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction>
		FoveationVisFinder(TEXT("/CXMR/Core/Input/Actions/IA_Varjo_FoveatedRenderingVisualizationToggle"));
	if (FoveationVisFinder.Succeeded())
	{
		FoveationVisualizationToggleAction = FoveationVisFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction>
		RangeToggleFinder(TEXT("/CXMR/Core/Input/Actions/IA_Varjo_DepthTestRangeToggle"));
	if (RangeToggleFinder.Succeeded())
	{
		DepthRangeToggleAction = RangeToggleFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction>
		RangeNearFinder(TEXT("/CXMR/Core/Input/Actions/IA_Varjo_DepthTestRangeNearZ"));
	if (RangeNearFinder.Succeeded())
	{
		DepthRangeNearZAction = RangeNearFinder.Object;
	}

	static ConstructorHelpers::FObjectFinder<UInputAction>
		RangeFarFinder(TEXT("/CXMR/Core/Input/Actions/IA_Varjo_DepthTestRangeFarZ"));
	if (RangeFarFinder.Succeeded())
	{
		DepthRangeFarZAction = RangeFarFinder.Object;
	}
}

float UCXMRVarjoInputComponent::DeltaSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetDeltaSeconds() : 0.0f;
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
		UE_LOG(LogCXMRInput, Warning, TEXT("SetupInput called with null EnhancedInputComponent."));
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
	if (GazeVisualizationToggleAction)      { EIC->BindAction(GazeVisualizationToggleAction,      ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnGazeVisualizationToggle); }
	if (FoveationVisualizationToggleAction) { EIC->BindAction(FoveationVisualizationToggleAction, ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnFoveationVisualizationToggle); }
	if (RecalibrateAction)      { EIC->BindAction(RecalibrateAction,      ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnRecalibrate); }
	if (PlaceVehicleAction)     { EIC->BindAction(PlaceVehicleAction,     ETriggerEvent::Started, this, &UCXMRVarjoInputComponent::OnPlaceVehicle); }

	// Depth range: Triggered on the bounds so holding the key sweeps them, like the turntable stick.
	if (DepthRangeToggleAction) { EIC->BindAction(DepthRangeToggleAction, ETriggerEvent::Started,   this, &UCXMRVarjoInputComponent::OnDepthRangeToggle); }
	if (DepthRangeNearZAction)  { EIC->BindAction(DepthRangeNearZAction,  ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnDepthRangeNearZ); }
	if (DepthRangeFarZAction)   { EIC->BindAction(DepthRangeFarZAction,   ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnDepthRangeFarZ); }

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
	if (CycleManikinAction)
	{
		EIC->BindAction(CycleManikinAction, ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnCycleManikin);
		EIC->BindAction(CycleManikinAction, ETriggerEvent::Completed, this, &UCXMRVarjoInputComponent::OnCycleManikinReleased);
	}

	// Marker offset adjustment (fine-tuning calibration).
	if (OffsetAdjustXAction)   { EIC->BindAction(OffsetAdjustXAction,   ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnOffsetAdjustX); }
	if (OffsetAdjustYAction)   { EIC->BindAction(OffsetAdjustYAction,   ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnOffsetAdjustY); }
	if (OffsetAdjustZAction)   { EIC->BindAction(OffsetAdjustZAction,   ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnOffsetAdjustZ); }
	if (OffsetAdjustYawAction) { EIC->BindAction(OffsetAdjustYawAction, ETriggerEvent::Triggered, this, &UCXMRVarjoInputComponent::OnOffsetAdjustYaw); }
	if (OffsetSaveAction)      { EIC->BindAction(OffsetSaveAction,      ETriggerEvent::Started,   this, &UCXMRVarjoInputComponent::OnOffsetSave); }
	if (OffsetResetAction)     { EIC->BindAction(OffsetResetAction,     ETriggerEvent::Started,   this, &UCXMRVarjoInputComponent::OnOffsetReset); }
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
void UCXMRVarjoInputComponent::OnGazeVisualizationToggle(const FInputActionValue&) { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleGazeVisualization(); } }
void UCXMRVarjoInputComponent::OnFoveationVisualizationToggle(const FInputActionValue&) { if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleFoveationVisualization(); } }
void UCXMRVarjoInputComponent::OnRecalibrate(const FInputActionValue&)      { if (UCXMRSubsystem* S = GetCXMR()) { S->RequestRecalibrate(); } }
void UCXMRVarjoInputComponent::OnPlaceVehicle(const FInputActionValue&)     { if (UCXMRSubsystem* S = GetCXMR()) { S->RequestPlaceInFront(); } }

// ---------- Depth test range ----------

void UCXMRVarjoInputComponent::OnDepthRangeToggle(const FInputActionValue&)
{
	if (UCXMRSubsystem* S = GetCXMR()) { S->ToggleDepthTestRange(); }
}

void UCXMRVarjoInputComponent::OnDepthRangeNearZ(const FInputActionValue& Value)
{
	if (UCXMRSubsystem* S = GetCXMR())
	{
		S->AdjustDepthTestRange(Value.Get<float>() * DepthRangeAdjustSpeed * DeltaSeconds(), 0.0f);
	}
}

void UCXMRVarjoInputComponent::OnDepthRangeFarZ(const FInputActionValue& Value)
{
	if (UCXMRSubsystem* S = GetCXMR())
	{
		S->AdjustDepthTestRange(0.0f, Value.Get<float>() * DepthRangeAdjustSpeed * DeltaSeconds());
	}
}

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

int32 UCXMRVarjoInputComponent::StepOnFlick(float AxisValue, bool& bLatched)
{
	const float Magnitude = FMath::Abs(AxisValue);

	if (!bLatched && Magnitude >= CycleThreshold)
	{
		bLatched = true;
		return AxisValue > 0.0f ? 1 : -1;
	}
	if (bLatched && Magnitude <= CycleReleaseThreshold)
	{
		bLatched = false;
	}
	return 0;
}

void UCXMRVarjoInputComponent::StepOnFlick(float AxisValue, bool& bLatched, ECXMRViewerAction Positive, ECXMRViewerAction Negative)
{
	const int32 Step = StepOnFlick(AxisValue, bLatched);
	if (Step != 0)
	{
		if (UCXMRSubsystem* S = GetCXMR())
		{
			S->RequestViewerAction(Step > 0 ? Positive : Negative);
		}
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

// Ergonomics is a step relay rather than a ECXMRViewerAction, so it uses the bare latch.
void UCXMRVarjoInputComponent::OnCycleManikin(const FInputActionValue& Value)
{
	const int32 Step = StepOnFlick(Value.Get<float>(), bManikinLatched);
	if (Step != 0)
	{
		if (UCXMRSubsystem* S = GetCXMR()) { S->RequestErgonomicsStep(Step); }
	}
}

void UCXMRVarjoInputComponent::OnCycleTrimReleased(const FInputActionValue&)    { bTrimLatched = false; }
void UCXMRVarjoInputComponent::OnCycleVehicleReleased(const FInputActionValue&) { bVehicleLatched = false; }
void UCXMRVarjoInputComponent::OnCycleManikinReleased(const FInputActionValue&) { bManikinLatched = false; }

// ---------- Marker offset adjustment ----------

// The placement component lives on the vehicle rig, NOT on the pawn, so these used to find nothing:
// FindComponentByClass on the pawn always returned null and the keys silently did nothing. Every
// other placement action already goes through the subsystem — these now do too.

void UCXMRVarjoInputComponent::OnOffsetAdjustX(const FInputActionValue& Value)
{
	if (UCXMRSubsystem* S = GetCXMR())
	{
		const float Delta = Value.Get<float>() * OffsetAdjustSpeed * DeltaSeconds();
		S->RequestAdjustMarkerOffset(FVector(Delta, 0.0f, 0.0f), FRotator::ZeroRotator);
	}
}

void UCXMRVarjoInputComponent::OnOffsetAdjustY(const FInputActionValue& Value)
{
	if (UCXMRSubsystem* S = GetCXMR())
	{
		const float Delta = Value.Get<float>() * OffsetAdjustSpeed * DeltaSeconds();
		S->RequestAdjustMarkerOffset(FVector(0.0f, Delta, 0.0f), FRotator::ZeroRotator);
	}
}

void UCXMRVarjoInputComponent::OnOffsetAdjustZ(const FInputActionValue& Value)
{
	if (UCXMRSubsystem* S = GetCXMR())
	{
		const float Delta = Value.Get<float>() * OffsetAdjustSpeed * DeltaSeconds();
		S->RequestAdjustMarkerOffset(FVector(0.0f, 0.0f, Delta), FRotator::ZeroRotator);
	}
}

void UCXMRVarjoInputComponent::OnOffsetAdjustYaw(const FInputActionValue& Value)
{
	if (UCXMRSubsystem* S = GetCXMR())
	{
		const float Delta = Value.Get<float>() * OffsetAdjustSpeed * DeltaSeconds();
		S->RequestAdjustMarkerOffset(FVector::ZeroVector, FRotator(0.0f, Delta, 0.0f));
	}
}

void UCXMRVarjoInputComponent::OnOffsetSave(const FInputActionValue&)
{
	if (UCXMRSubsystem* S = GetCXMR()) { S->RequestSaveMarkerOffset(); }
}

void UCXMRVarjoInputComponent::OnOffsetReset(const FInputActionValue&)
{
	if (UCXMRSubsystem* S = GetCXMR()) { S->RequestResetMarkerOffset(); }
}
