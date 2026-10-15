// Copyright GMTCK CX.
#pragma once
#include "CoreMinimal.h"
struct FCXMRTunable;
class UCXMRTuningSubsystem;

struct FCXMRAdvancedRow
{
	FName Id;
	const TCHAR* Tab;
	const TCHAR* Group;
	const TCHAR* Label;
	const TCHAR* Help;
	const TCHAR* Unit;
	const TCHAR* Type;
	FName Requires;
};
namespace CXMRAdvancedSettings
{
	CXMR_API TConstArrayView<FCXMRAdvancedRow> Rows();
	CXMR_API void Describe(FCXMRTunable& Row);
	CXMR_API int32 ChangedCount(const UCXMRTuningSubsystem* Tuning, const FString& Tab = FString());
	CXMR_API void ResetTab(UCXMRTuningSubsystem* Tuning, const FString& Tab);
}
