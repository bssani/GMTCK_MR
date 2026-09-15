// Copyright GMTCK CX.
//
// ACXMRUsbPortTarget — marks one USB opening in the vehicle CAD and lights up when the plug lines up with it.
//
// The CAD carries a port as an opening only: nothing in it says whether a plug goes in. Place one of these on
// each opening, arrow pointing out of the port toward the person, and it answers the question the demo asks —
// could the wearer bring the plug to this port, straight, from where they sit.
//
// The plug comes from the pawn's UCXMRVirtualHandComponent::GetPlugTip — the same grip math that draws the
// plug — so what lights up matches what the wearer sees. That also works with the virtual hands hidden.
//
// The marker is a four-bar frame around the opening by default. Set IndicatorMesh to show a mesh of your own
// instead (a USB connector from the CAD, say): it keeps its own materials while idle and turns AlignedColor when
// the plug lines up.
//
// Hand tracking is not millimetre-accurate, so the defaults are loose on purpose. Green means "lined up at this
// port", not "fully inserted": the CAD has no receptacle and nothing here measures insertion.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CXMRUsbPortTarget.generated.h"

class UArrowComponent;
class UCXMRVirtualHandComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UStaticMesh;
class UStaticMeshComponent;

UCLASS(DisplayName = "CXMR USB Port Target")
class CXMR_API ACXMRUsbPortTarget : public AActor
{
	GENERATED_BODY()

public:
	ACXMRUsbPortTarget();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** Name used in the log, e.g. "USB-C left". Empty = the actor label. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FString Label;

	/** Plug tip within this distance of the port centre, and within MaxAngle, turns the port green. cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "0.1"))
	float EnterDistance = 1.5f;

	/** Stays green until the tip is farther than this. A single threshold flickers at the edge. cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "0.1"))
	float ExitDistance = 3.0f;

	/** Largest angle between the plug and the port axis that still counts as straight. Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float MaxAngle = 25.0f;

	/** Inner size of the frame around the opening, cm: X across the port (actor Y), Y up the port (actor Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FVector2D FrameSize = FVector2D(1.4f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "0.05"))
	float FrameThickness = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FLinearColor IdleColor    = FLinearColor(0.35f, 0.35f, 0.38f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FLinearColor AlignedColor = FLinearColor(0.10f, 0.95f, 0.25f, 1.0f);

	/** Off = the marker appears only while aligned, leaving the CAD untouched the rest of the time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") bool bShowWhenIdle = true;

	/**
	 * A mesh of your own for the marker, shown instead of the four-bar frame — one piece, any shape. Empty = the frame.
	 * Detection does not use it: the port is still this actor's location and arrow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Custom Mesh") TObjectPtr<UStaticMesh> IndicatorMesh;

	/**
	 * Where IndicatorMesh sits relative to this actor (X out of the port). A mesh exported from CAD often has its
	 * origin far from the part — move it back onto the opening here.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Custom Mesh") FTransform IndicatorOffset = FTransform::Identity;

	/** On = the mesh keeps its own materials until the plug lines up. Off = it shows IdleColor like the frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Custom Mesh") bool bKeepMeshMaterialsWhenIdle = true;

	UFUNCTION(BlueprintPure, Category = "CXMR|Port") bool IsAligned() const { return bAligned; }

	/** Applies FrameSize, IndicatorMesh, IndicatorOffset and the colours again after changing them during play. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Port") void RefreshMarker();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets") TObjectPtr<UStaticMesh> BarMesh;

	/** Needs a vector parameter named "Color" — the engine's BasicShapeMaterial has one. Also paints IndicatorMesh. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets") TObjectPtr<UMaterialInterface> BarMaterial;

protected:
	virtual void BeginPlay() override;

private:
	/** Places the four bars around the opening from FrameSize and FrameThickness, and IndicatorMesh if set. */
	void LayoutFrame();
	void ApplyState();
	UCXMRVirtualHandComponent* FindHands();

	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<USceneComponent> PortRoot;

	/** Editor only. Must point out of the port, toward the person reaching for it. */
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UArrowComponent> OutOfPort;

	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> BarTop;
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> BarBottom;
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> BarLeft;
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> BarRight;

	/** Shows IndicatorMesh. Hidden while it is empty. */
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> Indicator;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FrameMaterial;

	TWeakObjectPtr<UCXMRVirtualHandComponent> Hands;
	bool bAligned = false;
};
