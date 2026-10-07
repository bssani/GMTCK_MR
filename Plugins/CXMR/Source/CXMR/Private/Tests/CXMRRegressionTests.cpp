// Copyright GMTCK CX.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CXMRPlacementComponent.h"
#include "CXMRMarkerProfile.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRPartVariantComponent.h"
#include "CXMRUsbPortTarget.h"
#include "CXMRPartVariantTestTypes.h"
#include "CXMRSpectatorComponent.h"
#include "CXMRVehicleRoot.h"
#include "Components/SceneCaptureComponent2D.h"
#include "HAL/FileManager.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "TimerManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Light.h"

namespace
{
	struct FTestWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
		AActor* Anchor = nullptr;
		UCXMRPlacementComponent* Placement = nullptr;

		FTestWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			Anchor = World->SpawnActor<AActor>();
			USceneComponent* Root = NewObject<USceneComponent>(Anchor);
			Anchor->SetRootComponent(Root);
			Root->RegisterComponent();
			Placement = NewObject<UCXMRPlacementComponent>(Anchor);
			Placement->RegisterComponent();
			Placement->bFreezeAfterCalibration = false;
		}

		~FTestWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
		}
	};

	UCXMRMarkerProfile* MakeMarkerProfile()
	{
		UCXMRMarkerProfile* Profile = NewObject<UCXMRMarkerProfile>();
		// 현장 저장값이 테스트에 섞이지 않게 함.
		Profile->CalibrationId = FGuid::NewGuid().ToString();
		FCXMRMarkerEntry Entry;
		Entry.MarkerId = 1;
		Profile->Markers.Add(Entry);
		return Profile;
	}

	void SendMarker(UCXMRPlacementComponent* Placement, int32 Id, FVector Position, float Yaw = 0.f)
	{
		// 헤드셋 없이 실제 수신 함수에 마커 이벤트를 넣음.
		struct FMarkerParams
		{
			int32 MarkerId;
			FVector Position;
			FRotator Rotation;
			FVector2D Size;
		} Params{Id, Position, FRotator(0, Yaw, 0), FVector2D(10, 10)};
		Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerMoved")), &Params);
	}

	void SendMarkerLost(UCXMRPlacementComponent* Placement, int32 Id)
	{
		Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerLost")), &Id);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRNudgeLatchTest, "CXMR.Regression.NudgeLatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRNudgeLatchTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	Fixture.Placement->NudgeVehicleInFrame(FVector(10, 0, 0), 0, 0, FVector::ZeroVector);
	Fixture.Placement->NudgeVehicleInFrame(FVector(-10, 0, 0), 0, 90, FVector::ZeroVector);
	TestTrue(TEXT("Direct nudges latch the first heading"), Fixture.Placement->HasNudgeHeading());
	TestTrue(TEXT("Opposite nudges cancel after the viewer turns"), Fixture.Anchor->GetActorLocation().IsNearlyZero());
	Fixture.Placement->Recalibrate();
	TestFalse(TEXT("Recalibration releases the heading"), Fixture.Placement->HasNudgeHeading());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRWorldYawTest, "CXMR.Regression.WorldYaw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRWorldYawTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	Fixture.Placement->SetMarkerProfile(MakeMarkerProfile());
	SendMarker(Fixture.Placement, 1, FVector(100, 0, 0), 3.f);
	Fixture.Placement->NudgeVehicleInFrame(FVector::ZeroVector, 5.f, 0.f, FVector::ZeroVector);
	const FRotator Rotation = Fixture.Anchor->GetActorRotation();
	TestTrue(TEXT("World yaw 3 plus 5 becomes 8"), FMath::IsNearlyEqual(Rotation.Yaw, 8.0, 0.001));
	TestTrue(TEXT("Yaw nudge does not introduce world pitch"), FMath::IsNearlyZero(Rotation.Pitch, 0.001));
	TestTrue(TEXT("Yaw nudge does not introduce world roll"), FMath::IsNearlyZero(Rotation.Roll, 0.001));
	Fixture.Placement->SaveMarkerOffsetToProfile();
	const FRotator SavedRotation = Fixture.Anchor->GetActorRotation();
	TestTrue(TEXT("Saving marker calibration preserves world rotation"), SavedRotation.Equals(Rotation, 0.001));
	IFileManager::Get().Delete(*Fixture.Placement->GetCalibrationFilePath());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRProfileSwapTest, "CXMR.Regression.ProfileSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRProfileSwapTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	Fixture.Placement->SetMarkerProfile(MakeMarkerProfile());
	SendMarker(Fixture.Placement, 1, FVector(100, 0, 0));
	SendMarker(Fixture.Placement, 99, FVector(150, 0, 0));
	Fixture.Placement->NudgeVehicleInFrame(FVector(10, 0, 0), 0, 0, FVector::ZeroVector);
	const FTransform BeforeSwap = Fixture.Anchor->GetActorTransform();
	Fixture.Placement->SetMarkerProfile(MakeMarkerProfile());
	TestTrue(TEXT("Swapping preserves the visible anchor pose"), Fixture.Anchor->GetActorTransform().Equals(BeforeSwap));
	TestTrue(TEXT("Previous manual offsets are consumed"), Fixture.Placement->GetMarkerLocationOffset().IsNearlyZero());
	SendMarker(Fixture.Placement, 1, FVector(200, 0, 0));
	TestTrue(TEXT("A reused ID starts with fresh samples and no old offset"),
		Fixture.Anchor->GetActorLocation().Equals(FVector(200, 0, 0), 0.01));
	Fixture.Placement->LearnMarkerLayout();
	TestEqual(TEXT("Learning excludes markers from the outgoing profile"), Fixture.Placement->MarkerProfile->Markers.Num(), 1);
	IFileManager::Get().Delete(*Fixture.Placement->GetCalibrationFilePath());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRDegenerateMarkersTest, "CXMR.Regression.DegenerateMarkers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRDegenerateMarkersTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	UCXMRMarkerProfile* Profile = MakeMarkerProfile();
	FCXMRMarkerEntry Second = Profile->Markers[0];
	Second.MarkerId = 2;
	Profile->Markers.Add(Second);
	Fixture.Placement->SetMarkerProfile(Profile);
	Fixture.Placement->CalibrationSettleSeconds = 0.f;
	SendMarker(Fixture.Placement, 1, FVector(100, 0, 0), 45);
	SendMarker(Fixture.Placement, 2, FVector(100, 0, 0), 45);
	TestTrue(TEXT("Coincident markers retain the single-marker heading"),
		FMath::IsNearlyEqual(Fixture.Anchor->GetActorRotation().Yaw, 45.0, 0.01));
	TestFalse(TEXT("Fallback placement cannot complete a two-marker calibration"), Fixture.Placement->bCalibrated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRManualBaseTest, "CXMR.Regression.ManualBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRManualBaseTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	Fixture.Placement->SetMarkerProfile(MakeMarkerProfile());
	SendMarker(Fixture.Placement, 1, FVector(100, 0, 0));
	// 새 기준으로 조정할 때 옛 마커 위치로 돌아가면 안 됨.
	Fixture.Anchor->SetActorLocation(FVector(500, 0, 0));
	Fixture.Placement->RebaseToCurrentTransform();
	Fixture.Placement->NudgeVehicleInFrame(FVector(10, 0, 0), 0, 0, FVector::ZeroVector);
	TestTrue(TEXT("Nudge stays on the manual base"), Fixture.Anchor->GetActorLocation().Equals(FVector(510, 0, 0), 0.01));
	Fixture.Placement->ResetMarkerOffset();
	TestTrue(TEXT("Discard returns to the manual base"), Fixture.Anchor->GetActorLocation().Equals(FVector(500, 0, 0), 0.01));
	Fixture.Placement->AdjustMarkerOffset(FVector(5, 0, 0), FRotator::ZeroRotator);
	TestTrue(TEXT("Script offset stays on the manual base"), Fixture.Anchor->GetActorLocation().Equals(FVector(495, 0, 0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPlaceThenNudgeTest, "CXMR.Regression.PlaceThenNudge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPlaceThenNudgeTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	APlayerController* Controller = Fixture.World->SpawnActor<APlayerController>();
	Fixture.World->AddController(Controller);
	APawn* Pawn = Fixture.World->SpawnActor<APawn>();
	USceneComponent* PawnRoot = NewObject<USceneComponent>(Pawn);
	Pawn->SetRootComponent(PawnRoot);
	PawnRoot->RegisterComponent();
	Controller->Possess(Pawn);
	if (!Controller->PlayerCameraManager)
	{
		Controller->PlayerCameraManager = Fixture.World->SpawnActor<APlayerCameraManager>();
	}
	Fixture.Placement->SetMarkerProfile(MakeMarkerProfile());
	SendMarker(Fixture.Placement, 1, FVector(100, 0, 0));
	Fixture.Placement->PlaceInFrontOfPawn();
	const FVector Placed = Fixture.Anchor->GetActorLocation();
	TestTrue(TEXT("Place in front establishes a new base"), Placed.Equals(FVector(350, 0, 0), 0.01));
	Fixture.Placement->NudgeVehicleInFrame(FVector(10, 0, 0), 0, 0, FVector::ZeroVector);
	TestTrue(TEXT("Nudge follows the new placement"), Fixture.Anchor->GetActorLocation().Equals(FVector(360, 0, 0), 0.01));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRStaleLearnTest, "CXMR.Regression.StaleLearn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRStaleLearnTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	UCXMRMarkerProfile* Profile = MakeMarkerProfile();
	Fixture.Placement->SetMarkerProfile(Profile);
	SendMarker(Fixture.Placement, 1, FVector(100, 0, 0));
	Fixture.Anchor->SetActorLocation(FVector(500, 0, 0));
	Fixture.Placement->RebaseToCurrentTransform();
	Fixture.World->RealTimeSeconds += 10.0;
	Fixture.Placement->LearnMarkerLayout();
	TestTrue(TEXT("Stale observation cannot change the layout"), Profile->Markers[0].LocalOffset.Equals(FTransform::Identity));
	TestFalse(TEXT("Stale observation cannot create a calibration file"), IFileManager::Get().FileExists(*Fixture.Placement->GetCalibrationFilePath()));
	TestTrue(TEXT("Rejected learn preserves manual placement"), Fixture.Anchor->GetActorLocation().Equals(FVector(500, 0, 0), 0.01));
	IFileManager::Get().Delete(*Fixture.Placement->GetCalibrationFilePath());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRLostLearnTest, "CXMR.Regression.LostLearn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRLostLearnTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	UCXMRMarkerProfile* Profile = MakeMarkerProfile();
	Fixture.Placement->SetMarkerProfile(Profile);
	SendMarker(Fixture.Placement, 1, FVector(100, 0, 0));
	SendMarkerLost(Fixture.Placement, 1);
	Fixture.Anchor->SetActorLocation(FVector(500, 0, 0));
	Fixture.Placement->RebaseToCurrentTransform();
	Fixture.Placement->LearnMarkerLayout();
	TestTrue(TEXT("Lost marker cannot change its authored offset"), Profile->Markers[0].LocalOffset.Equals(FTransform::Identity));
	TestFalse(TEXT("Lost marker cannot write calibration"), IFileManager::Get().FileExists(*Fixture.Placement->GetCalibrationFilePath()));
	// 재관측 좌표에 이전 평균이 섞이면 안 됨.
	SendMarker(Fixture.Placement, 1, FVector(700, 0, 0));
	TestTrue(TEXT("Reacquisition discards samples from before loss"), Fixture.Anchor->GetActorLocation().Equals(FVector(700, 0, 0), 0.01));
	Fixture.Anchor->SetActorLocation(FVector(500, 0, 0));
	Fixture.Placement->RebaseToCurrentTransform();
	Fixture.Placement->LearnMarkerLayout();
	TestTrue(TEXT("Fresh reacquisition learns the new marker position"), Profile->Markers[0].LocalOffset.GetLocation().Equals(FVector(200, 0, 0), 0.01));
	TestTrue(TEXT("Fresh learning writes calibration"), IFileManager::Get().FileExists(*Fixture.Placement->GetCalibrationFilePath()));
	TestTrue(TEXT("Fresh learning keeps the aligned vehicle in place"), Fixture.Anchor->GetActorLocation().Equals(FVector(500, 0, 0), 0.01));
	IFileManager::Get().Delete(*Fixture.Placement->GetCalibrationFilePath());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRExpiredSettleTest, "CXMR.Regression.ExpiredSettle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRExpiredSettleTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	UCXMRMarkerProfile* Profile = MakeMarkerProfile();
	FCXMRMarkerEntry Second = Profile->Markers[0];
	Second.MarkerId = 2;
	Second.LocalOffset.SetLocation(FVector(100, 0, 0));
	Profile->Markers.Add(Second);
	Fixture.Placement->SetMarkerProfile(Profile);
	Fixture.Placement->CalibrationSettleSeconds = 1.f;
	SendMarker(Fixture.Placement, 1, FVector(0, 0, 0));
	SendMarker(Fixture.Placement, 2, FVector(100, 0, 0));
	TestFalse(TEXT("Calibration waits for settling"), Fixture.Placement->bCalibrated);
	Fixture.World->RealTimeSeconds += 2.0;
	Fixture.World->GetTimerManager().Tick(2.f);
	TestFalse(TEXT("Expired observations cannot complete at the end of settling"), Fixture.Placement->bCalibrated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRTrackingStoppedLearnTest, "CXMR.Regression.TrackingStoppedLearn",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRTrackingStoppedLearnTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	UCXMRMarkerProfile* Profile = MakeMarkerProfile();
	Fixture.Placement->SetMarkerProfile(Profile);
	SendMarker(Fixture.Placement, 1, FVector(100, 0, 0));
	bool bEnabled = false;
	Fixture.Placement->ProcessEvent(Fixture.Placement->FindFunctionChecked(TEXT("HandleMarkerTrackingChanged")), &bEnabled);
	Fixture.Anchor->SetActorLocation(FVector(500, 0, 0));
	Fixture.Placement->LearnMarkerLayout();
	TestTrue(TEXT("Stopping tracking invalidates previous observations"), Profile->Markers[0].LocalOffset.Equals(FTransform::Identity));
	TestFalse(TEXT("Tracking stop cannot save old observations"), IFileManager::Get().FileExists(*Fixture.Placement->GetCalibrationFilePath()));
	IFileManager::Get().Delete(*Fixture.Placement->GetCalibrationFilePath());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRFitQualityTest, "CXMR.Regression.FitQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRFitQualityTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	UCXMRMarkerProfile* Profile = MakeMarkerProfile();
	FCXMRMarkerEntry Second = Profile->Markers[0];
	Second.MarkerId = 2;
	Second.LocalOffset.SetLocation(FVector(100, 0, 0));
	Profile->Markers.Add(Second);
	Fixture.Placement->SetMarkerProfile(Profile);
	Fixture.Placement->CalibrationSettleSeconds = 0.f;
	SendMarker(Fixture.Placement, 1, FVector(0, 0, 0));
	SendMarker(Fixture.Placement, 2, FVector(200, 0, 0));
	TestFalse(TEXT("A successful fit with large residual cannot calibrate"), Fixture.Placement->bCalibrated);
	Fixture.Placement->Recalibrate();
	SendMarker(Fixture.Placement, 1, FVector(0, 0, 0));
	SendMarker(Fixture.Placement, 2, FVector(100, 0, 0));
	TestTrue(TEXT("A valid fit completes calibration"), Fixture.Placement->bCalibrated);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRFailedVehicleSwapTest, "CXMR.Regression.FailedVehicleSwap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRFailedVehicleSwapTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	UCXMRVehicleLoaderComponent* Loader = NewObject<UCXMRVehicleLoaderComponent>(Fixture.Anchor);
	Loader->RegisterComponent();
	UCXMRVehicleProfile* Good = NewObject<UCXMRVehicleProfile>();
	Good->VehicleActor = ACXMRUsbPortTarget::StaticClass();
	UCXMRVehicleProfile* Bad = NewObject<UCXMRVehicleProfile>();
	Loader->Catalog = NewObject<UCXMRVehicleCatalog>();
	Loader->Catalog->Vehicles = {Good, Bad};
	Loader->LoadVehicle(Good);
	AActor* Original = Loader->GetSpawnedVehicle();
	if (!TestNotNull(TEXT("Initial vehicle spawned"), Original)) { return false; }
	Loader->NextVehicle();
	TestTrue(TEXT("Failed cycle preserves the actor"), Loader->GetSpawnedVehicle() == Original && IsValid(Original));
	TestTrue(TEXT("Failed cycle preserves the profile"), Loader->Profile == Good);
	TestEqual(TEXT("Failed cycle preserves the catalog index"), Loader->GetVehicleIndex(), 0);
	Loader->LoadVehicle(nullptr);
	TestTrue(TEXT("Null replacement preserves the actor"), Loader->GetSpawnedVehicle() == Original && IsValid(Original));
	Bad->VehicleActor = ALight::StaticClass();
	Loader->LoadVehicle(Bad);
	TestTrue(TEXT("Spawn failure preserves the actor and profile"), Loader->GetSpawnedVehicle() == Original && IsValid(Original) && Loader->Profile == Good);
	Bad->VehicleActor = AActor::StaticClass();
	Loader->LoadVehicle(Bad);
	TestTrue(TEXT("Attachment failure preserves the actor and profile"), Loader->GetSpawnedVehicle() == Original && IsValid(Original) && Loader->Profile == Good);
	Bad->VehicleActor = ACXMRUsbPortTarget::StaticClass();
	Bad->VehicleRootOffset.SetScale3D(FVector(1, 0, 1));
	Loader->LoadVehicle(Bad);
	TestTrue(TEXT("Zero scale rejects replacement without changing accepted actor"), Loader->GetSpawnedVehicle() == Original && Loader->Profile == Good);
	TestFalse(TEXT("Invalid transform exposes a reason"), Loader->GetLastFailureReason().IsEmpty());
	ACXMRUsbPortTarget* Port = Cast<ACXMRUsbPortTarget>(Original);
	TestTrue(TEXT("Rejected vehicle leaves accepted contact active"), Port && Port->IsContactEnabled());
	Loader->UnloadVehicle();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRVehicleSelectionsTest, "CXMR.Regression.VehicleSelections",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRVehicleSelectionsTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	Fixture.Placement->SetMarkerProfile(MakeMarkerProfile());
	UCXMRVehicleLoaderComponent* Loader = NewObject<UCXMRVehicleLoaderComponent>(Fixture.Anchor);
	Loader->RegisterComponent();
	UCXMRVehicleProfile* First = NewObject<UCXMRVehicleProfile>();
	First->VehicleActor = ACXMRUsbPortTarget::StaticClass();
	UCXMRVehicleProfile* Second = NewObject<UCXMRVehicleProfile>();
	Second->VehicleActor = ACXMRUsbPortTarget::StaticClass();
	Second->VehicleRootOffset = FTransform(FRotator(0, 17, 0), FVector(120, -30, 45));
	Loader->Catalog = NewObject<UCXMRVehicleCatalog>();
	Loader->Catalog->Vehicles = {First, Second};
	const FTransform AnchorPose(FRotator(0, 32, 0), FVector(500, 200, 80));
	Fixture.Anchor->SetActorTransform(AnchorPose);
	TestTrue(TEXT("Direct catalog selection succeeds"), Loader->SelectVehicle(1));
	TestEqual(TEXT("Direct selection publishes index"), Loader->GetVehicleIndex(), 1);
	TestNull(TEXT("Vehicle without marker profile clears prior profile"), Fixture.Placement->MarkerProfile.Get());
	AActor* Vehicle = Loader->GetSpawnedVehicle();
	if (!TestNotNull(TEXT("Selected vehicle spawned"), Vehicle)) { return false; }
	TestTrue(TEXT("CAD offset applied once below anchor"), Vehicle->GetActorTransform().Equals(Second->VehicleRootOffset * AnchorPose, 0.01f));
	TestTrue(TEXT("Catalog selection preserves anchor"), Fixture.Anchor->GetActorTransform().Equals(AnchorPose, 0.01f));
	TestFalse(TEXT("Invalid direct selection rejected"), Loader->SelectVehicle(2));
	TestTrue(TEXT("Rejected selection preserves vehicle and profile"), Loader->GetSpawnedVehicle() == Vehicle && Loader->Profile == Second);
	TestEqual(TEXT("Rejected selection preserves index"), Loader->GetVehicleIndex(), 1);
	TestFalse(TEXT("Rejected selection exposes reason"), Loader->GetLastFailureReason().IsEmpty());
	TestTrue(TEXT("Next wraps catalog"), (Loader->NextVehicle(), Loader->Profile == First));
	TestTrue(TEXT("Successful selection clears failure"), Loader->GetLastFailureReason().IsEmpty());
	AActor* Accepted = Loader->GetSpawnedVehicle();
	TestFalse(TEXT("Committed vehicle restores authored visibility"), Accepted->IsHidden());
	TestTrue(TEXT("Committed vehicle restores authored collision"), Accepted->GetActorEnableCollision());
	TestTrue(TEXT("Committed USB restores contact"), Cast<ACXMRUsbPortTarget>(Accepted)->IsContactEnabled());
	TArray<UCXMRPartVariantComponent*> Controllers;
	Accepted->GetComponents(Controllers);
	TestEqual(TEXT("Vehicle has one part controller"), Controllers.Num(), 1);
	FActorSpawnParameters OwnedParams;
	OwnedParams.Owner = Accepted;
	ACXMRUsbPortTarget* OwnedPort = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(ACXMRUsbPortTarget::StaticClass(), FTransform::Identity, OwnedParams);
	OwnedPort->AttachToComponent(Accepted->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	ACXMRUsbPortTarget* ForeignPort = Fixture.World->SpawnActor<ACXMRUsbPortTarget>();
	ForeignPort->AttachToComponent(Accepted->GetRootComponent(), FAttachmentTransformRules::KeepWorldTransform);
	const FTransform ForeignPose = ForeignPort->GetActorTransform();
	Loader->UnloadVehicle();
	TestFalse(TEXT("Unload destroys owned port descendants"), IsValid(OwnedPort));
	TestFalse(TEXT("Owned port contact disabled on unload"), OwnedPort->IsContactEnabled());
	TestTrue(TEXT("Unload preserves externally attached actors"), IsValid(ForeignPort) && ForeignPort->IsContactEnabled());
	TestTrue(TEXT("Preserved external actor retains world transform"), ForeignPort->GetActorTransform().Equals(ForeignPose, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRVehicleRootContractTest, "CXMR.Regression.VehicleRootContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRVehicleRootContractTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	ACXMRVehicleRoot* Root = Fixture.World->SpawnActor<ACXMRVehicleRoot>();
	if (!TestNotNull(TEXT("Vehicle root spawned"), Root)) { return false; }
	TestTrue(TEXT("Anchor is the root"), Root->GetRootComponent() == Root->VehicleAnchor);
	TestTrue(TEXT("Loader attaches directly to anchor"), Root->Loader->AttachTarget == Root->VehicleAnchor);
	TArray<UActorComponent*> Components;
	Root->GetComponents(Components);
	TestEqual(TEXT("Root owns anchor, placement and loader only"), Components.Num(), 3);
	const FTransform AnchorPose(FRotator(0, 45, 0), FVector(100, -150, 70));
	Root->SetActorTransform(AnchorPose);
	UCXMRVehicleProfile* Profile = NewObject<UCXMRVehicleProfile>();
	Profile->VehicleActor = ACXMRUsbPortTarget::StaticClass();
	Profile->VehicleRootOffset = FTransform(FRotator(0, -20, 0), FVector(300, 40, 15));
	Root->Loader->LoadVehicle(Profile);
	AActor* Vehicle = Root->Loader->GetSpawnedVehicle();
	if (!TestNotNull(TEXT("Vehicle spawned under minimal root"), Vehicle)) { return false; }
	TestTrue(TEXT("Vehicle parent is calibrated anchor"), Vehicle->GetRootComponent()->GetAttachParent() == Root->VehicleAnchor);
	TestTrue(TEXT("Root preserves accepted anchor"), Root->GetActorTransform().Equals(AnchorPose, 0.01f));
	TestTrue(TEXT("Vehicle preserves imported offset"), Vehicle->GetActorTransform().Equals(Profile->VehicleRootOffset * AnchorPose, 0.01f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRVehicleConstructedStateTest, "CXMR.Regression.ConstructedVehicleState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRVehicleConstructedStateTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	ACXMRVehicleRoot* Root = Fixture.World->SpawnActor<ACXMRVehicleRoot>();
	UCXMRVehicleProfile* Profile = NewObject<UCXMRVehicleProfile>();
	Profile->VehicleActor = ACXMRPartAuthoredActivityTestAssembly::StaticClass();
	Root->Loader->LoadVehicle(Profile);
	AActor* Vehicle = Root->Loader->GetSpawnedVehicle();
	if (!TestNotNull(TEXT("Constructed vehicle accepted"), Vehicle)) { return false; }
	TestTrue(TEXT("Construction hidden state preserved"), Vehicle->IsHidden());
	TestFalse(TEXT("Construction collision state preserved"), Vehicle->GetActorEnableCollision());
	TestTrue(TEXT("Construction tick state preserved"), Vehicle->IsActorTickEnabled());
	Root->Loader->UnloadVehicle();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRSpectatorBoundsTest, "CXMR.Regression.SpectatorBounds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRSpectatorBoundsTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	ACXMRVehicleRoot* Root = Fixture.World->SpawnActor<ACXMRVehicleRoot>();
	UCXMRVehicleProfile* Profile = NewObject<UCXMRVehicleProfile>();
	Profile->VehicleActor = ACXMRUsbPortTarget::StaticClass();
	Profile->VehicleRootOffset.SetLocation(FVector(1000, 500, 200));
	Root->Loader->LoadVehicle(Profile);
	AActor* Vehicle = Root->Loader->GetSpawnedVehicle();
	if (!TestNotNull(TEXT("Vehicle spawned for bounds test"), Vehicle)) { return false; }
	FVector Centre, Extent;
	Vehicle->GetActorBounds(false, Centre, Extent, true);
	UCXMRSpectatorComponent* Spectator = NewObject<UCXMRSpectatorComponent>(Fixture.Anchor);
	Spectator->RegisterComponent();
	Spectator->Resolution = FIntPoint(64, 64);
	Spectator->OrbitDegreesPerSecond = 0.f;
	Spectator->SetMode(ECXMRSpectatorMode::Orbit);
	Spectator->TickComponent(0.016f, LEVELTICK_All, nullptr);
	USceneCaptureComponent2D* Camera = Fixture.Anchor->FindComponentByClass<USceneCaptureComponent2D>();
	if (!TestNotNull(TEXT("Orbit camera created"), Camera)) { return false; }
	FVector Expected = Centre + FVector(Spectator->OrbitDistance, 0, 0);
	Expected.Z = Centre.Z - Extent.Z + Spectator->OrbitHeight;
	TestTrue(TEXT("Orbit centres on the loaded geometry and uses its ground height"),
		Camera->GetComponentLocation().Equals(Expected, 0.01));
	TestTrue(TEXT("Orbit camera looks at the vehicle centre"),
		Camera->GetForwardVector().Equals((Centre - Expected).GetSafeNormal(), 0.001));
	Spectator->SetMode(ECXMRSpectatorMode::Off);
	Root->Loader->UnloadVehicle();
	return true;
}

#endif
