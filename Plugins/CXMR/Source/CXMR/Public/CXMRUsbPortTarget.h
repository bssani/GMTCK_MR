// Copyright GMTCK CX.

// 차량 USB의 접근·근접·정렬 상태 표시함.

// 포트 중심에 배치하고 화살표는 바깥으로 향하게 함.

// IndicatorOffset과 AxisTurn으로 실제 포트 방향에 맞춤.

// 포트 중심은 액터 위치임.

// 손 관절로 추정한 플러그 끝 사용함.

// 포트 크기의 테두리와 위치 기반 소리로 반응함.

// Idle은 대기. bShowWhenIdle이 꺼져 있으면 숨김.
// Approach는 가까워질수록 테두리가 밝아짐.
// Near는 방향과 관계없이 가까워지면 반응함.
// Aligned는 거리·각도를 일정 시간 유지하면 반응함.
// 안내 빔과 확대 효과는 기본값에서 꺼둠.
// 가장 가까운 포트만 반응함. Unlit으로 카메라 배경에서도 보이게 함.

// 거리와 각도 판정 기준은 포트 액터임. 디버그 표시로 범위 확인함.

// 손 추적 정확도에 맞춰 여유를 둠. 정렬 반응은 삽입 확인이 아님.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CXMRUsbPortTarget.generated.h"

class UArrowComponent;
class UCXMRTuningSubsystem;
class UCXMRPlugTipComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class USoundAttenuation;

UENUM(BlueprintType)
enum class ECXMRPortState : uint8
{
	Idle,
	/*~ 가장 가까운 포트가 반응할 거리 안에 있음. */
	Approach UMETA(ToolTip="The plug is near and this is the nearest port."),
	/*~ 포트의 거리와 방향에 맞음. */
	Aligned UMETA(ToolTip="The plug is lined up with this port."),
	/*~ 방향과 관계없이 근접 반응함. 기존 enum 순번 유지함. */
	Near UMETA(ToolTip="Near feedback regardless of plug orientation."),
};

UCLASS(DisplayName = "CXMR USB Port Target")
class CXMR_API ACXMRUsbPortTarget : public AActor
{
	GENERATED_BODY()

public:
	ACXMRUsbPortTarget();

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** 로그용 이름. 비우면 액터 이름 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta=(ToolTip="Name used in the log, e.g. \"USB-C left\". Empty = the actor label.")) FString Label;

	/** 정렬 진입 거리(cm). 메시가 아니라 액터 위치 기준. 전역 설정으로 덮어쓸 수 있음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ToolTip="Plug tip within this distance of the port centre, and within MaxAngle, lines the port up. cm. Measured from THIS ACTOR's location, never from the marker mesh. The tuning window overrides it for every port.", ClampMin = "0.1"))
	float EnterDistance = 1.5f;

	/** 정렬 해제 거리(cm). 진입 거리보다 크게 둠. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ToolTip="Stays lined up until the tip is farther than this. A single threshold flickers at the edge. cm.", ClampMin = "0.1"))
	float ExitDistance = 3.0f;

	/** 허용 방향 오차(deg). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ToolTip="Largest angle between the plug and the port axis that still counts as straight. Degrees.", ClampMin = "1.0", ClampMax = "80.0"))
	float MaxAngle = 25.0f;

	/** 메시는 유지하고 포트 축만 로컬 기준으로 돌림. 화살표를 포트 밖으로 맞춤. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta=(ToolTip="Adjust the insertion axis in mesh space without moving the indicator mesh.")) FRotator AxisTurn = FRotator::ZeroRotator;

	/** 포트 테두리 안쪽 크기(cm). X는 액터 Y, Y는 액터 Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta=(ToolTip="Inner size of the frame around the opening, cm: X across the port (actor Y), Y up the port (actor Z).")) FVector2D FrameSize = FVector2D(1.4f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "0.05"))
	float FrameThickness = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FLinearColor IdleColor     = FLinearColor(0.35f, 0.35f, 0.38f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FLinearColor ApproachColor = FLinearColor(1.00f, 0.72f, 0.05f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FLinearColor AlignedColor  = FLinearColor(0.10f, 0.95f, 0.25f, 1.0f);

	/** 끄면 접근이나 정렬 중에만 표시함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta=(ToolTip="Off = the marker appears only while the plug approaches or lines up, leaving the CAD untouched the rest of the time.")) bool bShowWhenIdle = true;

	// 접근

	/** 접근 반응 시작 거리(cm). 0이면 접근 단계 생략함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ToolTip="The nearest port starts guiding once the plug tip is this close, cm. 0 = no approach stage.", ClampMin = "0.0"))
	float ApproachDistance = 12.0f;

	/** 방향과 관계없이 근접 반응할 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ToolTip="Distance for near feedback regardless of orientation, in cm.", ClampMin = "0.1")) float NearDistance = 4.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach") FLinearColor NearColor = FLinearColor(0.05f, 0.8f, 1.f, 1.f);
	/** 가까이 왔다는 안내음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta=(ToolTip="Sound played when the plug comes near. Does not confirm contact.")) TObjectPtr<USoundBase> NearSound;
	/** 표면 앞에 표시를 띄울 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ToolTip="Distance to push feedback out from the surface, in cm.", ClampMin = "0.0")) float FeedbackPushOut = 0.05f;

	/** 손 추적이 잠깐 끊겼을 때 유지할 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ToolTip="Seconds to keep feedback during a brief tracking loss.", ClampMin = "0.0")) float TrackingGraceSeconds = 0.15f;
	/** 정렬 조건을 유지해야 할 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ToolTip="Seconds the alignment conditions must hold before confirmation.", ClampMin = "0.0")) float AlignmentDwellSeconds = 0.15f;

	/** 접근과 정렬 중 삽입 축에 안내 빔 표시함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta=(ToolTip="A beam standing out of the opening along the insertion axis while the plug approaches (and while lined up).")) bool bShowGuide = false;

	/** GuideGap부터 시작할 빔 길이(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ToolTip="cm of beam, starting GuideGap out from the opening.", ClampMin = "1.0"))
	float GuideLength = 15.0f;

	/** 포트 앞에 비울 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ToolTip="cm left clear in front of the opening, so the beam never covers the marker or the plug's last stretch.", ClampMin = "0.0"))
	float GuideGap = 3.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ClampMin = "0.05"))
	float GuideDiameter = 0.35f;

	/** 접근 중 초당 밝기 변화 횟수. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Approach", meta = (ToolTip="Pulses per second while approaching.", ClampMin = "0.1"))
	float PulseRate = 2.5f;

	// 정렬

	/** 정렬 상태의 밝기. 재질의 Glow 파라미터 필요함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned", meta = (ToolTip="Brightness of the self-lit colours when lined up; approaching pulses up to it. Needs BarMaterial's \"Glow\".", ClampMin = "0.1"))
	float GlowStrength = 3.0f;

	/** 정렬 순간 확대 배율. 1이면 확대하지 않음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned", meta = (ToolTip="How far the marker swells for a moment on lining up. 1 = no pop.", ClampMin = "1.0", ClampMax = "4.0"))
	float PopScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned", meta = (ClampMin = "0.05"))
	float PopSeconds = 0.35f;

	/** 포트 위치에서 정렬 안내음 한 번 재생함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned", meta=(ToolTip="Alignment sound played once at the port. Does not confirm insertion.")) TObjectPtr<USoundBase> AlignedSound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Aligned", meta = (ClampMin = "0.0"))
	float SoundVolume = 1.0f;

	// 표시 메시

	/** 포트 표시용 메시. 비우면 테두리만 사용함. 중심은 액터 위치 유지함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Custom Mesh", meta=(ToolTip="Optional port indicator mesh. The actor location remains the port centre.")) TObjectPtr<UStaticMesh> IndicatorMesh;
	/** 원래 USB 형상과 재질 유지하고 테두리만 반응함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Custom Mesh", meta=(ToolTip="Preserve the USB geometry and materials; show state through the outer frame.")) bool bKeepMeshMaterialsWhenActive = true;

	/** 메시의 위치와 방향 조정함. bAxisFromMesh가 켜져 있으면 회전이 포트 축에도 반영됨. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Custom Mesh", meta=(ToolTip="Indicator transform relative to the actor. Its rotation also steers the axis when bAxisFromMesh is enabled.")) FTransform IndicatorOffset = FTransform::Identity;

	/** 메시 회전을 삽입 축에 반영함. 위치는 반영하지 않으므로 피벗을 포트 중심에 맞춰야 함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Custom Mesh", meta=(ToolTip="Include the mesh rotation in the insertion axis. Keep the mesh pivot at the port centre.")) bool bAxisFromMesh = true;

	/** 대기 중 원래 메시 재질 유지함. 끄면 IdleColor 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Custom Mesh", meta=(ToolTip="On = the mesh keeps its own materials while idle. Off = it shows IdleColor like the frame.")) bool bKeepMeshMaterialsWhenIdle = true;

	UFUNCTION(BlueprintPure, Category = "CXMR|Port") ECXMRPortState GetPortState() const { return State; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Port") bool IsAligned() const { return State == ECXMRPortState::Aligned; }

	/** 월드 기준 포트 바깥 방향 반환함. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Port", meta=(ToolTip="The way out of the port in world space — the axis the plug is judged against. Follows the mesh, see bAxisFromMesh.")) FVector GetPortAxis() const;

	// 판정·디버그·상태 표시 모두 같은 설정값 사용함. 전역 값이 없으면 액터 기본값 사용함.

	UFUNCTION(BlueprintPure, Category = "CXMR|Port") float GetEnterDistance() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Port") float GetExitDistance() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Port") float GetMaxAngle() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Port") float GetApproachDistance() const;

	/** 포트와 추정 끝 거리(cm). 추적 유예가 지나면 음수. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Port", meta=(ToolTip="Estimated tip-to-port distance in cm. Uses the last pose during tracking grace; negative afterwards.")) float GetPlugDistance() const { return LastDistance; }

	/** 삽입 축과 추정 방향 각도(deg). 추적 유예가 지나면 음수. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Port", meta=(ToolTip="Estimated plug angle to the insertion axis in degrees. Negative after tracking grace.")) float GetPlugAngle() const { return LastAngleDeg; }
	UFUNCTION(BlueprintPure, Category = "CXMR|Port") float GetApproachAmount() const { return ApproachAmount; }

	/** 실행 중 바뀐 메시·테두리·색상 다시 적용함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Port", meta=(ToolTip="Applies the frame, guide, IndicatorMesh and colours again after changing them during play.")) void RefreshMarker();

	// 애셋

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets") TObjectPtr<UStaticMesh> BarMesh;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets") TObjectPtr<UStaticMesh> GuideMesh;

	/** 테두리와 빔 재질. Color는 필수, Glow는 선택. 기본값은 Unlit. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets", meta=(ToolTip="Frame and guide material. Requires Color; Glow is optional. Defaults to unlit.")) TObjectPtr<UMaterialInterface> BarMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	/** 메시 회전과 AxisTurn을 합친 포트 방향. */
	FQuat GetPortTurn() const;

	/** 포트 기준으로 표시 배치함. 확대는 포트 중심 기준. */
	void LayoutFrame(float Swell = 1.0f);

	/** 현재 상태의 색상·가시성·재질 적용함. */
	void ApplyState();
	void SetState(ECXMRPortState NewState);

	/** 매 프레임 밝기와 확대 반응 갱신함. */
	void Animate(float DeltaSeconds);
	void PlayFeedbackSound(USoundBase* Sound, float Volume, float Pitch);

	/** 가장 가까운 포트인지 확인함. 반응 중이면 작은 우선권을 둠. */
	bool IsNearestPort(const FVector& Tip, float Distance) const;

	UCXMRPlugTipComponent* FindPlugTip();

	/** 디버그가 켜져 있으면 거리·각도·연결선 표시함. */
	void DrawDebug(const FVector& Tip, bool bHavePlug) const;

	/** 포트 설정 등록함. 처음 포트가 소유하고 없어지면 다른 포트에 넘김. */
	void RegisterTunables();
	UCXMRTuningSubsystem* GetTuning() const;

	/** 가장 가까운 포트의 거리와 각도 표시함. */
	FText DescribeNearest() const;

	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<USceneComponent> PortRoot;

	/** 에디터 화살표. 포트 바깥을 향해야 함. */
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (ToolTip="Editor only. Must point out of the port, toward the person reaching for it.", AllowPrivateAccess = "true")) TObjectPtr<UArrowComponent> OutOfPort;

	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> BarTop;
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> BarBottom;
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> BarLeft;
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> BarRight;

	/** IndicatorMesh 표시함. 비어 있으면 숨김. */
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (ToolTip="Shows IndicatorMesh. Hidden while it is empty.", AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> Indicator;

	/** 삽입 축 방향 안내 빔. */
	UPROPERTY(VisibleAnywhere, Category = "CXMR|Port", meta = (ToolTip="The approach beam along the insertion axis.", AllowPrivateAccess = "true")) TObjectPtr<UStaticMeshComponent> GuideBeam;

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FrameMaterial;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> FeedbackAttenuation;

	TWeakObjectPtr<UCXMRPlugTipComponent> PlugTip;
	ECXMRPortState State = ECXMRPortState::Idle;

	/** 접근 반응 진행 시간(초). */
	float PulseTime = 0.0f;

	/** 확대 반응 진행 시간(초). 반응 전에는 음수. */
	float PopElapsed = -1.0f;

	/** 디버그와 상태 표시용 마지막 측정값. 플러그가 없으면 음수. */
	float LastDistance = -1.0f;
	float LastAngleDeg = -1.0f;
	float ApproachAmount = 0.f;
	FVector LastTrackedTip = FVector::ZeroVector;
	FVector LastTrackedDirection = FVector::ForwardVector;
	bool bHaveTrackedPose = false;
	bool bNearSoundArmed = true;
	float TrackingLostSeconds = 0.f;
	float AlignmentElapsed = 0.f;
};
