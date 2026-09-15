// Copyright GMTCK CX.
//
// ACXMRUsbPortTarget — marks one USB opening in the vehicle CAD and shows the wearer bringing a plug to it.
//
// The CAD carries a port as an opening only: nothing in it says whether a plug goes in. Place one of these on
// each opening, arrow pointing out of the port toward the person, and it answers the question the demo asks —
// could the wearer bring the plug to this port, straight, from where they sit.
//
// The plug comes from the pawn's hand cut-out component (UCXMRVirtualHandComponent::GetPlugTip): the tracked thumb and
// index tips plus its PlugOffset. What the wearer sees is the real plug.
//
// An opening is a centimetre across — too small to read in a headset — so the port says more than its size:
//   Idle      the marker only (a four-bar frame, or IndicatorMesh), dim; or nothing, with bShowWhenIdle off
//   Approach  the plug is within ApproachDistance and this is the nearest port: a guide beam stands out of the
//             opening along the insertion axis, and marker and beam pulse in ApproachColor
//   Aligned   within EnterDistance and MaxAngle: AlignedColor at full glow, a pop, and AlignedSound
// Colours are self-lit (BarMaterial is unlit) so they read against the camera image. Only the port nearest the plug
// reacts, so a 2x2 block of openings does not light up as one.
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
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;

UENUM(BlueprintType)
enum class ECXMRPortState : uint8
{
	Idle,
	/** The plug is near and this is the nearest port. */
	Approach,
	/** The plug is lined up with this port. */
	Aligned,
};

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

	/** Plug tip within this distance of the port centre, and within MaxAngle, lines the port up. cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "0.1"))
	float EnterDistance = 1.5f;

	/** Stays lined up until the tip is farther than this. A single threshold flickers at the edge. cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "0.1"))
	float ExitDistance = 3.0f;

	/** Largest angle between the plug and the port axis that still counts as straight. Degrees. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "1.0", ClampMax = "80.0"))
	float MaxAngle = 25.0f;

	/** Inner size of the frame around the opening, cm: X across the port (actor Y), Y up the port (actor Z). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FVector2D FrameSize = FVector2D(1.4f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "0.05"))
	float FrameThickness = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FLinearColor IdleColor     = FLinearColor(0.35f, 0.35f, 0.38f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FLinearColor ApproachColor = FLinearColor(1.00f, 0.72f, 0.05f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FLinearColor AlignedColor  = FLinearColor(0.10f, 0.95f, 0.25f, 1.0f);

	/** Off = the marker appears only while the plug approaches or lines up, leaving the CAD untouched the rest of the time. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") bool bShowWhenIdle = true;

	// --- Approach ---

	/** The nearest port starts guiding once the plug tip is this close, cm. 0 = no approach stage. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ClampMin = "0.0"))
	float ApproachDistance = 12.0f;

	/** A beam standing out of the opening along the insertion axis while the plug approaches (and while lined up). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach") bool bShowGuide = true;

	/** cm of beam, starting GuideGap out from the opening. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ClampMin = "1.0"))
	float GuideLength = 15.0f;

	/** cm left clear in front of the opening, so the beam never covers the marker or the plug's last stretch. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ClampMin = "0.0"))
	float GuideGap = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ClampMin = "0.05"))
	float GuideDiameter = 0.35f;

	/** Pulses per second while approaching. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ClampMin = "0.1"))
	float PulseRate = 2.5f;

	// --- Aligned ---

	/** Brightness of the self-lit colours when lined up; approaching pulses up to it. Needs BarMaterial's "Glow". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned", meta = (ClampMin = "0.1"))
	float GlowStrength = 3.0f;

	/** How far the marker swells for a moment on lining up. 1 = no pop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned", meta = (ClampMin = "1.0", ClampMax = "4.0"))
	float PopScale = 1.6f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned", meta = (ClampMin = "0.05"))
	float PopSeconds = 0.35f;

	/** Played once on lining up, through the PC's audio. Empty = silent. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned") TObjectPtr<USoundBase> AlignedSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned", meta = (ClampMin = "0.0"))
	float SoundVolume = 1.0f;

	// --- Custom Mesh ---

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

	/** On = the mesh keeps its own materials while idle. Off = it shows IdleColor like the frame. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Custom Mesh") bool bKeepMeshMaterialsWhenIdle = true;

	UFUNCTION(BlueprintPure, Category = "CXMR|Port") ECXMRPortState GetPortState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Port") bool IsAligned() const { return State == ECXMRPortState::Aligned; }

	/** Applies the frame, guide, IndicatorMesh and colours again after changing them during play. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Port") void RefreshMarker();

	// --- Assets ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets") TObjectPtr<UStaticMesh> BarMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets") TObjectPtr<UStaticMesh> GuideMesh;

	/**
	 * Paints the frame, the guide and (when not idle) IndicatorMesh. Needs a vector parameter "Color"; a scalar "Glow"
	 * makes it brighten. Default /CXMR/Core/Materials/M_CXMRUnlitColor is self-lit; the engine's BasicShapeMaterial
	 * works too, lit and without glow.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets") TObjectPtr<UMaterialInterface> BarMaterial;

protected:
	virtual void BeginPlay() override;

private:
	/** Places the bars, IndicatorMesh and the guide. Swell scales the marker about the port centre, for the pop. */
	void LayoutFrame(float Swell = 1.0f);

	/** Colour, visibility and materials for the current state. */
	void ApplyState();
	void SetState(ECXMRPortState NewState);

	/** Pulse and pop, every frame. */
	void Animate(float DeltaSeconds);

	/** True when no other port is nearer the tip; a port already reacting keeps a small lead. */
	bool IsNearestPort(const FVector& Tip, float Distance) const;

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

	/** The approach beam along the insertion axis. */
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> GuideBeam;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FrameMaterial;

	TWeakObjectPtr<UCXMRVirtualHandComponent> Hands;
	ECXMRPortState State = ECXMRPortState::Idle;

	/** Seconds into the approach pulse. */
	float PulseTime = 0.0f;

	/** Seconds into the pop; negative while not popping. */
	float PopElapsed = -1.0f;
};
