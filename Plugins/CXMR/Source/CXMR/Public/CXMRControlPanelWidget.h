// Copyright GMTCK CX.
//
// 사용자 패널. 차량 선택과 기본 보기 조작을 담당함.
// 데스크톱 창과 손목 패널에서 같은 위젯을 사용함.
// 보정, 장비 설정, 진단은 개발자 패널에 둠.
// 값은 Subsystem에서 읽고 조작함. Slate 행 UI는 개발자 패널과 공유함.
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
	/** 0 Vehicle, 1 View. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void SetActiveTab(int32 TabIndex);
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") int32 GetActiveTab() const { return ActiveTab; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void ReleaseSlateResources(bool bReleaseChildren) override;

private:
	UCXMRSubsystem* GetCXMR() const;
	UCXMRTuningSubsystem* GetTuning() const;

	TSharedRef<SWidget> BuildDisplayPage();
	TSharedRef<SWidget> BuildViewerPage();

	int32 ActiveTab = 0;
	TSharedPtr<SWidgetSwitcher> Pages;
};
