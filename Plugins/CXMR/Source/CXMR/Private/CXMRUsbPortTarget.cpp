// Copyright GMTCK CX.

#include "CXMRUsbPortTarget.h"
#include "CXMRVirtualHandComponent.h"

#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRPort, Log, All);

namespace
{
	/** The engine basic cube and cylinder are 100 cm across. */
	constexpr float CubeCm = 100.f;

	/** Extra degrees allowed before an aligned port lets go, so a plug held at the limit does not flicker. */
	constexpr float AngleHysteresis = 10.f;

	/** Approach lets go this much farther out than it starts, cm. */
	constexpr float PortApproachHysteresis = 2.f;

	/** A port already reacting stays the nearest until another is this much nearer, cm — no flicker between neighbours. */
	constexpr float PortNearestLead = 0.5f;

	/** Glow of the resting colours: idle, and the bottom of the approach pulse. */
	constexpr float PortRestGlow = 1.f;
}

ACXMRUsbPortTarget::ACXMRUsbPortTarget()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	PortRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PortRoot"));
	SetRootComponent(PortRoot);

	OutOfPort = CreateDefaultSubobject<UArrowComponent>(TEXT("OutOfPort"));
	OutOfPort->SetupAttachment(PortRoot);
	OutOfPort->ArrowSize = 0.1f;
	OutOfPort->SetHiddenInGame(true);

	auto MakeShape = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Shape = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Shape->SetupAttachment(PortRoot);
		Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shape->SetCastShadow(false);
		Shape->SetCanEverAffectNavigation(false);
		return Shape;
	};
	BarTop    = MakeShape(TEXT("BarTop"));
	BarBottom = MakeShape(TEXT("BarBottom"));
	BarLeft   = MakeShape(TEXT("BarLeft"));
	BarRight  = MakeShape(TEXT("BarRight"));
	Indicator = MakeShape(TEXT("Indicator"));
	GuideBeam = MakeShape(TEXT("GuideBeam"));

	// Engine and plugin content only, never project content — the plugin stays portable.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder"));
	// Self-lit, so the colours read against the camera image; the lit engine material is the fallback.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitFinder(TEXT("/CXMR/Core/Materials/M_CXMRUnlitColor"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<USoundBase> SoundFinder(TEXT("/Engine/VREditor/Sounds/UI/Object_Snaps_To_Grid"));
	if (CubeFinder.Succeeded())     { BarMesh   = CubeFinder.Object; }
	if (CylinderFinder.Succeeded()) { GuideMesh = CylinderFinder.Object; }
	if (UnlitFinder.Succeeded())    { BarMaterial = UnlitFinder.Object; }
	else if (BasicFinder.Succeeded()) { BarMaterial = BasicFinder.Object; }
	if (SoundFinder.Succeeded())    { AlignedSound = SoundFinder.Object; }
}

void ACXMRUsbPortTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Here as well as at play, so the marker follows FrameSize / IndicatorOffset while it is being fitted to the CAD opening.
	LayoutFrame();
	ApplyState();
}

FQuat ACXMRUsbPortTarget::GetPortTurn() const
{
	// Only turns, never IndicatorOffset's location: the port centre stays this actor's location, which is what
	// IsNearestPort and the distance test measure from.
	const FQuat MeshTurn = (bAxisFromMesh && IndicatorMesh) ? IndicatorOffset.GetRotation() : FQuat::Identity;
	// AxisTurn goes first so it reads in the mesh's own space — a property of that mesh, not of where the port sits.
	return MeshTurn * AxisTurn.Quaternion();
}

FVector ACXMRUsbPortTarget::GetPortAxis() const
{
	// Quat rather than the actor's transform: a scaled actor must not skew the axis.
	return (GetActorQuat() * GetPortTurn()).GetForwardVector();
}

void ACXMRUsbPortTarget::LayoutFrame(float Swell)
{
	UMaterialInterface* Paint = FrameMaterial ? static_cast<UMaterialInterface*>(FrameMaterial) : BarMaterial.Get();

	// Everything but IndicatorMesh itself is placed in the port's frame; the mesh already carries IndicatorOffset whole.
	const FQuat Turn = GetPortTurn();

	const float T = FrameThickness;
	const float HalfW = FrameSize.X * 0.5f;
	const float HalfH = FrameSize.Y * 0.5f;
	// X points out of the port. The bars stand just in front of the opening so they never sink into the CAD face.
	const float X = T * 0.5f;

	struct FBarLayout { UStaticMeshComponent* Bar; FVector Location; FVector Size; };
	const FBarLayout Layout[] = {
		{ BarTop,    FVector(X, 0.,  HalfH + T * 0.5f),  FVector(T, FrameSize.X + 2.f * T, T) },
		{ BarBottom, FVector(X, 0., -(HalfH + T * 0.5f)), FVector(T, FrameSize.X + 2.f * T, T) },
		{ BarLeft,   FVector(X, -(HalfW + T * 0.5f), 0.), FVector(T, T, FrameSize.Y) },
		{ BarRight,  FVector(X,  HalfW + T * 0.5f, 0.),  FVector(T, T, FrameSize.Y) },
	};
	for (const FBarLayout& Item : Layout)
	{
		if (!Item.Bar)
		{
			continue;
		}
		Item.Bar->SetStaticMesh(BarMesh);
		Item.Bar->SetMaterial(0, Paint);
		// Scaling location and size together swells the frame about the port centre.
		Item.Bar->SetRelativeLocation(Turn.RotateVector(Item.Location * Swell));
		Item.Bar->SetRelativeRotation(Turn);
		Item.Bar->SetRelativeScale3D(Item.Size * Swell / CubeCm);
	}

	if (Indicator)
	{
		// The same about the port centre, so a mesh whose origin sits far away still swells in place.
		FTransform Offset = IndicatorOffset;
		Offset.SetLocation(Offset.GetLocation() * Swell);
		Offset.SetScale3D(Offset.GetScale3D() * Swell);
		Indicator->SetStaticMesh(IndicatorMesh);
		Indicator->SetRelativeTransform(Offset);
	}

	if (OutOfPort)
	{
		OutOfPort->SetRelativeRotation(Turn);
	}

	if (GuideBeam)
	{
		// The basic cylinder stands on Z; turned onto X it runs out of the opening toward the person.
		const FQuat AlongX(FVector::YAxisVector, UE_HALF_PI);
		const float Diameter = GuideDiameter / CubeCm;
		GuideBeam->SetStaticMesh(GuideMesh);
		GuideBeam->SetMaterial(0, Paint);
		GuideBeam->SetRelativeTransform(FTransform(
			Turn * AlongX,
			Turn.RotateVector(FVector(GuideGap + GuideLength * 0.5f, 0., 0.)),
			FVector(Diameter, Diameter, GuideLength / CubeCm)));
	}
}

void ACXMRUsbPortTarget::BeginPlay()
{
	Super::BeginPlay();

	if (Label.IsEmpty())
	{
		Label = GetActorNameOrLabel();
	}

	if (BarMaterial)
	{
		FrameMaterial = UMaterialInstanceDynamic::Create(BarMaterial, this);
		FrameMaterial->SetScalarParameterValue(TEXT("Glow"), PortRestGlow);
	}
	LayoutFrame();
	ApplyState();
}

void ACXMRUsbPortTarget::RefreshMarker()
{
	LayoutFrame();
	ApplyState();
}

UCXMRVirtualHandComponent* ACXMRUsbPortTarget::FindHands()
{
	// Looked up lazily: the pawn may spawn after this actor, and may be replaced during a session.
	if (!Hands.IsValid())
	{
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			Hands = Pawn->FindComponentByClass<UCXMRVirtualHandComponent>();
		}
	}
	return Hands.Get();
}

bool ACXMRUsbPortTarget::IsNearestPort(const FVector& Tip, float Distance) const
{
	// Ports sit a couple of centimetres apart on a console. Only the nearest reacts, and one already reacting keeps a
	// small lead, so a tip hovering between neighbours does not hand the guide back and forth every frame.
	const float Mine = Distance - (State != ECXMRPortState::Idle ? PortNearestLead : 0.f);
	for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
	{
		const ACXMRUsbPortTarget* Other = *It;
		if (Other == this)
		{
			continue;
		}
		const float Theirs = FVector::Distance(Tip, Other->GetActorLocation()) - (Other->State != ECXMRPortState::Idle ? PortNearestLead : 0.f);
		if (Theirs < Mine || (Theirs == Mine && Other < this))
		{
			return false;
		}
	}
	return true;
}

void ACXMRUsbPortTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector Tip;
	FVector Direction;
	const UCXMRVirtualHandComponent* HandComponent = FindHands();
	const bool bHavePlug = HandComponent && HandComponent->GetPlugTip(Tip, Direction);

	ECXMRPortState NewState = ECXMRPortState::Idle;
	if (bHavePlug)
	{
		const float Distance = FVector::Distance(Tip, GetActorLocation());
		if (IsNearestPort(Tip, Distance))
		{
			// Going in means travelling against the arrow, which points out of the port — turned with the mesh, so a
			// slanted opening is judged along the way it actually faces.
			const float Alignment = FVector::DotProduct(Direction, -GetPortAxis());
			const bool bWasAligned = State == ECXMRPortState::Aligned;
			const float AngleLimit = FMath::Cos(FMath::DegreesToRadians(bWasAligned ? FMath::Min(MaxAngle + AngleHysteresis, 89.f) : MaxAngle));
			const float DistanceLimit = bWasAligned ? ExitDistance : EnterDistance;
			const float ApproachLimit = ApproachDistance + (State != ECXMRPortState::Idle ? PortApproachHysteresis : 0.f);

			if (Distance <= DistanceLimit && Alignment >= AngleLimit)
			{
				NewState = ECXMRPortState::Aligned;
			}
			else if (ApproachDistance > 0.f && Distance <= ApproachLimit)
			{
				NewState = ECXMRPortState::Approach;
			}
		}
	}

	SetState(NewState);
	Animate(DeltaSeconds);
}

void ACXMRUsbPortTarget::SetState(ECXMRPortState NewState)
{
	if (NewState == State)
	{
		return;
	}
	const bool bWasAligned = State == ECXMRPortState::Aligned;
	State = NewState;

	if (State == ECXMRPortState::Aligned)
	{
		UE_LOG(LogCXMRPort, Log, TEXT("Port '%s' ALIGNED (plug lined up)"), *Label);
		PopElapsed = 0.f;
		if (AlignedSound)
		{
			UGameplayStatics::PlaySound2D(this, AlignedSound, SoundVolume);
		}
	}
	else if (bWasAligned)
	{
		UE_LOG(LogCXMRPort, Log, TEXT("Port '%s' released"), *Label);
	}
	if (State == ECXMRPortState::Approach)
	{
		PulseTime = 0.f;
	}
	ApplyState();
}

void ACXMRUsbPortTarget::ApplyState()
{
	const bool bActive = State != ECXMRPortState::Idle;
	if (FrameMaterial)
	{
		const FLinearColor Color = State == ECXMRPortState::Aligned ? AlignedColor : (bActive ? ApproachColor : IdleColor);
		FrameMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	const bool bVisible = bActive || bShowWhenIdle;
	const bool bCustomMesh = IndicatorMesh != nullptr;

	for (UStaticMeshComponent* Bar : { BarTop.Get(), BarBottom.Get(), BarLeft.Get(), BarRight.Get() })
	{
		if (Bar)
		{
			Bar->SetVisibility(bVisible && !bCustomMesh);
		}
	}

	if (Indicator)
	{
		Indicator->SetVisibility(bVisible && bCustomMesh);
		if (bCustomMesh)
		{
			// Idle keeps the mesh's own look; approach and aligned paint every slot. A null override hands a slot back to the mesh.
			const bool bOwnMaterials = !bActive && bKeepMeshMaterialsWhenIdle;
			UMaterialInterface* Paint = FrameMaterial ? static_cast<UMaterialInterface*>(FrameMaterial) : BarMaterial.Get();
			for (int32 Slot = 0; Slot < Indicator->GetNumMaterials(); ++Slot)
			{
				Indicator->SetMaterial(Slot, bOwnMaterials ? nullptr : Paint);
			}
		}
	}

	if (GuideBeam)
	{
		GuideBeam->SetVisibility(bShowGuide && bActive);
	}
}

void ACXMRUsbPortTarget::Animate(float DeltaSeconds)
{
	if (FrameMaterial)
	{
		float Glow = PortRestGlow;
		if (State == ECXMRPortState::Approach)
		{
			PulseTime += DeltaSeconds;
			const float Wave = 0.5f + 0.5f * FMath::Sin(UE_TWO_PI * PulseRate * PulseTime);
			Glow = FMath::Lerp(PortRestGlow, GlowStrength, Wave);
		}
		else if (State == ECXMRPortState::Aligned)
		{
			Glow = GlowStrength;
		}
		FrameMaterial->SetScalarParameterValue(TEXT("Glow"), Glow);
	}

	if (PopElapsed >= 0.f)
	{
		PopElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(PopElapsed / PopSeconds, 0.f, 1.f);
		// Up and back down in one arch; ends exactly at 1.
		LayoutFrame(Alpha < 1.f ? 1.f + (PopScale - 1.f) * FMath::Sin(UE_PI * Alpha) : 1.f);
		if (Alpha >= 1.f)
		{
			PopElapsed = -1.f;
		}
	}
}
