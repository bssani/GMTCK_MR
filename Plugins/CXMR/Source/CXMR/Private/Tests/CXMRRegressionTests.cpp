// Copyright GMTCK CX.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CXMRPlacementComponent.h"
#include "CXMRMarkerProfile.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRUsbPortTarget.h"
#include "CXMRHandGrabComponent.h"
#include "CXMRSpectatorComponent.h"
#include "CXMRVehicleRoot.h"
#include "Components/SceneCaptureComponent2D.h"
#include "HAL/FileManager.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "Materials/Material.h"

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
		// 실제 작업에서 저장한 보정값이 테스트에 들어오지 않게 함.
		Profile->CalibrationId = FGuid::NewGuid().ToString();
		FCXMRMarkerEntry Entry;
		Entry.MarkerId = 1;
		Profile->Markers.Add(Entry);
		return Profile;
	}

	void SendMarker(UCXMRPlacementComponent* Placement, int32 Id, FVector Position, float Yaw = 0.f)
	{
		// 헤드셋 없이 마커 이벤트를 넣음. 실제 수신 함수는 그대로 사용함.
		struct FMarkerParams
		{
			int32 MarkerId;
			FVector Position;
			FRotator Rotation;
			FVector2D Size;
		} Params{Id, Position, FRotator(0, Yaw, 0), FVector2D(10, 10)};
		Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerMoved")), &Params);
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
	SendMarker(Fixture.Placement, 1, FVector(100, 0, 0), 45);
	SendMarker(Fixture.Placement, 2, FVector(100, 0, 0), 45);
	TestTrue(TEXT("Coincident markers retain the single-marker heading"),
		FMath::IsNearlyEqual(Fixture.Anchor->GetActorRotation().Yaw, 45.0, 0.01));
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
	UCXMRVehicleProfile* Profile = NewObject<UCXMRVehicleProfile>();
	// 프로젝트 에셋 없이 재질 복원을 확인하려고 플러그인 액터를 사용함.
	Profile->VehicleActor = ACXMRUsbPortTarget::StaticClass();
	Profile->Trims.SetNum(2);
	Profile->Trims[0].CMFOptions.SetNum(2);
	FCXMRMaterialOverride Override;
	Override.Material = UMaterial::GetDefaultMaterial(MD_Surface);
	Profile->Trims[0].CMFOptions[1].Materials.Add(Override);
	Loader->LoadVehicle(Profile);
	TestNull(TEXT("A vehicle without a marker profile clears the previous one"), Fixture.Placement->MarkerProfile.Get());
	AActor* Vehicle = Loader->GetSpawnedVehicle();
	if (!TestNotNull(TEXT("Vehicle spawned"), Vehicle)) { return false; }
	UStaticMeshComponent* Mesh = Vehicle->FindComponentByClass<UStaticMeshComponent>();
	if (!TestNotNull(TEXT("Vehicle has geometry"), Mesh)) { return false; }
	UMaterialInterface* Original = Mesh->GetMaterial(0);
	Loader->SetCMF(1);
	TestTrue(TEXT("CMF override applied"), Mesh->GetMaterial(0) == Override.Material.Get());
	Loader->SetCMF(0);
	TestTrue(TEXT("Empty CMF restores authored material"), Mesh->GetMaterial(0) == Original);
	Loader->SetCMF(1);
	Loader->SetTrim(1);
	TestTrue(TEXT("Trim without CMF restores authored material"), Mesh->GetMaterial(0) == Original);
	Loader->UnloadVehicle();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPinchThresholdsTest, "CXMR.Regression.PinchThresholds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPinchThresholdsTest::RunTest(const FString& Parameters)
{
	FTestWorld Fixture;
	UCXMRHandGrabComponent* Grab = NewObject<UCXMRHandGrabComponent>(Fixture.Anchor);
	Grab->RegisterComponent();
	Grab->SetSimulatedPinch(EControllerHand::Left, false, FTransform::Identity);
	Grab->SetSimulatedPinch(EControllerHand::Right, false, FTransform::Identity);
	// BP에서 직접 넣은 값도 처리되는지 확인함. 에디터 범위 제한만 믿으면 안 됨.
	Grab->PinchCloseDistance = 4.f;
	Grab->PinchOpenDistance = 2.f;
	Grab->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Inverted thresholds are corrected before evaluating hands"),
		Grab->PinchOpenDistance > Grab->PinchCloseDistance);
	Grab->PinchCloseDistance = -1.f;
	Grab->TickComponent(0.016f, LEVELTICK_All, nullptr);
	TestTrue(TEXT("Runtime thresholds stay positive"), Grab->PinchCloseDistance > 0.f);
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
