// Copyright GMTCK CX.
//
// UCXMRDesktopPanelComponent — the operator's window on the desktop monitor.
//
// In a CXR session the person wearing the headset is a decision maker looking at a clay model, not
// an operator: they will not aim a laser at a panel strapped to their wrist. Someone else runs the
// session from the desk. This puts the same control panel in a normal OS window on the monitor,
// beside (not inside) the headset view — the way Varjo Lab sits next to the running app.
//
// It hosts the SAME widget class the hand panel uses. Both read and write the subsystem, so two
// live instances stay in sync for free: toggling MR here updates the readout on the wrist.
//
// Headset and controller input is unaffected by which window has focus (it comes from the OpenXR
// runtime), so the operator can click here while the wearer keeps using the controllers. Keyboard
// shortcuts DO follow focus — that is the trade, and it splits the roles cleanly.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CXMRDesktopPanelComponent.generated.h"

class SWindow;
class UUserWidget;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Desktop Panel")
class CXMR_API UCXMRDesktopPanelComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRDesktopPanelComponent();

	/** Widget shown in the window. Defaults to the same panel the hand holds. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Desktop Panel")
	TSubclassOf<UUserWidget> PanelClass;

	/** Open automatically on BeginPlay. Off if the session should start with a clean desktop. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Desktop Panel")
	bool bOpenOnBeginPlay = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Desktop Panel")
	FVector2D WindowSize = FVector2D(480.f, 640.f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Desktop Panel")
	FText WindowTitle;

	UFUNCTION(BlueprintCallable, Category = "CXMR|Desktop Panel") void OpenWindow();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Desktop Panel") void CloseWindow();
	UFUNCTION(BlueprintCallable, Category = "CXMR|Desktop Panel") void ToggleWindow();
	UFUNCTION(BlueprintPure,     Category = "CXMR|Desktop Panel") bool IsWindowOpen() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** Slate window is not a UObject — held by shared ptr and destroyed explicitly in EndPlay,
	 *  or it outlives PIE and stays on the editor desktop. */
	TSharedPtr<SWindow> Window;

	UPROPERTY(Transient) TObjectPtr<UUserWidget> PanelWidget;
};
