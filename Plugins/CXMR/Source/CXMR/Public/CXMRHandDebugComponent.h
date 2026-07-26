// Copyright GMTCK CX.
//
// UCXMRHandDebugComponent — headset verification instrument for hand tracking.
//
// Draws the tracked hand skeleton so a session can answer "does the headset see my hands, and how
// well" without any interaction feature existing yet. Reads the engine's IHandTracker modular
// feature (XR_EXT_hand_tracking via OpenXRHandTracking), NOT the Varjo plugin — hand JOINTS are an
// engine-level capability; the Varjo module only adds aim/grip interaction poses on top.
//
// Visibility follows the subsystem's hand-visualization state (bound to IA_Varjo_HandVisualization),
// so it is a toggle in the headset rather than a console command.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "CXMRHandDebugComponent.generated.h"

class UCXMRSubsystem;
class IHandTracker;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Hand Debug")
class CXMR_API UCXMRHandDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRHandDebugComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** True when a hand tracker is present AND currently reporting valid state. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Hands") bool IsHandTrackingAvailable() const;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands") FLinearColor LeftColor  = FLinearColor(0.20f, 0.75f, 1.00f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands") FLinearColor RightColor = FLinearColor(1.00f, 0.65f, 0.15f, 1.0f);

	/** Joints are drawn at the tracker's own reported radius; this scales it for legibility. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands", meta = (ClampMin = "0.1", ClampMax = "5.0"))
	float RadiusScale = 1.0f;

	/** Connect the joints of each finger. Off = joint spheres only. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands") bool bDrawBones = true;

	/** Log a line whenever either hand gains or loses tracking — the headset answer we are after. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Hands") bool bLogTrackingChanges = true;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	IHandTracker* GetHandTracker() const;

	/** Returns true if this hand was drawn (i.e. is actually being tracked right now). */
	bool DrawHand(EControllerHand Hand, const FLinearColor& Color);

	UFUNCTION() void HandleVisualizationChanged(bool bOn);

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;

	// Previous tracked state per hand, so the log fires on transitions only.
	bool bLeftWasTracked  = false;
	bool bRightWasTracked = false;
};
