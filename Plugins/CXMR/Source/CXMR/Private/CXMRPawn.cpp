// Copyright GMTCK CX.

#include "CXMRPawn.h"
#include "CXMRVarjoInputComponent.h"
#include "CXMRMaskingComponent.h"
#include "CXMRMarkerDebugComponent.h"
#include "CXMRHandDebugComponent.h"
#include "CXMRVirtualHandComponent.h"
#include "CXMRGazeDebugComponent.h"
#include "CXMRFoveationOverlayComponent.h"
#include "CXMRHandGrabComponent.h"
#include "CXMRSpectatorComponent.h"
#include "CXMRControlPanelWidget.h"
#include "CXMRDesktopPanelComponent.h"
#include "CXMRTuningWindowComponent.h"

#include "Camera/CameraComponent.h"
#include "MotionControllerComponent.h"
#include "EnhancedInputComponent.h"
#include "InputAction.h"
#include "Components/WidgetComponent.h"
#include "Components/WidgetInteractionComponent.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRPawn, Log, All);

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

	// The operator runs the session from the desk, not from the wearer's wrist.
	DesktopPanel = CreateDefaultSubobject<UCXMRDesktopPanelComponent>(TEXT("DesktopPanel"));

	// Numbers the operator tunes live (depth range, exposure, vehicle nudges), beside the control window.
	TuningWindow = CreateDefaultSubobject<UCXMRTuningWindowComponent>(TEXT("TuningWindow"));

	// Always present, drawn only when CXMR.DebugMarkers is set — a headset session is a bad time to
	// discover the instrument was not in the build.
	MarkerDebug = CreateDefaultSubobject<UCXMRMarkerDebugComponent>(TEXT("MarkerDebug"));

	// Same reasoning: the instrument ships in the build, drawn only while its toggle is on.
	HandDebug = CreateDefaultSubobject<UCXMRHandDebugComponent>(TEXT("HandDebug"));
	GazeDebug = CreateDefaultSubobject<UCXMRGazeDebugComponent>(TEXT("GazeDebug"));
	FoveationOverlay = CreateDefaultSubobject<UCXMRFoveationOverlayComponent>(TEXT("FoveationOverlay"));

	// Grabbing does nothing until the level has a CXMR Grabbable actor, so it costs a joint read per hand per frame.
	HandGrab = CreateDefaultSubobject<UCXMRHandGrabComponent>(TEXT("HandGrab"));

	// Off unless chosen: a second camera renders the whole scene again.
	Spectator = CreateDefaultSubobject<UCXMRSpectatorComponent>(TEXT("Spectator"));

	// On while mixed reality is on (CXMR.HandCutOut). A virtual console hides the real hand in MR; this cuts the hand's
	// shape out of it so the camera image of the hand shows.
	VirtualHands = CreateDefaultSubobject<UCXMRVirtualHandComponent>(TEXT("VirtualHands"));

	// --- Control panel: world-space quad on the left hand ---
	ControlPanel = CreateDefaultSubobject<UWidgetComponent>(TEXT("ControlPanel"));
	ControlPanel->SetupAttachment(LeftController);
	ControlPanel->SetWidgetSpace(EWidgetSpace::World);
	ControlPanel->SetDrawAtDesiredSize(false);   // PanelDrawSize rules; the panel pages scroll inside it
	ControlPanel->SetPivot(FVector2D(0.5f, 0.5f));
	ControlPanel->SetTwoSided(true);             // the hand turns; a one-sided quad vanishes at the wrong angle
	ControlPanel->SetTickWhenOffscreen(false);

	// The pointer traces on Visibility. WidgetComponent's default UI collision profile does not block it,
	// so the ray would sail straight through the panel and nothing would ever hover.
	ControlPanel->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	ControlPanel->SetCollisionResponseToAllChannels(ECR_Ignore);
	ControlPanel->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);

	ApplyPanelTransform();

	// --- Right-hand pointer ---
	PanelPointer = CreateDefaultSubobject<UWidgetInteractionComponent>(TEXT("PanelPointer"));
	PanelPointer->SetupAttachment(RightController);
	PanelPointer->InteractionSource   = EWidgetInteractionSource::World;
	PanelPointer->TraceChannel        = ECC_Visibility;
	PanelPointer->InteractionDistance = 150.f;   // arm's length; the panel is on the other hand
	PanelPointer->bShowDebug          = false;

	// Defaults resolved here, in C++, on purpose. Setting these as Blueprint class defaults does NOT
	// survive: PIE reinstances the Blueprint and rebuilds its CDO, and the value silently reverts to
	// null (measured — the same PIE session had the class one run and None the next). The panel is a C++
	// widget with no widget blueprint behind it; the click action ships inside /CXMR/, so this is plugin
	// content referencing itself. A project can still override either property on its own BP subclass.
	ControlPanelClass = UCXMRControlPanelWidget::StaticClass();

	static ConstructorHelpers::FObjectFinder<UInputAction>
		ClickActionFinder(TEXT("/CXMR/Core/Input/Actions/IA_CXMR_PanelClick"));
	if (ClickActionFinder.Succeeded())
	{
		PanelClickAction = ClickActionFinder.Object;
	}
}

void ACXMRPawn::BeginPlay()
{
	Super::BeginPlay();

	// Re-apply here as well: the BP subclass may have overridden the tunables after the ctor ran.
	ApplyPanelTransform();

	if (ControlPanel && ControlPanelClass)
	{
		ControlPanel->SetWidgetClass(ControlPanelClass);
		ControlPanel->InitWidget();   // SetWidgetClass alone does not rebuild once the component is registered

		UE_LOG(LogCXMRPawn, Log, TEXT("Control panel: class=%s widget=%s"),
			*GetNameSafe(ControlPanelClass),
			*GetNameSafe(ControlPanel->GetUserWidgetObject()));
	}
	else
	{
		// Silent absence is what cost us a debugging session — say it out loud.
		UE_LOG(LogCXMRPawn, Warning,
			TEXT("Control panel will NOT appear: ControlPanelClass is unset on %s."), *GetName());
	}

	// The widget is still created above, so turning the panel on later needs no rebuild of it.
	SetControlPanelVisible(bShowHandPanel);
}

void ACXMRPawn::ApplyPanelTransform()
{
	if (!ControlPanel) { return; }

	ControlPanel->SetDrawSize(PanelDrawSize);
	ControlPanel->SetRelativeLocation(PanelOffset);
	ControlPanel->SetRelativeRotation(PanelRotation);
	ControlPanel->SetRelativeScale3D(FVector(PanelScale));
}

void ACXMRPawn::SetControlPanelVisible(bool bVisible)
{
	if (ControlPanel)
	{
		ControlPanel->SetVisibility(bVisible, true);
		// Stop the ray from clicking a panel nobody can see.
		ControlPanel->SetCollisionEnabled(bVisible ? ECollisionEnabled::QueryOnly : ECollisionEnabled::NoCollision);
	}
	if (PanelPointer)
	{
		// With no panel there is nothing to point at; stop tracing every frame for it.
		PanelPointer->SetActive(bVisible);
	}
}

void ACXMRPawn::ToggleControlPanel()
{
	if (ControlPanel)
	{
		SetControlPanelVisible(!ControlPanel->IsVisible());
	}
}

void ACXMRPawn::OnPanelClickPressed()
{
	if (PanelPointer) { PanelPointer->PressPointerKey(EKeys::LeftMouseButton); }
}

void ACXMRPawn::OnPanelClickReleased()
{
	if (PanelPointer) { PanelPointer->ReleasePointerKey(EKeys::LeftMouseButton); }
}

void ACXMRPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);

	// Correct timing: InputComponent is valid here. Hand it to the input layer.
	if (UEnhancedInputComponent* EIC = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		VarjoInput->SetupInput(EIC);

		// Bound on the pawn, not in the input component: the target (PanelPointer) is a pawn-owned
		// component, and press/release must pair on the same object.
		if (PanelClickAction)
		{
			EIC->BindAction(PanelClickAction, ETriggerEvent::Started,   this, &ACXMRPawn::OnPanelClickPressed);
			EIC->BindAction(PanelClickAction, ETriggerEvent::Completed, this, &ACXMRPawn::OnPanelClickReleased);
			EIC->BindAction(PanelClickAction, ETriggerEvent::Canceled,  this, &ACXMRPawn::OnPanelClickReleased);
		}
	}
}
