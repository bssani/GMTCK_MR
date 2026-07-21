// Copyright GMTCK CX.
//
// UCXMRMarkerDebugComponent — the instrument for headset verification.
//
// Calibration cannot be judged by eye. When the car sits slightly wrong, the cause is either the
// marker pose the plugin reports, the LocalOffset authored in the profile, or the fitting maths —
// and without seeing the raw marker pose those three are indistinguishable. This draws the pose
// EXACTLY as the plugin reports it, with no transform applied, so the first of the three can be
// confirmed or ruled out on its own.
//
// Verifies:
//   * marker world-space assumption  — the drawn axes must sit on the physical marker
//   * detection lifecycle            — Detected fires once per ID per session; re-acquire is Moved
//   * Recalibrate                    — cycling marker tracking must produce a fresh Detected
//
// Enable with the console command `CXMR.DebugMarkers 1` — no input action or IMC wiring needed,
// and it works the same in PIE and in a packaged build on the headset.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRMarkerDebugComponent.generated.h"

class UCXMRSubsystem;

USTRUCT()
struct FCXMRDebugMarker
{
	GENERATED_BODY()

	FVector  Position = FVector::ZeroVector;
	FRotator Rotation = FRotator::ZeroRotator;
	FVector2D Size    = FVector2D::ZeroVector;

	/** Seconds since level start, so the log can be read as a timeline. */
	float LastUpdateTime = 0.0f;
	int32 MoveCount      = 0;
	bool  bLost          = false;
};

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Marker Debug")
class CXMR_API UCXMRMarkerDebugComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRMarkerDebugComponent();

	/** Axis length for the drawn marker pose (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Debug") float AxisLength = 10.0f;

	/** Also draw the calibrated vehicle anchor, so marker and result can be compared side by side. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Debug") bool bDrawVehicleAnchor = true;

	/** Dumps every marker seen this session to the log. Console: CXMR.DumpMarkers */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Debug") void DumpMarkers() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	float Now() const;

	UFUNCTION() void HandleMarkerDetected(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size);
	UFUNCTION() void HandleMarkerMoved(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size);
	UFUNCTION() void HandleMarkerLost(int32 MarkerId);

	void Draw() const;

	/** Every marker ID seen this session, including lost ones — losing the history hides the bug. */
	TMap<int32, FCXMRDebugMarker> Markers;

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
};
