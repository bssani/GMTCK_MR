// Copyright GMTCK CX.

#include "CXMRUsbPortTarget.h"
#include "CXMRVirtualHandComponent.h"

#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRPort, Log, All);

namespace
{
	/** The engine basic cube is 100 cm on a side. */
	constexpr float CubeCm = 100.f;

	/** Extra degrees allowed before an aligned port lets go, so a plug held at the limit does not flicker. */
	constexpr float AngleHysteresis = 10.f;
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

	// Engine content, not project content — the plugin stays portable.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (CubeFinder.Succeeded())     { BarMesh     = CubeFinder.Object; }
	if (MaterialFinder.Succeeded()) { BarMaterial = MaterialFinder.Object; }
}

void ACXMRUsbPortTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// Here as well as at play, so the marker follows FrameSize / IndicatorOffset while it is being fitted to the CAD opening.
	LayoutFrame();
}

void ACXMRUsbPortTarget::LayoutFrame()
{
	const bool bCustomMesh = IndicatorMesh != nullptr;

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
		Item.Bar->SetMaterial(0, FrameMaterial ? static_cast<UMaterialInterface*>(FrameMaterial) : BarMaterial.Get());
		Item.Bar->SetRelativeLocation(Item.Location);
		Item.Bar->SetRelativeScale3D(Item.Size / CubeCm);
		Item.Bar->SetVisibility(!bCustomMesh);
	}

	if (Indicator)
	{
		Indicator->SetStaticMesh(IndicatorMesh);
		Indicator->SetRelativeTransform(IndicatorOffset);
		Indicator->SetVisibility(bCustomMesh);
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

void ACXMRUsbPortTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector Tip;
	FVector Direction;
	const UCXMRVirtualHandComponent* HandComponent = FindHands();
	const bool bHavePlug = HandComponent && HandComponent->GetPlugTip(Tip, Direction);

	bool bNowAligned = false;
	if (bHavePlug)
	{
		const float Distance = FVector::Distance(Tip, GetActorLocation());
		// Going in means travelling against the arrow, which points out of the port.
		const float Alignment = FVector::DotProduct(Direction, -GetActorForwardVector());

		if (bAligned)
		{
			const float ExitLimit = FMath::Cos(FMath::DegreesToRadians(FMath::Min(MaxAngle + AngleHysteresis, 89.f)));
			bNowAligned = Distance <= ExitDistance && Alignment >= ExitLimit;
		}
		else
		{
			const float EnterLimit = FMath::Cos(FMath::DegreesToRadians(MaxAngle));
			bNowAligned = Distance <= EnterDistance && Alignment >= EnterLimit;
		}
	}

	if (bNowAligned != bAligned)
	{
		bAligned = bNowAligned;
		UE_LOG(LogCXMRPort, Log, TEXT("Port '%s' %s"), *Label, bAligned ? TEXT("ALIGNED (plug lined up)") : TEXT("released"));
		ApplyState();
	}
}

void ACXMRUsbPortTarget::ApplyState()
{
	if (FrameMaterial)
	{
		FrameMaterial->SetVectorParameterValue(TEXT("Color"), bAligned ? AlignedColor : IdleColor);
	}

	const bool bVisible = bAligned || bShowWhenIdle;
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
			// Idle keeps the mesh's own look; aligned paints every slot. A null override hands a slot back to the mesh.
			const bool bOwnMaterials = !bAligned && bKeepMeshMaterialsWhenIdle;
			UMaterialInterface* Paint = FrameMaterial ? static_cast<UMaterialInterface*>(FrameMaterial) : BarMaterial.Get();
			for (int32 Slot = 0; Slot < Indicator->GetNumMaterials(); ++Slot)
			{
				Indicator->SetMaterial(Slot, bOwnMaterials ? nullptr : Paint);
			}
		}
	}
}
