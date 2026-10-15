// Copyright GMTCK CX.
#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "CXMRAdvancedSettings.h"
class UCXMRTuningWindowComponent;
class UCXMRTuningSubsystem;
struct FCXMRTunable;
class SCXMRAdvancedPanel : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCXMRAdvancedPanel) {}
		SLATE_ARGUMENT(UCXMRTuningWindowComponent*, Control)
		SLATE_ARGUMENT(UCXMRTuningSubsystem*, Tuning)
	SLATE_END_ARGS()
	void Construct(const FArguments& Args);
private:
	TSharedRef<SWidget> Page(int32 Index);
	TSharedRef<SWidget> Row(const FCXMRAdvancedRow& Spec,const FCXMRTunable& Tunable);
	TWeakObjectPtr<UCXMRTuningWindowComponent> Control;
	TWeakObjectPtr<UCXMRTuningSubsystem> Tuning;
	TMap<FName,FString> Done;
};
