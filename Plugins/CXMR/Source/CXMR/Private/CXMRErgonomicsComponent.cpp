// Copyright GMTCK CX.

#include "CXMRErgonomicsComponent.h"
#include "CXMRErgonomicsProfile.h"
#include "CXMRSubsystem.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRPlacementComponent.h"

#include "Camera/CameraComponent.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRErgo, Log, All);

UCXMRErgonomicsComponent::UCXMRErgonomicsComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

UCXMRSubsystem* UCXMRErgonomicsComponent::GetCXMR() const
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

void UCXMRErgonomicsComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = GetCXMR();
	if (Subsystem)
	{
		Subsystem->OnErgonomicsStepRequested.AddDynamic(this, &UCXMRErgonomicsComponent::HandleErgonomicsStep);
	}
}

void UCXMRErgonomicsComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnErgonomicsStepRequested.RemoveDynamic(this, &UCXMRErgonomicsComponent::HandleErgonomicsStep);
	}
	Super::EndPlay(Reason);
}

void UCXMRErgonomicsComponent::HandleErgonomicsStep(int32 Step)
{
	Cycle(Step);
}

UCXMRVehicleLoaderComponent* UCXMRErgonomicsComponent::GetLoader() const
{
	return GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
}

UCXMRErgonomicsProfile* UCXMRErgonomicsComponent::ResolveProfile() const
{
	if (ProfileOverride)
	{
		return ProfileOverride;
	}
	// Otherwise take it from the loaded vehicle, so it swaps with the car.
	if (const UCXMRVehicleLoaderComponent* Loader = GetLoader())
	{
		if (Loader->Profile && !Loader->Profile->Ergonomics.IsNull())
		{
			return Loader->Profile->Ergonomics.LoadSynchronous();
		}
	}
	return nullptr;
}

FName UCXMRErgonomicsComponent::GetManikinName() const
{
	const UCXMRErgonomicsProfile* Profile = ResolveProfile();
	if (Profile && Profile->IsValidPosition(CurrentIndex))
	{
		return Profile->Positions[CurrentIndex].Name;
	}
	return NAME_None;
}

bool UCXMRErgonomicsComponent::ComputeEyeWorld(FTransform& OutEyeWorld) const
{
	const UCXMRErgonomicsProfile* Profile = ResolveProfile();
	const UCXMRVehicleLoaderComponent* Loader = GetLoader();
	if (!Profile || !Loader || !Profile->IsValidPosition(CurrentIndex))
	{
		return false;
	}

	const AActor* Vehicle = Loader->GetSpawnedVehicle();
	if (!Vehicle)
	{
		return false;   // the eye point is vehicle-local and means nothing without the vehicle
	}

	// childWorld = childLocal * parentWorld — the eye point is authored in the vehicle's own frame.
	OutEyeWorld = Profile->Positions[CurrentIndex].EyePoint * Vehicle->GetActorTransform();
	return true;
}

bool UCXMRErgonomicsComponent::GetPlayer(APawn*& OutPawn, UCameraComponent*& OutCamera) const
{
	OutPawn = nullptr;
	OutCamera = nullptr;

	const UWorld* World = GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	OutPawn = PC ? PC->GetPawn() : nullptr;
	if (!OutPawn)
	{
		return false;
	}

	OutCamera = OutPawn->FindComponentByClass<UCameraComponent>();
	return OutCamera != nullptr;
}

void UCXMRErgonomicsComponent::MoveViewpointToEye(const FTransform& EyeWorld)
{
	APawn* Pawn = nullptr;
	UCameraComponent* Camera = nullptr;
	if (!GetPlayer(Pawn, Camera))
	{
		return;
	}

	// Seated recentre: first rotate the pawn about the head so the view faces vehicle-forward, then
	// slide it so the head lands on the eye point. Yaw only — the pawn must stay level under the HMD.
	const FVector  CamLoc     = Camera->GetComponentLocation();
	const float    CamYaw     = Camera->GetComponentRotation().Yaw;
	const float    DesiredYaw = EyeWorld.Rotator().Yaw;
	const float    DeltaYaw   = FMath::FindDeltaAngleDegrees(CamYaw, DesiredYaw);

	const FVector  PawnLoc = Pawn->GetActorLocation();
	const FRotator PawnRot = Pawn->GetActorRotation();
	const FVector  Rel     = FRotator(0.0f, DeltaYaw, 0.0f).RotateVector(PawnLoc - CamLoc);

	Pawn->SetActorLocationAndRotation(CamLoc + Rel, FRotator(PawnRot.Pitch, PawnRot.Yaw + DeltaYaw, PawnRot.Roll));

	// Camera moved with the rotation; close the remaining gap, height included.
	const FVector Offset = EyeWorld.GetLocation() - Camera->GetComponentLocation();
	Pawn->AddActorWorldOffset(Offset);
}

void UCXMRErgonomicsComponent::MoveVehicleToEye(const FTransform& EyeWorld)
{
	// With a real seat (marker-anchored) the eye point is already physical — moving the car would
	// fight both the calibration and the real seat, so leave it.
	if (const UCXMRPlacementComponent* Placement = GetOwner()->FindComponentByClass<UCXMRPlacementComponent>())
	{
		if (Placement->Mode == ECXMRPlacementMode::MarkerAnchor)
		{
			UE_LOG(LogCXMRErgo, Warning,
				TEXT("Ergonomics: vehicle is marker-anchored to a real seat; the eye point is physical. Not moving the car."));
			return;
		}
	}

	APawn* Pawn = nullptr;
	UCameraComponent* Camera = nullptr;
	if (!GetPlayer(Pawn, Camera))
	{
		return;
	}

	USceneComponent* Anchor = GetOwner()->GetRootComponent();
	if (!Anchor)
	{
		return;
	}

	// Target: the user's real head, yaw only (keep the car level).
	const FTransform Head(FRotator(0.0f, Camera->GetComponentRotation().Yaw, 0.0f), Camera->GetComponentLocation());

	// Everything below the anchor (turntable, offset, eye-local) is baked into this relative transform,
	// so solving for the anchor moves the eye point to the head without disturbing that chain.
	//   EyeWorld = EyeRelAnchor * AnchorWorld   ->   AnchorNew = EyeRelAnchor^-1 * Head
	const FTransform EyeRelAnchor = EyeWorld.GetRelativeTransform(Anchor->GetComponentTransform());
	Anchor->SetWorldTransform(EyeRelAnchor.Inverse() * Head);
}

void UCXMRErgonomicsComponent::ApplyManikin(int32 Index)
{
	const UCXMRErgonomicsProfile* Profile = ResolveProfile();
	if (!Profile || !Profile->IsValidPosition(Index))
	{
		return;
	}
	CurrentIndex = Index;

	FTransform EyeWorld;
	if (ComputeEyeWorld(EyeWorld))
	{
		const bool bMR = Subsystem && Subsystem->IsMixedRealityOn();
		if (bMR)
		{
			MoveVehicleToEye(EyeWorld);
		}
		else
		{
			MoveViewpointToEye(EyeWorld);
		}
	}

	if (Subsystem)
	{
		Subsystem->ReportManikin(FText::FromName(Profile->Positions[CurrentIndex].Name), CurrentIndex, Profile->Positions.Num());
	}
}

void UCXMRErgonomicsComponent::Cycle(int32 Step)
{
	const UCXMRErgonomicsProfile* Profile = ResolveProfile();
	if (!Profile || Profile->Positions.Num() == 0)
	{
		return;
	}
	const int32 Count = Profile->Positions.Num();
	ApplyManikin(((CurrentIndex + Step) % Count + Count) % Count);
}

void UCXMRErgonomicsComponent::SetManikin(int32 Index)   { ApplyManikin(Index); }
void UCXMRErgonomicsComponent::NextManikin()             { Cycle(1); }
void UCXMRErgonomicsComponent::PreviousManikin()         { Cycle(-1); }
void UCXMRErgonomicsComponent::ReapplyCurrent()          { ApplyManikin(CurrentIndex); }
