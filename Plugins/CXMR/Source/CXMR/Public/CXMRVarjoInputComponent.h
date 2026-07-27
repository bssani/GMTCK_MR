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
class UEnhancedInputLocalPlayerSubsystem;
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

	// --- Depth test range (Y + arrow keys) ---
	// These three IA assets shipped with the Varjo example and were mapped in IMC_Varjo from the start,
	// but nothing ever bound them, so the range stayed at the plugin's unbounded default and the depth
	// test flickered across the whole room. Defaulted in C++ for the same reason the hand-visualization
	// action is: they are plugin content that no Blueprint has ever pointed at.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> DepthRangeToggleAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> DepthRangeNearZAction;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> DepthRangeFarZAction;

	/** Metres per second while a range key is held. Varjo's example uses 0.01 per tick; this is the
	 *  frame-rate-independent equivalent, so the feel does not change with headroom. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions", meta = (ClampMin = "0.0"))
	float DepthRangeAdjustSpeed = 0.5f;

	/** Hand-tracking skeleton overlay. Defaulted in C++ to the plugin's own IA_Varjo_HandVisualizationToggle
	 *  (key H) — that asset shipped with the Varjo example but had never been bound to anything. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Actions") TObjectPtr<UInputAction> HandVisualizationToggleAction;

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

	/** Percentile manikin (Human Factors) cycling. Same Axis1D flick shape as the two above.
	 *  Until this existed, UCXMRErgonomicsComponent was fully implemented and subscribed but nothing
	 *  in the project could ever ask it to move — RequestErgonomicsStep had no callers at all. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer") TObjectPtr<UInputAction> CycleManikinAction;

	/** Stick must pass this to register a step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CycleThreshold = 0.6f;

	/** ...and fall back below this before the next one. Hysteresis, so a wobbling stick cannot double-step. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Input|Viewer", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float CycleReleaseThreshold = 0.3f;

protected:
	/** Removes the mapping context this component added. Without it a pawn swap leaves IMC_Varjo
	 *  applied to the player and the old bindings keep firing. */
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;

	/** The input subsystem we added the context to. Cached at add time on purpose: by EndPlay the pawn
	 *  may already be unpossessed, and the owner->controller->local-player chain no longer resolves. */
	UPROPERTY(Transient) TObjectPtr<UEnhancedInputLocalPlayerSubsystem> AddedToInputSystem;

	void OnMRToggle(const FInputActionValue& Value);
	void OnVRBackgroundToggle(const FInputActionValue& Value);
	void OnViewOffsetToggle(const FInputActionValue& Value);
	void OnDepthTestToggle(const FInputActionValue& Value);
	void OnEnvDepthToggle(const FInputActionValue& Value);
	void OnMaskToggle(const FInputActionValue& Value);
	void OnMarkerToggle(const FInputActionValue& Value);
	void OnHandVisualizationToggle(const FInputActionValue& Value);
	void OnRecalibrate(const FInputActionValue& Value);
	void OnPlaceVehicle(const FInputActionValue& Value);

	void OnDepthRangeToggle(const FInputActionValue& Value);
	void OnDepthRangeNearZ(const FInputActionValue& Value);
	void OnDepthRangeFarZ(const FInputActionValue& Value);

	void OnTurntableAxis(const FInputActionValue& Value);
	void OnSpinLeftToggle(const FInputActionValue& Value);
	void OnSpinRightToggle(const FInputActionValue& Value);
	void OnCycleTrim(const FInputActionValue& Value);
	void OnCycleVehicle(const FInputActionValue& Value);
	void OnCycleManikin(const FInputActionValue& Value);
	void OnCycleTrimReleased(const FInputActionValue& Value);
	void OnCycleVehicleReleased(const FInputActionValue& Value);
	void OnCycleManikinReleased(const FInputActionValue& Value);

	/** Shared latch: true exactly once per flick, then rearms when the stick falls back to rest.
	 *  Returns the step (+1 / -1) on the frame it fires, 0 otherwise. */
	int32 StepOnFlick(float AxisValue, bool& bLatched);

	/** Viewer flavour of the above — relays a Next/Previous action instead of a bare step. */
	void StepOnFlick(float AxisValue, bool& bLatched, ECXMRViewerAction Positive, ECXMRViewerAction Negative);

	/** Seconds elapsed this frame, for the held-key range adjustment. */
	float DeltaSeconds() const;

	bool bTrimLatched    = false;
	bool bVehicleLatched = false;
	bool bManikinLatched = false;
};
