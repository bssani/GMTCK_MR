// Copyright GMTCK CX.

#include "CXMRUsbPortTarget.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRVirtualHandComponent.h"

#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRPort, Log, All);

#define LOCTEXT_NAMESPACE "CXMRPort"

// The four numbers are per-actor properties, but an operator tuning in a headset wants one knob for every port at once
// — and the tuning window's saved value must land whatever order the ports begin play in. A console variable is both:
// global, and read fresh every frame. Below zero means "leave each port its own value", which is how they all start.
static TAutoConsoleVariable<int32> CVarPortDebug(
	TEXT("CXMR.Port.Debug"),
	0,
	TEXT("Draw what a USB port actually judges: the ball of Enter Distance around the PORT (not around the marker mesh, ")
	TEXT("which may sit anywhere), the fainter ball where it lets go again, the Max Angle cone along the insertion ")
	TEXT("axis, and a line to the plug tip once it is within Approach Distance."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPortEnter(
	TEXT("CXMR.Port.EnterDistance"),
	-1.f,
	TEXT("cm from the port centre that lines the plug up, for every port. Below zero = each port keeps its own."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPortExit(
	TEXT("CXMR.Port.ExitDistance"),
	-1.f,
	TEXT("cm the plug must pass to let a lined-up port go again. Below zero = each port keeps its own."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPortMaxAngle(
	TEXT("CXMR.Port.MaxAngle"),
	-1.f,
	TEXT("Degrees off the port axis that still count as straight. Below zero = each port keeps its own."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPortApproach(
	TEXT("CXMR.Port.ApproachDistance"),
	-1.f,
	TEXT("cm at which the nearest port starts guiding. Below zero = each port keeps its own."),
	ECVF_Default);

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

float ACXMRUsbPortTarget::GetEnterDistance() const
{
	const float Tuned = CVarPortEnter.GetValueOnGameThread();
	return Tuned >= 0.f ? Tuned : EnterDistance;
}

float ACXMRUsbPortTarget::GetExitDistance() const
{
	const float Tuned = CVarPortExit.GetValueOnGameThread();
	// Never inside the entering ball: the two exist to stop the state flickering at the edge, and crossed over they
	// would do the opposite.
	return FMath::Max(Tuned >= 0.f ? Tuned : ExitDistance, GetEnterDistance());
}

float ACXMRUsbPortTarget::GetMaxAngle() const
{
	const float Tuned = CVarPortMaxAngle.GetValueOnGameThread();
	return Tuned >= 0.f ? Tuned : MaxAngle;
}

float ACXMRUsbPortTarget::GetApproachDistance() const
{
	const float Tuned = CVarPortApproach.GetValueOnGameThread();
	return Tuned >= 0.f ? Tuned : ApproachDistance;
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
	RegisterTunables();
}

void ACXMRUsbPortTarget::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
		// Hand the rows on, so reloading a vehicle that carries the ports does not empty the window for the session.
		for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->IsActorBeingDestroyed())
			{
				It->RegisterTunables();
				break;
			}
		}
	}
	Super::EndPlay(Reason);
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

	// Measured whether or not this port is the one reacting, so the debug draw and the readout can show a port that is
	// NOT lighting up — which is the case worth looking at. Going in means travelling against the arrow, which points
	// out of the port, turned with the mesh: a slanted opening is judged along the way it actually faces.
	LastDistance = bHavePlug ? FVector::Distance(Tip, GetActorLocation()) : -1.f;
	LastAngleDeg = bHavePlug
		? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp<float>(FVector::DotProduct(Direction, -GetPortAxis()), -1.f, 1.f)))
		: -1.f;

	ECXMRPortState NewState = ECXMRPortState::Idle;
	if (bHavePlug && IsNearestPort(Tip, LastDistance))
	{
		const bool bWasAligned = State == ECXMRPortState::Aligned;
		const float AngleLimit = bWasAligned ? FMath::Min(GetMaxAngle() + AngleHysteresis, 89.f) : GetMaxAngle();
		const float DistanceLimit = bWasAligned ? GetExitDistance() : GetEnterDistance();
		const float Approach = GetApproachDistance();
		const float ApproachLimit = Approach + (State != ECXMRPortState::Idle ? PortApproachHysteresis : 0.f);

		if (LastDistance <= DistanceLimit && LastAngleDeg <= AngleLimit)
		{
			NewState = ECXMRPortState::Aligned;
		}
		else if (Approach > 0.f && LastDistance <= ApproachLimit)
		{
			NewState = ECXMRPortState::Approach;
		}
	}

	SetState(NewState);
	Animate(DeltaSeconds);
	DrawDebug(Tip, bHavePlug);
}

void ACXMRUsbPortTarget::DrawDebug(const FVector& Tip, bool bHavePlug) const
{
	UWorld* World = GetWorld();
	if (!World || CVarPortDebug.GetValueOnGameThread() == 0)
	{
		return;
	}

	const FVector Centre = GetActorLocation();
	const FColor Colour = State == ECXMRPortState::Aligned  ? FColor(40, 240, 70)
	                    : State == ECXMRPortState::Approach ? FColor(255, 185, 20)
	                                                        : FColor(120, 120, 135);

	// What actually judges: a ball around the PORT, not around the marker mesh. Seeing the two apart is the point.
	DrawDebugSphere(World, Centre, GetEnterDistance(), 16, Colour, false, -1.f, SDPG_World, 0.05f);
	// Where a lined-up port lets go again.
	DrawDebugSphere(World, Centre, GetExitDistance(), 12, FColor(70, 70, 85), false, -1.f, SDPG_World, 0.03f);
	// The angle window, opening out of the port along the axis the plug is measured against.
	const float Half = FMath::DegreesToRadians(GetMaxAngle());
	DrawDebugCone(World, Centre, GetPortAxis(), GetEnterDistance() * 4.f, Half, Half, 16, Colour, false, -1.f, SDPG_World, 0.05f);

	// Only once the plug is near, or every port in the level draws a line across the cabin.
	if (bHavePlug && LastDistance >= 0.f && LastDistance <= GetApproachDistance())
	{
		DrawDebugLine(World, Tip, Centre, Colour, false, -1.f, SDPG_World, 0.05f);
	}
}

UCXMRTuningSubsystem* ACXMRUsbPortTarget::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

FText ACXMRUsbPortTarget::DescribeNearest() const
{
	const ACXMRUsbPortTarget* Near = nullptr;
	for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
	{
		if (It->LastDistance >= 0.f && (!Near || It->LastDistance < Near->LastDistance))
		{
			Near = *It;
		}
	}
	if (!Near)
	{
		return LOCTEXT("PlugUntracked", "no plug hand tracked");
	}

	const FText Verdict = Near->State == ECXMRPortState::Aligned  ? LOCTEXT("VerdictAligned", "lined up")
	                    : Near->State == ECXMRPortState::Approach ? LOCTEXT("VerdictApproach", "approaching")
	                                                             : LOCTEXT("VerdictIdle", "not reacting");
	// Two decimals would read as precision the hand tracker does not have.
	return FText::FromString(FString::Printf(TEXT("%s: %.1f cm, %.0f\u00B0 off - %s"),
		*Near->Label, Near->LastDistance, Near->LastAngleDeg, *Verdict.ToString()));
}

void ACXMRUsbPortTarget::RegisterTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return;
	}

	// Every port asks; the first one here owns the rows (Register refuses an id already taken). The values are console
	// variables, so one row reaches every port however many there are and whatever order they began play in.
	const FText Category = LOCTEXT("CatPort", "USB port");
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
	auto WriteTo = [](TAutoConsoleVariable<float>& CVar)
	{
		return [&CVar](float Value) { CVar->Set(Value, ECVF_SetByConsole); };
	};

	{
		FCXMRTunable T = Make("Port.Live", LOCTEXT("Live", "Plug at the nearest port"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeNearest(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.Debug", LOCTEXT("Debug", "Show what the port judges"), ECXMRTunableKind::Bool);
		T.Get = [] { return CVarPortDebug.GetValueOnGameThread() != 0 ? 1.0f : 0.0f; };
		T.Set = [](float Value) { CVarPortDebug->Set(Value > 0.5f ? 1 : 0, ECVF_SetByConsole); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.EnterDistance", LOCTEXT("Enter", "Lines up within"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.2f; T.Max = 10.0f; T.Delta = 0.1f; T.Default = 1.5f; T.bPersist = true;
		T.Get = [this] { return GetEnterDistance(); };
		T.Set = WriteTo(CVarPortEnter);
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.ExitDistance", LOCTEXT("Exit", "Lets go past"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.2f; T.Max = 20.0f; T.Delta = 0.1f; T.Default = 3.0f; T.bPersist = true;
		T.Get = [this] { return GetExitDistance(); };
		T.Set = WriteTo(CVarPortExit);
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.MaxAngle", LOCTEXT("Angle", "Still counts as straight"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("deg", "\u00B0"); T.Min = 5.0f; T.Max = 80.0f; T.Delta = 1.0f; T.Default = 25.0f; T.bPersist = true;
		T.Get = [this] { return GetMaxAngle(); };
		T.Set = WriteTo(CVarPortMaxAngle);
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.ApproachDistance", LOCTEXT("Approach", "Starts guiding at"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.0f; T.Max = 40.0f; T.Delta = 0.5f; T.Default = 12.0f; T.bPersist = true;
		T.Get = [this] { return GetApproachDistance(); };
		T.Set = WriteTo(CVarPortApproach);
		Tuning->Register(MoveTemp(T));
	}
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

#undef LOCTEXT_NAMESPACE
