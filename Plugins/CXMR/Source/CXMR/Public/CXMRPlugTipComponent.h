// Copyright GMTCK CX.

// 손 관절로 실물 플러그의 끝과 방향 추정함.

// 손 마스크는 추적 지연이 있어 사용하지 않음.
// 가림은 런타임 깊이 합성에서 처리함.
// 플러그 끝 추정용이며 실제 삽입은 측정하지 않음.

// 관절 방향 대신 위치로 잡는 축 계산함.

// 헤드셋 없이 Preview로 손 자세 확인함. PlugTipDebug로 추정 끝 표시함.

// 추정 위치만 담당함. 손 메시와 포즈 보정은 별도로 처리함.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "InputCoreTypes.h"
#include "CXMRPlugTipComponent.generated.h"

class UCXMRTuningSubsystem;

UCLASS(ClassGroup = (CXMR), meta = (BlueprintSpawnableComponent), DisplayName = "CXMR Plug Tip")
class CXMR_API UCXMRPlugTipComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UCXMRPlugTipComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	/** 플러그 끝과 방향 반환함. 손 추적이 없으면 false. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Plug Tip", meta=(ToolTip="Front end of the held plug's metal tip and the direction it points, from the tracker (or the preview pose), PlugOffset and PlugTipReach. False = the plug hand is not tracked.")) bool GetPlugTip(FVector& OutTipLocation, FVector& OutDirection) const;

	/** 손 추적 유효 여부. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Plug Tip", meta=(ToolTip="True while the plug hand is tracked, so GetPlugTip has something to answer with.")) bool IsPlugHandTracked() const;

	/** 플러그를 잡는 손 선택함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Plug Tip", meta=(ToolTip="Left or Right. The console sits to the driver's right in a left-hand-drive car.")) EControllerHand PlugHand = EControllerHand::Right;

	/** 핀치 위치에서 금속 끝까지 거리(cm). USB-C 기본값은 1.75cm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Plug Tip", meta = (ToolTip="cm from the pinch point to the plug's metal tip, along the index finger. A USB-C cable end is about 1.75.", ClampMin = "0.0", ClampMax = "15.0"))
	float PlugTipReach = 1.75f;

	/** 엄지·검지 사이가 원점, X는 검지 방향, Z는 엄지 방향. 손을 잡는 방식에 맞춰 조정함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Plug Tip", meta=(ToolTip="Grip-local offset: origin between thumb and index tips, X along the index, Z toward the thumb.")) FTransform PlugOffset = FTransform::Identity;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	UCXMRTuningSubsystem* GetTuning() const;

	/** 플러그 거리와 표시 설정 등록함. */
	void RegisterTunables();

	/** 플러그 추적 상태와 미추적 이유 표시함. */
	FText DescribeState() const;

	/** 추적 관절이나 미리보기 좌표 반환함. */
	bool GetJoints(bool bPreview, TArray<FVector>& OutPositions) const;

	/** 잡는 좌표계에 PlugOffset 적용함. 끝 계산과 디버그 표시에서 공유함. */
	bool ComputeGrip(const TArray<FVector>& Positions, FTransform& OutGrip) const;

	void DrawPlugTipDebug() const;
};
