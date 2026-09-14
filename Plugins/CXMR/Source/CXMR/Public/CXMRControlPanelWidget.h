// Copyright GMTCK CX.
//
// UCXMRControlPanelWidget — the operator's control panel: the session's switches and actions on three tabs
// (Display / Calibration / Viewer), in the desktop control window and, when enabled, on the wearer's hand.
//
// Drawn with the same rows as the tuning window (CXMRPanelUI), so the two operator windows look and behave alike.
// Built in C++ instead of a widget-blueprint layout: that layout was bound to C++ by widget names and broke silently
// whenever a widget was renamed or lost. The panel talks only to the subsystem, and shows readouts other features
// publish through the tuning registry, so the desktop copy and the hand-held copy mirror each other with no wiring.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CXMRControlPanelWidget.generated.h"

class SWidget;
class SWidgetSwitcher;
class UCXMRSubsystem;
class UCXMRTuningSubsystem;

UCLASS(DisplayName = "CXMR Control Panel")
class CXMR_API UCXMRControlPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 0 Display, 1 Calibration, 2 Viewer. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void SetActiveTab(int32 TabIndex);
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") int32 GetActiveTab() const { return ActiveTab; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	UCXMRTuningSubsystem* GetTuning() const;

	TSharedRef<SWidget> BuildDisplayPage();
	TSharedRef<SWidget> BuildCalibrationPage();
	TSharedRef<SWidget> BuildViewerPage();

	int32 ActiveTab = 0;
	TSharedPtr<SWidgetSwitcher> Pages;
};
