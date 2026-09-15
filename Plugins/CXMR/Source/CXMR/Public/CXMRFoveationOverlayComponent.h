// Copyright GMTCK CX.
//
// UCXMRFoveationOverlayComponent — shows the foveated (full-resolution) area while its toggle is on.
//
// In Quad View rendering Varjo draws a small focus view that follows the gaze at full resolution, inside a
// lower-resolution context view. This tints what the focus view covers with a post-process
// (/CXMR/Core/Materials/PP_CXMRFoveationVisualization, copied from the Varjo example's
// PP_FoveatedRenderingVisualization), so a session can see whether foveation runs and follows the eyes.
// Bound to IA_Varjo_FoveatedRenderingVisualizationToggle (I).
//
// Nothing gets tinted while foveated rendering is not running — Stereo rendering mode, the FoveatedRendering project
// setting off, or no eye tracking. The toggle still switches, and the log and the tuning window say why it shows nothing.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRFoveationOverlayComponent.generated.h"

class UCXMRSubsystem;
class UCXMRTuningSubsystem;
class UMaterialInterface;
class UPostProcessComponent;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Foveation Overlay")
class CXMR_API UCXMRFoveationOverlayComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRFoveationOverlayComponent();

	/** Post-process material that tints the focus view. Defaults to the plugin's copy of the Varjo example material. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "CXMR|Foveation")
	TObjectPtr<UMaterialInterface> OverlayMaterial;

	/** True while the overlay is applied (the toggle is on and a material is set). */
	UFUNCTION(BlueprintPure, Category = "CXMR|Foveation") bool IsOverlayApplied() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	UCXMRTuningSubsystem* GetTuning() const;

	void Apply(bool bOn);
	void RegisterTunables();
	FText DescribeFoveation() const;

	UFUNCTION() void HandleVisualizationChanged(bool bOn);

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;

	/** Unbound post-process volume created at BeginPlay, enabled only while the overlay is on. */
	UPROPERTY(Transient) TObjectPtr<UPostProcessComponent> Volume;
};
