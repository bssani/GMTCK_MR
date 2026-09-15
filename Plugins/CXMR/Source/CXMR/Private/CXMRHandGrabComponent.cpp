// Copyright GMTCK CX.

#include "CXMRHandGrabComponent.h"
#include "CXMRGrabbableComponent.h"
#include "CXMRHandTracking.h"
#include "CXMRTuningSubsystem.h"

#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayTypes.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "CXMRGrab"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRGrab, Log, All);

static FAutoConsoleCommandWithWorldAndArgs GCXMRSpawnGrabCube(
	TEXT("CXMR.SpawnGrabCube"),
	TEXT("Spawn an 8 cm grabbable cube 40 cm in front of the viewer. CXMR.SpawnGrabCube 1 = with physics."),
	FConsoleCommandWithWorldAndArgsDelegate::CreateLambda([](const TArray<FString>& Args, UWorld* World)
	{
		const APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
		UCXMRHandGrabComponent* HandGrab = Pawn ? Pawn->FindComponentByClass<UCXMRHandGrabComponent>() : nullptr;
		if (!HandGrab)
		{
			UE_LOG(LogCXMRGrab, Warning, TEXT("CXMR.SpawnGrabCube: the player pawn has no CXMR Hand Grab component."));
			return;
		}
		HandGrab->SpawnTestCube(Args.Num() > 0 && Args[0].ToBool());
	}));

namespace
{
	// Named for this file: bare keypoint names collide with other files in the same unity blob.
	const int32 GrabThumbTipKeypoint = static_cast<int32>(EHandKeypoint::ThumbTip);
	const int32 GrabIndexTipKeypoint = static_cast<int32>(EHandKeypoint::IndexTip);
	const int32 GrabPalmKeypoint     = static_cast<int32>(EHandKeypoint::Palm);

	const TCHAR* GrabHandName(EControllerHand Hand)
	{
		return Hand == EControllerHand::Left ? TEXT("Left") : TEXT("Right");
	}
}

UCXMRHandGrabComponent::UCXMRHandGrabComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	TestCubeMesh = TSoftObjectPtr<UStaticMesh>(FSoftObjectPath(TEXT("/Engine/BasicShapes/Cube.Cube")));
}

UCXMRTuningSubsystem* UCXMRHandGrabComponent::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

void UCXMRHandGrabComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterTunables();
}

void UCXMRHandGrabComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	// Hand back whatever is held with its physics, or it stays frozen in the air for the rest of the level.
	Release(EControllerHand::Left, /*bRestorePhysics*/ true);
	Release(EControllerHand::Right, /*bRestorePhysics*/ true);

	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
	}
	Super::EndPlay(Reason);
}

void UCXMRHandGrabComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	UpdateHand(EControllerHand::Left, DeltaTime);
	UpdateHand(EControllerHand::Right, DeltaTime);
}

void UCXMRHandGrabComponent::UpdateHand(EControllerHand Hand, float DeltaTime)
{
	FCXMRGrabHand& State = StateFor(Hand);
	const bool bWasTracked = State.bTracked;
	const FVector PreviousLocation = State.Frame.GetLocation();

	bool bPinchNow = false;
	if (State.bSimulated)
	{
		State.bTracked = true;
		State.PinchGap = -1.0f;
		State.Frame = State.SimulatedFrame;
		bPinchNow = State.bSimulatedPinching;
	}
	else
	{
		TArray<FVector> Positions;
		TArray<FQuat> Rotations;
		TArray<float> Radii;
		State.bTracked = CXMRHands::GetJoints(GetWorld(), Hand, Positions, Rotations, Radii);
		if (State.bTracked)
		{
			const FVector& Thumb = Positions[GrabThumbTipKeypoint];
			const FVector& Index = Positions[GrabIndexTipKeypoint];
			State.PinchGap = static_cast<float>(FVector::Dist(Thumb, Index));
			State.Frame = FTransform(Rotations[GrabPalmKeypoint], (Thumb + Index) * 0.5);
			// Two thresholds: fingertips resting between them keep the state they were in.
			bPinchNow = State.bPinching ? State.PinchGap < PinchOpenDistance : State.PinchGap <= PinchCloseDistance;
		}
	}

	if (!State.bTracked)
	{
		State.Velocity = FVector::ZeroVector;
		State.UntrackedSeconds += DeltaTime;
		if (State.UntrackedSeconds < LostTrackingGraceSeconds)
		{
			return;   // a dropout: keep holding where the hand was last seen and decide nothing yet
		}
		bPinchNow = false;
	}
	else
	{
		State.UntrackedSeconds = 0.0f;
		State.Velocity = (bWasTracked && DeltaTime > 0.0f)
			? (State.Frame.GetLocation() - PreviousLocation) / DeltaTime
			: FVector::ZeroVector;
	}

	const bool bPinchStarted = bPinchNow && !State.bPinching;
	State.bPinching = bPinchNow;

	UCXMRGrabbableComponent* Held = State.Held.Get();
	if (!Held)
	{
		State.Held.Reset();   // the held actor may have been destroyed while in the hand
	}

	if (Held)
	{
		if (!bPinchNow || !bHandGrabEnabled)
		{
			Release(Hand, /*bRestorePhysics*/ true);
		}
		else if (AActor* Actor = Held->GetOwner())
		{
			Actor->SetActorTransform(State.HeldOffset * State.Frame);
		}
	}
	else if (bPinchStarted && bHandGrabEnabled)
	{
		// Only a pinch that starts next to something picks it up; closed fingers sweeping through an object do not.
		if (UCXMRGrabbableComponent* Target = FindGrabbable(State.Frame.GetLocation()))
		{
			Grab(Hand, Target);
		}
	}
}

UCXMRGrabbableComponent* UCXMRHandGrabComponent::FindGrabbable(const FVector& Point) const
{
	UCXMRGrabbableComponent* Best = nullptr;
	double BestDistanceSquared = FMath::Square(static_cast<double>(GrabRadius));

	for (UCXMRGrabbableComponent* Grabbable : UCXMRGrabbableComponent::GetAll(GetWorld()))
	{
		const AActor* Actor = Grabbable->GetOwner();
		if (!Grabbable->bGrabbable || !Actor || Actor == GetOwner())
		{
			continue;
		}

		// Bounds rather than collision, so an actor with collision switched off can still be picked up.
		const FBox Bounds = Actor->GetComponentsBoundingBox(/*bNonColliding*/ true);
		if (!Bounds.IsValid)
		{
			continue;
		}

		const double DistanceSquared = Bounds.ComputeSquaredDistanceToPoint(Point);
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			Best = Grabbable;
		}
	}
	return Best;
}

void UCXMRHandGrabComponent::Grab(EControllerHand Hand, UCXMRGrabbableComponent* Target)
{
	AActor* Actor = Target->GetOwner();
	USceneComponent* Root = Actor ? Actor->GetRootComponent() : nullptr;
	if (!Root || Root->Mobility != EComponentMobility::Movable)
	{
		UE_LOG(LogCXMRGrab, Warning, TEXT("%s hand cannot pick up %s: its root component is not Movable."),
			GrabHandName(Hand), *GetNameSafe(Actor));
		return;
	}

	const EControllerHand OtherHand = (Hand == EControllerHand::Left) ? EControllerHand::Right : EControllerHand::Left;
	FCXMRGrabHand& Other = StateFor(OtherHand);

	bool bWasSimulating = false;
	if (Other.Held.Get() == Target)
	{
		// Passed from hand to hand: the physics the first hand paused is what the second one restores later.
		bWasSimulating = Other.bHeldWasSimulating;
		Release(OtherHand, /*bRestorePhysics*/ false);
	}
	else if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Root))
	{
		bWasSimulating = Body->IsSimulatingPhysics();
		if (bWasSimulating)
		{
			Body->SetSimulatePhysics(false);
		}
	}

	FCXMRGrabHand& State = StateFor(Hand);
	State.Held = Target;
	State.bHeldWasSimulating = bWasSimulating;
	State.HeldOffset = Actor->GetActorTransform().GetRelativeTransform(State.Frame);
	Target->NotifyGrabbed(Hand);

	UE_LOG(LogCXMRGrab, Log, TEXT("%s hand grabbed %s%s"), GrabHandName(Hand), *Actor->GetActorNameOrLabel(),
		bWasSimulating ? TEXT(" (physics paused)") : TEXT(""));
}

void UCXMRHandGrabComponent::Release(EControllerHand Hand, bool bRestorePhysics)
{
	FCXMRGrabHand& State = StateFor(Hand);
	UCXMRGrabbableComponent* Grabbable = State.Held.Get();
	const bool bWasSimulating = State.bHeldWasSimulating;
	State.Held.Reset();
	State.bHeldWasSimulating = false;
	if (!Grabbable)
	{
		return;
	}

	AActor* Actor = Grabbable->GetOwner();
	if (bRestorePhysics && bWasSimulating && Actor)
	{
		if (UPrimitiveComponent* Body = Cast<UPrimitiveComponent>(Actor->GetRootComponent()))
		{
			Body->SetSimulatePhysics(true);
			if (Grabbable->bThrowOnRelease)
			{
				Body->SetPhysicsLinearVelocity(State.Velocity);
			}
		}
	}
	Grabbable->NotifyReleased(Hand);

	UE_LOG(LogCXMRGrab, Log, TEXT("%s hand released %s"), GrabHandName(Hand), *GetNameSafe(Actor));
}

bool UCXMRHandGrabComponent::IsPinching(EControllerHand Hand) const
{
	return StateFor(Hand).bPinching;
}

AActor* UCXMRHandGrabComponent::GetHeldActor(EControllerHand Hand) const
{
	const UCXMRGrabbableComponent* Held = StateFor(Hand).Held.Get();
	return Held ? Held->GetOwner() : nullptr;
}

void UCXMRHandGrabComponent::ReleaseHand(EControllerHand Hand)
{
	Release(Hand, /*bRestorePhysics*/ true);
}

void UCXMRHandGrabComponent::SetSimulatedPinch(EControllerHand Hand, bool bPinching, FTransform PinchFrame)
{
	FCXMRGrabHand& State = StateFor(Hand);
	if (!State.bSimulated)
	{
		State.bTracked = false;   // the first simulated frame must not inherit a velocity from the real hand
	}
	State.bSimulated = true;
	State.bSimulatedPinching = bPinching;
	State.SimulatedFrame = PinchFrame;
}

void UCXMRHandGrabComponent::ClearSimulatedPinch(EControllerHand Hand)
{
	FCXMRGrabHand& State = StateFor(Hand);
	State.bSimulated = false;
	State.bSimulatedPinching = false;
}

AActor* UCXMRHandGrabComponent::SpawnTestCube(bool bSimulatePhysics)
{
	UWorld* World = GetWorld();
	FTransform Head;
	if (!World || !CXMRHands::GetHeadTransform(World, Head))
	{
		UE_LOG(LogCXMRGrab, Warning, TEXT("Test cube not spawned: there is no player view yet."));
		return nullptr;
	}

	UStaticMesh* Mesh = TestCubeMesh.LoadSynchronous();
	if (!Mesh)
	{
		UE_LOG(LogCXMRGrab, Warning, TEXT("Test cube not spawned: mesh %s did not load."), *TestCubeMesh.ToString());
		return nullptr;
	}

	// Level with the view direction and a little below the eyes: where a hand reaches without stretching.
	FVector Forward = Head.GetRotation().GetForwardVector();
	Forward.Z = 0.0;
	if (!Forward.Normalize())
	{
		Forward = FVector::ForwardVector;
	}
	const FVector Location = Head.GetLocation() + Forward * 40.0 - FVector(0.0, 0.0, 15.0);

	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AStaticMeshActor* Cube = World->SpawnActor<AStaticMeshActor>(Location, FRotator(0.0, Forward.Rotation().Yaw, 0.0), Params);
	if (!Cube)
	{
		return nullptr;
	}

	UStaticMeshComponent* Body = Cube->GetStaticMeshComponent();
	Body->SetMobility(EComponentMobility::Movable);   // first: a Static component refuses to be moved afterwards
	Body->SetStaticMesh(Mesh);
	Body->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	Cube->SetActorScale3D(FVector(0.08));             // the engine cube is 1 m
	Body->SetSimulatePhysics(bSimulatePhysics);

	UCXMRGrabbableComponent* Grabbable = NewObject<UCXMRGrabbableComponent>(Cube, TEXT("Grabbable"));
	Cube->AddInstanceComponent(Grabbable);
	Grabbable->RegisterComponent();

	UE_LOG(LogCXMRGrab, Log, TEXT("Spawned grabbable test cube %s%s"), *Cube->GetName(),
		bSimulatePhysics ? TEXT(" with physics") : TEXT(""));
	return Cube;
}

// ---- Tuning window rows ----

void UCXMRHandGrabComponent::RegisterTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return;
	}

	const FText Category = LOCTEXT("CatGrab", "Grab");
	auto Make = [this, &Category](FName Id, const FText& Label, ECXMRTunableKind Kind)
	{
		FCXMRTunable Tunable;
		Tunable.Id = Id;
		Tunable.Category = Category;
		Tunable.Label = Label;
		Tunable.Kind = Kind;
		Tunable.Owner = this;
		return Tunable;
	};

	{
		FCXMRTunable T = Make("Grab.Right", LOCTEXT("RightHand", "Right hand"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeHand(EControllerHand::Right); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Grab.Left", LOCTEXT("LeftHand", "Left hand"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeHand(EControllerHand::Left); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Grab.PinchClose", LOCTEXT("PinchClose", "Pinch closes below"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.5f; T.Max = 6.0f; T.Delta = 0.1f; T.Default = 2.0f; T.bPersist = true;
		T.Get = [this] { return PinchCloseDistance; };
		T.Set = [this](float Value) { PinchCloseDistance = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Grab.PinchOpen", LOCTEXT("PinchOpen", "Pinch opens above"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 1.0f; T.Max = 10.0f; T.Delta = 0.1f; T.Default = 3.5f; T.bPersist = true;
		T.Get = [this] { return PinchOpenDistance; };
		T.Set = [this](float Value) { PinchOpenDistance = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Grab.Radius", LOCTEXT("GrabRadius", "Reach from the pinch"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.0f; T.Max = 30.0f; T.Delta = 0.5f; T.Default = 5.0f; T.bPersist = true;
		T.Get = [this] { return GrabRadius; };
		T.Set = [this](float Value) { GrabRadius = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Grab.SpawnCube", LOCTEXT("SpawnCube", "Spawn a test cube in front of me"), ECXMRTunableKind::Action);
		T.Invoke = [this] { SpawnTestCube(false); };
		Tuning->Register(MoveTemp(T));
	}
}

FText UCXMRHandGrabComponent::DescribeHand(EControllerHand Hand) const
{
	const FCXMRGrabHand& State = StateFor(Hand);
	if (!State.bTracked)
	{
		return CXMRHands::IsTrackerPresent() ? LOCTEXT("NotTracked", "not tracked") : LOCTEXT("NoTracker", "no hand tracker");
	}

	FString Text = State.bSimulated ? FString(TEXT("simulated")) : FString::Printf(TEXT("gap %.1f cm"), State.PinchGap);
	Text += State.bPinching ? TEXT(", pinching") : TEXT(", open");
	if (const UCXMRGrabbableComponent* Held = State.Held.Get())
	{
		Text += FString::Printf(TEXT(", holding %s"), *GetNameSafe(Held->GetOwner()));
	}
	return FText::FromString(Text);
}

#undef LOCTEXT_NAMESPACE
