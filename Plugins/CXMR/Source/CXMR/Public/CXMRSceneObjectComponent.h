// Copyright GMTCK CX.
//
// UCXMRSceneObjectComponent — declares what an actor is FOR in mixed reality.
//
// The Varjo example manages this from the other end: BP_MRControls holds a hand-populated
// VROnlyObjects array and one MRMaskObject reference. That does not survive a template used across
// programs and levels — every new map means rebuilding the array by hand, and a forgotten wall stays
// visible in MR with nothing to reveal the omission.
//
// Here the object declares its own role and subscribes to the subsystem directly. There is no central
// list to maintain, nothing to register, and no lifetime to manage. Actors with no component are
// always visible, so the vehicle needs no marking at all — only exceptions are tagged.
//
// For MaskMesh this also applies the render flags §4-1 of Docs/Varjo-Capabilities.md requires
// (CustomDepth on, main pass / depth pass / shadows / reflections / sky / ray tracing / decals off).
// That is seven checkboxes per mesh by hand, and missing "Render in Main Pass" silently leaves the
// mask visible. Doing it in code makes it impossible to get wrong.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRTypes.h"
#include "CXMRSceneObjectComponent.generated.h"

class UCXMRSubsystem;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Scene Object")
class CXMR_API UCXMRSceneObjectComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRSceneObjectComponent();

	/** What this actor is for. VROnly = virtual room, hidden in MR. MaskMesh = hole for a real object. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Scene") ECXMRSceneRole Role = ECXMRSceneRole::VROnly;

	/** Re-applies the role now. Call after spawning or re-parenting meshes at runtime. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Scene") void ApplyRole();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;

	UFUNCTION() void HandleVRBackgroundChanged(bool bVisible);

	/** Custom-Depth render flags for mask geometry (§4-1). */
	void ApplyMaskRenderFlags();

	/** VROnly: drop out of the main view while still feeding lighting (sky light, reflections, shadows). */
	void ApplyVROnlyVisibility(bool bVisible);

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
};
