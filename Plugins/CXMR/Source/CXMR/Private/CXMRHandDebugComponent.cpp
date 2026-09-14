// Copyright GMTCK CX.

#include "CXMRHandDebugComponent.h"
#include "CXMRHandTracking.h"
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"

#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "HeadMountedDisplayTypes.h"
#include "IHandTracker.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRHands, Log, All);

namespace
{
	/** Contiguous keypoint runs, one per finger — EHandKeypoint orders each finger metacarpal->tip. */
	struct FFingerRun { int32 First; int32 Last; };

	static const FFingerRun FingerRuns[] = {
		{ static_cast<int32>(EHandKeypoint::ThumbMetacarpal),  static_cast<int32>(EHandKeypoint::ThumbTip)  },
		{ static_cast<int32>(EHandKeypoint::IndexMetacarpal),  static_cast<int32>(EHandKeypoint::IndexTip)  },
		{ static_cast<int32>(EHandKeypoint::MiddleMetacarpal), static_cast<int32>(EHandKeypoint::MiddleTip) },
		{ static_cast<int32>(EHandKeypoint::RingMetacarpal),   static_cast<int32>(EHandKeypoint::RingTip)   },
		{ static_cast<int32>(EHandKeypoint::LittleMetacarpal), static_cast<int32>(EHandKeypoint::LittleTip) },
	};

	// Named for what it indexes: a bare "IndexTip" collides with locals in other files of the same unity blob.
	const int32 IndexTipKeypoint = static_cast<int32>(EHandKeypoint::IndexTip);

	const FName HandOffsetIds[3] = { "Hands.OffsetForward", "Hands.OffsetRight", "Hands.OffsetUp" };

	/** A vector already expressed in the head frame. */
	FString HeadFrameText(const FVector& V)
	{
		return FString::Printf(TEXT("fwd %+.1f  right %+.1f  up %+.1f"), V.X, V.Y, V.Z);
	}
}

UCXMRHandDebugComponent::UCXMRHandDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;   // switched on with the visualization
}

UCXMRSubsystem* UCXMRHandDebugComponent::GetCXMR() const
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

UCXMRTuningSubsystem* UCXMRHandDebugComponent::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

IHandTracker* UCXMRHandDebugComponent::GetHandTracker() const
{
	IModularFeatures& Features = IModularFeatures::Get();
	const FName Feature = IHandTracker::GetModularFeatureName();
	if (Features.GetModularFeatureImplementationCount(Feature) == 0)
	{
		return nullptr;
	}
	return &Features.GetModularFeature<IHandTracker>(Feature);
}

bool UCXMRHandDebugComponent::IsHandTrackingAvailable() const
{
	const IHandTracker* Tracker = GetHandTracker();
	return Tracker && Tracker->IsHandTrackingStateValid();
}

void UCXMRHandDebugComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = GetCXMR();
	if (Subsystem)
	{
		Subsystem->OnHandVisualizationChanged.AddDynamic(this, &UCXMRHandDebugComponent::HandleVisualizationChanged);
		Subsystem->OnMarkerDetected.AddDynamic(this, &UCXMRHandDebugComponent::HandleMarkerPose);
		Subsystem->OnMarkerMoved.AddDynamic(this, &UCXMRHandDebugComponent::HandleMarkerPose);
		Subsystem->OnMarkerLost.AddDynamic(this, &UCXMRHandDebugComponent::HandleMarkerLost);
		SetComponentTickEnabled(Subsystem->IsHandVisualizationOn());
	}

	// Say up front whether the feature is even present — a headset session is a bad time to wonder
	// whether "no hands" means "not tracked" or "no tracker plugin at all".
	UE_LOG(LogCXMRHands, Log, TEXT("Hand tracker present: %s"),
		GetHandTracker() ? TEXT("yes") : TEXT("NO (OpenXRHandTracking plugin missing or inactive)"));

	RegisterTunables();
}

void UCXMRHandDebugComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnHandVisualizationChanged.RemoveDynamic(this, &UCXMRHandDebugComponent::HandleVisualizationChanged);
		Subsystem->OnMarkerDetected.RemoveDynamic(this, &UCXMRHandDebugComponent::HandleMarkerPose);
		Subsystem->OnMarkerMoved.RemoveDynamic(this, &UCXMRHandDebugComponent::HandleMarkerPose);
		Subsystem->OnMarkerLost.RemoveDynamic(this, &UCXMRHandDebugComponent::HandleMarkerLost);
	}
	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
	}

	// The correction lives for the process, but a session's value belongs to that session: the next one gets its
	// own back from Tuning.json when the rows register again.
	CXMRHands::SetOffset(FVector::ZeroVector);

	Super::EndPlay(Reason);
}

void UCXMRHandDebugComponent::HandleVisualizationChanged(bool bOn)
{
	SetComponentTickEnabled(bOn);

	if (bOn)
	{
		UE_LOG(LogCXMRHands, Log, TEXT("Hand visualization ON — tracker %s, state %s"),
			GetHandTracker() ? TEXT("present") : TEXT("MISSING"),
			IsHandTrackingAvailable() ? TEXT("valid") : TEXT("not valid yet"));
	}
}

void UCXMRHandDebugComponent::HandleMarkerPose(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size)
{
	MarkerPositions.Add(MarkerId, Position);
}

void UCXMRHandDebugComponent::HandleMarkerLost(int32 MarkerId)
{
	MarkerPositions.Remove(MarkerId);
}

void UCXMRHandDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	const bool bLeft  = DrawHand(EControllerHand::Left,  LeftColor);
	const bool bRight = DrawHand(EControllerHand::Right, RightColor);

	if (bLogTrackingChanges)
	{
		if (bLeft != bLeftWasTracked)
		{
			UE_LOG(LogCXMRHands, Warning, TEXT("LEFT hand %s"), bLeft ? TEXT("acquired") : TEXT("lost"));
		}
		if (bRight != bRightWasTracked)
		{
			UE_LOG(LogCXMRHands, Warning, TEXT("RIGHT hand %s"), bRight ? TEXT("acquired") : TEXT("lost"));
		}
	}
	bLeftWasTracked  = bLeft;
	bRightWasTracked = bRight;
}

bool UCXMRHandDebugComponent::DrawHand(EControllerHand Hand, const FLinearColor& Color)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	TArray<FVector> Positions;
	TArray<FQuat>   Rotations;
	TArray<float>   Radii;

	// Through CXMRHands, so the skeleton shows exactly what every other hand consumer gets: tracked hands only,
	// alignment correction applied.
	if (!CXMRHands::GetJoints(World, Hand, Positions, Rotations, Radii))
	{
		return false;
	}

	const FColor DrawColor = Color.ToFColor(true);

	// Positions are already world-space (the tracker applies TrackingToWorld) and radii are already
	// in cm (scaled by WorldToMeters), so nothing here converts units.
	for (int32 i = 0; i < EHandKeypointCount; ++i)
	{
		const float Radius = FMath::Max(Radii.IsValidIndex(i) ? Radii[i] : 0.5f, 0.15f) * RadiusScale;
		DrawDebugSphere(World, Positions[i], Radius, 8, DrawColor, false, -1.0f, 0, 0.0f);
	}

	if (bDrawBones)
	{
		const FVector& Wrist = Positions[static_cast<int32>(EHandKeypoint::Wrist)];
		for (const FFingerRun& Run : FingerRuns)
		{
			DrawDebugLine(World, Wrist, Positions[Run.First], DrawColor, false, -1.0f, 0, 0.2f);
			for (int32 i = Run.First; i < Run.Last; ++i)
			{
				DrawDebugLine(World, Positions[i], Positions[i + 1], DrawColor, false, -1.0f, 0, 0.2f);
			}
		}
	}

	return true;
}

// ---- Alignment: readouts, correction, snap to marker ----

void UCXMRHandDebugComponent::RegisterTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return;
	}

	const FText Category = NSLOCTEXT("CXMRHands", "CatHands", "Hands");
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
		FCXMRTunable T = Make("Hands.RightTip", NSLOCTEXT("CXMRHands", "RightTip", "Right index tip (cm)"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeTip(EControllerHand::Right); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Hands.LeftTip", NSLOCTEXT("CXMRHands", "LeftTip", "Left index tip (cm)"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeTip(EControllerHand::Left); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Hands.TipToMarker", NSLOCTEXT("CXMRHands", "TipToMarker", "Tip minus marker"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeTipToMarker(); };
		Tuning->Register(MoveTemp(T));
	}

	const FText Labels[3] = {
		NSLOCTEXT("CXMRHands", "OffsetForward", "Hand offset forward"),
		NSLOCTEXT("CXMRHands", "OffsetRight", "Hand offset right"),
		NSLOCTEXT("CXMRHands", "OffsetUp", "Hand offset up") };
	for (int32 Axis = 0; Axis < 3; ++Axis)
	{
		FCXMRTunable T = Make(HandOffsetIds[Axis], Labels[Axis], ECXMRTunableKind::Float);
		T.Unit = NSLOCTEXT("CXMRHands", "cm", "cm");
		T.Min = -30.0f; T.Max = 30.0f; T.Delta = 0.5f; T.Default = 0.0f; T.bPersist = true;
		T.Get = [Axis] { return static_cast<float>(CXMRHands::GetOffset()[Axis]); };
		T.Set = [Axis](float Value)
		{
			FVector Offset = CXMRHands::GetOffset();
			Offset[Axis] = Value;
			CXMRHands::SetOffset(Offset);
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Hands.SnapToMarker", NSLOCTEXT("CXMRHands", "SnapToMarker", "Snap hand to marker (index tip on its centre)"), ECXMRTunableKind::Action);
		T.Invoke = [this] { CalibrateToNearestMarker(); };
		Tuning->Register(MoveTemp(T));
	}
}

FText UCXMRHandDebugComponent::DescribeTip(EControllerHand Hand) const
{
	TArray<FVector> Positions;
	TArray<FQuat>   Rotations;
	TArray<float>   Radii;
	if (!CXMRHands::GetJoints(GetWorld(), Hand, Positions, Rotations, Radii))
	{
		return CXMRHands::IsTrackerPresent()
			? NSLOCTEXT("CXMRHands", "NotTracked", "not tracked")
			: NSLOCTEXT("CXMRHands", "NoTracker", "no hand tracker");
	}

	FTransform Head;
	if (!CXMRHands::GetHeadTransform(GetWorld(), Head))
	{
		return NSLOCTEXT("CXMRHands", "NoView", "no view");
	}
	return FText::FromString(HeadFrameText(Head.InverseTransformPositionNoScale(Positions[IndexTipKeypoint])));
}

FText UCXMRHandDebugComponent::DescribeTipToMarker() const
{
	FVector Tip;
	FVector Marker;
	int32 MarkerId = INDEX_NONE;
	if (!FindTipAndMarker(Tip, MarkerId, Marker))
	{
		return MarkerPositions.Num() == 0
			? NSLOCTEXT("CXMRHands", "NoMarker", "no marker in view")
			: NSLOCTEXT("CXMRHands", "NoHand", "no hand tracked");
	}

	FTransform Head;
	if (!CXMRHands::GetHeadTransform(GetWorld(), Head))
	{
		return NSLOCTEXT("CXMRHands", "NoView", "no view");
	}
	const FVector Error = Head.GetRotation().UnrotateVector(Tip - Marker);
	return FText::FromString(FString::Printf(TEXT("#%d  %s  = %.1f"), MarkerId, *HeadFrameText(Error), Error.Size()));
}

bool UCXMRHandDebugComponent::FindTipAndMarker(FVector& OutTip, int32& OutMarkerId, FVector& OutMarker) const
{
	if (MarkerPositions.Num() == 0)
	{
		return false;
	}

	TArray<FVector> Positions;
	TArray<FQuat>   Rotations;
	TArray<float>   Radii;
	const bool bHand = CXMRHands::GetJoints(GetWorld(), EControllerHand::Right, Positions, Rotations, Radii)
		|| CXMRHands::GetJoints(GetWorld(), EControllerHand::Left, Positions, Rotations, Radii);
	if (!bHand)
	{
		return false;
	}

	OutTip = Positions[IndexTipKeypoint];
	double Best = TNumericLimits<double>::Max();
	for (const TPair<int32, FVector>& Pair : MarkerPositions)
	{
		const double Distance = FVector::DistSquared(OutTip, Pair.Value);
		if (Distance < Best)
		{
			Best = Distance;
			OutMarkerId = Pair.Key;
			OutMarker = Pair.Value;
		}
	}
	return true;
}

bool UCXMRHandDebugComponent::CalibrateToNearestMarker()
{
	FVector Tip;
	FVector Marker;
	int32 MarkerId = INDEX_NONE;
	if (!FindTipAndMarker(Tip, MarkerId, Marker))
	{
		UE_LOG(LogCXMRHands, Warning, TEXT("Hand not snapped: needs a tracked hand and a marker in view (%d marker(s) known)."),
			MarkerPositions.Num());
		return false;
	}
	UE_LOG(LogCXMRHands, Log, TEXT("Snapping hand to marker %d"), MarkerId);
	return CalibrateHandOffset(Tip, Marker);
}

bool UCXMRHandDebugComponent::CalibrateHandOffset(FVector TipWorld, FVector TargetWorld)
{
	FTransform Head;
	if (!CXMRHands::GetHeadTransform(GetWorld(), Head))
	{
		return false;
	}

	// The tip already carries the current correction, so the remaining error comes off on top of it.
	const FVector ErrorInHead = Head.GetRotation().UnrotateVector(TipWorld - TargetWorld);
	const FVector Wanted = CXMRHands::GetOffset() - ErrorInHead;

	// Through the registry so the result is clamped and saved exactly like an edit in the window.
	UCXMRTuningSubsystem* Tuning = GetTuning();
	const bool bThroughTuning = Tuning
		&& Tuning->SetTunableValue(HandOffsetIds[0], static_cast<float>(Wanted.X))
		&& Tuning->SetTunableValue(HandOffsetIds[1], static_cast<float>(Wanted.Y))
		&& Tuning->SetTunableValue(HandOffsetIds[2], static_cast<float>(Wanted.Z));
	if (!bThroughTuning)
	{
		CXMRHands::SetOffset(Wanted);
	}

	const FVector Applied = CXMRHands::GetOffset();
	UE_LOG(LogCXMRHands, Log, TEXT("Hand offset: error %s cm -> correction %s cm%s"),
		*HeadFrameText(ErrorInHead), *HeadFrameText(Applied),
		Applied.Equals(Wanted, 0.01) ? TEXT("") : TEXT(" (CLAMPED to 30 cm - was it the right marker and hand?)"));
	return true;
}
