// Copyright GMTCK CX.

#include "CXMRPlugTipComponent.h"
#include "CXMRHandTracking.h"
#include "CXMRTuningSubsystem.h"

#include "Camera/CameraComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "HeadMountedDisplayTypes.h"

#define LOCTEXT_NAMESPACE "CXMRPlugTip"

static TAutoConsoleVariable<int32> CVarPlugTipPreview(
	TEXT("CXMR.PlugTip.Preview"),
	0,
	TEXT("Use a canned hand pose in front of the camera instead of the tracker, so the plug tip can be dialled in ")
	TEXT("during PIE without a headset. Pair it with CXMR.PlugTipDebug 1."),
	ECVF_Default);

static TAutoConsoleVariable<int32> CVarPlugTipDebug(
	TEXT("CXMR.PlugTipDebug"),
	0,
	TEXT("Draw the plug tip USB ports judge: a line that starts at the estimated tip and runs on in the plug's ")
	TEXT("direction. Tune the pawn's PlugTip > Plug Offset and Plug Tip Reach until the line starts at the real ")
	TEXT("plug's tip."),
	ECVF_Default);

namespace
{
	int32 K(EHandKeypoint Keypoint) { return static_cast<int32>(Keypoint); }

	// A relaxed right hand, loosely pinching. X toward the fingertips, Y toward the thumb, Z out of the back of the
	// hand; cm, wrist at the origin. Order matches EHandKeypoint. Only the thumb tip, index tip and index proximal are
	// read — the rest keeps the array the shape every EHandKeypoint index expects.
	const FVector CannedHand[] = {
		FVector( 4.5f,  0.2f,  0.0f),   // Palm
		FVector( 0.0f,  0.0f,  0.0f),   // Wrist
		FVector( 1.2f,  1.8f, -0.6f),   // Thumb
		FVector( 3.8f,  4.0f, -1.2f),
		FVector( 6.4f,  5.2f, -1.8f),
		FVector( 8.6f,  5.6f, -2.4f),
		FVector( 1.5f,  1.2f,  0.0f),   // Index
		FVector( 8.0f,  3.0f,  0.0f),
		FVector(12.0f,  3.3f, -0.8f),
		FVector(14.2f,  3.4f, -2.0f),
		FVector(15.6f,  3.4f, -3.2f),
		FVector( 1.5f,  0.4f,  0.0f),   // Middle
		FVector( 8.3f,  1.0f,  0.0f),
		FVector(12.8f,  1.1f,  0.0f),
		FVector(15.6f,  1.2f,  0.0f),
		FVector(17.7f,  1.2f,  0.0f),
		FVector( 1.5f, -0.4f,  0.0f),   // Ring
		FVector( 7.8f, -1.0f,  0.0f),
		FVector(12.0f, -1.2f,  0.0f),
		FVector(14.6f, -1.3f,  0.0f),
		FVector(16.6f, -1.4f,  0.0f),
		FVector( 1.5f, -1.2f,  0.0f),   // Little
		FVector( 7.0f, -2.8f,  0.0f),
		FVector(10.3f, -3.3f,  0.0f),
		FVector(12.2f, -3.6f,  0.0f),
		FVector(14.0f, -3.9f,  0.0f),
	};
	static_assert(UE_ARRAY_COUNT(CannedHand) == EHandKeypointCount, "Canned pose must list every EHandKeypoint");
}

UCXMRPlugTipComponent::UCXMRPlugTipComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	// Late in the frame, so the pose read is the newest the tracker has for this frame.
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

UCXMRTuningSubsystem* UCXMRPlugTipComponent::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

void UCXMRPlugTipComponent::BeginPlay()
{
	Super::BeginPlay();
	RegisterTunables();
}

void UCXMRPlugTipComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
	}
	Super::EndPlay(Reason);
}

void UCXMRPlugTipComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Nothing else to do every frame: the tip is worked out on demand, by whoever asks.
	if (CVarPlugTipDebug.GetValueOnGameThread() != 0)
	{
		DrawPlugTipDebug();
	}
}

bool UCXMRPlugTipComponent::GetJoints(bool bPreview, TArray<FVector>& OutPositions) const
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
		const bool bRightHand = PlugHand == EControllerHand::Right;

		// Held out low and to the side, palm down, fingers pointing away from the viewer.
		const FVector Wrist = Camera->GetComponentLocation() + Forward * 30.f + Right * (bRightHand ? 10.f : -10.f) - Up * 18.f;
		// The canned pose is a right hand; a left hand is the same pose with the thumb side mirrored.
		const FVector ThumbSide = bRightHand ? -Right : Right;

		OutPositions.SetNum(EHandKeypointCount);
		for (int32 i = 0; i < EHandKeypointCount; ++i)
		{
			const FVector& Local = CannedHand[i];
			OutPositions[i] = Wrist + Forward * Local.X + ThumbSide * Local.Y + Up * Local.Z;
		}
		return true;
	}

	// Through CXMRHands like the H skeleton, so the plug tip gets the same alignment correction: snapping the skeleton
	// to a marker moves the tip with it.
	TArray<FQuat> Rotations;   // required by the API, deliberately unused — see the header
	TArray<float> Radii;       // the cut-out is gone; nothing here has a thickness any more
	return CXMRHands::GetJoints(GetWorld(), PlugHand, OutPositions, Rotations, Radii);
}

bool UCXMRPlugTipComponent::ComputeGrip(const TArray<FVector>& Positions, FTransform& OutGrip) const
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

bool UCXMRPlugTipComponent::GetPlugTip(FVector& OutTipLocation, FVector& OutDirection) const
{
	const bool bPreview = CVarPlugTipPreview.GetValueOnGameThread() != 0;

	TArray<FVector> Positions;
	FTransform Grip;
	if (!GetJoints(bPreview, Positions) || !ComputeGrip(Positions, Grip))
	{
		return false;
	}

	OutTipLocation = Grip.TransformPosition(FVector(PlugTipReach, 0., 0.));
	OutDirection = Grip.GetUnitAxis(EAxis::X);
	return true;
}

bool UCXMRPlugTipComponent::IsPlugHandTracked() const
{
	FVector Tip;
	FVector Direction;
	return GetPlugTip(Tip, Direction);
}

void UCXMRPlugTipComponent::DrawPlugTipDebug() const
{
	FVector Tip;
	FVector Direction;
	if (!GetPlugTip(Tip, Direction))
	{
		return;
	}
	const FVector End = Tip + Direction * 6.f;
	DrawDebugLine(GetWorld(), Tip, End, FColor::Magenta, false, -1.f, 0, 0.15f);
	DrawDebugSphere(GetWorld(), End, 0.4f, 8, FColor::Magenta, false, -1.f, 0, 0.1f);
}

// ---- Tuning window rows ----

FText UCXMRPlugTipComponent::DescribeState() const
{
	if (CVarPlugTipPreview.GetValueOnGameThread() != 0)
	{
		return IsPlugHandTracked()
			? LOCTEXT("PreviewOn", "canned preview pose (no tracker)")
			: LOCTEXT("PreviewNoCamera", "preview on, but the pawn has no camera to place it in front of");
	}
	return IsPlugHandTracked()
		? LOCTEXT("Tracked", "plug hand tracked")
		: LOCTEXT("Untracked", "plug hand not tracked — no tip for the ports to judge");
}

void UCXMRPlugTipComponent::RegisterTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return;
	}

	const FText Category = LOCTEXT("CatPlugTip", "Plug tip");
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
		FCXMRTunable T = Make("PlugTip.State", LOCTEXT("State", "Plug hand"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeState(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("PlugTip.Reach", LOCTEXT("Reach", "Plug tip ahead of the pinch"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.0f; T.Max = 15.0f; T.Delta = 0.05f; T.Default = 1.75f; T.bPersist = true;
		T.Get = [this] { return PlugTipReach; };
		T.Set = [this](float Value) { PlugTipReach = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("PlugTip.Debug", LOCTEXT("Debug", "Show the plug tip line"), ECXMRTunableKind::Bool);
		T.Get = [] { return CVarPlugTipDebug.GetValueOnGameThread() != 0 ? 1.0f : 0.0f; };
		T.Set = [](float Value) { CVarPlugTipDebug->Set(Value > 0.5f ? 1 : 0, ECVF_SetByConsole); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("PlugTip.Preview", LOCTEXT("Preview", "Use a canned hand (no headset)"), ECXMRTunableKind::Bool);
		T.Get = [] { return CVarPlugTipPreview.GetValueOnGameThread() != 0 ? 1.0f : 0.0f; };
		T.Set = [](float Value) { CVarPlugTipPreview->Set(Value > 0.5f ? 1 : 0, ECVF_SetByConsole); };
		Tuning->Register(MoveTemp(T));
	}
}

#undef LOCTEXT_NAMESPACE
