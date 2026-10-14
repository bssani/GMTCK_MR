// Copyright GMTCK CX.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CXMRMarkerFollower.h"
#include "CXMRSubsystem.h"
#include "CXMRVehicleRoot.h"
#include "CXMRPlacementComponent.h"
#include "CXMRMarkerProfile.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace
{
	struct FMarkerFollowerFixture
	{
		TStrongObjectPtr<UGameInstance> GI;
		UWorld* World = nullptr;
		UCXMRSubsystem* Subsystem = nullptr;
		FMarkerFollowerFixture()
		{
			GI.Reset(NewObject<UGameInstance>(GEngine));
			GI->InitializeStandalone();
			World = GI->GetWorld();
			// 액터 ProcessEvent와 EndPlay가 실제 게임 월드처럼 허용되도록 초기화함.
			World->InitializeActorsForPlay(FURL());
			Subsystem = GI->GetSubsystem<UCXMRSubsystem>();
		}
		ACXMRMarkerFollower* Spawn(int32 Id = 42)
		{
			ACXMRMarkerFollower* Actor = World->SpawnActor<ACXMRMarkerFollower>();
			Actor->MarkerId = Id;
			Actor->SetActorLocation(FVector(800, 900, 100));
			Actor->DispatchBeginPlay();
			return Actor;
		}
		void Pose(int32 Id, const FTransform& Transform, bool bDetected = false)
		{
			(bDetected ? Subsystem->OnMarkerDetected : Subsystem->OnMarkerMoved).Broadcast(
				Id, Transform.GetLocation(), Transform.Rotator(), FVector2D(15, 15));
		}
		~FMarkerFollowerFixture()
		{
			World->DestroyWorld(false);
			GI->Shutdown();
			GEngine->DestroyWorldContext(World);
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRMarkerFollowerPoseTest, "CXMR.MarkerFollower.PoseAndFollow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRMarkerFollowerPoseTest::RunTest(const FString& Parameters)
{
	FMarkerFollowerFixture Fixture;
	ACXMRMarkerFollower* Actor = Fixture.Spawn();
	Actor->MarkerToActor = FTransform(FRotator(15, 30, -10), FVector(11, -7, 20));
	const FTransform Initial = Actor->GetActorTransform();
	const FTransform Marker(FRotator(-25, 65, 12), FVector(130, -85, 40));
	Fixture.Pose(43, Marker, true);
	TestTrue(TEXT("Other marker poses are ignored"), Actor->GetActorTransform().Equals(Initial));
	TestFalse(TEXT("Other IDs do not mark this actor tracked"), Actor->IsMarkerTracked());
	Fixture.Pose(42, Marker, true);
	TestTrue(TEXT("Detection applies offset before world pose"), Actor->GetActorTransform().Equals(Actor->MarkerToActor * Marker, 0.001f));
	TestTrue(TEXT("Detection marks this actor tracked"), Actor->IsMarkerTracked());
	const FTransform Moved(FRotator(5, -45, 20), FVector(-200, 55, 100));
	Fixture.Pose(42, Moved);
	TestTrue(TEXT("Moved applies translation and rotation"), Actor->GetActorTransform().Equals(Actor->MarkerToActor * Moved, 0.001f));
	Actor->SetFollow(false);
	const FTransform Paused = Actor->GetActorTransform();
	Fixture.Pose(42, Marker);
	TestTrue(TEXT("Follow off preserves pose"), Actor->GetActorTransform().Equals(Paused));
	TestTrue(TEXT("Follow off still receives marker state"), Actor->IsMarkerTracked());
	Actor->SetFollow(true);
	TestTrue(TEXT("Enabling follow waits for the next pose"), Actor->GetActorTransform().Equals(Paused));
	Fixture.Pose(42, Marker);
	TestTrue(TEXT("Next pose resumes following"), Actor->GetActorTransform().Equals(Actor->MarkerToActor * Marker, 0.001f));
	Actor->Destroy();
	TestFalse(TEXT("EndPlay removes pose subscription"), Fixture.Subsystem->OnMarkerMoved.Contains(Actor, FName(TEXT("HandleMarkerPose"))));
	TestFalse(TEXT("EndPlay removes detection subscription"), Fixture.Subsystem->OnMarkerDetected.Contains(Actor, FName(TEXT("HandleMarkerPose"))));
	TestFalse(TEXT("EndPlay removes loss subscription"), Fixture.Subsystem->OnMarkerLost.Contains(Actor, FName(TEXT("HandleMarkerLost"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRMarkerFollowerLossTest, "CXMR.MarkerFollower.LossAndReacquisition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRMarkerFollowerLossTest::RunTest(const FString& Parameters)
{
	FMarkerFollowerFixture Fixture;
	ACXMRMarkerFollower* Actor = Fixture.Spawn();
	const FTransform Marker(FRotator(0, 25, 0), FVector(10, 20, 30));
	Fixture.Pose(42, Marker);
	Fixture.Subsystem->OnMarkerLost.Broadcast(43);
	TestTrue(TEXT("Other marker loss is ignored"), Actor->IsMarkerTracked());
	Fixture.Subsystem->OnMarkerLost.Broadcast(42);
	TestFalse(TEXT("Lost clears tracked state"), Actor->IsMarkerTracked());
	TestFalse(TEXT("Hold remains visible"), Actor->IsHidden());
	TestTrue(TEXT("Hold keeps the last pose"), Actor->GetActorTransform().Equals(Marker));
	TestEqual(TEXT("Loss label"), Actor->FindComponentByClass<UTextRenderComponent>()->Text.ToString(), FString(TEXT("Marker 42 · lost")));
	Actor->WhenLost = ECXMRMarkerWhenLost::Hide;
	Fixture.Pose(42, Marker);
	Fixture.Subsystem->OnMarkerLost.Broadcast(42);
	TestTrue(TEXT("Hide hides on loss"), Actor->IsHidden());
	Actor->SetFollow(false);
	Fixture.Pose(42, FTransform(FVector(100, 200, 300)));
	TestFalse(TEXT("Moved reacquisition restores visibility even with follow off"), Actor->IsHidden());
	TestTrue(TEXT("Moved reacquisition restores tracked state"), Actor->IsMarkerTracked());
	TestTrue(TEXT("Reacquisition with follow off keeps pose"), Actor->GetActorTransform().Equals(Marker));
	Fixture.Subsystem->OnMarkerTrackingChanged.Broadcast(false);
	TestFalse(TEXT("Tracking off invalidates tracked state"), Actor->IsMarkerTracked());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRMarkerFollowerWaitingTest, "CXMR.MarkerFollower.WaitingAndZeroId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRMarkerFollowerWaitingTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Marker Id is 0."), EAutomationExpectedErrorFlags::Contains, 1);
	FMarkerFollowerFixture Fixture;
	ACXMRMarkerFollower* Actor = Fixture.Spawn();
	const FTransform Initial = Actor->GetActorTransform();
	Actor->SetTrackingMode(ECXMRMarkerTrackingMode::Stationary);
#if WITH_EDITOR
	Actor->TrackingMode = ECXMRMarkerTrackingMode::Dynamic;
	FPropertyChangedEvent Changed(FindFProperty<FProperty>(Actor->GetClass(), TEXT("TrackingMode")));
	Actor->PostEditChangeProperty(Changed);
#endif
	Actor->Tick(0.f);
	TestTrue(TEXT("No headset or marker preserves the placed pose"), Actor->GetActorTransform().Equals(Initial));
	TestFalse(TEXT("Waiting is not tracked"), Actor->IsMarkerTracked());
	TestEqual(TEXT("Before detection the label does not claim a requested mode"), Actor->FindComponentByClass<UTextRenderComponent>()->Text.ToString(), FString(TEXT("Marker 42 · waiting")));
	Fixture.Pose(42, FTransform(FVector(1, 2, 3)));
	TestEqual(TEXT("Synthetic observation does not invent a runtime mode"), Actor->FindComponentByClass<UTextRenderComponent>()->Text.ToString(), FString(TEXT("Marker 42 · mode ?")));
	ACXMRMarkerFollower* Zero = Fixture.Spawn(0);
	const FTransform ZeroInitial = Zero->GetActorTransform();
	Fixture.Pose(0, FTransform(FVector(20, 30, 40)), true);
	Fixture.Subsystem->OnMarkerLost.Broadcast(0);
	Zero->SetTrackingMode(ECXMRMarkerTrackingMode::Dynamic);
	TestTrue(TEXT("Zero ID remains inert"), Zero->GetActorTransform().Equals(ZeroInitial));
	TestFalse(TEXT("Zero ID never becomes tracked"), Zero->IsMarkerTracked());
	TestFalse(TEXT("Zero ID label is hidden"), Zero->FindComponentByClass<UTextRenderComponent>()->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRMarkerFollowerWarningsTest, "CXMR.MarkerFollower.CalibrationAndDuplicateWarnings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRMarkerFollowerWarningsTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Marker 42 is a calibration marker."), EAutomationExpectedErrorFlags::Contains, 2);
	AddExpectedError(TEXT("Multiple actors follow Marker 42."), EAutomationExpectedErrorFlags::Contains, 1);
	FMarkerFollowerFixture Fixture;
	ACXMRVehicleRoot* Root = Fixture.World->SpawnActor<ACXMRVehicleRoot>();
	Root->Placement->MarkerProfile = NewObject<UCXMRMarkerProfile>(Root);
	FCXMRMarkerEntry Entry;
	Entry.MarkerId = 42;
	Root->Placement->MarkerProfile->Markers.Add(Entry);
	const FTransform RootBefore = Root->GetActorTransform();
	ACXMRMarkerFollower* First = Fixture.Spawn();
	ACXMRMarkerFollower* Second = Fixture.Spawn();
	const FTransform Marker(FVector(100, 200, 300));
	Fixture.Pose(42, Marker, true);
	TestTrue(TEXT("First duplicate still follows"), First->GetActorTransform().Equals(Marker));
	TestTrue(TEXT("Second duplicate still follows"), Second->GetActorTransform().Equals(Marker));
	TestTrue(TEXT("Follower does not move the calibration anchor"), Root->GetActorTransform().Equals(RootBefore));
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRMarkerFollowerScaleTest, "CXMR.MarkerFollower.PreservePlacedScale",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRMarkerFollowerScaleTest::RunTest(const FString& Parameters)
{
	FMarkerFollowerFixture Fixture;
	ACXMRMarkerFollower* Actor = Fixture.Spawn();
	const FVector PlacedScale(0.3, 0.3, 0.3);
	Actor->SetActorScale3D(PlacedScale);
	Actor->MarkerToActor = FTransform(FRotator(15, 30, -10), FVector(11, -7, 20));
	for (int32 Index = 0; Index < 2; ++Index)
	{
		const FTransform Marker(FRotator(-25, 65 + Index * 30, 12), FVector(130 + Index * 50, -85, 40));
		Fixture.Pose(42, Marker, Index == 0);
		FTransform Expected = Actor->MarkerToActor * Marker;
		Expected.SetScale3D(PlacedScale);
		TestTrue(TEXT("Detected and moved preserve scale while following location and rotation"), Actor->GetActorTransform().Equals(Expected, 0.001f));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRMarkerFollowerTrackingWarningTest, "CXMR.MarkerFollower.TrackingOffWarningAfterEnabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRMarkerFollowerTrackingWarningTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("Marker tracking is off."), EAutomationExpectedErrorFlags::Contains, 1);
	FMarkerFollowerFixture Fixture;
	ACXMRMarkerFollower* Actor = Fixture.Spawn();
	Fixture.Subsystem->OnMarkerTrackingChanged.Broadcast(false);
	Fixture.Subsystem->OnMarkerTrackingChanged.Broadcast(true);
	Fixture.Pose(42, FTransform(FVector(10, 20, 30)));
	Fixture.Subsystem->OnMarkerTrackingChanged.Broadcast(false);
	TestFalse(TEXT("Turning tracking off invalidates observation"), Actor->IsMarkerTracked());
	Fixture.Subsystem->OnMarkerTrackingChanged.Broadcast(false);
	Fixture.Subsystem->OnMarkerTrackingChanged.Broadcast(true);
	Fixture.Subsystem->OnMarkerTrackingChanged.Broadcast(false);
	return true;
}
#endif
