// Copyright GMTCK CX.
//
// UCXMRTurntableComponent (Viewer) — the exterior viewing strategy: the user stands still and the
// vehicle turns. Chosen because walking around a car drifts badly on the 2023 inside-out rig.
//
// Rotation is applied to a dedicated Turntable node, NEVER to VehicleAnchor. The anchor holds what
// calibration computed; spinning it would silently destroy the alignment. Anchor -> Turntable ->
// vehicle keeps the two concerns in separate transforms.
//
// Exterior only: in interior mode the user is sitting inside the car, so spinning it would break
// both the mise-en-scene and the alignment with the real seat.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRTypes.h"
#include "CXMRTurntableComponent.generated.h"

class UCXMRPlacementComponent;
class UCXMRSubsystem;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Turntable")
class CXMR_API UCXMRTurntableComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRTurntableComponent();

	/** Node that receives the yaw. Set by ACXMRVehicleRoot; falls back to the owner root. */
	UPROPERTY(EditInstanceOnly, BlueprintReadWrite, Category = "CXMR|Turntable") TObjectPtr<USceneComponent> TurntableTarget;

	/** Degrees per second while the stick is held. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Turntable") float ManualSpeed = 60.0f;

	/** Degrees per second for the A / B continuous spin. Slower — it runs unattended. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Turntable") float AutoSpeed = 20.0f;

	/** Stick input below this is ignored (thumbstick rest drift). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Turntable", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float AxisDeadzone = 0.15f;

	/** Held-stick rotation. Feed the raw axis; sign is the direction. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Turntable") void AddYawInput(float AxisValue);

	/** Continuous spin toggles. Pressing the opposite direction switches rather than stacking. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Turntable") void ToggleSpinLeft();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Turntable") void ToggleSpinRight();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Turntable") void StopSpin();

	/** Back to the calibrated orientation. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Turntable") void ResetRotation();

	UFUNCTION(BlueprintPure, Category = "CXMR|Turntable") int32 GetSpinDirection() const { return SpinDirection; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Turntable") float GetYaw() const { return CurrentYaw; }

	/** False in interior (MarkerAnchor) mode — the turntable is an exterior-only strategy. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Turntable") bool IsTurntableAllowed() const;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	USceneComponent* ResolveTarget() const;

	UFUNCTION() void HandleViewerAction(ECXMRViewerAction Action);
	UFUNCTION() void HandleTurntableAxis(float AxisValue);

	void ApplyYaw();

	/** -1 left, 0 off, +1 right. */
	int32 SpinDirection = 0;

	float CurrentYaw   = 0.0f;
	float PendingAxis  = 0.0f;

	UPROPERTY(Transient) TObjectPtr<UCXMRPlacementComponent> Placement;
	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
};
