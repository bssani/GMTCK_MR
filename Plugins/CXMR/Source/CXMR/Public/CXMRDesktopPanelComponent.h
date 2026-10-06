// Copyright GMTCK CX.

// 진행자용 데스크톱 창.

// 헤드셋 착용 중에도 데스크톱에서 조작할 수 있게 함.

// 같은 액터의 Tuning Window로 창 조작을 넘김.
// Tuning Window가 없으면 기존 간단한 창 사용함.

// HMD 입력은 포커스와 무관함. 키보드 단축키는 포커스를 따름.

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

	/** Tuning Window가 없을 때 사용할 위젯. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Desktop Panel", meta=(ToolTip="Widget used when the actor has no Tuning Window component."))
	TSubclassOf<UUserWidget> PanelClass;

	/** 시작 시 창 자동으로 열음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Desktop Panel", meta=(ToolTip="Open automatically on BeginPlay. Off if the session should start with a clean desktop."))
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
	/** Slate 창은 GC가 정리하지 않으므로 EndPlay에서 닫음. */
	TSharedPtr<SWindow> Window;

	UPROPERTY(Transient) TObjectPtr<UUserWidget> PanelWidget;
};
