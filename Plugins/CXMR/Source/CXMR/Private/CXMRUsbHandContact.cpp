// Copyright GMTCK CX.

#include "CXMRUsbPortTarget.h"
#include "CXMRHandTracking.h"
#include "CXMRHandDebugComponent.h"
#include "CXMRTuningSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"

#define LOCTEXT_NAMESPACE "CXMRUsbContact"

static TAutoConsoleVariable<float> CVarHandContactDistance(TEXT("CXMR.Port.HandContactDistance"), -1.f,
	TEXT("Contact margin beyond the generated hand surface, cm. Negative uses each port's setting."), ECVF_Default);
static TAutoConsoleVariable<float> CVarHandReleaseDistance(TEXT("CXMR.Port.HandReleaseDistance"), -1.f,
	TEXT("Release margin beyond the generated hand surface, cm. Negative uses each port's setting."), ECVF_Default);

float ACXMRUsbPortTarget::GetHandContactDistance() const
{
	const float Value = CVarHandContactDistance.GetValueOnGameThread();
	return FMath::Max(0.f, Value >= 0.f ? Value : HandContactDistance);
}

float ACXMRUsbPortTarget::GetHandReleaseDistance() const
{
	const float Value = CVarHandReleaseDistance.GetValueOnGameThread();
	return FMath::Max(GetHandContactDistance() + 0.1f, Value >= 0.f ? Value : HandReleaseDistance);
}

void ACXMRUsbPortTarget::TickHandContact(float DeltaSeconds)
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const UCXMRHandDebugComponent* HandDisplay = Pawn ? Pawn->FindComponentByClass<UCXMRHandDebugComponent>() : nullptr;
	TArray<CXMRHands::FContactShape> Shapes;
	const bool bFreshHand = CXMRHands::GetContactShapes(GetWorld(), HandDisplay ? HandDisplay->RadiusScale : 1.f,
		HandDisplay ? HandDisplay->bDrawBones : true, Shapes);
	const float Dt = FMath::Max(0.f, DeltaSeconds);
	FVector ClosestPoint = FVector::ZeroVector;
	EControllerHand ClosestHand = EControllerHand::AnyHand;
	float Distance = bFreshHand ? CXMRHands::GetSurfaceDistance(Shapes, GetActorLocation(), ClosestPoint, &ClosestHand) : -1.f;
	const bool bContactHandTracked = Shapes.ContainsByPredicate([this](const CXMRHands::FContactShape& Shape) { return Shape.Hand == LastContactHand; });
	// 해제 여유는 접촉 중이던 손에만 적용함. 다른 손은 새 접촉 조건 확인함.
	if (State == ECXMRPortState::Near && bContactHandTracked && Distance > GetHandContactDistance())
	{
		const TArray<CXMRHands::FContactShape> HeldShapes = Shapes.FilterByPredicate([this](const CXMRHands::FContactShape& Shape) { return Shape.Hand == LastContactHand; });
		FVector HeldPoint;
		const float HeldDistance = CXMRHands::GetSurfaceDistance(HeldShapes, GetActorLocation(), HeldPoint);
		if (HeldDistance <= GetHandReleaseDistance()) { Distance = HeldDistance; ClosestPoint = HeldPoint; ClosestHand = LastContactHand; }
	}
	const float Limit = State == ECXMRPortState::Near && ClosestHand == LastContactHand ? GetHandReleaseDistance() : GetHandContactDistance();
	ECXMRPortState NewState = ECXMRPortState::Idle;
	if (bFreshHand && (State != ECXMRPortState::Near || bContactHandTracked || Distance <= GetHandContactDistance()))
	{
		LastDistance = Distance;
		TrackingLostSeconds = 0.f;
		bHaveTrackedPose = true;
		if (!IsHidden() && LastDistance <= Limit && IsNearestPort(LastDistance))
		{
			LastContactHand = ClosestHand;
			NewState = ECXMRPortState::Near;
		}
	}
	else
	{
		LastDistance = -1.f;
		TrackingLostSeconds += Dt;
		// 추적 유예는 기존 반응만 유지함. 과거 손으로 새 접촉 만들지 않음.
		if (!IsHidden() && bHaveTrackedPose && State == ECXMRPortState::Near && TrackingLostSeconds <= FMath::Max(0.f, TrackingGraceSeconds))
		{
			NewState = ECXMRPortState::Near;
		}
	}
	if (NewState == ECXMRPortState::Near)
	{
		// 갱신 순서와 관계없이 접촉한 한 포트만 반응함.
		for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
		{
			if (*It != this && !It->IsActorBeingDestroyed() && It->State != ECXMRPortState::Idle)
			{
				It->SetState(ECXMRPortState::Idle);
			}
		}
	}
	SetState(NewState);
	Animate(Dt);
	DrawDebug(ClosestPoint, bFreshHand && LastDistance >= 0.f);
}

bool ACXMRUsbPortTarget::IsNearestPort(float Distance) const
{
	const APawn* Pawn = UGameplayStatics::GetPlayerPawn(this, 0);
	const UCXMRHandDebugComponent* Display = Pawn ? Pawn->FindComponentByClass<UCXMRHandDebugComponent>() : nullptr;
	TArray<CXMRHands::FContactShape> Shapes;
	CXMRHands::GetContactShapes(GetWorld(), Display ? Display->RadiusScale : 1.f, Display ? Display->bDrawBones : true, Shapes);
	const float Mine = Distance - (State != ECXMRPortState::Idle ? 0.1f : 0.f);
	for (TActorIterator<ACXMRUsbPortTarget> It(GetWorld()); It; ++It)
	{
		const ACXMRUsbPortTarget* Other = *It;
		if (Other == this || Other->IsActorBeingDestroyed() || Other->IsHidden() || !Other->IsContactEnabled()) { continue; }
		float OtherDistance, Limit;
		if (Shapes.IsEmpty()) { continue; }
		FVector Point;
		OtherDistance = CXMRHands::GetSurfaceDistance(Shapes, Other->GetActorLocation(), Point);
		Limit = Other->GetHandContactDistance();
		if (Other->State == ECXMRPortState::Near)
		{
			const TArray<CXMRHands::FContactShape> HeldShapes = Shapes.FilterByPredicate([Other](const CXMRHands::FContactShape& Shape) { return Shape.Hand == Other->LastContactHand; });
			if (!HeldShapes.IsEmpty())
			{
				const float HeldDistance = CXMRHands::GetSurfaceDistance(HeldShapes, Other->GetActorLocation(), Point);
				if (OtherDistance > Limit && HeldDistance <= Other->GetHandReleaseDistance()) { OtherDistance = HeldDistance; Limit = Other->GetHandReleaseDistance(); }
			}
		}
		if (OtherDistance > Limit) { continue; }
		const float Theirs = OtherDistance - (Other->State != ECXMRPortState::Idle ? 0.1f : 0.f);
		if (Theirs < Mine || (Theirs == Mine && Other < this)) { return false; }
	}
	return true;
}

void ACXMRUsbPortTarget::RegisterHandContactTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning) { return; }
	for (bool bRelease : {false, true})
	{
		FCXMRTunable T;
		T.Id = bRelease ? TEXT("Port.HandReleaseDistance") : TEXT("Port.HandContactDistance");
		T.Category = LOCTEXT("Contact", "USB Hand Contact");
		T.Label = bRelease ? LOCTEXT("Release", "Hand Release Margin") : LOCTEXT("Touch", "Hand Contact Margin");
		T.Kind = ECXMRTunableKind::Float;
		T.Owner = this;
		T.Unit = LOCTEXT("Cm", "cm");
		T.Min = 0.f; T.Max = 3.f; T.Delta = 0.05f; T.Default = bRelease ? 0.75f : 0.25f; T.bPersist = true;
		T.Get = [this, bRelease] { return bRelease ? GetHandReleaseDistance() : GetHandContactDistance(); };
		T.Set = [bRelease](float Value)
		{
			if (bRelease) { CVarHandReleaseDistance->Set(Value, ECVF_SetByConsole); }
			else { CVarHandContactDistance->Set(Value, ECVF_SetByConsole); }
		};
		Tuning->Register(MoveTemp(T));
	}
}

#undef LOCTEXT_NAMESPACE
