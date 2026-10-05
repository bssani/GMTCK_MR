// Copyright GMTCK CX.
//
// 개발자 패널. 보정 / 화면·장비 / 진단 탭으로 나눠서 보여줌.
// 기능별로 등록한 설정을 Slate 행으로 만듦. 위젯 BP를 수정할 필요는 없음.
// 다른 입력으로 값이 바뀌어도 현재 값을 읽어서 화면에 반영함.
// MR, 깊이, 노출, 조정 속도는 여기서 등록함. 나머지는 각 기능에서 등록함.
// 콘솔의 CXMR.Tuning으로도 창을 열고 닫을 수 있음.
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
	int32 ActiveTab = 0;
};
