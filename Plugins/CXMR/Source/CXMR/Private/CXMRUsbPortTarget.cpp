// Copyright GMTCK CX.

#include "CXMRUsbPortTarget.h"
#include "CXMRTuningSubsystem.h"

#include "Components/ArrowComponent.h"
#include "Components/AudioComponent.h"
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

static TAutoConsoleVariable<int32> CVarPortDebug(TEXT("CXMR.Port.Debug"), 0,
	TEXT("Draw tracked hand contact and release margins around the port."), ECVF_Default);

namespace
{
	constexpr float CubeCm = 100.f;
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

	// 엔진과 플러그인 애셋만 사용함.
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeFinder(TEXT("/Engine/BasicShapes/Cube"));
	// 카메라 배경에서도 보이도록 Unlit 재질 사용함.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> UnlitFinder(TEXT("/CXMR/Core/Materials/M_CXMRUnlitColor"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> BasicFinder(TEXT("/Engine/BasicShapes/BasicShapeMaterial"));
	static ConstructorHelpers::FObjectFinder<USoundBase> SoundFinder(TEXT("/Engine/VREditor/Sounds/UI/Object_Snaps_To_Grid"));
	if (CubeFinder.Succeeded())     { BarMesh   = CubeFinder.Object; }
	if (UnlitFinder.Succeeded())    { BarMaterial = UnlitFinder.Object; }
	else if (BasicFinder.Succeeded()) { BarMaterial = BasicFinder.Object; }
	if (SoundFinder.Succeeded())    { NearSound = SoundFinder.Object; }
}

void ACXMRUsbPortTarget::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	// 에디터에서도 크기와 오프셋 변경을 반영함.
	LayoutFrame();
	ApplyState();
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
	SetContactEnabled(false);
	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
		// 기존 포트가 없어지면 다른 포트로 설정 등록을 넘김.
		for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->IsActorBeingDestroyed() && It->bContactEnabled && !It->IsHidden())
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

void ACXMRUsbPortTarget::SetContactEnabled(bool bEnabled)
{
	bContactEnabled = bEnabled;
	SetActorTickEnabled(bEnabled);
	// 교체 시 비활성 후보는 이전 설정 소유권을 받지 못하므로 활성화 후 등록함.
	if (bEnabled && HasActorBegunPlay())
	{
		if (UCXMRTuningSubsystem* Tuning = GetTuning())
		{
			if (!Tuning->GetTunableIds().Contains(TEXT("Port.Live"))) { RegisterTunables(); }
		}
	}
	if (!bEnabled)
	{
		SetState(ECXMRPortState::Idle);
		if (IsValid(FeedbackAudio)) { FeedbackAudio->Stop(); }
		LastDistance = -1.f;
		bHaveTrackedPose = false;
		TrackingLostSeconds = 0.f;
		LastContactHand = EControllerHand::AnyHand;
	}
	ApplyState();
}

void ACXMRUsbPortTarget::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!bContactEnabled || IsHidden())
	{
		SetState(ECXMRPortState::Idle);
		bHaveTrackedPose = false;
		LastDistance = -1.f;
		return;
	}
	TickHandContact(DeltaSeconds);
}

void ACXMRUsbPortTarget::DrawDebug(const FVector& Point, bool bHaveHand) const
{
	UWorld* World = GetWorld();
	if (!World || CVarPortDebug.GetValueOnGameThread() == 0) { return; }
	const FVector Centre = GetActorLocation();
	const FColor Color = State == ECXMRPortState::Near ? FColor::Cyan : FColor::Silver;
	DrawDebugSphere(World, Centre, GetHandContactDistance(), 16, Color, false, -1.f, SDPG_World, 0.05f);
	DrawDebugSphere(World, Centre, GetHandReleaseDistance(), 16, FColor::Silver, false, -1.f, SDPG_World, 0.03f);
	if (bHaveHand) { DrawDebugLine(World, Point, Centre, Color, false, -1.f, SDPG_World, 0.05f); }
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
		if (It->bContactEnabled && !It->IsHidden() && !It->IsActorBeingDestroyed() && It->LastDistance >= 0.f
			&& (!Near || It->LastDistance < Near->LastDistance)) { Near = *It; }
	}
	if (!Near) { return LOCTEXT("HandUntracked", "No tracked hand"); }
	return FText::FromString(FString::Printf(TEXT("%s: %.2f cm from hand surface - %s"), *Near->Label, Near->LastDistance,
		Near->State == ECXMRPortState::Near ? TEXT("touching") : TEXT("not touching")));
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


	{
		FCXMRTunable T = Make("Port.Live", LOCTEXT("Live", "Hand contact at nearest USB"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeNearest(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Port.Debug", LOCTEXT("Debug", "Show contact margins"), ECXMRTunableKind::Bool);
		T.Get = [] { return CVarPortDebug.GetValueOnGameThread() != 0 ? 1.0f : 0.0f; };
		T.Set = [](float Value) { CVarPortDebug->Set(Value > 0.5f ? 1 : 0, ECVF_SetByConsole); };
		Tuning->Register(MoveTemp(T));
	}
	RegisterHandContactTunables();
}

void ACXMRUsbPortTarget::SetState(ECXMRPortState NewState)
{
	if (NewState == State)
	{
		return;
	}
	State = NewState;
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
	const bool bActive = bContactEnabled && State == ECXMRPortState::Near;
	if (FrameMaterial)
	{
		const FLinearColor Color = bActive ? NearColor : IdleColor;
		FrameMaterial->SetVectorParameterValue(TEXT("Color"), Color);
	}

	const bool bVisible = bActive || bShowWhenIdle;
	const bool bFeedbackVisible = bActive;
	const bool bCustomMesh = IndicatorMesh != nullptr;

	for (UStaticMeshComponent* Bar : { BarTop.Get(), BarBottom.Get(), BarLeft.Get(), BarRight.Get() })
	{
		if (Bar)
		{
			Bar->SetVisibility(bFeedbackVisible && (!bCustomMesh || bActive));
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


}

void ACXMRUsbPortTarget::Animate(float DeltaSeconds)
{
	if (FrameMaterial)
	{
		float Glow = PortRestGlow;
		if (State == ECXMRPortState::Near)
		{
			PulseTime += DeltaSeconds;
			Glow = GlowStrength * (0.9f + 0.1f * FMath::Sin(UE_TWO_PI * PulseRate * PulseTime));
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
		if (IsValid(FeedbackAudio)) { FeedbackAudio->Stop(); }
		FeedbackAudio = UGameplayStatics::SpawnSoundAtLocation(this, Sound, GetActorLocation(), FRotator::ZeroRotator, Volume, Pitch, 0.f, FeedbackAttenuation);
	}
}

#undef LOCTEXT_NAMESPACE
