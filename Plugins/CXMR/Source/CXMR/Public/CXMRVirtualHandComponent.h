// Copyright GMTCK CX.
//
// UCXMRVirtualHandComponent — the tracked hands drawn as solid virtual geometry, one holding a virtual plug.
//
// In mixed reality anything virtual is composited OVER the camera image, whatever its depth. Put a virtual
// console in front of the wearer and the real hand vanishes the moment it passes over it. When the physical
// stand-in is a plain box and the console being judged exists only in CAD, the hand has to live in the same
// virtual depth space as that console to show in front of its surfaces and behind its rim.
//
// The shape is built from the tracked joints themselves — a sphere per joint at the tracker's own radius, a
// capsule per bone, an ellipsoid for the palm — rather than a skinned mesh. It follows exactly what the
// tracker reports and needs no bone retargeting. Joint ORIENTATIONS are never read: every frame (bones,
// palm, plug) is derived from joint positions, so no OpenXR-to-UE axis convention can turn the plug sideways.
//
// Toggle: CXMR.VirtualHands 1. With no headset, CXMR.VirtualHands.Preview 1 poses a canned pair of hands in
// front of the camera so the geometry can be checked in PIE.
//
// Not here on purpose: a forearm (the tracker reports no elbow — EHandKeypoint stops at the wrist), wrist
// angle readouts, pose smoothing.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "CXMRVirtualHandComponent.generated.h"

class IHandTracker;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Virtual Hands")
class CXMR_API UCXMRVirtualHandComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRVirtualHandComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/**
	 * Front end of the plug's metal tip and the direction it points, from the tracker (or the preview pose) with
	 * the same grip math that draws the plug. Works while the hands are hidden, so a port can still judge
	 * alignment when the real hand is shown through depth test instead. False = the plug hand is not tracked.
	 */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Virtual Hands") bool GetPlugTip(FVector& OutTipLocation, FVector& OutDirection) const;

	// --- Hand ---

	/** Applied when play starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands") FLinearColor SkinColor = FLinearColor(0.80f, 0.60f, 0.48f, 1.0f);

	/** Scales the tracker's reported joint radii. Above 1 thickens fingers that read too thin. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands", meta = (ClampMin = "0.5", ClampMax = "2.0"))
	float RadiusScale = 1.0f;

	/** Floor for a joint radius in cm — a runtime reporting 0 would otherwise draw invisible fingers. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands", meta = (ClampMin = "0.1", ClampMax = "2.0"))
	float MinJointRadius = 0.5f;

	/** Thickness in cm of the ellipsoid that fills the palm between wrist and knuckles. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands", meta = (ClampMin = "0.5", ClampMax = "6.0"))
	float PalmThickness = 2.6f;

	// --- Plug ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Plug") bool bShowPlug = true;

	/** Left or Right. The console sits to the driver's right in a left-hand-drive car. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Plug") EControllerHand PlugHand = EControllerHand::Right;

	/** Moulded body in cm: X length (along the index finger), Y width, Z thickness. Default: a USB-C cable end. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Plug") FVector PlugBodySize = FVector(2.2f, 1.2f, 0.65f);

	/** Metal tongue ahead of the body, cm, same axes. USB-C: 0.65 long, 0.83 wide, 0.25 thick. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Plug") FVector PlugTipSize = FVector(0.65f, 0.83f, 0.25f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Plug") float CableLength = 6.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Plug") float CableDiameter = 0.4f;

	/**
	 * Applied in the grip frame: origin between the thumb tip and index tip, X along the index finger, Z from
	 * the index tip toward the thumb tip. Tune with the headset on — people pinch a plug differently.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Plug") FTransform PlugOffset = FTransform::Identity;

	/** Applied when play starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Plug") FLinearColor PlugColor = FLinearColor(0.04f, 0.04f, 0.045f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Plug") FLinearColor TipColor  = FLinearColor(0.60f, 0.60f, 0.62f, 1.0f);

	// --- Assets: engine basic shapes by default; swap for nicer meshes without touching code ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Assets") TObjectPtr<UStaticMesh> SphereMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Assets") TObjectPtr<UStaticMesh> CylinderMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Assets") TObjectPtr<UStaticMesh> CubeMesh;

	/** Needs a vector parameter named "Color" — the engine's BasicShapeMaterial has one. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Virtual Hands|Assets") TObjectPtr<UMaterialInterface> ShapeMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	IHandTracker* GetHandTracker() const;

	/** Tracker joints for one hand, or the canned preview pose. False = draw nothing for this hand. */
	bool GetJoints(EControllerHand Hand, bool bPreview, TArray<FVector>& OutPositions, TArray<float>& OutRadii) const;

	/** Grip frame between thumb tip and index tip, PlugOffset applied. Shared by drawing and GetPlugTip. */
	bool ComputeGrip(const TArray<FVector>& Positions, FTransform& OutGrip) const;

	void UpdateHand(EControllerHand Hand, const TArray<FVector>& Positions, const TArray<float>& Radii);
	void UpdatePlug(const TArray<FVector>& Positions);
	void SetHandVisible(EControllerHand Hand, bool bVisible);
	void SetPlugVisible(bool bVisible);

	/** Shared setup for every drawn part: material, no collision or shadow, world-space transform. */
	void ConfigureShape(UStaticMeshComponent* Component, UStaticMesh* Mesh, const FLinearColor& Color);

	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> LeftJoints;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> LeftBones;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> RightJoints;
	UPROPERTY(Transient) TObjectPtr<UInstancedStaticMeshComponent> RightBones;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> PlugBody;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> PlugTip;
	UPROPERTY(Transient) TObjectPtr<UStaticMeshComponent> PlugCable;

	/** Previous on/off state, so the log fires on transitions only. */
	bool bWasOn = false;
};
