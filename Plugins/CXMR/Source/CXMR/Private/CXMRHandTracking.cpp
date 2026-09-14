// Copyright GMTCK CX.

#include "CXMRHandTracking.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "HeadMountedDisplayTypes.h"
#include "IHandTracker.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	// Not a console variable: a value typed at the console outranks one set by code, and the tuning window would then
	// silently stop moving the hands.
	FVector GHeadFrameOffset = FVector::ZeroVector;

	IHandTracker* FindTracker()
	{
		IModularFeatures& Features = IModularFeatures::Get();
		const FName Feature = IHandTracker::GetModularFeatureName();
		if (Features.GetModularFeatureImplementationCount(Feature) == 0)
		{
			return nullptr;
		}
		return &Features.GetModularFeature<IHandTracker>(Feature);
	}
}

bool CXMRHands::IsTrackerPresent()
{
	return FindTracker() != nullptr;
}

FVector CXMRHands::GetOffset()
{
	return GHeadFrameOffset;
}

void CXMRHands::SetOffset(const FVector& HeadFrameOffset)
{
	GHeadFrameOffset = HeadFrameOffset;
}

bool CXMRHands::GetHeadTransform(const UWorld* World, FTransform& OutHead)
{
	const APlayerCameraManager* Camera = World ? UGameplayStatics::GetPlayerCameraManager(World, 0) : nullptr;
	if (!Camera)
	{
		return false;
	}
	OutHead = FTransform(Camera->GetCameraRotation(), Camera->GetCameraLocation());
	return true;
}

bool CXMRHands::GetJoints(const UWorld* World, EControllerHand Hand, TArray<FVector>& OutPositions, TArray<FQuat>& OutRotations, TArray<float>& OutRadii)
{
	IHandTracker* Tracker = FindTracker();
	if (!Tracker)
	{
		return false;
	}

	bool bIsTracked = false;
	if (!Tracker->GetAllKeypointStates(Hand, OutPositions, OutRotations, OutRadii, bIsTracked)
		|| !bIsTracked || OutPositions.Num() < EHandKeypointCount)
	{
		return false;
	}

	if (!GHeadFrameOffset.IsNearlyZero())
	{
		FTransform Head;
		if (GetHeadTransform(World, Head))
		{
			const FVector WorldOffset = Head.GetRotation().RotateVector(GHeadFrameOffset);
			for (FVector& Position : OutPositions)
			{
				Position += WorldOffset;
			}
		}
	}
	return true;
}
