// Copyright GMTCK CX.

#include "CXMRTurntableComponent.h"
#include "CXMRPlacementComponent.h"
#include "CXMRSubsystem.h"
#include "Engine/GameInstance.h"

#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"

UCXMRTurntableComponent::UCXMRTurntableComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UCXMRTurntableComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		Placement = Owner->FindComponentByClass<UCXMRPlacementComponent>();
	}

	// Input lives on the pawn, the turntable on the vehicle — the subsystem is the rendezvous.
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			Subsystem = GI->GetSubsystem<UCXMRSubsystem>();
		}
	}
	if (Subsystem)
	{
		Subsystem->OnViewerAction.AddDynamic(this, &UCXMRTurntableComponent::HandleViewerAction);
		Subsystem->OnTurntableAxis.AddDynamic(this, &UCXMRTurntableComponent::HandleTurntableAxis);
	}
}

void UCXMRTurntableComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnViewerAction.RemoveDynamic(this, &UCXMRTurntableComponent::HandleViewerAction);
		Subsystem->OnTurntableAxis.RemoveDynamic(this, &UCXMRTurntableComponent::HandleTurntableAxis);
	}
	Super::EndPlay(Reason);
}

void UCXMRTurntableComponent::HandleViewerAction(ECXMRViewerAction Action)
{
	switch (Action)
	{
	case ECXMRViewerAction::SpinLeft:      ToggleSpinLeft();  break;
	case ECXMRViewerAction::SpinRight:     ToggleSpinRight(); break;
	case ECXMRViewerAction::StopSpin:      StopSpin();        break;
	case ECXMRViewerAction::ResetRotation: ResetRotation();   break;
	default: break;   // vehicle / trim cycling belongs to the loader
	}
}

void UCXMRTurntableComponent::HandleTurntableAxis(float AxisValue)
{
	AddYawInput(AxisValue);
}

USceneComponent* UCXMRTurntableComponent::ResolveTarget() const
{
	if (TurntableTarget)
	{
		return TurntableTarget;
	}
	return GetOwner() ? GetOwner()->GetRootComponent() : nullptr;
}

bool UCXMRTurntableComponent::IsTurntableAllowed() const
{
	// Interior anchors the car to the real cockpit; spinning it there is never wanted.
	return !Placement || Placement->Mode == ECXMRPlacementMode::PawnRelative;
}

void UCXMRTurntableComponent::AddYawInput(float AxisValue)
{
	if (!IsTurntableAllowed() || FMath::Abs(AxisValue) < AxisDeadzone)
	{
		return;
	}

	// Taking the stick means taking control — an unattended spin should not fight the user.
	SpinDirection = 0;
	PendingAxis   = AxisValue;
}

void UCXMRTurntableComponent::ToggleSpinLeft()
{
	if (!IsTurntableAllowed())
	{
		return;
	}
	SpinDirection = (SpinDirection == -1) ? 0 : -1;
}

void UCXMRTurntableComponent::ToggleSpinRight()
{
	if (!IsTurntableAllowed())
	{
		return;
	}
	SpinDirection = (SpinDirection == 1) ? 0 : 1;
}

void UCXMRTurntableComponent::StopSpin()
{
	SpinDirection = 0;
}

void UCXMRTurntableComponent::ResetRotation()
{
	SpinDirection = 0;
	PendingAxis   = 0.0f;
	CurrentYaw    = 0.0f;
	ApplyYaw();
}

void UCXMRTurntableComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!IsTurntableAllowed())
	{
		return;
	}

	float Delta = 0.0f;
	if (!FMath::IsNearlyZero(PendingAxis))
	{
		Delta = PendingAxis * ManualSpeed * DeltaTime;
		PendingAxis = 0.0f;   // Enhanced Input re-sends this every frame the stick is held.
	}
	else if (SpinDirection != 0)
	{
		Delta = SpinDirection * AutoSpeed * DeltaTime;
	}

	if (!FMath::IsNearlyZero(Delta))
	{
		CurrentYaw = FRotator::NormalizeAxis(CurrentYaw + Delta);
		ApplyYaw();
	}
}

void UCXMRTurntableComponent::ApplyYaw()
{
	if (USceneComponent* Target = ResolveTarget())
	{
		// Relative: the anchor above it still holds the calibrated world transform.
		Target->SetRelativeRotation(FRotator(0.0f, CurrentYaw, 0.0f));
	}
}
