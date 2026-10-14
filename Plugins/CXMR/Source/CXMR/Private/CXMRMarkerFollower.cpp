// Copyright GMTCK CX.
#include "CXMRMarkerFollower.h"
#include "CXMRSubsystem.h"
#include "CXMRMarkerProfile.h"
#include "CXMRVehicleRoot.h"
#include "CXMRPlacementComponent.h"
#include "CXMRHandTracking.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRMarkerFollower, Log, All);

ACXMRMarkerFollower::ACXMRMarkerFollower()
{
	PrimaryActorTick.bCanEverTick = true;
	SetRootComponent(CreateDefaultSubobject<USceneComponent>(TEXT("Root")));
	Label = CreateDefaultSubobject<UTextRenderComponent>(TEXT("MarkerLabel"));
	Label->SetupAttachment(GetRootComponent());
	// 박스 회전·크기에 영향받지 않고 읽히도록 절대 좌표를 씀.
	Label->SetAbsolute(true, true, true);
	Label->SetHorizontalAlignment(EHTA_Center);
	Label->SetVerticalAlignment(EVRTA_TextBottom);
	Label->SetWorldSize(5.f);
	Label->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Label->SetCastShadow(false);
}

void ACXMRMarkerFollower::BeginPlay()
{
	Super::BeginPlay();
	if (MarkerId == 0)
	{
		UE_LOG(LogCXMRMarkerFollower, Warning, TEXT("Marker Id is 0. Set the printed marker number to follow it."));
		Label->SetVisibility(false);
		SetActorTickEnabled(false);
		return;
	}
	if (UGameInstance* GI = GetGameInstance())
	{
		Subsystem = GI->GetSubsystem<UCXMRSubsystem>();
	}
	if (Subsystem)
	{
		Subsystem->OnMarkerDetected.AddDynamic(this, &ACXMRMarkerFollower::HandleMarkerPose);
		Subsystem->OnMarkerMoved.AddDynamic(this, &ACXMRMarkerFollower::HandleMarkerPose);
		Subsystem->OnMarkerLost.AddDynamic(this, &ACXMRMarkerFollower::HandleMarkerLost);
		Subsystem->OnMarkerTrackingChanged.AddDynamic(this, &ACXMRMarkerFollower::HandleTrackingChanged);
		HandleTrackingChanged(Subsystem->IsMarkerTrackingOn());
		// 캐시 조회는 런타임을 부르지 않으며 감지된 ID만 성공함.
		TryApplyTrackingMode();
	}
	FCXMRMarkerEntry Entry;
	if (FindCalibrationEntry(Entry))
	{
		UE_LOG(LogCXMRMarkerFollower, Warning, TEXT("Marker %d is a calibration marker. Car calibration is less accurate while it is Dynamic."), MarkerId);
	}
	for (TActorIterator<ACXMRMarkerFollower> It(GetWorld()); It; ++It)
	{
		if (*It != this && It->HasActorBegunPlay() && It->MarkerId == MarkerId)
		{
			UE_LOG(LogCXMRMarkerFollower, Warning, TEXT("Multiple actors follow Marker %d. The last tracking mode request wins."), MarkerId);
			break;
		}
	}
	UpdateLabel();
}

void ACXMRMarkerFollower::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnMarkerDetected.RemoveDynamic(this, &ACXMRMarkerFollower::HandleMarkerPose);
		Subsystem->OnMarkerMoved.RemoveDynamic(this, &ACXMRMarkerFollower::HandleMarkerPose);
		Subsystem->OnMarkerLost.RemoveDynamic(this, &ACXMRMarkerFollower::HandleMarkerLost);
		Subsystem->OnMarkerTrackingChanged.RemoveDynamic(this, &ACXMRMarkerFollower::HandleTrackingChanged);
		ECXMRMarkerTrackingMode CurrentMode;
		if (bModeApplied && Subsystem->GetMarkerTrackingMode(MarkerId, CurrentMode))
		{
			FCXMRMarkerEntry Entry;
			const ECXMRMarkerTrackingMode RestoreMode = FindCalibrationEntry(Entry)
				? Entry.TrackingMode : ECXMRMarkerTrackingMode::Stationary;
			// 종료 중 세션이 없어도 서브시스템의 running 검사로 보호됨.
			Subsystem->SetMarkerTrackingMode(MarkerId, RestoreMode);
		}
	}
	Super::EndPlay(Reason);
}

bool ACXMRMarkerFollower::FindCalibrationEntry(FCXMRMarkerEntry& OutEntry) const
{
	for (TActorIterator<ACXMRVehicleRoot> It(GetWorld()); It; ++It)
	{
		const UCXMRMarkerProfile* Profile = It->Placement ? It->Placement->MarkerProfile.Get() : nullptr;
		if (Profile && Profile->FindEntry(MarkerId, OutEntry) && OutEntry.Role == ECXMRMarkerRole::Calibration)
		{
			return true;
		}
	}
	return false;
}

void ACXMRMarkerFollower::SetFollow(bool bEnable)
{
	bFollow = bEnable;
}

void ACXMRMarkerFollower::SetTrackingMode(ECXMRMarkerTrackingMode Mode)
{
	TrackingMode = Mode;
	bModePending = true;
	TryApplyTrackingMode();
}

void ACXMRMarkerFollower::TryApplyTrackingMode()
{
	if (!Subsystem || MarkerId == 0 || !bModePending) { return; }
	ECXMRMarkerTrackingMode CurrentMode;
	if (!bCanConfigure && !Subsystem->GetMarkerTrackingMode(MarkerId, CurrentMode)) { return; }
	if (Subsystem->SetMarkerTrackingMode(MarkerId, TrackingMode))
	{
		bModePending = false;
		bModeApplied = true;
	}
}

void ACXMRMarkerFollower::HandleMarkerPose(int32 Id, FVector Position, FRotator Rotation, FVector2D Size)
{
	if (MarkerId == 0 || Id != MarkerId) { return; }
	const bool bFound = !bTracked;
	bObserved = true;
	bCanConfigure = true;
	bTracked = true;
	TryApplyTrackingMode();
	SetActorHiddenInGame(false);
	if (bFollow)
	{
		SetActorTransform(MarkerToActor * FTransform(Rotation, Position));
	}
	UpdateLabel();
	if (bFound) { OnMarkerFound(); }
}

void ACXMRMarkerFollower::HandleMarkerLost(int32 Id)
{
	if (MarkerId == 0 || Id != MarkerId || !bTracked) { return; }
	bTracked = false;
	if (WhenLost == ECXMRMarkerWhenLost::Hide) { SetActorHiddenInGame(true); }
	UpdateLabel();
	OnMarkerLost();
}

void ACXMRMarkerFollower::HandleTrackingChanged(bool bEnabled)
{
	if (!bEnabled)
	{
		if (!bWarnedTrackingOff)
		{
			UE_LOG(LogCXMRMarkerFollower, Warning, TEXT("Marker tracking is off. Press V or turn it on in the control window."));
			bWarnedTrackingOff = true;
		}
		HandleMarkerLost(MarkerId);
		// 추적을 다시 켜면 플러그인 설정도 지워지므로 재감지 뒤 재요청함.
		bCanConfigure = false;
		bModePending = true;
	}
}

void ACXMRMarkerFollower::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateLabel();
}

void ACXMRMarkerFollower::UpdateLabel()
{
	Label->SetVisibility(bShowLabel && MarkerId != 0);
	if (!bShowLabel || MarkerId == 0) { return; }
	ECXMRMarkerTrackingMode Mode = ECXMRMarkerTrackingMode::Stationary;
	const bool bKnownMode = bTracked && Subsystem && Subsystem->GetMarkerTrackingMode(MarkerId, Mode);
	const TCHAR* State = !bTracked ? (bObserved ? TEXT("lost") : TEXT("waiting"))
		: !bKnownMode ? TEXT("mode ?") : Mode == ECXMRMarkerTrackingMode::Dynamic ? TEXT("Dynamic") : TEXT("Stationary");
	Label->SetText(FText::FromString(FString::Printf(TEXT("Marker %d · %s"), MarkerId, State)));
	Label->SetTextRenderColor(bTracked ? FColor::Green : FColor::Yellow);
	const FVector Location = GetActorLocation() + FVector(0, 0, 30);
	Label->SetWorldLocation(Location);
	FTransform Head;
	if (CXMRHands::GetHeadTransform(GetWorld(), Head))
	{
		Label->SetWorldRotation((Head.GetLocation() - Location).Rotation());
	}
}

#if WITH_EDITOR
void ACXMRMarkerFollower::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.GetPropertyName();
	Super::PostEditChangeProperty(PropertyChangedEvent);
	if (HasActorBegunPlay() && PropertyName == GET_MEMBER_NAME_CHECKED(ACXMRMarkerFollower, TrackingMode))
	{
		SetTrackingMode(TrackingMode);
	}
}
#endif
