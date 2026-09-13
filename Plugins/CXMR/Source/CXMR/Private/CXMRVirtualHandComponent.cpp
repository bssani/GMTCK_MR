// Copyright GMTCK CX.

#include "CXMRVirtualHandComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayTypes.h"
#include "IHandTracker.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRVirtualHands, Log, All);

static TAutoConsoleVariable<int32> CVarVirtualHands(
	TEXT("CXMR.VirtualHands"),
	0,
	TEXT("Draw the tracked hands as solid virtual geometry, one holding a virtual USB plug. 0 = off, 1 = on.\n")
	TEXT("Leave depth test (T/U) off while comparing, or the real hand shows through as well."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarVirtualHandsPreview(
	TEXT("CXMR.VirtualHands.Preview"),
	0,
	TEXT("Draw a canned pair of hands in front of the camera instead of reading the tracker.\n")
	TEXT("For checking the geometry in PIE without a headset."),
	ECVF_Default);

namespace
{
	int32 K(EHandKeypoint Keypoint) { return static_cast<int32>(Keypoint); }

	/** Engine basic shapes are 100 cm across: sphere diameter, cylinder height and diameter, cube edge. */
	constexpr float ShapeCm = 100.f;

	struct FBone { EHandKeypoint A; EHandKeypoint B; };

	const FBone Bones[] = {
		// Finger chains, metacarpal base to tip.
		{ EHandKeypoint::ThumbMetacarpal,  EHandKeypoint::ThumbProximal },
		{ EHandKeypoint::ThumbProximal,    EHandKeypoint::ThumbDistal },
		{ EHandKeypoint::ThumbDistal,      EHandKeypoint::ThumbTip },
		{ EHandKeypoint::IndexMetacarpal,  EHandKeypoint::IndexProximal },
		{ EHandKeypoint::IndexProximal,    EHandKeypoint::IndexIntermediate },
		{ EHandKeypoint::IndexIntermediate, EHandKeypoint::IndexDistal },
		{ EHandKeypoint::IndexDistal,      EHandKeypoint::IndexTip },
		{ EHandKeypoint::MiddleMetacarpal, EHandKeypoint::MiddleProximal },
		{ EHandKeypoint::MiddleProximal,   EHandKeypoint::MiddleIntermediate },
		{ EHandKeypoint::MiddleIntermediate, EHandKeypoint::MiddleDistal },
		{ EHandKeypoint::MiddleDistal,     EHandKeypoint::MiddleTip },
		{ EHandKeypoint::RingMetacarpal,   EHandKeypoint::RingProximal },
		{ EHandKeypoint::RingProximal,     EHandKeypoint::RingIntermediate },
		{ EHandKeypoint::RingIntermediate, EHandKeypoint::RingDistal },
		{ EHandKeypoint::RingDistal,       EHandKeypoint::RingTip },
		{ EHandKeypoint::LittleMetacarpal, EHandKeypoint::LittleProximal },
		{ EHandKeypoint::LittleProximal,   EHandKeypoint::LittleIntermediate },
		{ EHandKeypoint::LittleIntermediate, EHandKeypoint::LittleDistal },
		{ EHandKeypoint::LittleDistal,     EHandKeypoint::LittleTip },
		// Wrist to the base of each metacarpal.
		{ EHandKeypoint::Wrist, EHandKeypoint::ThumbMetacarpal },
		{ EHandKeypoint::Wrist, EHandKeypoint::IndexMetacarpal },
		{ EHandKeypoint::Wrist, EHandKeypoint::MiddleMetacarpal },
		{ EHandKeypoint::Wrist, EHandKeypoint::RingMetacarpal },
		{ EHandKeypoint::Wrist, EHandKeypoint::LittleMetacarpal },
		// Knuckle line, so the back of the hand reads as one surface rather than five sticks.
		{ EHandKeypoint::IndexProximal,  EHandKeypoint::MiddleProximal },
		{ EHandKeypoint::MiddleProximal, EHandKeypoint::RingProximal },
		{ EHandKeypoint::RingProximal,   EHandKeypoint::LittleProximal },
	};

	struct FCannedJoint { FVector Position; float Radius; };

	// A relaxed right hand, loosely pinching. X toward the fingertips, Y toward the thumb, Z out of the back
	// of the hand; cm, wrist at the origin. Order matches EHandKeypoint.
	const FCannedJoint CannedHand[] = {
		{ FVector( 4.5f,  0.2f,  0.0f), 1.8f },   // Palm
		{ FVector( 0.0f,  0.0f,  0.0f), 1.6f },   // Wrist
		{ FVector( 1.2f,  1.8f, -0.6f), 1.1f },   // Thumb
		{ FVector( 3.8f,  4.0f, -1.2f), 1.0f },
		{ FVector( 6.4f,  5.2f, -1.8f), 0.85f },
		{ FVector( 8.6f,  5.6f, -2.4f), 0.7f },
		{ FVector( 1.5f,  1.2f,  0.0f), 1.0f },   // Index
		{ FVector( 8.0f,  3.0f,  0.0f), 1.0f },
		{ FVector(12.0f,  3.3f, -0.8f), 0.85f },
		{ FVector(14.2f,  3.4f, -2.0f), 0.75f },
		{ FVector(15.6f,  3.4f, -3.2f), 0.65f },
		{ FVector( 1.5f,  0.4f,  0.0f), 1.0f },   // Middle
		{ FVector( 8.3f,  1.0f,  0.0f), 1.0f },
		{ FVector(12.8f,  1.1f,  0.0f), 0.85f },
		{ FVector(15.6f,  1.2f,  0.0f), 0.75f },
		{ FVector(17.7f,  1.2f,  0.0f), 0.65f },
		{ FVector( 1.5f, -0.4f,  0.0f), 1.0f },   // Ring
		{ FVector( 7.8f, -1.0f,  0.0f), 0.95f },
		{ FVector(12.0f, -1.2f,  0.0f), 0.8f },
		{ FVector(14.6f, -1.3f,  0.0f), 0.7f },
		{ FVector(16.6f, -1.4f,  0.0f), 0.6f },
		{ FVector( 1.5f, -1.2f,  0.0f), 0.9f },   // Little
		{ FVector( 7.0f, -2.8f,  0.0f), 0.85f },
		{ FVector(10.3f, -3.3f,  0.0f), 0.7f },
		{ FVector(12.2f, -3.6f,  0.0f), 0.6f },
		{ FVector(14.0f, -3.9f,  0.0f), 0.55f },
	};
	static_assert(UE_ARRAY_COUNT(CannedHand) == EHandKeypointCount, "Canned pose must list every EHandKeypoint");

	FTransform SphereAt(const FVector& Center, float Radius)
	{
		return FTransform(FQuat::Identity, Center, FVector(2.f * Radius / ShapeCm));
	}

	/** Cylinder from A to B. The basic cylinder stands on its Z axis. */
	FTransform CylinderBetween(const FVector& A, const FVector& B, float Radius)
	{
		const FVector Delta = B - A;
		const float Length = Delta.Size();
		if (Length < KINDA_SMALL_NUMBER)
		{
			return FTransform(FQuat::Identity, A, FVector::ZeroVector);
		}
		const FQuat Rotation = FRotationMatrix::MakeFromZ(Delta / Length).ToQuat();
		const float Diameter = 2.f * Radius / ShapeCm;
		return FTransform(Rotation, (A + B) * 0.5f, FVector(Diameter, Diameter, Length / ShapeCm));
	}

	/** Ellipsoid from the wrist to the middle knuckle, as wide as the knuckle line. */
	FTransform PalmBetween(const TArray<FVector>& P, float KnuckleRadius, float Thickness)
	{
		const FVector Wrist   = P[K(EHandKeypoint::Wrist)];
		const FVector Knuckle = P[K(EHandKeypoint::MiddleProximal)];

		FVector Along = Knuckle - Wrist;
		const float Length = Along.Size();
		FVector Across = P[K(EHandKeypoint::IndexProximal)] - P[K(EHandKeypoint::LittleProximal)];
		if (Length < 1.f)
		{
			return FTransform(FQuat::Identity, Wrist, FVector::ZeroVector);
		}
		Along /= Length;
		Across -= (Across | Along) * Along;
		const float Width = Across.Size();
		if (Width < 1.f)
		{
			return FTransform(FQuat::Identity, Wrist, FVector::ZeroVector);
		}

		const FQuat Rotation = FRotationMatrix::MakeFromXY(Along, Across / Width).ToQuat();
		const FVector Scale(Length / ShapeCm, (Width + 2.f * KnuckleRadius) / ShapeCm, Thickness / ShapeCm);
		return FTransform(Rotation, (Wrist + Knuckle) * 0.5f, Scale);
	}

	void WriteInstances(UInstancedStaticMeshComponent* Component, const TArray<FTransform>& Transforms)
	{
		if (Component->GetInstanceCount() != Transforms.Num())
		{
			Component->ClearInstances();
			Component->AddInstances(Transforms, /*bShouldReturnIndices*/ false, /*bWorldSpace*/ false, /*bUpdateNavigation*/ false);
		}
		else
		{
			Component->BatchUpdateInstancesTransforms(0, Transforms, /*bWorldSpace*/ false, /*bMarkRenderStateDirty*/ true, /*bTeleport*/ true);
		}
	}
}

UCXMRVirtualHandComponent::UCXMRVirtualHandComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Late in the frame, so the pose read is the newest the tracker has for this frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;

	// Engine content, not project content — the plugin stays portable.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereFinder(TEXT("/Engine/BasicShapes/Sphere"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaterialFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	if (SphereFinder.Succeeded())   { SphereMesh    = SphereFinder.Object; }
	if (CylinderFinder.Succeeded()) { CylinderMesh  = CylinderFinder.Object; }
	if (CubeFinder.Succeeded())     { CubeMesh      = CubeFinder.Object; }
	if (MaterialFinder.Succeeded()) { ShapeMaterial = MaterialFinder.Object; }
}

IHandTracker* UCXMRVirtualHandComponent::GetHandTracker() const
{
	IModularFeatures& Features = IModularFeatures::Get();
	const FName Feature = IHandTracker::GetModularFeatureName();
	if (Features.GetModularFeatureImplementationCount(Feature) == 0)
	{
		return nullptr;
	}
	return &Features.GetModularFeature<IHandTracker>(Feature);
}

void UCXMRVirtualHandComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!SphereMesh || !CylinderMesh || !CubeMesh || !ShapeMaterial)
	{
		// Say it now: a hand that silently never draws looks exactly like a tracker that never sees one.
		UE_LOG(LogCXMRVirtualHands, Warning, TEXT("Virtual hands are missing a mesh or the material and will not draw."));
		return;
	}

	AActor* Owner = GetOwner();
	LeftJoints  = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("VirtualHand_LeftJoints"));
	LeftBones   = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("VirtualHand_LeftBones"));
	RightJoints = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("VirtualHand_RightJoints"));
	RightBones  = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("VirtualHand_RightBones"));
	PlugBody    = NewObject<UStaticMeshComponent>(Owner, TEXT("VirtualHand_PlugBody"));
	PlugTip     = NewObject<UStaticMeshComponent>(Owner, TEXT("VirtualHand_PlugTip"));
	PlugCable   = NewObject<UStaticMeshComponent>(Owner, TEXT("VirtualHand_PlugCable"));

	ConfigureShape(LeftJoints,  SphereMesh,   SkinColor);
	ConfigureShape(LeftBones,   CylinderMesh, SkinColor);
	ConfigureShape(RightJoints, SphereMesh,   SkinColor);
	ConfigureShape(RightBones,  CylinderMesh, SkinColor);
	ConfigureShape(PlugBody,    CubeMesh,     PlugColor);
	ConfigureShape(PlugTip,     CubeMesh,     TipColor);
	ConfigureShape(PlugCable,   CylinderMesh, PlugColor);
}

void UCXMRVirtualHandComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	const TArray<UStaticMeshComponent*> Parts = {
		LeftJoints.Get(), LeftBones.Get(), RightJoints.Get(), RightBones.Get(), PlugBody.Get(), PlugTip.Get(), PlugCable.Get() };
	for (UStaticMeshComponent* Part : Parts)
	{
		if (Part)
		{
			Part->DestroyComponent();
		}
	}
	Super::EndPlay(Reason);
}

void UCXMRVirtualHandComponent::ConfigureShape(UStaticMeshComponent* Component, UStaticMesh* Mesh, const FLinearColor& Color)
{
	Component->SetStaticMesh(Mesh);

	UMaterialInstanceDynamic* Material = UMaterialInstanceDynamic::Create(ShapeMaterial, Component);
	Material->SetVectorParameterValue(TEXT("Color"), Color);
	Component->SetMaterial(0, Material);

	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);

	// Poses arrive in world space. Absolute transforms on an identity component make instance space equal
	// world space, so the pawn moving underneath never drags the hands along a second time.
	Component->SetUsingAbsoluteLocation(true);
	Component->SetUsingAbsoluteRotation(true);
	Component->SetUsingAbsoluteScale(true);
	if (USceneComponent* Root = GetOwner()->GetRootComponent())
	{
		Component->SetupAttachment(Root);
	}
	Component->RegisterComponent();
	Component->SetWorldTransform(FTransform::Identity);
	Component->SetVisibility(false);
}

void UCXMRVirtualHandComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bPreview = CVarVirtualHandsPreview.GetValueOnGameThread() != 0;
	const bool bOn = bPreview || CVarVirtualHands.GetValueOnGameThread() != 0;

	if (bOn != bWasOn)
	{
		bWasOn = bOn;
		UE_LOG(LogCXMRVirtualHands, Log, TEXT("Virtual hands %s%s, tracker %s"),
			bOn ? TEXT("ON") : TEXT("OFF"),
			bPreview ? TEXT(" (canned preview pose)") : TEXT(""),
			GetHandTracker() ? TEXT("present") : TEXT("MISSING"));
	}

	TArray<FVector> Positions;
	TArray<float> Radii;
	for (EControllerHand Hand : { EControllerHand::Left, EControllerHand::Right })
	{
		const bool bDrawn = bOn && GetJoints(Hand, bPreview, Positions, Radii);
		SetHandVisible(Hand, bDrawn);
		if (bDrawn)
		{
			UpdateHand(Hand, Positions, Radii);
		}

		if (Hand == PlugHand)
		{
			const bool bPlug = bDrawn && bShowPlug;
			SetPlugVisible(bPlug);
			if (bPlug)
			{
				UpdatePlug(Positions);
			}
		}
	}
}

bool UCXMRVirtualHandComponent::GetJoints(EControllerHand Hand, bool bPreview, TArray<FVector>& OutPositions, TArray<float>& OutRadii) const
{
	if (bPreview)
	{
		const UCameraComponent* Camera = GetOwner()->FindComponentByClass<UCameraComponent>();
		if (!Camera)
		{
			return false;
		}

		const FVector Forward = Camera->GetForwardVector();
		const FVector Right   = Camera->GetRightVector();
		const FVector Up      = Camera->GetUpVector();
		const bool bRightHand = Hand == EControllerHand::Right;

		// Held out low and to each side, palms down, fingers pointing away from the viewer.
		const FVector Wrist = Camera->GetComponentLocation() + Forward * 30.f + Right * (bRightHand ? 10.f : -10.f) - Up * 18.f;
		// The canned pose is a right hand; a left hand is the same pose with the thumb side mirrored.
		const FVector ThumbSide = bRightHand ? -Right : Right;

		OutPositions.SetNum(EHandKeypointCount);
		OutRadii.SetNum(EHandKeypointCount);
		for (int32 i = 0; i < EHandKeypointCount; ++i)
		{
			const FVector& Local = CannedHand[i].Position;
			OutPositions[i] = Wrist + Forward * Local.X + ThumbSide * Local.Y + Up * Local.Z;
			OutRadii[i] = CannedHand[i].Radius;
		}
		return true;
	}

	IHandTracker* Tracker = GetHandTracker();
	if (!Tracker)
	{
		return false;
	}

	TArray<FQuat> Rotations;   // required by the API, deliberately unused — see the header
	bool bIsTracked = false;
	if (!Tracker->GetAllKeypointStates(Hand, OutPositions, Rotations, OutRadii, bIsTracked))
	{
		return false;
	}

	// The tracker keeps returning the last pose after tracking drops. A virtual hand frozen beside the console
	// would read as a real hand that stopped moving, so it disappears instead.
	return bIsTracked && OutPositions.Num() >= EHandKeypointCount;
}

void UCXMRVirtualHandComponent::UpdateHand(EControllerHand Hand, const TArray<FVector>& Positions, const TArray<float>& Radii)
{
	const bool bLeft = Hand == EControllerHand::Left;
	UInstancedStaticMeshComponent* Joints = bLeft ? LeftJoints : RightJoints;
	UInstancedStaticMeshComponent* BoneParts = bLeft ? LeftBones : RightBones;
	if (!Joints || !BoneParts)
	{
		return;
	}

	auto RadiusOf = [&](int32 Index)
	{
		const float Reported = Radii.IsValidIndex(Index) ? Radii[Index] : 0.f;
		return FMath::Max(Reported, MinJointRadius) * RadiusScale;
	};

	TArray<FTransform> JointTransforms;
	JointTransforms.Reserve(EHandKeypointCount + 1);
	for (int32 i = 0; i < EHandKeypointCount; ++i)
	{
		JointTransforms.Add(SphereAt(Positions[i], RadiusOf(i)));
	}
	JointTransforms.Add(PalmBetween(Positions, RadiusOf(K(EHandKeypoint::MiddleProximal)), PalmThickness));
	WriteInstances(Joints, JointTransforms);

	TArray<FTransform> BoneTransforms;
	BoneTransforms.Reserve(UE_ARRAY_COUNT(Bones));
	for (const FBone& Bone : Bones)
	{
		const int32 A = K(Bone.A);
		const int32 B = K(Bone.B);
		BoneTransforms.Add(CylinderBetween(Positions[A], Positions[B], FMath::Min(RadiusOf(A), RadiusOf(B))));
	}
	WriteInstances(BoneParts, BoneTransforms);
}

bool UCXMRVirtualHandComponent::ComputeGrip(const TArray<FVector>& Positions, FTransform& OutGrip) const
{
	const FVector ThumbTip = Positions[K(EHandKeypoint::ThumbTip)];
	const FVector IndexTip = Positions[K(EHandKeypoint::IndexTip)];

	FVector Along = IndexTip - Positions[K(EHandKeypoint::IndexProximal)];
	if (!Along.Normalize())
	{
		return false;
	}

	// The plug is pinched flat between thumb and index, so its thin axis runs between the two tips.
	FVector Pinch = ThumbTip - IndexTip;
	Pinch -= (Pinch | Along) * Along;
	if (!Pinch.Normalize())
	{
		// Tips touching, or in line with the finger: any perpendicular keeps the plug drawable.
		const FVector Helper = FMath::Abs(Along.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		Pinch = FVector::CrossProduct(Along, Helper).GetSafeNormal();
	}

	OutGrip = PlugOffset * FTransform(FRotationMatrix::MakeFromXZ(Along, Pinch).ToQuat(), (ThumbTip + IndexTip) * 0.5f);
	return true;
}

bool UCXMRVirtualHandComponent::GetPlugTip(FVector& OutTipLocation, FVector& OutDirection) const
{
	const bool bPreview = CVarVirtualHandsPreview.GetValueOnGameThread() != 0;

	TArray<FVector> Positions;
	TArray<float> Radii;
	FTransform Grip;
	if (!GetJoints(PlugHand, bPreview, Positions, Radii) || !ComputeGrip(Positions, Grip))
	{
		return false;
	}

	OutTipLocation = Grip.TransformPosition(FVector(PlugBodySize.X * 0.5f + PlugTipSize.X, 0., 0.));
	OutDirection = Grip.GetUnitAxis(EAxis::X);
	return true;
}

void UCXMRVirtualHandComponent::UpdatePlug(const TArray<FVector>& Positions)
{
	if (!PlugBody || !PlugTip || !PlugCable)
	{
		return;
	}

	FTransform Grip;
	if (!ComputeGrip(Positions, Grip))
	{
		SetPlugVisible(false);
		return;
	}
	const float BodyHalf = PlugBodySize.X * 0.5f;

	PlugBody->SetWorldTransform(FTransform(FQuat::Identity, FVector::ZeroVector, PlugBodySize / ShapeCm) * Grip);
	PlugTip->SetWorldTransform(FTransform(FQuat::Identity, FVector(BodyHalf + PlugTipSize.X * 0.5f, 0., 0.), PlugTipSize / ShapeCm) * Grip);

	// The basic cylinder stands on Z; turn it onto X so it trails back out of the body.
	const FQuat LieAlongX(FVector::YAxisVector, UE_HALF_PI);
	const float CableScale = CableDiameter / ShapeCm;
	PlugCable->SetWorldTransform(
		FTransform(LieAlongX, FVector(-(BodyHalf + CableLength * 0.5f), 0., 0.), FVector(CableScale, CableScale, CableLength / ShapeCm)) * Grip);
}

void UCXMRVirtualHandComponent::SetHandVisible(EControllerHand Hand, bool bVisible)
{
	const bool bLeft = Hand == EControllerHand::Left;
	if (UInstancedStaticMeshComponent* Joints = bLeft ? LeftJoints : RightJoints)
	{
		Joints->SetVisibility(bVisible);
	}
	if (UInstancedStaticMeshComponent* BoneParts = bLeft ? LeftBones : RightBones)
	{
		BoneParts->SetVisibility(bVisible);
	}
}

void UCXMRVirtualHandComponent::SetPlugVisible(bool bVisible)
{
	for (UStaticMeshComponent* Part : { PlugBody.Get(), PlugTip.Get(), PlugCable.Get() })
	{
		if (Part)
		{
			Part->SetVisibility(bVisible);
		}
	}
}
