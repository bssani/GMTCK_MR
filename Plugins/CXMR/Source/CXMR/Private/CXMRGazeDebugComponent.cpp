// Copyright GMTCK CX.

#include "CXMRGazeDebugComponent.h"
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"

#include "CollisionQueryParams.h"
#include "DrawDebugHelpers.h"
#include "Engine/GameInstance.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "EyeTrackerFunctionLibrary.h"
#include "EyeTrackerTypes.h"
#include "GameFramework/Actor.h"

#define LOCTEXT_NAMESPACE "CXMRGaze"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRGaze, Log, All);

UCXMRGazeDebugComponent::UCXMRGazeDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;   // switched on with the visualization
}

UCXMRSubsystem* UCXMRGazeDebugComponent::GetCXMR() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRSubsystem>() : nullptr;
}

UCXMRTuningSubsystem* UCXMRGazeDebugComponent::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

void UCXMRGazeDebugComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = GetCXMR();
	if (Subsystem)
	{
		Subsystem->OnGazeVisualizationChanged.AddDynamic(this, &UCXMRGazeDebugComponent::HandleVisualizationChanged);
		SetComponentTickEnabled(Subsystem->IsGazeVisualizationOn());
	}

	// The tracker may connect only once the XR session is up, so "no" here is not yet a verdict — G logs it again.
	UE_LOG(LogCXMRGaze, Log, TEXT("Eye tracker connected at start: %s"),
		UEyeTrackerFunctionLibrary::IsEyeTrackerConnected() ? TEXT("yes") : TEXT("no"));

	RegisterTunables();
}

void UCXMRGazeDebugComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnGazeVisualizationChanged.RemoveDynamic(this, &UCXMRGazeDebugComponent::HandleVisualizationChanged);
	}
	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
	}
	Super::EndPlay(Reason);
}

void UCXMRGazeDebugComponent::HandleVisualizationChanged(bool bOn)
{
	SetComponentTickEnabled(bOn);
	bHadGaze = false;

	if (bOn)
	{
		UE_LOG(LogCXMRGaze, Log, TEXT("Gaze visualization ON - eye tracker %s"),
			UEyeTrackerFunctionLibrary::IsEyeTrackerConnected() ? TEXT("connected") : TEXT("NOT connected"));
	}
}

bool UCXMRGazeDebugComponent::GetGazePoint(FVector& OutPoint, FVector& OutOrigin, AActor*& OutHitActor) const
{
	OutHitActor = nullptr;

	const UWorld* World = GetWorld();
	FEyeTrackerGazeData Gaze;
	if (!World || !UEyeTrackerFunctionLibrary::GetGazeData(Gaze) || Gaze.GazeDirection.IsNearlyZero())
	{
		return false;
	}

	// Already world space: the eye tracker applies the tracking-to-world transform itself.
	const FVector Direction = Gaze.GazeDirection.GetSafeNormal();
	const FVector End = Gaze.GazeOrigin + Direction * TraceDistance;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(CXMRGazeTrace), /*bTraceComplex*/ false, GetOwner());
	FHitResult Hit;
	if (World->LineTraceSingleByChannel(Hit, Gaze.GazeOrigin, End, ECC_Visibility, Params))
	{
		OutPoint = Hit.ImpactPoint;
		OutHitActor = Hit.GetActor();
	}
	else
	{
		OutPoint = End;
	}
	OutOrigin = Gaze.GazeOrigin;
	return true;
}

void UCXMRGazeDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	FVector Point;
	FVector Origin;
	AActor* HitActor = nullptr;
	const bool bGaze = GetGazePoint(Point, Origin, HitActor);

	if (bGaze != bHadGaze)
	{
		UE_LOG(LogCXMRGaze, Warning, TEXT("Gaze %s"), bGaze ? TEXT("acquired") : TEXT("lost"));
		bHadGaze = bGaze;
	}

#if ENABLE_DRAW_DEBUG
	if (bGaze)
	{
		DrawDebugPoint(GetWorld(), Point, DotSize, (HitActor ? HitColor : MissColor).ToFColor(true), false, -1.0f, SDPG_Foreground);
	}
#endif
}

void UCXMRGazeDebugComponent::RegisterTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return;
	}

	const FText Category = LOCTEXT("CatEyes", "Eyes");
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
		FCXMRTunable T = Make("Eyes.GazeDot", LOCTEXT("GazeDot", "Gaze dot (G)"), ECXMRTunableKind::Bool);
		T.Get = [this] { return (Subsystem && Subsystem->IsGazeVisualizationOn()) ? 1.0f : 0.0f; };
		T.Set = [this](float Value) { if (Subsystem) { Subsystem->SetGazeVisualization(Value > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Eyes.Gaze", LOCTEXT("GazeLands", "Gaze lands"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeGaze(); };
		Tuning->Register(MoveTemp(T));
	}
}

FText UCXMRGazeDebugComponent::DescribeGaze() const
{
	FVector Point;
	FVector Origin;
	AActor* HitActor = nullptr;
	if (!GetGazePoint(Point, Origin, HitActor))
	{
		return UEyeTrackerFunctionLibrary::IsEyeTrackerConnected()
			? LOCTEXT("NoGaze", "no gaze data (eyes not tracked)")
			: LOCTEXT("NoTracker", "no eye tracker connected");
	}

	if (!HitActor)
	{
		return FText::FromString(FString::Printf(TEXT("nothing within %.0f m"), TraceDistance / 100.0f));
	}
	return FText::FromString(FString::Printf(TEXT("%.2f m on %s"),
		FVector::Dist(Origin, Point) / 100.0, *HitActor->GetActorNameOrLabel()));
}

#undef LOCTEXT_NAMESPACE
