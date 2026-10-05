// Copyright GMTCK CX.
#pragma once
#include "CoreMinimal.h"

namespace CXMRPanelSections
{
	// 번역된 이름 말고 ID로 탭을 나눔. 프로젝트에서 추가한 항목은 진단 탭에 넣음.
	inline int32 DeveloperTab(FName Id)
	{
		const FString Key = Id.ToString();
		if (Key.StartsWith(TEXT("Vehicle.")) || Key.StartsWith(TEXT("Box.")) || Key.StartsWith(TEXT("Input."))) { return 0; }
		if (Key.StartsWith(TEXT("MR.")) || Key.StartsWith(TEXT("Depth.")) || Key.StartsWith(TEXT("Display.")) || Key.StartsWith(TEXT("Spectator."))) { return 1; }
		return 2;
	}
}
