// Copyright GMTCK CX.

#include "CXMRMarkerDebugComponent.h"
#include "CXMRSubsystem.h"
#include "CXMRVehicleRoot.h"

#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRDebug, Log, All);

static TAutoConsoleVariable<int32> CVarDebugMarkers(
	TEXT("CXMR.DebugMarkers"),
	0,
	TEXT("Draw Varjo marker poses exactly as the plugin reports them (no transform applied).\n")
	TEXT("  0: off\n")
	TEXT("  1: on"),
	ECVF_Cheat);

UCXMRMarkerDebugComponent::UCXMRMarkerDebugComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

UCXMRSubsystem* UCXMRMarkerDebugComponent::GetCXMR() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UCXMRSubsystem>();
		}
	}
	return nullptr;
}

float UCXMRMarkerDebugComponent::Now() const
{
	const UWorld* World = GetWorld();
	return World ? World->GetTimeSeconds() : 0.0f;
}

void UCXMRMarkerDebugComponent::BeginPlay()
{
	Super::BeginPlay();

	Subsystem = GetCXMR();
	if (Subsystem)
	{
		Subsystem->OnMarkerDetected.AddDynamic(this, &UCXMRMarkerDebugComponent::HandleMarkerDetected);
		Subsystem->OnMarkerMoved.AddDynamic(this, &UCXMRMarkerDebugComponent::HandleMarkerMoved);
		Subsystem->OnMarkerLost.AddDynamic(this, &UCXMRMarkerDebugComponent::HandleMarkerLost);
	}
}

void UCXMRMarkerDebugComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnMarkerDetected.RemoveDynamic(this, &UCXMRMarkerDebugComponent::HandleMarkerDetected);
		Subsystem->OnMarkerMoved.RemoveDynamic(this, &UCXMRMarkerDebugComponent::HandleMarkerMoved);
		Subsystem->OnMarkerLost.RemoveDynamic(this, &UCXMRMarkerDebugComponent::HandleMarkerLost);
	}
	Super::EndPlay(Reason);
}

// Detected is the event that matters most: the plugin fires it once per ID per session, so a second
// one only appears if Recalibrate really did reset marker tracking.
void UCXMRMarkerDebugComponent::HandleMarkerDetected(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size)
{
	FCXMRDebugMarker& Entry = Markers.FindOrAdd(MarkerId);
	Entry.Position       = Position;
	Entry.Rotation       = Rotation;
	Entry.Size           = Size;
	Entry.LastUpdateTime = Now();
	Entry.bLost          = false;

	UE_LOG(LogCXMRDebug, Warning,
		TEXT("[%7.2fs] DETECTED  id=%d  pos=(%.1f, %.1f, %.1f)  rot=(P%.1f Y%.1f R%.1f)  size=(%.3f x %.3f)"),
		Entry.LastUpdateTime, MarkerId, Position.X, Position.Y, Position.Z,
		Rotation.Pitch, Rotation.Yaw, Rotation.Roll, Size.X, Size.Y);
}

void UCXMRMarkerDebugComponent::HandleMarkerMoved(int32 MarkerId, FVector Position, FRotator Rotation, FVector2D Size)
{
	FCXMRDebugMarker& Entry = Markers.FindOrAdd(MarkerId);

	// Re-acquisition after a loss arrives as Moved, never as Detected — worth seeing explicitly.
	if (Entry.bLost)
	{
		UE_LOG(LogCXMRDebug, Warning, TEXT("[%7.2fs] RE-ACQUIRED id=%d (as Moved, not Detected)"), Now(), MarkerId);
	}

	Entry.Position       = Position;
	Entry.Rotation       = Rotation;
	Entry.Size           = Size;
	Entry.LastUpdateTime = Now();
	Entry.bLost          = false;
	++Entry.MoveCount;
}

void UCXMRMarkerDebugComponent::HandleMarkerLost(int32 MarkerId)
{
	if (FCXMRDebugMarker* Entry = Markers.Find(MarkerId))
	{
		Entry->bLost = true;
	}
	UE_LOG(LogCXMRDebug, Warning, TEXT("[%7.2fs] LOST      id=%d"), Now(), MarkerId);
}

void UCXMRMarkerDebugComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (CVarDebugMarkers.GetValueOnGameThread() != 0)
	{
		Draw();
	}
}

void UCXMRMarkerDebugComponent::Draw() const
{
#if ENABLE_DRAW_DEBUG
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	for (const TPair<int32, FCXMRDebugMarker>& Pair : Markers)
	{
		const FCXMRDebugMarker& M = Pair.Value;

		// Drawn from the RAW reported pose. If these axes do not land on the physical marker, the
		// world-space assumption is wrong and no amount of calibration maths will fix it.
		DrawDebugCoordinateSystem(World, M.Position, M.Rotation, AxisLength, false, -1.0f, 0, 0.5f);

		const FColor Colour = M.bLost ? FColor::Red : FColor::Green;
		DrawDebugString(World, M.Position + FVector(0, 0, AxisLength),
			FString::Printf(TEXT("id %d%s  moves:%d"), Pair.Key, M.bLost ? TEXT(" LOST") : TEXT(""), M.MoveCount),
			nullptr, Colour, 0.0f, true);

		// The physical marker's footprint, so a wrong size shows up as an obviously wrong square.
		if (M.Size.X > 0.0f)
		{
			const float HalfCm = M.Size.X * 50.0f;   // metres -> cm, halved
			DrawDebugBox(World, M.Position, FVector(HalfCm, HalfCm, 0.1f), M.Rotation.Quaternion(), Colour, false, -1.0f, 0, 0.3f);
		}
	}

	if (bDrawVehicleAnchor)
	{
		for (TActorIterator<ACXMRVehicleRoot> It(World); It; ++It)
		{
			// Marker (above) vs calibrated result (here) drawn together — the gap between them is
			// the calibration error, separated from whatever the marker itself is doing.
			DrawDebugCoordinateSystem(World, It->GetActorLocation(), It->GetActorRotation(), AxisLength * 3.0f, false, -1.0f, 0, 1.0f);
			DrawDebugString(World, It->GetActorLocation() + FVector(0, 0, AxisLength * 3.0f),
				TEXT("VehicleAnchor"), nullptr, FColor::Cyan, 0.0f, true);
		}
	}
#endif
}

void UCXMRMarkerDebugComponent::DumpMarkers() const
{
	UE_LOG(LogCXMRDebug, Warning, TEXT("===== CXMR marker dump (%d seen this session) ====="), Markers.Num());
	for (const TPair<int32, FCXMRDebugMarker>& Pair : Markers)
	{
		const FCXMRDebugMarker& M = Pair.Value;
		UE_LOG(LogCXMRDebug, Warning,
			TEXT("  id=%-4d %s  pos=(%.1f, %.1f, %.1f)  rot=(P%.1f Y%.1f R%.1f)  moves=%d  last=%.2fs"),
			Pair.Key, M.bLost ? TEXT("LOST   ") : TEXT("tracked"),
			M.Position.X, M.Position.Y, M.Position.Z,
			M.Rotation.Pitch, M.Rotation.Yaw, M.Rotation.Roll, M.MoveCount, M.LastUpdateTime);
	}
}
