// Copyright GMTCK CX.

#include "CXMRPawn.h"
#include "CXMRVarjoInputComponent.h"
#include "CXMRMaskingComponent.h"
#include "CXMRMarkerDebugComponent.h"
#include "CXMRHandDebugComponent.h"
#include "CXMRSpectatorComponent.h"
#include "CXMRDesktopPanelComponent.h"
#include "CXMRTuningWindowComponent.h"

#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "EnhancedInputComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRPawn, Log, All);

ACXMRPawn::ACXMRPawn()
{
	PrimaryActorTick.bCanEverTick = false;

	VROrigin = CreateDefaultSubobject<USceneComponent>(TEXT("VROrigin"));
	SetRootComponent(VROrigin);

	// 카메라는 HMD 추적을 따름.
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

	// 운영자는 데스크톱 창에서 세션을 제어함.
	DesktopPanel = CreateDefaultSubobject<UCXMRDesktopPanelComponent>(TEXT("DesktopPanel"));

	// 깊이·노출·차량 미세 이동을 수치로 조정함.
	TuningWindow = CreateDefaultSubobject<UCXMRTuningWindowComponent>(TEXT("TuningWindow"));

	// CXMR.DebugMarkers가 켜진 동안만 표시함.
	MarkerDebug = CreateDefaultSubobject<UCXMRMarkerDebugComponent>(TEXT("MarkerDebug"));

	// 손 진단은 토글이 켜진 동안만 표시함.
	HandDebug = CreateDefaultSubobject<UCXMRHandDebugComponent>(TEXT("HandDebug"));
	// 관찰자 모드는 추가 카메라 비용이 있어 기본으로 끔.
	Spectator = CreateDefaultSubobject<UCXMRSpectatorComponent>(TEXT("Spectator"));

}

void ACXMRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// 입력 컴포넌트 생성 이후 바인딩함.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		VarjoInput->SetupInput(EIC);

	}
}
