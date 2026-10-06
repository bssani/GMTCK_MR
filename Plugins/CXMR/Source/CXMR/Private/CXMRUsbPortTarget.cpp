// Copyright GMTCK CX.

#include "CXMRUsbPortTarget.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRPlugTipComponent.h"

#include "Components/ArrowComponent.h"
#include "Components/StaticMeshComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundAttenuation.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRPort, Log, All);

#define LOCTEXT_NAMESPACE "CXMRPort"

// 전역 CVar로 모든 포트에 같은 설정 적용함. 음수면 각 포트의 기본값 사용함.

static TAutoConsoleVariable<int32> CVarPortDebug(
	TEXT("CXMR.Port.Debug"),
	0,
	TEXT("Draw what a USB port actually judges: the ball of Enter Distance around the PORT (not around the marker mesh, ")
	TEXT("which may sit anywhere), the fainter ball where it lets go again, the Max Angle cone along the insertion ")
	TEXT("axis, and a line to the plug tip once it is within Approach Distance."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPortEnter(
	TEXT("CXMR.Port.EnterDistance"),
	-1.f,
	TEXT("cm from the port centre that lines the plug up, for every port. Below zero = each port keeps its own."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPortExit(
	TEXT("CXMR.Port.ExitDistance"),
	-1.f,
	TEXT("cm the plug must pass to let a lined-up port go again. Below zero = each port keeps its own."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPortMaxAngle(
	TEXT("CXMR.Port.MaxAngle"),
	-1.f,
	TEXT("Degrees off the port axis that still count as straight. Below zero = each port keeps its own."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPortApproach(
	TEXT("CXMR.Port.ApproachDistance"),
	-1.f,
	TEXT("cm at which the nearest port starts guiding. Below zero = each port keeps its own."),
	ECVF_Default);

static TAutoConsoleVariable<float> CVarPortNear(TEXT("CXMR.Port.NearDistance"), -1.f,
	TEXT("USB proximity feedback distance in cm. Below zero uses each port's distance."), ECVF_Default);

namespace
{
	/** 엔진 기본 큐브와 원기둥 크기는 100cm. */
	constexpr float CubeCm = 100.f;

	/** 정렬 경계에서 깜빡이지 않게 해제 각도에 여유를 둠. */
	constexpr float AngleHysteresis = 10.f;

	/** 접근 해제 거리에 더할 여유(cm). */
	constexpr float PortApproachHysteresis = 2.f;

	/** 옆 포트로 반응이 자주 바뀌지 않게 거리 차이를 둠(cm). */
	constexpr float PortNearestLead = 0.5f;

	/** 대기 상태와 접근 반응의 최소 밝기. */
	constexpr float PortRestGlow = 1.f;
}

ACXMRUsbPortTarget::ACXMRUsbPortTarget()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.TickGroup = TG_PostUpdateWork;

	PortRoot = CreateDefaultSubobject<USceneComponent>(TEXT("PortRoot"));
	SetRootComponent(PortRoot);

	OutOfPort = CreateDefaultSubobject<UArrowComponent>(TEXT("OutOfPort"));
	OutOfPort->SetupAttachment(PortRoot);
	OutOfPort->ArrowSize = 0.1f;
	OutOfPort->SetHiddenInGame(true);

	auto MakeShape = [this](const TCHAR* Name)
	{
		UStaticMeshComponent* Shape = CreateDefaultSubobject<UStaticMeshComponent>(Name);
		Shape->SetupAttachment(PortRoot);
		Shape->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Shape->SetCastShadow(false);
		Shape->SetCanEverAffectNavigation(false);
		return Shape;
	};
	BarTop    = MakeShape(TEXT("BarTop"));
	BarBottom = MakeShape(TEXT("BarBottom"));
	BarLeft   = MakeShape(TEXT("BarLeft"));
	BarRight  = MakeShape(TEXT("BarRight"));
	Indicator = MakeShape(TEXT("Indicator"));
	GuideBeam = MakeShape(TEXT("GuideBeam"));

	// 엔진과 플러그인 애셋만 사용함.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderFinder(TEXT("/Engine/BasicShapes/Cylinder"));
	// 카메라 배경에서도 보이도록 Unlit 재질 사용함.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitFinder(TEXT("/CXMR/Core/Materials/M_CXMRUnlitColor"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<USoundBase> SoundFinder(TEXT("/Engine/VREditor/Sounds/UI/Object_Snaps_To_Grid"));
	if (CubeFinder.Succeeded())     { BarMesh   = CubeFinder.Object; }
	if (CylinderFinder.Succeeded()) { GuideMesh = CylinderFinder.Object; }
	if (UnlitFinder.Succeeded())    { BarMaterial = UnlitFinder.Object; }
	else if (BasicFinder.Succeeded()) { BarMaterial = BasicFinder.Object; }
	if (SoundFinder.Succeeded())    { AlignedSound = SoundFinder.Object; NearSound = SoundFinder.Object; }
}

void ACXMRUsbPortTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 에디터에서도 크기와 오프셋 변경을 반영함.
	LayoutFrame();
	ApplyState();
}

float ACXMRUsbPortTarget::GetEnterDistance() const
{
	const float Tuned = CVarPortEnter.GetValueOnGameThread();
	return Tuned >= 0.f ? Tuned : EnterDistance;
}

float ACXMRUsbPortTarget::GetExitDistance() const
{
	const float Tuned = CVarPortExit.GetValueOnGameThread();
	// 해제 거리는 진입 거리보다 크게 유지함.

	return FMath::Max(Tuned >= 0.f ? Tuned : ExitDistance, GetEnterDistance());
}

float ACXMRUsbPortTarget::GetMaxAngle() const
{
	const float Tuned = CVarPortMaxAngle.GetValueOnGameThread();
	return Tuned >= 0.f ? Tuned : MaxAngle;
}

float ACXMRUsbPortTarget::GetApproachDistance() const
{
	const float Tuned = CVarPortApproach.GetValueOnGameThread();
	return Tuned >= 0.f ? Tuned : ApproachDistance;
}

FQuat ACXMRUsbPortTarget::GetPortTurn() const
{
	// 오프셋의 회전만 사용함. 거리 기준은 액터 위치임.

	const FQuat MeshTurn = (bAxisFromMesh && IndicatorMesh) ? IndicatorOffset.GetRotation() : FQuat::Identity;
	// AxisTurn은 메시 로컬 기준으로 먼저 적용함.
	return MeshTurn * AxisTurn.Quaternion();
}

FVector ACXMRUsbPortTarget::GetPortAxis() const
{
	// 액터 스케일이 방향을 바꾸지 않게 회전만 사용함.
	return (GetActorQuat() * GetPortTurn()).GetForwardVector();
}

void ACXMRUsbPortTarget::LayoutFrame(float Swell)
{
	UMaterialInterface* Paint = FrameMaterial ? static_cast<UMaterialInterface*>(FrameMaterial) : BarMaterial.Get();

	// 표시 메시는 자체 오프셋을 쓰고 나머지는 포트 기준으로 배치함.
	const FQuat Turn = GetPortTurn();

	const float T = FrameThickness;
	const float HalfW = FrameSize.X * 0.5f;
	const float HalfH = FrameSize.Y * 0.5f;
	// X는 포트 바깥 방향. 테두리는 표면 앞에 띄움.
	const float X = FMath::Max(T * 0.5f, FeedbackPushOut);

	struct FBarLayout { UStaticMeshComponent* Bar; FVector Location; FVector Size; };
	const FBarLayout Layout[] = {
		{ BarTop,    FVector(X, 0.,  HalfH + T * 0.5f),  FVector(T, FrameSize.X + 2.f * T, T) },
		{ BarBottom, FVector(X, 0., -(HalfH + T * 0.5f)), FVector(T, FrameSize.X + 2.f * T, T) },
		{ BarLeft,   FVector(X, -(HalfW + T * 0.5f), 0.), FVector(T, T, FrameSize.Y) },
		{ BarRight,  FVector(X,  HalfW + T * 0.5f, 0.),  FVector(T, T, FrameSize.Y) },
	};
	for (const FBarLayout& Item : Layout)
	{
		if (!Item.Bar)
		{
			continue;
		}
		Item.Bar->SetStaticMesh(BarMesh);
		Item.Bar->SetMaterial(0, Paint);
		// 확대 효과 기본값은 실제 크기 유지함.
		Item.Bar->SetRelativeLocation(Turn.RotateVector(Item.Location * Swell));
		Item.Bar->SetRelativeRotation(Turn);
		Item.Bar->SetRelativeScale3D(Item.Size * Swell / CubeCm);
	}

	if (Indicator)
	{
		// 실제 포트 메시의 위치와 크기 유지함.
		FTransform Offset = IndicatorOffset;
		Indicator->SetStaticMesh(IndicatorMesh);
		Indicator->SetRelativeTransform(Offset);
	}

	if (OutOfPort)
	{
		OutOfPort->SetRelativeRotation(Turn);
	}

	if (GuideBeam)
	{
		// Z축 원기둥을 X축으로 돌려 포트 밖으로 표시함.
		const FQuat AlongX(FVector::YAxisVector, UE_HALF_PI);
		const float Diameter = GuideDiameter / CubeCm;
		GuideBeam->SetStaticMesh(GuideMesh);
		GuideBeam->SetMaterial(0, Paint);
		GuideBeam->SetRelativeTransform(FTransform(
			Turn * AlongX,
			Turn.RotateVector(FVector(GuideGap + GuideLength * 0.5f, 0., 0.)),
			FVector(Diameter, Diameter, GuideLength / CubeCm)));
	}
}

void ACXMRUsbPortTarget::BeginPlay()
{
	Super::BeginPlay();

	if (Label.IsEmpty())
	{
		Label = GetActorNameOrLabel();
	}

	if (BarMaterial)
	{
		FrameMaterial = UMaterialInstanceDynamic::Create(BarMaterial, this);
		FrameMaterial->SetScalarParameterValue(TEXT("Glow"), PortRestGlow);
	}
	LayoutFrame();
	ApplyState();
	FeedbackAttenuation = NewObject<USoundAttenuation>(this);
	FeedbackAttenuation->Attenuation.bSpatialize = true;
	FeedbackAttenuation->Attenuation.bAttenuate = true;
	FeedbackAttenuation->Attenuation.AttenuationShapeExtents = FVector(25.f);
	FeedbackAttenuation->Attenuation.FalloffDistance = 200.f;
	RegisterTunables();
}

void ACXMRUsbPortTarget::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
		// 기존 포트가 없어지면 다른 포트로 설정 등록을 넘김.
		for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->IsActorBeingDestroyed())
			{
				It->RegisterTunables();
				break;
			}
		}
	}
	Super::EndPlay(Reason);
}

void ACXMRUsbPortTarget::RefreshMarker()
{
	LayoutFrame();
	ApplyState();
}

UCXMRPlugTipComponent* ACXMRUsbPortTarget::FindPlugTip()
{
	// pawn이 나중에 생성되거나 바뀔 수 있어 필요할 때 찾음.
	if (!PlugTip.IsValid())
	{
		if (const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0))
		{
			PlugTip = Pawn->FindComponentByClass<UCXMRPlugTipComponent>();
		}
	}
	return PlugTip.Get();
}

bool ACXMRUsbPortTarget::IsNearestPort(const FVector& Tip, float Distance) const
{
	// 가장 가까운 포트만 반응함. 이미 반응 중인 포트에 작은 우선권을 둠.

	const float Mine = Distance - (State != ECXMRPortState::Idle ? PortNearestLead : 0.f);
	for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
	{
		const ACXMRUsbPortTarget* Other = *It;
		if (Other == this || Other->IsActorBeingDestroyed() || Other->IsHidden())
		{
			continue;
		}
		const float Theirs = FVector::Distance(Tip, Other->GetActorLocation()) - (Other->State != ECXMRPortState::Idle ? PortNearestLead : 0.f);
		if (Theirs < Mine || (Theirs == Mine && Other < this))
		{
			return false;
		}
	}
	return true;
}

void ACXMRUsbPortTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	FVector Tip = FVector::ZeroVector;
	FVector Direction = FVector::ForwardVector;
	const UCXMRPlugTipComponent* PlugTipComponent = FindPlugTip();
	const bool bFreshPlug = PlugTipComponent && PlugTipComponent->GetPlugTip(Tip, Direction)
		&& !Tip.ContainsNaN() && !Direction.ContainsNaN() && Direction.Normalize();
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	if (bFreshPlug)
	{
		LastTrackedTip = Tip;
		LastTrackedDirection = Direction;
		bHaveTrackedPose = true;
		TrackingLostSeconds = 0.f;
	}
	else
	{
		TrackingLostSeconds += Dt;
		AlignmentElapsed = 0.f; // 과거 자세로 새 정렬을 확정하지 않음.
		Tip = LastTrackedTip;
		Direction = LastTrackedDirection;
	}
	const bool bHavePlug = bFreshPlug || (bHaveTrackedPose && TrackingLostSeconds <= FMath::Max(0.f, TrackingGraceSeconds));

	// 반응 여부와 관계없이 거리와 각도 측정함. 삽입 방향은 포트 화살표의 반대임.

	LastDistance = bHavePlug ? FVector::Distance(Tip, GetActorLocation()) : -1.f;
	LastAngleDeg = bHavePlug
		? FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp<float>(FVector::DotProduct(Direction, -GetPortAxis()), -1.f, 1.f)))
		: -1.f;

	ECXMRPortState NewState = ECXMRPortState::Idle;
	const float Near = FMath::Max(GetEnterDistance(), CVarPortNear.GetValueOnGameThread() >= 0.f ? CVarPortNear.GetValueOnGameThread() : NearDistance);
	if (bHavePlug && IsNearestPort(Tip, LastDistance))
	{
		const bool bWasAligned = State == ECXMRPortState::Aligned;
		const float AngleLimit = bWasAligned ? FMath::Min(GetMaxAngle() + AngleHysteresis, 89.f) : GetMaxAngle();
		const float DistanceLimit = bWasAligned ? GetExitDistance() : GetEnterDistance();
		const float Approach = GetApproachDistance();
		const float ApproachLimit = Approach + (State != ECXMRPortState::Idle ? PortApproachHysteresis : 0.f);

		if (LastDistance <= DistanceLimit && LastAngleDeg <= AngleLimit)
		{
			if (bFreshPlug) { AlignmentElapsed += Dt; }
			NewState = bWasAligned || (bFreshPlug && AlignmentElapsed >= FMath::Max(0.f, AlignmentDwellSeconds))
				? ECXMRPortState::Aligned : ECXMRPortState::Near;
		}
		else if (LastDistance <= Near + (State == ECXMRPortState::Near ? 0.5f : 0.f))
		{
			AlignmentElapsed = 0.f;
			NewState = ECXMRPortState::Near;
		}
		else if (Approach > 0.f && LastDistance <= ApproachLimit)
		{
			AlignmentElapsed = 0.f;
			NewState = ECXMRPortState::Approach;
		}
	}
	if (NewState == ECXMRPortState::Idle) { AlignmentElapsed = 0.f; }
	ApproachAmount = NewState != ECXMRPortState::Idle
		? FMath::Clamp(1.f - LastDistance / FMath::Max(0.1f, GetApproachDistance()), 0.f, 1.f) : 0.f;
	if (NewState != ECXMRPortState::Idle)
	{
		// 갱신 순서와 관계없이 한 포트만 반응함.
		for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->IsActorBeingDestroyed() && It->State != ECXMRPortState::Idle)
			{
				It->SetState(ECXMRPortState::Idle);
				It->AlignmentElapsed = 0.f;
				It->ApproachAmount = 0.f;
			}
		}
	}

	SetState(NewState);
	Animate(Dt);
	DrawDebug(Tip, bHavePlug);
}

void ACXMRUsbPortTarget::DrawDebug(const FVector& Tip, bool bHavePlug) const
{
	UWorld* World = GetWorld();
	if (!World || CVarPortDebug.GetValueOnGameThread() == 0)
	{
		return;
	}

	const FVector Centre = GetActorLocation();
	const FColor Colour = State == ECXMRPortState::Aligned  ? FColor(40, 240, 70)
	                    : State == ECXMRPortState::Approach ? FColor(255, 185, 20)
	                                                        : FColor(120, 120, 135);

	// 거리 판정은 메시가 아니라 포트 액터 위치 기준임.
	DrawDebugSphere(World, Centre, GetEnterDistance(), 16, Colour, false, -1.f, SDPG_World, 0.05f);
	// 정렬 해제 거리 표시함.
	DrawDebugSphere(World, Centre, GetExitDistance(), 12, FColor(70, 70, 85), false, -1.f, SDPG_World, 0.03f);
	// 삽입 축 기준 허용 각도 표시함.
	const float Half = FMath::DegreesToRadians(GetMaxAngle());
	DrawDebugCone(World, Centre, GetPortAxis(), GetEnterDistance() * 4.f, Half, Half, 16, Colour, false, -1.f, SDPG_World, 0.05f);

	// 가까운 포트만 연결선 표시함.
	if (bHavePlug && LastDistance >= 0.f && LastDistance <= GetApproachDistance())
	{
		DrawDebugLine(World, Tip, Centre, Colour, false, -1.f, SDPG_World, 0.05f);
	}
}

UCXMRTuningSubsystem* ACXMRUsbPortTarget::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

FText ACXMRUsbPortTarget::DescribeNearest() const
{
	const ACXMRUsbPortTarget* Near = nullptr;
	for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
	{
		if (It->LastDistance >= 0.f && (!Near || It->LastDistance < Near->LastDistance))
		{
			Near = *It;
		}
	}
	if (!Near)
	{
		return LOCTEXT("PlugUntracked", "no plug hand tracked");
	}

	const FText Verdict = Near->State == ECXMRPortState::Aligned  ? LOCTEXT("VerdictAligned", "lined up")
	                    : Near->State == ECXMRPortState::Near     ? LOCTEXT("VerdictNear", "near the opening")
	                    : Near->State == ECXMRPortState::Approach ? LOCTEXT("VerdictApproach", "approaching")
	                                                             : LOCTEXT("VerdictIdle", "not reacting");
	// 손 추적 정확도에 맞춰 소수 한 자리만 표시함.
	return FText::FromString(FString::Printf(TEXT("%s: %.1f cm, %.0f\u00B0 off - %s"),
		*Near->Label, Near->LastDistance, Near->LastAngleDeg, *Verdict.ToString()));
}

void ACXMRUsbPortTarget::RegisterTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return;
	}

	// 처음 등록한 포트가 설정을 소유함. CVar로 모든 포트에 적용함.

	const FText Category = LOCTEXT("CatPort", "USB port");
	auto Make = [this, &Category](FName Id, const FText& Label, ECXMRTunableKind Kind)
	{
		FCXMRTunable Tunable;
		Tunable.Id = Id;
		Tunable.Category = Category;
		Tunable.Label = Label;
		Tunable.Kind = Kind;
		Tunable.Owner = this;
		return Tunable;
	};
	auto WriteTo = [](TAutoConsoleVariable<float>& CVar)
	{
		return [&CVar](float Value) { CVar->Set(Value, ECVF_SetByConsole); };
	};

	{
		FCXMRTunable T = Make("Port.Live", LOCTEXT("Live", "Plug at the nearest port"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeNearest(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.Debug", LOCTEXT("Debug", "Show what the port judges"), ECXMRTunableKind::Bool);
		T.Get = [] { return CVarPortDebug.GetValueOnGameThread() != 0 ? 1.0f : 0.0f; };
		T.Set = [](float Value) { CVarPortDebug->Set(Value > 0.5f ? 1 : 0, ECVF_SetByConsole); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.EnterDistance", LOCTEXT("Enter", "Lines up within"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.2f; T.Max = 10.0f; T.Delta = 0.1f; T.Default = 1.5f; T.bPersist = true;
		T.Get = [this] { return GetEnterDistance(); };
		T.Set = WriteTo(CVarPortEnter);
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.ExitDistance", LOCTEXT("Exit", "Lets go past"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.2f; T.Max = 20.0f; T.Delta = 0.1f; T.Default = 3.0f; T.bPersist = true;
		T.Get = [this] { return GetExitDistance(); };
		T.Set = WriteTo(CVarPortExit);
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.MaxAngle", LOCTEXT("Angle", "Still counts as straight"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("deg", "\u00B0"); T.Min = 5.0f; T.Max = 80.0f; T.Delta = 1.0f; T.Default = 25.0f; T.bPersist = true;
		T.Get = [this] { return GetMaxAngle(); };
		T.Set = WriteTo(CVarPortMaxAngle);
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.ApproachDistance", LOCTEXT("Approach", "Starts guiding at"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 0.0f; T.Max = 40.0f; T.Delta = 0.5f; T.Default = 12.0f; T.bPersist = true;
		T.Get = [this] { return GetApproachDistance(); };
		T.Set = WriteTo(CVarPortApproach);
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.NearDistance", LOCTEXT("NearDistance", "Close-range feedback within"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 1.f; T.Max = 10.f; T.Delta = 0.1f; T.Default = 4.f; T.bPersist = true;
		T.Get = [this] { const float V = CVarPortNear.GetValueOnGameThread(); return V >= 0.f ? V : NearDistance; };
		T.Set = WriteTo(CVarPortNear);
		Tuning->Register(MoveTemp(T));
	}
}

void ACXMRUsbPortTarget::SetState(ECXMRPortState NewState)
{
	if (NewState == State)
	{
		return;
	}
	const bool bWasAligned = State == ECXMRPortState::Aligned;
	State = NewState;

	if (State == ECXMRPortState::Aligned)
	{
		UE_LOG(LogCXMRPort, Log, TEXT("Port '%s' ALIGNED (plug lined up)"), *Label);
		PopElapsed = 0.f;
		if (AlignedSound)
		{
			PlayFeedbackSound(AlignedSound, SoundVolume, 1.2f);
		}
	}
	else if (bWasAligned)
	{
		UE_LOG(LogCXMRPort, Log, TEXT("Port '%s' released"), *Label);
	}
	if (State == ECXMRPortState::Approach)
	{
		PulseTime = 0.f;
	}
	if (State == ECXMRPortState::Near && bNearSoundArmed)
	{
		bNearSoundArmed = false;
		PopElapsed = 0.f;
		PlayFeedbackSound(NearSound, SoundVolume * 0.35f, 0.8f);
	}
	if (State == ECXMRPortState::Idle)
	{
		bNearSoundArmed = true;
		PopElapsed = -1.f;
		LayoutFrame();
	}
	ApplyState();
}

void ACXMRUsbPortTarget::ApplyState()
{
	const bool bActive = State != ECXMRPortState::Idle;
	if (FrameMaterial)
	{
		const FLinearColor Color = State == ECXMRPortState::Aligned ? AlignedColor : (State == ECXMRPortState::Near ? NearColor : (bActive ? ApproachColor : IdleColor));
		FrameMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	const bool bVisible = bActive || bShowWhenIdle;
	const bool bCustomMesh = IndicatorMesh != nullptr;

	for (UStaticMeshComponent* Bar : { BarTop.Get(), BarBottom.Get(), BarLeft.Get(), BarRight.Get() })
	{
		if (Bar)
		{
			Bar->SetVisibility(bVisible && (!bCustomMesh || bActive));
		}
	}

	if (Indicator)
	{
		Indicator->SetVisibility(bVisible && bCustomMesh);
		if (bCustomMesh)
		{
			// 원래 재질은 유지하고 테두리만 반응함.
			const bool bOwnMaterials = bActive ? bKeepMeshMaterialsWhenActive : bKeepMeshMaterialsWhenIdle;
			UMaterialInterface* Paint = FrameMaterial ? static_cast<UMaterialInterface*>(FrameMaterial) : BarMaterial.Get();
			for (int32 Slot = 0; Slot < Indicator->GetNumMaterials(); ++Slot)
			{
				Indicator->SetMaterial(Slot, bOwnMaterials ? nullptr : Paint);
			}
		}
	}

	if (GuideBeam)
	{
		GuideBeam->SetVisibility(bShowGuide && bActive);
	}
}

void ACXMRUsbPortTarget::Animate(float DeltaSeconds)
{
	if (FrameMaterial)
	{
		float Glow = PortRestGlow;
		if (State == ECXMRPortState::Approach)
		{
			PulseTime += DeltaSeconds;
			const float Wave = 0.5f + 0.5f * FMath::Sin(UE_TWO_PI * PulseRate * PulseTime);
			Glow = FMath::Lerp(PortRestGlow, GlowStrength, FMath::Clamp(ApproachAmount * 0.8f + Wave * 0.2f, 0.f, 1.f));
		}
		else if (State == ECXMRPortState::Near)
		{
			PulseTime += DeltaSeconds;
			Glow = GlowStrength * (0.9f + 0.1f * FMath::Sin(UE_TWO_PI * PulseRate * PulseTime));
		}
		else if (State == ECXMRPortState::Aligned)
		{
			Glow = GlowStrength;
		}
		FrameMaterial->SetScalarParameterValue(TEXT("Glow"), Glow);
	}

	if (PopElapsed >= 0.f)
	{
		PopElapsed += DeltaSeconds;
		const float Alpha = FMath::Clamp(PopElapsed / FMath::Max(0.05f, PopSeconds), 0.f, 1.f);
		// 확대 후 원래 크기인 1로 돌아감.
		LayoutFrame(1.f + (Alpha < 1.f ? (PopScale - 1.f) * FMath::Sin(UE_PI * Alpha) : 0.f));
		if (Alpha >= 1.f)
		{
			PopElapsed = -1.f;
		}
	}
}

void ACXMRUsbPortTarget::PlayFeedbackSound(USoundBase* Sound, float Volume, float Pitch)
{
	if (Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Sound, GetActorLocation(), Volume, Pitch, 0.f, FeedbackAttenuation);
	}
}

#undef LOCTEXT_NAMESPACE
