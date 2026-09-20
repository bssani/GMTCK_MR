// Copyright GMTCK CX.
//
// UCXMRPlugTipComponent — where the plug held in the tracked hand has its tip, for USB ports to judge.
//
// Nothing is drawn. The hand cut-out this component used to carry was removed on 2026-09-21: the mask was built from
// the tracked joints, so it trailed a moving hand by the tracker's latency, and a hole that lags the hand reads worse
// than no hole at all. So a real hand passing over a virtual console is hidden in mixed reality, like any other real
// object behind virtual content, and what tells the wearer they are lined up is the port marker lighting up
// (ACXMRUsbPortTarget). If the hands must be visible again, the path to try is Varjo Base's own Occlusion > Hands
// (Pro licence, Base 4.16+), not rebuilding the mask here; the removed cut-out is in git history.
//
// Joint ORIENTATIONS are never read: the grip frame comes from joint POSITIONS alone, so no OpenXR-to-UE axis
// convention can turn the plug sideways.
//
// CXMR.PlugTip.Preview 1 feeds a canned hand pose in front of the camera, so PlugOffset can be dialled in during PIE
// without a headset. CXMR.PlugTipDebug 1 draws the estimated tip. Both are rows in the tuning window under "Plug tip".
//
// Not here on purpose: any drawn hand, a forearm (the tracker reports no elbow — EHandKeypoint stops at the wrist),
// wrist angle readouts, pose smoothing.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "CXMRPlugTipComponent.generated.h"

class UCXMRTuningSubsystem;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Plug Tip")
class CXMR_API UCXMRPlugTipComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRPlugTipComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Front end of the held plug's metal tip and the direction it points, from the tracker (or the preview pose),
	 * PlugOffset and PlugTipReach. False = the plug hand is not tracked.
	 */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Plug Tip") bool GetPlugTip(FVector& OutTipLocation, FVector& OutDirection) const;

	/** True while the plug hand is tracked, so GetPlugTip has something to answer with. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Plug Tip") bool IsPlugHandTracked() const;

	/** Left or Right. The console sits to the driver's right in a left-hand-drive car. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Plug Tip") EControllerHand PlugHand = EControllerHand::Right;

	/** cm from the pinch point to the plug's metal tip, along the index finger. A USB-C cable end is about 1.75. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Plug Tip", meta = (ClampMin = "0.0", ClampMax = "15.0"))
	float PlugTipReach = 1.75f;

	/**
	 * Applied in the grip frame: origin between the thumb tip and index tip, X along the index finger, Z from the index
	 * tip toward the thumb tip. Tune with the headset on and CXMR.PlugTipDebug 1 — people pinch a plug differently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Plug Tip") FTransform PlugOffset = FTransform::Identity;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRTuningSubsystem* GetTuning() const;

	/** Rows in the tuning window: the plug tip reach and the two switches, so they can be dialled in during a session. */
	void RegisterTunables();

	/** What the tuning window shows as the state, and why there is no plug tip when there is none. */
	FText DescribeState() const;

	/** Tracker joint positions for the plug hand, or the canned preview pose. False = nothing for this hand. */
	bool GetJoints(bool bPreview, TArray<FVector>& OutPositions) const;

	/** Grip frame between thumb tip and index tip, PlugOffset applied. Shared by the plug tip and its debug draw. */
	bool ComputeGrip(const TArray<FVector>& Positions, FTransform& OutGrip) const;

	void DrawPlugTipDebug() const;
};
