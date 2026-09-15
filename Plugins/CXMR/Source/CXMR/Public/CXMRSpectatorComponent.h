// Copyright GMTCK CX.
//
// UCXMRSpectatorComponent — what the monitor shows while the headset is worn.
//
// By default the monitor mirrors the headset, so every head movement shakes the picture for the people watching.
// This renders a separate camera into a texture and puts it on the headset's spectator screen: either the wearer's view
// with the shake smoothed out and the horizon kept level, or a slow orbit around the vehicle.
// Off by default — it renders the scene a second time.
//
// Only virtual content is in this picture. Passthrough video is composited inside the headset runtime and never reaches
// the engine, so in mixed reality the real room is missing from it.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRSpectatorComponent.generated.h"

class USceneCaptureComponent2D;
class UTextureRenderTarget2D;
class UCXMRTuningSubsystem;

UENUM(BlueprintType)
enum class ECXMRSpectatorMode : uint8
{
	Off         UMETA(DisplayName = "Off (mirror the headset)"),
	FirstPerson UMETA(DisplayName = "Wearer's view, smoothed"),
	Orbit       UMETA(DisplayName = "Orbit the vehicle")
};

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Spectator")
class CXMR_API UCXMRSpectatorComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRSpectatorComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "CXMR|Spectator") void SetMode(ECXMRSpectatorMode NewMode);
	UFUNCTION(BlueprintPure, Category = "CXMR|Spectator") ECXMRSpectatorMode GetMode() const { return Mode; }

	/** The texture the spectator camera renders into. Null while Off. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Spectator") UTextureRenderTarget2D* GetRenderTarget() const { return RenderTarget; }

	/** True while that texture is on the headset's spectator screen. Always false without a headset. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Spectator") bool IsOnSpectatorScreen() const { return bOnSpectatorScreen; }

	/** Picture size in pixels. Applied the next time the camera starts. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Spectator") FIntPoint Resolution = FIntPoint(1920, 1080);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Spectator", meta = (ClampMin = "30.0", ClampMax = "140.0"))
	float FieldOfView = 90.0f;

	/** How quickly the smoothed view catches up with the head (per second). Lower is calmer but lags more. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Spectator", meta = (ClampMin = "0.1", ClampMax = "30.0"))
	float FollowSpeed = 4.0f;

	/** Orbit: horizontal distance from the vehicle's centre (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Spectator", meta = (ClampMin = "50.0"))
	float OrbitDistance = 600.0f;

	/** Orbit: camera height above the vehicle's lowest point (cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Spectator") float OrbitHeight = 160.0f;

	/** Orbit: degrees per second; negative turns the other way. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Spectator") float OrbitDegreesPerSecond = 8.0f;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRTuningSubsystem* GetTuning() const;

	void StartCapture();
	void StopCapture();
	void UpdateFirstPerson(float DeltaTime);
	void UpdateOrbit(float DeltaTime);

	/** Centre and lowest point of the first vehicle in the level. False when there is none. */
	bool FindOrbitTarget(FVector& OutCentre, double& OutGroundZ) const;

	void RegisterTunables();
	FText DescribeOutput() const;

	ECXMRSpectatorMode Mode = ECXMRSpectatorMode::Off;

	UPROPERTY(Transient) TObjectPtr<USceneCaptureComponent2D> Capture;
	UPROPERTY(Transient) TObjectPtr<UTextureRenderTarget2D> RenderTarget;

	bool bOnSpectatorScreen = false;
	/** The spectator screen mode (an ESpectatorScreenMode) before this took over, restored when it stops. */
	uint8 PreviousScreenMode = 0;
	bool bFollowStarted = false;
	float OrbitAngle = 0.0f;
	/** Orbit centre when the level has no vehicle: fixed in front of the viewer when the orbit started. */
	FVector FallbackOrbitCentre = FVector::ZeroVector;
};
