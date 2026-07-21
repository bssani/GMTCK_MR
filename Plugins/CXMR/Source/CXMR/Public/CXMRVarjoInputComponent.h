// Copyright GMTCK CX.
//
// UCXMRVarjoInputComponent — thin input layer.
// Binds IMC_Varjo actions to UCXMRSubsystem toggles. Knows nothing about Varjo internals.
// Asset refs (IMC + IAs) are assigned in the editor, so C++ stays generic/portable.
//
// Integration: add this component to the player Pawn, then call SetupInput() from the
// Pawn's SetupPlayerInputComponent (that is when the EnhancedInputComponent is valid).

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRTypes.h"
#include "CXMRVarjoInputComponent.generated.h"

class UInputMappingContext;
class UInputAction;
class UEnhancedInputComponent;
class UCXMRSubsystem;
struct FInputActionValue;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Varjo Input")
class CXMR_API UCXMRVarjoInputComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRVarjoInputComponent();

	/** Adds the mapping context and binds the toggle actions. Call from the Pawn's SetupPlayerInputComponent. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Input")
	void SetupInput(UEnhancedInputComponent* EnhancedInputComponent);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input") TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input") int32 MappingPriority = 0;

	// Assign the migrated IA_Varjo_* assets here.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> MRToggleAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> VRBackgroundToggleAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> ViewOffsetToggleAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> DepthTestToggleAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> EnvDepthToggleAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> MaskToggleAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> MarkerToggleAction;

	// Placement actions (routed via the subsystem to the placement component).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> RecalibrateAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> PlaceVehicleAction;

	// --- Exterior turntable (left controller) ---
	/** Axis1D. Held stick rotates the vehicle; sign is the direction. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer") TObjectPtr<UInputAction> TurntableAxisAction;
	/** Toggles continuous rotation; pressing the opposite direction switches rather than stacking. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer") TObjectPtr<UInputAction> SpinLeftToggleAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer") TObjectPtr<UInputAction> SpinRightToggleAction;

	// --- Cycling (right controller). Axis1D, one step per flick — see the latch below. ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer") TObjectPtr<UInputAction> CycleTrimAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer") TObjectPtr<UInputAction> CycleVehicleAction;

	/** Stick must pass this to register a step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CycleThreshold = 0.6f;

	/** ...and fall back below this before the next one. Hysteresis, so a wobbling stick cannot double-step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CycleReleaseThreshold = 0.3f;

private:
	UCXMRSubsystem* GetCXMR() const;

	void OnMRToggle(const FInputActionValue& Value);
	void OnVRBackgroundToggle(const FInputActionValue& Value);
	void OnViewOffsetToggle(const FInputActionValue& Value);
	void OnDepthTestToggle(const FInputActionValue& Value);
	void OnEnvDepthToggle(const FInputActionValue& Value);
	void OnMaskToggle(const FInputActionValue& Value);
	void OnMarkerToggle(const FInputActionValue& Value);
	void OnRecalibrate(const FInputActionValue& Value);
	void OnPlaceVehicle(const FInputActionValue& Value);

	void OnTurntableAxis(const FInputActionValue& Value);
	void OnSpinLeftToggle(const FInputActionValue& Value);
	void OnSpinRightToggle(const FInputActionValue& Value);
	void OnCycleTrim(const FInputActionValue& Value);
	void OnCycleVehicle(const FInputActionValue& Value);
	void OnCycleTrimReleased(const FInputActionValue& Value);
	void OnCycleVehicleReleased(const FInputActionValue& Value);

	/** Turns a continuous axis into discrete steps: fires once, then waits for the stick to return. */
	void StepOnFlick(float AxisValue, bool& bLatched, ECXMRViewerAction Positive, ECXMRViewerAction Negative);

	bool bTrimLatched    = false;
	bool bVehicleLatched = false;
};
