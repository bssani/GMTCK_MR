// Copyright GMTCK CX.

#include "CXMRVirtualHandComponent.h"
#include "CXMRHandTracking.h"
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayTypes.h"
#include "IHandTracker.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "CXMRHandCutOut"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRVirtualHands, Log, All);

static TAutoConsoleVariable<int32> CVarHandCutOut(
	TEXT("CXMR.HandCutOut"),
	1,
	TEXT("Cut the tracked hands out of the virtual scene while mixed reality is on, so the real hands show in front of\n")
	TEXT("virtual surfaces (switches masking on). 0 = off: show the hands with depth test (T/U) instead."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarHandCutOutPreview(
	TEXT("CXMR.HandCutOut.Preview"),
	0,
	TEXT("Cut out a canned pair of hands in front of the camera instead of reading the tracker, even outside mixed reality.\n")
	TEXT("For checking the shape in PIE without a headset: the hands show as black holes over virtual objects."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarPlugTipDebug(
	TEXT("CXMR.PlugTipDebug"),
	0,
	TEXT("Draw the plug tip USB ports judge: a line that starts at the estimated tip and runs on in the plug's direction.\n")
	TEXT("Tune the pawn's VirtualHands > Plug Offset and Plug Tip Reach until the line starts at the real plug's tip.\n")
	TEXT("Turn the cut-out off (CXMR.HandCutOut 0) meanwhile: where the line passes behind the fingers, the hole hides it."),
	ECVF_Default);

namespace
{
	int32 K(EHandKeypoint Keypoint) { return static_cast<int32>(Keypoint); }

	/** Engine basic shapes are 100 cm across: sphere diameter, cylinder height and diameter. */
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
		// Knuckle line, so the back of the hand is one surface rather than five sticks.
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
	if (SphereFinder.Succeeded())   { SphereMesh   = SphereFinder.Object; }
	if (CylinderFinder.Succeeded()) { CylinderMesh = CylinderFinder.Object; }
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

UCXMRSubsystem* UCXMRVirtualHandComponent::GetCXMR() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRSubsystem>() : nullptr;
}

UCXMRTuningSubsystem* UCXMRVirtualHandComponent::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

void UCXMRVirtualHandComponent::BeginPlay()
{
	Super::BeginPlay();

	if (!SphereMesh || !CylinderMesh)
	{
		// Say it now: a hand that silently never shows looks exactly like a tracker that never sees one.
		UE_LOG(LogCXMRVirtualHands, Warning, TEXT("Hand cut-out is missing a mesh and will not cut anything."));
		return;
	}

	AActor* Owner = GetOwner();
	LeftJoints  = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("VirtualHand_LeftJoints"));
	LeftBones   = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("VirtualHand_LeftBones"));
	RightJoints = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("VirtualHand_RightJoints"));
	RightBones  = NewObject<UInstancedStaticMeshComponent>(Owner, TEXT("VirtualHand_RightBones"));

	ConfigureShape(LeftJoints,  SphereMesh);
	ConfigureShape(LeftBones,   CylinderMesh);
	ConfigureShape(RightJoints, SphereMesh);
	ConfigureShape(RightBones,  CylinderMesh);

	RegisterTunables();
}

void UCXMRVirtualHandComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	UpdateCutOutMasking(false, false);

	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
	}

	for (UInstancedStaticMeshComponent* Part : GetParts())
	{
		Part->DestroyComponent();
	}
	Super::EndPlay(Reason);
}

TArray<UInstancedStaticMeshComponent*, TInlineAllocator<4>> UCXMRVirtualHandComponent::GetParts() const
{
	TArray<UInstancedStaticMeshComponent*, TInlineAllocator<4>> Parts = {
		LeftJoints.Get(), LeftBones.Get(), RightJoints.Get(), RightBones.Get() };
	Parts.Remove(nullptr);
	return Parts;
}

void UCXMRVirtualHandComponent::ConfigureShape(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh)
{
	Component->SetStaticMesh(Mesh);

	Component->SetMobility(EComponentMobility::Movable);
	Component->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Component->SetCanEverAffectNavigation(false);
	Component->SetCastShadow(false);

	// A mask, never a visible mesh: written into Custom Depth, which PP_MR reads as "open a hole here", and absent from
	// every other pass — the same flags a Mask scene object gets (CXMRSceneObjectComponent::ApplyMaskRenderFlags).
	Component->SetRenderCustomDepth(true);
	Component->SetRenderInMainPass(false);
	Component->SetRenderInDepthPass(false);
	Component->SetVisibleInRayTracing(false);
	Component->SetReceivesDecals(false);
	Component->bVisibleInReflectionCaptures  = false;
	Component->bVisibleInRealTimeSkyCaptures = false;

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

// ---- Tuning window rows ----

void UCXMRVirtualHandComponent::RegisterTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return;
	}

	const FText Category = LOCTEXT("CatCutOut", "Hand cut-out");
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
		FCXMRTunable T = Make("HandCutOut.State", LOCTEXT("State", "Real hands"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeState(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("HandCutOut.On", LOCTEXT("On", "Cut the real hands out (in MR)"), ECXMRTunableKind::Bool);
		T.Default = 1.0f;
		// The console variable stays the one switch, so the window and CXMR.HandCutOut can never disagree.
		T.Get = [] { return CVarHandCutOut.GetValueOnGameThread() != 0 ? 1.0f : 0.0f; };
		T.Set = [](float Value) { CVarHandCutOut->Set(Value > 0.5f ? 1 : 0, ECVF_SetByConsole); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("HandCutOut.Padding", LOCTEXT("Padding", "Extra rim around the hand"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.0f; T.Max = 2.0f; T.Delta = 0.05f; T.Default = 0.25f; T.bPersist = true;
		T.Get = [this] { return MaskPadding; };
		T.Set = [this](float Value) { MaskPadding = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("HandCutOut.FingerThickness", LOCTEXT("FingerThickness", "Finger thickness"), ECXMRTunableKind::Float);
		T.Min = 0.5f; T.Max = 2.0f; T.Delta = 0.05f; T.Default = 1.0f; T.bPersist = true;
		T.Get = [this] { return RadiusScale; };
		T.Set = [this](float Value) { RadiusScale = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("HandCutOut.PalmThickness", LOCTEXT("PalmThickness", "Palm thickness"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.5f; T.Max = 6.0f; T.Delta = 0.1f; T.Default = 2.2f; T.bPersist = true;
		T.Get = [this] { return PalmThickness; };
		T.Set = [this](float Value) { PalmThickness = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("HandCutOut.PlugTipReach", LOCTEXT("PlugTipReach", "Plug tip ahead of the pinch"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.0f; T.Max = 15.0f; T.Delta = 0.05f; T.Default = 1.75f; T.bPersist = true;
		T.Get = [this] { return PlugTipReach; };
		T.Set = [this](float Value) { PlugTipReach = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("HandCutOut.PlugTipDebug", LOCTEXT("PlugTipDebug", "Show the plug tip line"), ECXMRTunableKind::Bool);
		T.Get = [] { return CVarPlugTipDebug.GetValueOnGameThread() != 0 ? 1.0f : 0.0f; };
		T.Set = [](float Value) { CVarPlugTipDebug->Set(Value > 0.5f ? 1 : 0, ECVF_SetByConsole); };
		Tuning->Register(MoveTemp(T));
	}
}

FText UCXMRVirtualHandComponent::DescribeState() const
{
	if (bCutOutActive)
	{
		const bool bTracked = GetParts().Num() > 0 && (LeftJoints->IsVisible() || RightJoints->IsVisible());
		return bTracked ? LOCTEXT("CutOutShowing", "showing through the cut-out")
		                : LOCTEXT("CutOutNoHands", "cut-out on, no hand tracked right now");
	}
	if (CVarHandCutOut.GetValueOnGameThread() == 0)
	{
		return LOCTEXT("CutOutOff", "cut-out off — hands show only with depth test (T/U)");
	}
	const UCXMRSubsystem* CXMR = GetCXMR();
	if (!CXMR || !CXMR->IsMixedRealityOn())
	{
		return LOCTEXT("CutOutWaitingMR", "waiting for mixed reality (M)");
	}
	return LOCTEXT("CutOutIdle", "off");
}

void UCXMRVirtualHandComponent::UpdateCutOutMasking(bool bActive, bool bPreview)
{
	if (bActive == bCutOutActive)
	{
		return;
	}
	bCutOutActive = bActive;

	UCXMRSubsystem* CXMR = GetCXMR();
	if (bActive)
	{
		// PP_MR cuts nothing while masking is off, and a cut-out that silently shows no hand reads as a broken tracker.
		// A masking state the operator already chose is left alone.
		if (CXMR && !CXMR->IsMaskingOn())
		{
			CXMR->SetMasking(true);
			bTurnedMaskingOn = true;
		}
		UE_LOG(LogCXMRVirtualHands, Log, TEXT("Hand cut-out ON%s: real hands show through (masking %s, tracker %s). The forearm is not cut out."),
			bPreview ? TEXT(" (canned preview pose)") : TEXT(""),
			!CXMR ? TEXT("unavailable") : (bTurnedMaskingOn ? TEXT("switched on") : TEXT("already on")),
			GetHandTracker() ? TEXT("present") : TEXT("MISSING"));
	}
	else
	{
		const bool bRestore = bTurnedMaskingOn && CXMR && CXMR->IsMaskingOn();
		bTurnedMaskingOn = false;
		if (bRestore)
		{
			CXMR->SetMasking(false);
		}
		UE_LOG(LogCXMRVirtualHands, Log, TEXT("Hand cut-out OFF%s"), bRestore ? TEXT(" (masking switched back off)") : TEXT(""));
	}
}

void UCXMRVirtualHandComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bPreview = CVarHandCutOutPreview.GetValueOnGameThread() != 0;
	const UCXMRSubsystem* CXMR = GetCXMR();
	// Outside mixed reality there is no camera image to show, only a black hole — so the cut-out follows MR.
	const bool bActive = bPreview || (CVarHandCutOut.GetValueOnGameThread() != 0 && CXMR && CXMR->IsMixedRealityOn());
	UpdateCutOutMasking(bActive, bPreview);

	TArray<FVector> Positions;
	TArray<float> Radii;
	for (EControllerHand Hand : { EControllerHand::Left, EControllerHand::Right })
	{
		const bool bDrawn = bActive && GetJoints(Hand, bPreview, Positions, Radii);
		SetHandVisible(Hand, bDrawn);
		if (bDrawn)
		{
			UpdateHand(Hand, Positions, Radii);
		}
	}

	if (CVarPlugTipDebug.GetValueOnGameThread() != 0)
	{
		DrawPlugTipDebug();
	}
}

void UCXMRVirtualHandComponent::DrawPlugTipDebug() const
{
	FVector Tip;
	FVector Direction;
	if (!GetPlugTip(Tip, Direction))
	{
		return;
	}
	// Runs on ahead of the tip, where the hand's own cut-out cannot hide it.
	const FVector End = Tip + Direction * 6.f;
	DrawDebugLine(GetWorld(), Tip, End, FColor::Magenta, false, -1.f, 0, 0.15f);
	DrawDebugSphere(GetWorld(), End, 0.4f, 8, FColor::Magenta, false, -1.f, 0, 0.1f);
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

	// Through CXMRHands like the H skeleton: tracked hands only (a cut-out frozen beside the console would show a patch
	// of real room where the hand used to be), with the same alignment correction, so snapping the skeleton to a marker
	// moves the cut-out too.
	TArray<FQuat> Rotations;   // required by the API, deliberately unused — see the header
	return CXMRHands::GetJoints(GetWorld(), Hand, OutPositions, Rotations, OutRadii);
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

	// Every part grows by the padding, so tracking error does not shave the edge off the real finger.
	auto RadiusOf = [&](int32 Index)
	{
		const float Reported = Radii.IsValidIndex(Index) ? Radii[Index] : 0.f;
		return FMath::Max(Reported, MinJointRadius) * RadiusScale + MaskPadding;
	};

	TArray<FTransform> JointTransforms;
	JointTransforms.Reserve(EHandKeypointCount + 1);
	for (int32 i = 0; i < EHandKeypointCount; ++i)
	{
		JointTransforms.Add(SphereAt(Positions[i], RadiusOf(i)));
	}
	JointTransforms.Add(PalmBetween(Positions, RadiusOf(K(EHandKeypoint::MiddleProximal)), PalmThickness + 2.f * MaskPadding));
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
		// Tips touching, or in line with the finger: any perpendicular keeps the frame usable.
		const FVector Helper = FMath::Abs(Along.Z) < 0.9f ? FVector::UpVector : FVector::ForwardVector;
		Pinch = FVector::CrossProduct(Along, Helper).GetSafeNormal();
	}

	OutGrip = PlugOffset * FTransform(FRotationMatrix::MakeFromXZ(Along, Pinch).ToQuat(), (ThumbTip + IndexTip) * 0.5f);
	return true;
}

bool UCXMRVirtualHandComponent::GetPlugTip(FVector& OutTipLocation, FVector& OutDirection) const
{
	const bool bPreview = CVarHandCutOutPreview.GetValueOnGameThread() != 0;

	TArray<FVector> Positions;
	TArray<float> Radii;
	FTransform Grip;
	if (!GetJoints(PlugHand, bPreview, Positions, Radii) || !ComputeGrip(Positions, Grip))
	{
		return false;
	}

	OutTipLocation = Grip.TransformPosition(FVector(PlugTipReach, 0., 0.));
	OutDirection = Grip.GetUnitAxis(EAxis::X);
	return true;
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

#undef LOCTEXT_NAMESPACE
