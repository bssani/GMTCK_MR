// Copyright GMTCK CX.
//
// UCXMRTuningWindowComponent — the operator's tuning window: every registered tunable as a live number, checkbox or
// button, in a desktop OS window beside the headset view.
//
// Slate, not UMG: the rows are generated from the tuning registry, so adding a tunable never means editing a widget
// blueprint (and never risks the widget-tree breakage that editing one from Python has caused). Values are read back
// from the feature every frame, so a key press or a controller change shows up here straight away.
//
// It also registers the template's core tunables — mixed reality, depth test, view offset, exposure, adjust speed —
// because those need a live world and pawn to act on. Other features register their own (vehicle placement does).
// Console: CXMR.Tuning opens or closes it.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRTuningWindowComponent.generated.h"

class SWidget;
class SWindow;
class UCXMRTuningSubsystem;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Tuning Window")
class CXMR_API UCXMRTuningWindowComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRTuningWindowComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Tuning Window") bool bOpenOnBeginPlay = true;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Tuning Window") FVector2D WindowSize = FVector2D(480.f, 900.f);
	/** Where the window opens on the desktop, in pixels. Kept clear of the centred control window. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Tuning Window") FVector2D WindowPosition = FVector2D(40.f, 60.f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Tuning Window") FText WindowTitle;

	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning Window") void OpenWindow();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning Window") void CloseWindow();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning Window") void ToggleWindow();
	UFUNCTION(BlueprintPure,     Category = "CXMR|Tuning Window") bool IsWindowOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRTuningSubsystem* GetTuning() const;
	void RegisterCoreTunables();
	void RebuildContent();
	TSharedRef<SWidget> BuildPanel() const;

	/** Slate window is not a UObject — held by shared ptr and closed explicitly in EndPlay. */
	TSharedPtr<SWindow> Window;
	FDelegateHandle TunablesChangedHandle;
};
