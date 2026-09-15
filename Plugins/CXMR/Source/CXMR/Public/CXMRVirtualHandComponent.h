// Copyright GMTCK CX.
//
// UCXMRVirtualHandComponent — cuts the tracked hands out of the virtual scene so the REAL hands show in mixed
// reality; and estimates where the plug they hold has its tip, for USB ports to judge.
//
// In mixed reality anything virtual is composited OVER the camera image, whatever its depth. Put a virtual console in
// front of the wearer and the real hand vanishes the moment it passes over it; depth estimation (T/U) brings it back
// with a torn edge. This component builds the hand's shape from the tracked joints — a sphere per joint, a capsule per
// bone, an ellipsoid for the palm — and draws it into Custom Depth only. PP_MR turns that into a hole wherever the
// hand is nearer than the virtual scene, so the camera image of the hand shows through with a steady edge, and stays
// hidden where the hand goes behind a virtual surface. The edge is the tracked shape plus MaskPadding rather than the
// skin, and it trails a fast hand by the tracker's latency.
//
// Nothing of the plug is drawn or cut out (2026-09-16): a plug-shaped hole that does not sit exactly on the real plug
// reads worse than no hole at all. The plug survives only as PlugTipReach — how far ahead of the pinch its tip is —
// which is what a port needs. So a plug held over a virtual surface is hidden like any other real object; the port
// lighting up is what tells the wearer they are lined up.
//
// Joint ORIENTATIONS are never read: bones, palm and the plug tip are derived from joint positions, so no
// OpenXR-to-UE axis convention can turn anything sideways.
//
// On by default while mixed reality is on; CXMR.HandCutOut 0 turns it off. CXMR.HandCutOut.Preview 1 cuts out a canned
// pair of hands in front of the camera, even outside MR, to check the shape in PIE. CXMR.PlugTipDebug 1 draws the
// estimated plug tip for tuning PlugOffset. The sizes below are also rows in the tuning window, under "Hand cut-out".
//
// Not here on purpose: a forearm (the tracker reports no elbow — EHandKeypoint stops at the wrist, so the cut-out ends
// there), wrist angle readouts, pose smoothing.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "CXMRVirtualHandComponent.generated.h"

class IHandTracker;
class UCXMRSubsystem;
class UCXMRTuningSubsystem;
class UInstancedStaticMeshComponent;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Hand Cut-out")
class CXMR_API UCXMRVirtualHandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRVirtualHandComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Front end of the held plug's metal tip and the direction it points, from the tracker (or the preview pose),
	 * PlugOffset and PlugTipReach. Works whether or not the cut-out is drawn. False = the plug hand is not tracked.
	 */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Hand Cut-out") bool GetPlugTip(FVector& OutTipLocation, FVector& OutDirection) const;

	/** True while the cut-out is drawn: mixed reality on (and CXMR.HandCutOut), or the preview. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Hand Cut-out") bool IsCutOutActive() const { return bCutOutActive; }

	// --- Hand shape. Also tuning-window rows, so these can be dialled in with the headset on. ---

	/** Scales the tracker's reported joint radii. Above 1 thickens fingers that read too thin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hand Cut-out", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float RadiusScale = 1.0f;

	/** Floor for a joint radius in cm — a runtime reporting 0 would otherwise cut out no fingers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hand Cut-out", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MinJointRadius = 0.5f;

	/** Thickness in cm of the ellipsoid that fills the palm between wrist and knuckles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hand Cut-out", meta = (ClampMin = "0.5", ClampMax = "6.0"))
	float PalmThickness = 2.2f;

	/**
	 * cm added around every part, so tracking error does not shave the edge off the real fingers.
	 * More = a wider rim of real background around the hand.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hand Cut-out", meta = (ClampMin = "0.0", ClampMax = "3.0"))
	float MaskPadding = 0.25f;

	// --- Plug: a point, not a model ---

	/** Left or Right. The console sits to the driver's right in a left-hand-drive car. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hand Cut-out|Plug") EControllerHand PlugHand = EControllerHand::Right;

	/** cm from the pinch point to the plug's metal tip, along the index finger. A USB-C cable end is about 1.75. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hand Cut-out|Plug", meta = (ClampMin = "0.0", ClampMax = "15.0"))
	float PlugTipReach = 1.75f;

	/**
	 * Applied in the grip frame: origin between the thumb tip and index tip, X along the index finger, Z from the index
	 * tip toward the thumb tip. Tune with the headset on and CXMR.PlugTipDebug 1 — people pinch a plug differently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hand Cut-out|Plug") FTransform PlugOffset = FTransform::Identity;

	// --- Assets: engine basic shapes; only their shape matters, nothing is ever seen ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hand Cut-out|Assets") TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hand Cut-out|Assets") TObjectPtr<UStaticMesh> CylinderMesh;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	IHandTracker* GetHandTracker() const;
	UCXMRSubsystem* GetCXMR() const;
	UCXMRTuningSubsystem* GetTuning() const;

	/** Rows in the tuning window: the sizes above, so they can be dialled in during a session. */
	void RegisterTunables();

	/** What the tuning window shows as the cut-out's state, and why it is off when it is. */
	FText DescribeState() const;

	/** Tracker joints for one hand, or the canned preview pose. False = nothing for this hand. */
	bool GetJoints(EControllerHand Hand, bool bPreview, TArray<FVector>& OutPositions, TArray<float>& OutRadii) const;

	/** Grip frame between thumb tip and index tip, PlugOffset applied. Shared by the plug tip and its debug draw. */
	bool ComputeGrip(const TArray<FVector>& Positions, FTransform& OutGrip) const;

	void UpdateHand(EControllerHand Hand, const TArray<FVector>& Positions, const TArray<float>& Radii);
	void SetHandVisible(EControllerHand Hand, bool bVisible);

	/** Shared setup for every part: a mask in Custom Depth only, no collision, world-space transform. */
	void ConfigureShape(UInstancedStaticMeshComponent* Component, UStaticMesh* Mesh);

	/** Every part that exists. */
	TArray<UInstancedStaticMeshComponent*, TInlineAllocator<4>> GetParts() const;

	/** Switches masking on when the cut-out starts, if it was off, and back off when the cut-out ends. */
	void UpdateCutOutMasking(bool bActive, bool bPreview);

	void DrawPlugTipDebug() const;

	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> LeftJoints;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> LeftBones;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> RightJoints;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> RightBones;

	/** The cut-out is being drawn right now. */
	bool bCutOutActive = false;

	/** Masking was off when the cut-out started and this component switched it on. */
	bool bTurnedMaskingOn = false;
};
