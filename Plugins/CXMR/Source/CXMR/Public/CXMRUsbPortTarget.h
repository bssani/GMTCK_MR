// Copyright GMTCK CX.

// 생성 손 표면이 닿으면 테두리와 소리로 반응함. 삽입 확인은 아님.
// 포트 중심은 액터 위치. 화살표는 포트 바깥으로 맞춤.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "InputCoreTypes.h"
#include "CXMRUsbPortTarget.generated.h"

class UArrowComponent;
class UAudioComponent;
class UCXMRTuningSubsystem;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class USoundBase;
class UStaticMesh;
class UStaticMeshComponent;
class USoundAttenuation;

UENUM(BlueprintType)
enum class ECXMRPortState : uint8
{
	Idle = 0,
	Near = 3 UMETA(ToolTip="Tracked hand surface is touching the port."),
};

UCLASS(DisplayName = "CXMR USB Port Target")
class CXMR_API ACXMRUsbPortTarget : public AActor
{
	GENERATED_BODY()

public:
	ACXMRUsbPortTarget();

	/** 손 표면과 포트 사이의 접촉 여유(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Contact", meta=(ClampMin="0.0", ToolTip="Contact margin beyond the generated hand surface, cm.")) float HandContactDistance = 0.25f;
	/** 접촉 해제 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Contact", meta=(ClampMin="0.0", ToolTip="Release margin beyond the hand surface, cm. Kept larger than the contact margin.")) float HandReleaseDistance = 0.75f;
	UFUNCTION(BlueprintPure, Category = "CXMR|Port") float GetHandContactDistance() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Port") float GetHandReleaseDistance() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Port", meta=(ToolTip="Distance from the tracked hand surface, cm. Negative without a current observation.")) float GetHandDistance() const { return LastDistance; }

	UFUNCTION(BlueprintCallable, Category = "CXMR|Port") void SetContactEnabled(bool bEnabled);
	UFUNCTION(BlueprintPure, Category = "CXMR|Port") bool IsContactEnabled() const { return bContactEnabled; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void OnConstruction(const FTransform& Transform) override;

	/** 로그용 이름. 비우면 액터 이름 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta=(ToolTip="Name used in the log, e.g. \"USB-C left\". Empty = the actor label.")) FString Label;

	/** 메시는 유지하고 포트 축만 로컬 기준으로 돌림. 화살표를 포트 밖으로 맞춤. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta=(ToolTip="Adjust the insertion axis in mesh space without moving the indicator mesh.")) FRotator AxisTurn = FRotator::ZeroRotator;

	/** 포트 테두리 안쪽 크기(cm). X는 액터 Y, Y는 액터 Z. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta=(ToolTip="Inner size of the frame around the opening, cm: X across the port (actor Y), Y up the port (actor Z).")) FVector2D FrameSize = FVector2D(1.4f, 0.9f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ClampMin = "0.05"))
	float FrameThickness = 0.15f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port") FLinearColor IdleColor     = FLinearColor(0.35f, 0.35f, 0.38f, 1.0f);

	/** 끄면 손이 닿을 때만 원래 메시 표시함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta=(ToolTip="Show the original port mesh while idle. The feedback frame only appears on hand contact.")) bool bShowWhenIdle = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Feedback") FLinearColor NearColor = FLinearColor(0.05f, 0.8f, 1.f, 1.f);
	/** 손 접촉 안내음. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Feedback", meta=(ToolTip="Sound played once on tracked hand contact. Does not confirm insertion.")) TObjectPtr<USoundBase> NearSound;
	/** 표면 앞에 표시를 띄울 거리(cm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Feedback", meta = (ToolTip="Distance to push feedback out from the surface, in cm.", ClampMin = "0.0")) float FeedbackPushOut = 0.05f;

	/** 손 추적이 잠깐 끊겼을 때 유지할 시간(초). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port", meta = (ToolTip="Seconds to keep feedback during a brief tracking loss.", ClampMin = "0.0")) float TrackingGraceSeconds = 0.15f;
	/** 접촉 중 초당 밝기 변화 횟수. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Feedback", meta = (ToolTip="Pulses per second during hand contact.", ClampMin = "0.1"))
	float PulseRate = 2.5f;

	// 접촉 반응

	/** 접촉 밝기. 재질의 Glow 사용함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Feedback", meta = (ToolTip="Contact glow strength. Requires the material Glow parameter.", ClampMin = "0.1"))
	float GlowStrength = 3.0f;

	/** 접촉 순간 확대 배율. 1이면 실제 크기 유지함. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Feedback", meta = (ToolTip="Feedback frame swell on contact. 1 keeps the actual size.", ClampMin = "1.0", ClampMax = "4.0"))
	float PopScale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Feedback", meta = (ClampMin = "0.05"))
	float PopSeconds = 0.35f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Feedback", meta = (ClampMin = "0.0"))
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

	/** 월드 기준 포트 바깥 방향 반환함. */
	UFUNCTION(BlueprintPure, Category = "CXMR|Port", meta=(ToolTip="The way out of the port in world space — the axis of the opening. Follows the mesh, see bAxisFromMesh.")) FVector GetPortAxis() const;

	/** 실행 중 바뀐 메시·테두리·색상 다시 적용함. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Port", meta=(ToolTip="Apply the frame, port mesh and feedback colors after changes during play.")) void RefreshMarker();

	// 애셋

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets") TObjectPtr<UStaticMesh> BarMesh;

	/** 테두리 재질. Color 필수, Glow 선택. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|Port|Assets", meta=(ToolTip="Frame material. Requires Color; Glow is optional. Defaults to unlit.")) TObjectPtr<UMaterialInterface> BarMaterial;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

private:
	void TickHandContact(float DeltaSeconds);
	void RegisterHandContactTunables();
	bool bContactEnabled = true;
	EControllerHand LastContactHand = EControllerHand::AnyHand;
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
	bool IsNearestPort(float Distance) const;


	/** 디버그가 켜져 있으면 접촉 거리 표시함. */
	void DrawDebug(const FVector& Point, bool bHaveHand) const;

	/** 포트 설정 등록함. 처음 포트가 소유하고 없어지면 다른 포트에 넘김. */
	void RegisterTunables();
	UCXMRTuningSubsystem* GetTuning() const;

	/** 가장 가까운 포트의 접촉 상태 표시함. */
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

	UPROPERTY(Transient) TObjectPtr<UMaterialInstanceDynamic> FrameMaterial;
	UPROPERTY(Transient) TObjectPtr<USoundAttenuation> FeedbackAttenuation;
	UPROPERTY(Transient) TObjectPtr<UAudioComponent> FeedbackAudio;

	ECXMRPortState State = ECXMRPortState::Idle;

	/** 접근 반응 진행 시간(초). */
	float PulseTime = 0.0f;

	/** 확대 반응 진행 시간(초). 반응 전에는 음수. */
	float PopElapsed = -1.0f;

	/** 손 표면 거리(cm). 현재 관측이 없으면 음수. */
	float LastDistance = -1.0f;
	bool bHaveTrackedPose = false;
	bool bNearSoundArmed = true;
	float TrackingLostSeconds = 0.f;
};
