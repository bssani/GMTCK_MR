// Copyright GMTCK CX.
#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "UObject/StrongObjectPtr.h"
#include "CXMRControlStatus.h"
class UCXMRTuningWindowComponent;
class UCXMRTuningSubsystem;
class UCXMRControlPanelWidget;
class SBox;
class SCXMRControlPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCXMRControlPanel) {}
		SLATE_ARGUMENT(UCXMRTuningWindowComponent*, Control)
		SLATE_ARGUMENT(UCXMRTuningSubsystem*, Tuning)
		SLATE_ARGUMENT(UCXMRControlPanelWidget*, Viewer)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual FReply OnPreviewKeyDown(const FGeometry&, const FKeyEvent&) override;
	virtual bool SupportsKeyboardFocus() const override { return true; }
	virtual void Tick(const FGeometry&, double, float) override;
private:
	FCXMRControlStatus State() const;
	bool ModeSelected(bool bMR) const;
	TSharedRef<SWidget> Header();
	TSharedRef<SWidget> Status();
	TSharedRef<SWidget> Calibration();
	TSharedRef<SWidget> Vehicles();
	void Refresh();
	TWeakObjectPtr<UCXMRTuningWindowComponent> Control;
	TWeakObjectPtr<UCXMRTuningSubsystem> Tuning;
	TSharedPtr<SBox> Content;
	FString LastContentKey;
	TArray<TStrongObjectPtr<UObject>> VehicleAssets;
};
