// Copyright GMTCK CX.

#include "CXMRHandDebugComponent.h"
#include "CXMRSubsystem.h"

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
		SetComponentTickEnabled(Subsystem->IsHandVisualizationOn());
	}

	// Say up front whether the feature is even present — a headset session is a bad time to wonder
	// whether "no hands" means "not tracked" or "no tracker plugin at all".
	UE_LOG(LogCXMRHands, Log, TEXT("Hand tracker present: %s"),
		GetHandTracker() ? TEXT("yes") : TEXT("NO (OpenXRHandTracking plugin missing or inactive)"));
}

void UCXMRHandDebugComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnHandVisualizationChanged.RemoveDynamic(this, &UCXMRHandDebugComponent::HandleVisualizationChanged);
	}
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
	IHandTracker* Tracker = GetHandTracker();
	UWorld* World = GetWorld();
	if (!Tracker || !World)
	{
		return false;
	}

	TArray<FVector> Positions;
	TArray<FQuat>   Rotations;
	TArray<float>   Radii;
	bool bIsTracked = false;

	if (!Tracker->GetAllKeypointStates(Hand, Positions, Rotations, Radii, bIsTracked))
	{
		return false;
	}

	// CRITICAL: the tracker keeps returning the LAST poses after tracking drops, deliberately, to
	// avoid a snap back to the origin. Drawing without this check leaves a ghost hand frozen in the
	// air that looks exactly like working tracking.
	if (!bIsTracked || Positions.Num() < EHandKeypointCount)
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
