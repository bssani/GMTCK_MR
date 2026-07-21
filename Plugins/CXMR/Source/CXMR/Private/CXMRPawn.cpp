// Copyright GMTCK CX.

#include "CXMRPawn.h"
#include "CXMRVarjoInputComponent.h"
#include "CXMRMaskingComponent.h"

#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "EnhancedInputComponent.h"

ACXMRPawn::ACXMRPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	SetRootComponent(VROrigin);

	// Camera auto-locks to the HMD (bLockToHmd defaults true).
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(VROrigin);

	LeftController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("LeftController"));
	LeftController->SetupAttachment(VROrigin);
	LeftController->SetTrackingMotionSource(FName("Left"));

	RightController = CreateDefaultSubobject<UMotionControllerComponent>(TEXT("RightController"));
	RightController->SetupAttachment(VROrigin);
	RightController->SetTrackingMotionSource(FName("Right"));

	VarjoInput = CreateDefaultSubobject<UCXMRVarjoInputComponent>(TEXT("VarjoInput"));
	Masking    = CreateDefaultSubobject<UCXMRMaskingComponent>(TEXT("Masking"));
}

void ACXMRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Correct timing: InputComponent is valid here. Hand it to the input layer.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		VarjoInput->SetupInput(EIC);
	}
}
