// Copyright GMTCK CX.
//
// CXMRHands — the one place CXMR reads tracked hand joints.
//
// The engine's hand tracker (XR_EXT_hand_tracking) reports joints in world space. On the XR-4 the joints were seen
// sitting off the real hand in passthrough (2026-09-14: the H skeleton floated above the hand), although the pawn,
// its camera and the joints were checked to share one tracking origin — so the gap comes from the tracking data or
// the render position, not from CXMR's transforms. Every hand consumer reads through here, so one correction,
// measured against a marker, moves the debug skeleton and any virtual hand together.
//
// The correction is a translation in the viewer's head frame (X forward, Y right, Z up, cm): a sensor that sits off
// the eyes shifts the hands along with the head, not along with the room.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"

class UWorld;

namespace CXMRHands
{
	/** Tracked joints of one hand in world space, correction applied. False while the hand is not tracked — the
	 *  tracker keeps returning the last pose after a loss, and a frozen hand drawn in the air reads as a working one. */
	CXMR_API bool GetJoints(const UWorld* World, EControllerHand Hand, TArray<FVector>& OutPositions, TArray<FQuat>& OutRotations, TArray<float>& OutRadii);

	/** Correction in the head frame (X forward, Y right, Z up), cm. */
	CXMR_API FVector GetOffset();
	CXMR_API void SetOffset(const FVector& HeadFrameOffset);

	/** The player's view in world space. False before a player camera exists. */
	CXMR_API bool GetHeadTransform(const UWorld* World, FTransform& OutHead);

	/** True when a hand tracker plugin is present at all (not whether a hand is in view). */
	CXMR_API bool IsTrackerPresent();
}
