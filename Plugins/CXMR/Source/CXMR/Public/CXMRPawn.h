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

UCLASS()
class CXMR_API ACXMRPawn : public APawn
{
	GENERATED_BODY()

public:
	ACXMRPawn();

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
};
