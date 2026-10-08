// Copyright GMTCK CX.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class UCXMRTuningWindowComponent;
class UCXMRTuningSubsystem;
class UCXMRControlPanelWidget;
class UCXMRVehicleLoaderComponent;
class SBox;
enum class ECXMRControlPage : uint8;

/** 표시만 담당함. 탐색과 보정 상태는 창 컴포넌트에서 관리함. */
class SCXMRControlPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCXMRControlPanel) {}
		SLATE_ARGUMENT(UCXMRTuningWindowComponent*, Control)
		SLATE_ARGUMENT(UCXMRTuningSubsystem*, Tuning)
		SLATE_ARGUMENT(UCXMRControlPanelWidget*, Viewer)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
	virtual void Tick(const FGeometry& Geometry, double CurrentTime, float DeltaTime) override;

private:
	TSharedRef<SWidget> Header();
	TSharedRef<SWidget> Navigation(ECXMRControlPage Page, const FText& Label);
	TSharedRef<SWidget> RegistryPage(ECXMRControlPage Page, bool bAdvanced = false);
	TSharedRef<SWidget> AlignmentSteps();
	TSharedRef<SWidget> CalibrationPage();
	TSharedRef<SWidget> SettingsPage(UCXMRControlPanelWidget* InViewer);
	FText Status() const;
	TWeakObjectPtr<UCXMRTuningWindowComponent> Control;
	TWeakObjectPtr<UCXMRTuningSubsystem> Tuning;
	TWeakObjectPtr<UCXMRControlPanelWidget> Viewer;
	TSharedPtr<SBox> ReviewHost;
	TWeakObjectPtr<UCXMRVehicleLoaderComponent> LastLoader;
	TWeakObjectPtr<UObject> LastProfile;
	TWeakObjectPtr<UObject> LastCatalog;
	TWeakObjectPtr<AActor> LastVehicle;
};
