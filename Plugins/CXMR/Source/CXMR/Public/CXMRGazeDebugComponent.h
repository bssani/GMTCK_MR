// Copyright GMTCK CX.
//
// UCXMRGazeDebugComponent — headset verification instrument for eye tracking.
//
// Draws a dot where the wearer is looking: the combined gaze ray from the engine's eye tracker (OpenXREyeTracker,
// XR_EXT_eye_gaze_interaction — switched on by the Varjo plugin) traced into the scene. It answers "does gaze come
// through, and does it land where I look" before anything is built on top of gaze.
//
// Follows the subsystem's gaze-visualization state, bound to IA_Varjo_GazeVisualizationToggle (G) — mapped in
// IMC_Varjo since the Varjo example was brought in, but never wired to anything until now.
//
// OpenXR delivers only the combined gaze pose: no per-eye rays, no fixation point, no confidence value.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRGazeDebugComponent.generated.h"

class UCXMRSubsystem;
class UCXMRTuningSubsystem;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Gaze Debug")
class CXMR_API UCXMRGazeDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRGazeDebugComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** Where the gaze ray lands this frame (the trace end when it hits nothing). False without gaze data. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Eyes")
	bool GetGazePoint(FVector& OutPoint, FVector& OutOrigin, AActor*& OutHitActor) const;

	/** How far the gaze ray is traced (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Eyes", meta = (ClampMin = "10.0"))
	float TraceDistance = 1000.0f;

	/** Dot size in pixels. Drawn in front of everything, so it never hides behind what it landed on. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Eyes", meta = (ClampMin = "1.0", ClampMax = "64.0"))
	float DotSize = 12.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Eyes") FLinearColor HitColor  = FLinearColor(0.10f, 1.00f, 0.30f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Eyes") FLinearColor MissColor = FLinearColor(1.00f, 0.80f, 0.10f, 1.0f);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	UCXMRTuningSubsystem* GetTuning() const;

	void RegisterTunables();
	FText DescribeGaze() const;

	UFUNCTION() void HandleVisualizationChanged(bool bOn);

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;

	/** Gaze state last frame, so the log fires on transitions only. */
	bool bHadGaze = false;
};
