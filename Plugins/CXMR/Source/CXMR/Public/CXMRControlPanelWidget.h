// Copyright GMTCK CX.

// 차량 선택과 보기 조작 담당함.
// 데스크톱 선택 바인딩 유지함.
// 보정과 설정은 통합 창에서 조작함.
// Subsystem 값을 읽고 씀.
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CXMRControlPanelWidget.generated.h"

class SWidget;
class SWidgetSwitcher;
class UCXMRSubsystem;
class UCXMRVehicleLoaderComponent;
class UCXMRTuningSubsystem;

UCLASS(DisplayName = "CXMR Control Panel")
class CXMR_API UCXMRControlPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	/** 0은 차량, 1은 보기. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI", meta=(ToolTip="0 Vehicle, 1 View.")) void SetActiveTab(int32 TabIndex);
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") int32 GetActiveTab() const { return ActiveTab; }
	/** 통합 창에서도 같은 바인딩 사용함. */
	TSharedRef<SWidget> BuildDisplayPage();
	TSharedRef<SWidget> BuildViewerPage();
	UCXMRVehicleLoaderComponent* FindLoader() const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	UCXMRTuningSubsystem* GetTuning() const;

	// Slate 행이 소프트 애셋을 표시하는 동안 유지함.
	UPROPERTY(Transient) TArray<TObjectPtr<UObject>> ReviewAssets;
	mutable TWeakObjectPtr<UCXMRVehicleLoaderComponent> CachedLoader;
	int32 ActiveTab = 0;
	TSharedPtr<SWidgetSwitcher> Pages;
};
