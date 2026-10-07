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

bool CXMRHands::HasReceivedHandData(EControllerHand Hand)
{
	IHandTracker* Tracker = FindTracker();
	if (!Tracker)
	{
		return false;
	}

	// OpenXRHandTracking answers false here until the runtime has marked the hand active once (XrHandJointLocationsEXT
	// isActive), and true from then on even while the hand is lost — exactly the difference this reports.
	TArray<FVector> Positions;
	TArray<FQuat> Rotations;
	TArray<float> Radii;
	bool bIsTracked = false;
	return Tracker->GetAllKeypointStates(Hand, Positions, Rotations, Radii, bIsTracked);
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

const TArray<CXMRHands::FBone>& CXMRHands::GetBones()
{
	static const TArray<FBone> Bones = []
	{
		TArray<FBone> Result;
		const EHandKeypoint Runs[][2] = {
			{EHandKeypoint::ThumbMetacarpal, EHandKeypoint::ThumbTip},
			{EHandKeypoint::IndexMetacarpal, EHandKeypoint::IndexTip},
			{EHandKeypoint::MiddleMetacarpal, EHandKeypoint::MiddleTip},
			{EHandKeypoint::RingMetacarpal, EHandKeypoint::RingTip},
			{EHandKeypoint::LittleMetacarpal, EHandKeypoint::LittleTip}
		};
		for (const auto& Run : Runs)
		{
			const int32 First = static_cast<int32>(Run[0]);
			const int32 Last = static_cast<int32>(Run[1]);
			Result.Add({static_cast<int32>(EHandKeypoint::Wrist), First});
			for (int32 Index = First; Index < Last; ++Index) { Result.Add({Index, Index + 1}); }
		}
		return Result;
	}();
	return Bones;
}

float CXMRHands::GetJointRadius(const TArray<float>& Radii, int32 Index, float Scale)
{
	const float Radius = Radii.IsValidIndex(Index) && FMath::IsFinite(Radii[Index]) && Radii[Index] >= 0.f ? Radii[Index] : 0.5f;
	return FMath::Max(Radius, 0.15f) * (FMath::IsFinite(Scale) ? FMath::Clamp(Scale, 0.1f, 5.f) : 1.f);
}

bool CXMRHands::GetContactShapes(const UWorld* World, float RadiusScale, bool bIncludeBones, TArray<FContactShape>& OutShapes)
{
	OutShapes.Reset();
	for (EControllerHand Hand : {EControllerHand::Left, EControllerHand::Right})
	{
		TArray<FVector> Positions;
		TArray<FQuat> Rotations;
		TArray<float> Radii;
		if (!GetJoints(World, Hand, Positions, Rotations, Radii)
			|| Positions.ContainsByPredicate([](const FVector& Position) { return Position.ContainsNaN(); })) { continue; }
		for (int32 Index = 0; Index < EHandKeypointCount; ++Index)
		{
			OutShapes.Add({Positions[Index], Positions[Index], GetJointRadius(Radii, Index, RadiusScale), Hand});
		}
		if (bIncludeBones)
		{
			for (const FBone& Bone : GetBones())
			{
				const float Radius = FMath::Max(GetJointRadius(Radii, Bone.Start, RadiusScale), GetJointRadius(Radii, Bone.End, RadiusScale));
				OutShapes.Add({Positions[Bone.Start], Positions[Bone.End], Radius, Hand});
			}
		}
	}
	return !OutShapes.IsEmpty();
}

float CXMRHands::GetSurfaceDistance(const TArray<FContactShape>& Shapes, FVector Point, FVector& ClosestPoint, EControllerHand* ClosestHand)
{
	float Distance = MAX_flt;
	for (const FContactShape& Shape : Shapes)
	{
		const FVector Centre = FMath::ClosestPointOnSegment(Point, Shape.Start, Shape.End);
		const FVector Delta = Point - Centre;
		const float Candidate = FMath::Max(0.f, static_cast<float>(Delta.Size()) - Shape.Radius);
		if (Candidate < Distance)
		{
			Distance = Candidate;
			ClosestPoint = Candidate > 0.f ? Centre + Delta.GetSafeNormal() * Shape.Radius : Point;
			if (ClosestHand) { *ClosestHand = Shape.Hand; }
		}
	}
	return Distance;
}
