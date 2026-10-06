// Copyright GMTCK CX.

// 실시간 조정값과 저장값 관리함.

// 기능이 항목을 등록하면 창에서 표시함.

// 영속화 항목은 Tuning.json에 저장하고 다음 등록 때 적용함.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CXMRTuningSubsystem.generated.h"

enum class ECXMRTunableKind : uint8
{
	Bool,       // 체크박스. 값은 0 또는 1.
	Float,      // 범위 안에서 숫자 조정함.
	Choice,     // 목록 선택. 값은 옵션 순번.
	Stepper,    // - / + 버튼으로 Step 호출함.
	Action,     // 버튼으로 Invoke 호출함.
	Readout     // Text의 현재 값 표시함.
};

/** 값을 소유한 기능이 등록하는 조작 항목. */
struct CXMR_API FCXMRTunable
{
	/** 저장 키. 기능별 ID는 유지해야 함. */
	FName Id;
	FText Category;
	FText Label;
	/** 항목 설명용 도움말. */
	FText Help;
	FText Unit;
	ECXMRTunableKind Kind = ECXMRTunableKind::Float;

	float Min = 0.0f;
	float Max = 1.0f;
	/** 숫자 조정 간격. */
	float Delta = 0.01f;
	float Default = 0.0f;
	/** 변경 후 저장하고 다음 등록 때 복원함. */
	bool bPersist = false;

	TArray<FText> Options;                  // 선택 목록
	TFunction<float()> Get;                 // 숫자 읽기
	TFunction<void(float)> Set;             // 숫자 변경
	TFunction<void(float)> Step;            // 순환 방향은 +1 또는 -1.
	TFunction<void()> Invoke;               // 버튼 동작
	TFunction<FText()> Text;                // 상태 표시. 순환 버튼 옆에도 표시함.
	TFunction<bool()> IsEnabled;            // false면 조작 비활성화함.

	/** 소유자가 살아 있는 동안만 항목 유지함. */
	TWeakObjectPtr<const UObject> Owner;
};

DECLARE_MULTICAST_DELEGATE(FCXMROnTunablesChanged);

UCLASS()
class CXMR_API UCXMRTuningSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** 항목 등록하고 저장값 적용함. ID가 겹치면 false. */
	bool Register(FCXMRTunable Tunable);

	/** EndPlay에서 소유자 항목 모두 해제함. */
	void UnregisterOwner(const UObject* Owner);

	/** 등록 순서대로 반환함. 등록이 바뀌면 포인터를 다시 받아야 함. */
	TArray<const FCXMRTunable*> GetTunables() const;

	/** 범위 보정 후 적용함. 슬라이더 조작 중에는 bSave=false. */
	bool ApplyValue(FName Id, float Value, bool bSave);

	/** 기본값으로 되돌리고 저장값 지움. */
	bool ResetToDefault(FName Id);

	/** IsEnabled가 false면 비활성화함. 없는 ID는 활성 상태로 처리함. */
	bool IsTunableEnabled(FName Id) const;

	/** 등록 변경 시 열린 창 갱신 요청함. */
	FCXMROnTunablesChanged OnTunablesChanged;

	// BP·Python 호출

	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning") TArray<FName> GetTunableIds() const;
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning") float GetTunableValue(FName Id) const;
	/** 값 적용 후 저장함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning", meta=(ToolTip="Applies and saves, as a committed edit in the window would.")) bool SetTunableValue(FName Id, float Value);
	/** 버튼 동작이나 순환 조작 실행함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning", meta=(ToolTip="Runs an Action row, or steps a Stepper row by Direction (+1 / -1).")) bool InvokeTunable(FName Id, float Direction = 1.0f);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning") FString GetTunableText(FName Id) const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Tuning") FString GetTuningFilePath() const;

private:
	const FCXMRTunable* Find(FName Id) const;
	void LoadFromDisk();
	void SaveToDisk() const;

	TArray<FCXMRTunable> Tunables;
	TMap<FName, float> SavedValues;
};
