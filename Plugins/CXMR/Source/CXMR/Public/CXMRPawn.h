// Copyright GMTCK CX.
//
// ACXMRPawn — CXMR base VR/MR pawn. Camera (HMD) + L/R motion controllers + the Varjo input layer.
// Hooks input in C++ (SetupPlayerInputComponent -> VarjoInput->SetupInput), so no BP input wiring.
// Locomotion (teleport / turntable orbit) and interaction (hand / widget / grab) are added later
// as features on top of this base.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "CXMRPawn.generated.h"

class UCameraComponent;
class UMotionControllerComponent;
class UCXMRVarjoInputComponent;
class UCXMRMaskingComponent;
class UCXMRMarkerDebugComponent;
class UCXMRHandDebugComponent;
class UCXMRControlPanelWidget;
class UWidgetComponent;
class UWidgetInteractionComponent;
class UInputAction;

UCLASS()
class CXMR_API ACXMRPawn : public APawn
{
	GENERATED_BODY()

public:
	ACXMRPawn();

	virtual void BeginPlay() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Pawn") TObjectPtr<USceneComponent> VROrigin;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Pawn") TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Pawn") TObjectPtr<UMotionControllerComponent> LeftController;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Pawn") TObjectPtr<UMotionControllerComponent> RightController;

	/** Thin input layer — assign IMC_Varjo + IA_Varjo_* on this component in the BP subclass. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Pawn") TObjectPtr<UCXMRVarjoInputComponent> VarjoInput;

	/** Applies masking state to PP_MR — assign the PP_MRParameters collection in the BP subclass. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Pawn") TObjectPtr<UCXMRMaskingComponent> Masking;

	/** Headset verification instrument. Draws raw marker poses; see CXMR.DebugMarkers. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Debug") TObjectPtr<UCXMRMarkerDebugComponent> MarkerDebug;

	/** Headset verification instrument. Draws the tracked hand skeleton; toggled with H. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|Debug") TObjectPtr<UCXMRHandDebugComponent> HandDebug;

	// ============================================================================
	//  Control panel — world space, on the left hand.
	//
	//  It has to be a WidgetComponent: a screen-space widget (AddToViewport) renders only to the
	//  spectator window and is INVISIBLE in the headset. Left hand holds it, right hand points.
	// ============================================================================

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|UI") TObjectPtr<UWidgetComponent> ControlPanel;

	/** Right-hand ray that hovers/clicks the panel. Presses come from PanelClickAction. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "CXMR|UI") TObjectPtr<UWidgetInteractionComponent> PanelPointer;

	/** Assign WBP_CXMRControlPanel in the BP subclass — C++ carries no content reference. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = "CXMR|UI") TSubclassOf<UCXMRControlPanelWidget> ControlPanelClass;

	/** Trigger / pinch that presses whatever the pointer is over. Unassigned = pointer hovers only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|UI") TObjectPtr<UInputAction> PanelClickAction;

	/** Quad size in pixels. Must match the WBP's content size, or the layout is cropped or stretched. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|UI") FVector2D PanelDrawSize = FVector2D(432.f, 520.f);

	/** cm per pixel. 0.03 puts a 432x520 panel at roughly 13 x 16 cm — a tablet in the hand. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|UI", meta = (ClampMin = "0.005", ClampMax = "0.2"))
	float PanelScale = 0.03f;

	/** Offset from the left controller. Tune in the BP with the headset on — this default is a starting point. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|UI") FVector PanelOffset = FVector(8.f, 0.f, 4.f);

	/** Yaw 180 turns the quad back toward the wearer; pitch tilts it up like a held clipboard. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|UI") FRotator PanelRotation = FRotator(-25.f, 180.f, 0.f);

	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void SetControlPanelVisible(bool bVisible);
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleControlPanel();

private:
	void OnPanelClickPressed();
	void OnPanelClickReleased();

	/** Pushes the tunables onto the component. Called from the ctor and BeginPlay so BP overrides apply. */
	void ApplyPanelTransform();
};
