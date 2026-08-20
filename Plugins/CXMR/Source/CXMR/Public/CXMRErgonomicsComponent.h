// Copyright GMTCK CX.
//
// UCXMRErgonomicsComponent (Evaluation / HF) — snaps the study to a percentile eye point.
//
// "Move to the eye position" means opposite things in the two modes, so the component branches on
// whether MR is on:
//   * VR  — the world is virtual, so it moves the VIEWPOINT: the pawn is recentred until the HMD
//           sits at the design eye point, looking down the vehicle's forward axis.
//   * MR  — the user's body is real and cannot be moved, so it moves the VEHICLE: the car is
//           repositioned until its eye point coincides with the user's real eyes.
//
// The MR move only makes sense with no physical seat (PawnRelative placement). When the vehicle is
// marker-anchored to a real buck, the eye point is already physically real and the component leaves
// it alone — moving the car would fight the calibration and the real seat.
//
// Lives on ACXMRVehicleRoot, beside the loader: the eye point is vehicle-local, so it needs the
// spawned vehicle's transform, and the loader owns that.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRErgonomicsComponent.generated.h"

class UCXMRSubsystem;
class UCXMRErgonomicsProfile;
class UCXMRVehicleLoaderComponent;
class UCameraComponent;
class APawn;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Ergonomics")
class CXMR_API UCXMRErgonomicsComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRErgonomicsComponent();

	/** Overrides the vehicle profile's linked ergonomics. Leave empty to use the loaded vehicle's. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Ergonomics") TObjectPtr<UCXMRErgonomicsProfile> ProfileOverride;

	/** Snap to a percentile by index / by cycling. Cycling wraps like the vehicle selector. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Ergonomics") void SetManikin(int32 Index);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Ergonomics") void NextManikin();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Ergonomics") void PreviousManikin();

	/** Re-applies the current manikin — e.g. after the vehicle is re-placed. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Ergonomics") void ReapplyCurrent();

	/** Clamp to the newly loaded vehicle's profile and refresh the readout, WITHOUT moving anything.
	 *  A vehicle swap must not teleport the viewpoint (VR) or the car (MR) on its own — but leaving
	 *  the index past the end of a shorter profile makes the row go blank and every getter fail. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Ergonomics") void RefreshForNewVehicle();

	UFUNCTION(BlueprintPure, Category = "CXMR|Ergonomics") int32 GetManikinIndex() const { return CurrentIndex; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Ergonomics") FName GetManikinName() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	UCXMRErgonomicsProfile* ResolveProfile() const;
	UCXMRVehicleLoaderComponent* GetLoader() const;

	/** The vehicle-local eye point turned into a world transform via the spawned vehicle. */
	bool ComputeEyeWorld(FTransform& OutEyeWorld) const;
	bool GetPlayer(APawn*& OutPawn, UCameraComponent*& OutCamera) const;

	void MoveViewpointToEye(const FTransform& EyeWorld);   // VR
	void MoveVehicleToEye(const FTransform& EyeWorld);     // MR (PawnRelative only)

	void ApplyManikin(int32 Index);
	void Cycle(int32 Step);

	UFUNCTION() void HandleErgonomicsStep(int32 Step);

	UPROPERTY(Transient) int32 CurrentIndex = 0;
	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
};
