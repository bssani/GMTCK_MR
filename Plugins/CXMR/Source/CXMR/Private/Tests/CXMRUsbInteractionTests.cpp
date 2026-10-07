// Copyright GMTCK CX.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CXMRUsbPortTarget.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "CXMRTuningSubsystem.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "CXMRHandTracking.h"
#include "CXMRHandDebugComponent.h"
#include "Features/IModularFeatures.h"
#include "HeadMountedDisplayTypes.h"
#include "IHandTracker.h"
#include <limits>

namespace
{
	struct FUsbWorld
	{
		UWorld* World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, false);
		APawn* Pawn = nullptr;
		TArray<TPair<IConsoleVariable*, float>> Previous;

		FUsbWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			for (const TCHAR* Name : {TEXT("CXMR.Port.HandContactDistance"), TEXT("CXMR.Port.HandReleaseDistance")})
			{
				IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
				check(Var);
				Previous.Emplace(Var, Var->GetFloat());
				Var->Set(-1.f, ECVF_SetByConsole);
			}
			Pawn = World->SpawnActor<APawn>();
			UCameraComponent* Camera = NewObject<UCameraComponent>(Pawn);
			Pawn->SetRootComponent(Camera);
			Camera->RegisterComponent();
			APlayerController* Controller = World->SpawnActor<APlayerController>();
			World->AddController(Controller);
			Controller->Possess(Pawn);
			if (!Controller->PlayerCameraManager) { Controller->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>(); }
		}
		ACXMRUsbPortTarget* Port(float Distance)
		{
			ACXMRUsbPortTarget* Result = World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, Distance, 0), FRotator::ZeroRotator);
			Result->DispatchBeginPlay();
			return Result;
		}

		~FUsbWorld()
		{
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			for (const auto& Pair : Previous) { Pair.Key->Set(Pair.Value, ECVF_SetByConsole); }
		}
	};

	struct FContactTracker final : IHandTracker
	{
		TArray<FVector> Positions;
		TArray<FVector> OtherPositions;
		TArray<float> Radii;
		bool bTracked = true;
		bool bOtherTracked = false;
		EControllerHand ActiveHand = EControllerHand::Right;
		TArray<IHandTracker*> Previous;
		FVector PreviousOffset = CXMRHands::GetOffset();
		FContactTracker()
		{
			Positions.Init(FVector(0, 0, -1000), EHandKeypointCount);
			OtherPositions.Init(FVector(1000, 1000, 1000), EHandKeypointCount);
			Radii.Init(1.f, EHandKeypointCount);
			Positions[static_cast<int32>(EHandKeypoint::IndexTip)] = FVector::ZeroVector;
			IModularFeatures& Features = IModularFeatures::Get();
			Previous = Features.GetModularFeatureImplementations<IHandTracker>(GetModularFeatureName());
			for (IHandTracker* Tracker : Previous) { Features.UnregisterModularFeature(GetModularFeatureName(), Tracker); }
			Features.RegisterModularFeature(GetModularFeatureName(), this);
			CXMRHands::SetOffset(FVector::ZeroVector);
		}
		~FContactTracker()
		{
			IModularFeatures& Features = IModularFeatures::Get();
			Features.UnregisterModularFeature(GetModularFeatureName(), this);
			for (IHandTracker* Tracker : Previous) { Features.RegisterModularFeature(GetModularFeatureName(), Tracker); }
			CXMRHands::SetOffset(PreviousOffset);
		}
		FName GetHandTrackerDeviceTypeName() const override { return TEXT("ContactFixture"); }
		bool IsHandTrackingStateValid() const override { return bTracked; }
		bool GetKeypointState(EControllerHand Hand, EHandKeypoint Keypoint, FTransform& Pose, float& Radius) const override
		{
			Pose = FTransform(Positions[static_cast<int32>(Keypoint)]);
			Radius = Radii[static_cast<int32>(Keypoint)];
			return bTracked && Hand == ActiveHand;
		}
		bool GetAllKeypointStates(EControllerHand Hand, TArray<FVector>& OutPositions, TArray<FQuat>& OutRotations, TArray<float>& OutRadii, bool& OutTracked) const override
		{
			OutPositions = Hand == ActiveHand ? Positions : OtherPositions;
			OutRotations.Init(FQuat::Identity, EHandKeypointCount);
			OutRadii = Radii;
			OutTracked = Hand == ActiveHand ? bTracked : bOtherTracked;
			return true;
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbPreservePortTest, "CXMR.USB.PreservePort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbPreservePortTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	ACXMRUsbPortTarget* Port = Fixture.Port(1.1f);
	Port->IndicatorMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	Port->IndicatorOffset = FTransform(FRotator::ZeroRotator, FVector(0.5f, 0.1f, 0.2f), FVector(0.01f));
	Port->RefreshMarker();
	TArray<UStaticMeshComponent*> Meshes;
	Port->GetComponents(Meshes);
	UStaticMeshComponent* Indicator = nullptr;
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetFName() == TEXT("Indicator")) { Indicator = Mesh; }
	}
	if (!TestNotNull(TEXT("Port mesh exists"), Indicator)) { return false; }
	UMaterialInterface* OriginalMaterial = Indicator->GetMaterial(0);
	const FTransform OriginalTransform = Indicator->GetRelativeTransform();
	Port->Tick(0.2f);
	Port->Tick(0.2f);
	TestEqual(TEXT("Tracked hand contact activates feedback"), Port->GetPortState(), ECXMRPortState::Near);
	TestTrue(TEXT("Original port is visible during contact"), Indicator->IsVisible());
	TestEqual(TEXT("Active feedback preserves the actual USB material"), Indicator->GetMaterial(0), OriginalMaterial);
	TestTrue(TEXT("Active feedback preserves USB position and size"), Indicator->GetRelativeTransform().Equals(OriginalTransform));
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetFName() == TEXT("GuideBeam"))
		{
			TestFalse(TEXT("Realistic defaults hide the long guide beam"), Mesh->IsVisible());
		}
	}
	Port->SetActorLocation(FVector(0, 50, 0));
	Port->Tick(0.2f);
	TestEqual(TEXT("Release preserves the actual USB material"), Indicator->GetMaterial(0), OriginalMaterial);
	TestTrue(TEXT("Release preserves USB position and size"), Indicator->GetRelativeTransform().Equals(OriginalTransform));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbHandSurfaceTest, "CXMR.USB.HandContact.SurfaceAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbHandSurfaceTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	ACXMRUsbPortTarget* Port = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.1, 0), FRotator(0, 170, 0));
	Port->DispatchBeginPlay();
	TArray<UStaticMeshComponent*> Meshes;
	Port->GetComponents(Meshes);
	UStaticMeshComponent* Frame = nullptr;
	for (UStaticMeshComponent* Mesh : Meshes) { if (Mesh->GetFName() == TEXT("BarTop")) { Frame = Mesh; } }
	if (!TestNotNull(TEXT("Contact feedback frame exists"), Frame)) { return false; }
	TestFalse(TEXT("Feedback stays hidden until contact"), Frame->IsVisible());
	Port->Tick(0.2f);
	TestTrue(TEXT("Contact makes the feedback visible"), Frame->IsVisible());
	TestEqual(TEXT("The generated finger surface lights the USB without a plug estimate"), Port->GetPortState(), ECXMRPortState::Near);
	TestEqual(TEXT("Contact measures distance from the hand surface"), Port->GetHandDistance(), 0.1f, 0.01f);
	Port->SetActorLocation(FVector(0, 2, 0));
	Port->Tick(0.02f);
	TestEqual(TEXT("Moving away from the hand releases feedback"), Port->GetPortState(), ECXMRPortState::Idle);
	TestFalse(TEXT("Release hides contact feedback"), Frame->IsVisible());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbHandBoneTest, "CXMR.USB.HandContact.BoneAndLeftHand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbHandBoneTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	Tracker.ActiveHand = EControllerHand::Left;
	Tracker.Positions[static_cast<int32>(EHandKeypoint::IndexIntermediate)] = FVector(-5, 0, 0);
	Tracker.Positions[static_cast<int32>(EHandKeypoint::IndexDistal)] = FVector(5, 0, 0);
	Tracker.Positions[static_cast<int32>(EHandKeypoint::IndexTip)] = FVector(10, 0, 0);
	ACXMRUsbPortTarget* Port = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.1, 0), FRotator::ZeroRotator);
	Port->DispatchBeginPlay();
	Port->Tick(0.02f);
	TestEqual(TEXT("Contact between joints on the left finger lights the port"), Port->GetPortState(), ECXMRPortState::Near);
	Tracker.bTracked = false;
	Port->Tick(0.05f);
	TestEqual(TEXT("Brief loss preserves existing feedback"), Port->GetPortState(), ECXMRPortState::Near);
	Port->Tick(0.2f);
	TestEqual(TEXT("Frozen tracker output cannot keep contact forever"), Port->GetPortState(), ECXMRPortState::Idle);
	ACXMRUsbPortTarget* NewPort = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.1, 0), FRotator::ZeroRotator);
	NewPort->DispatchBeginPlay();
	NewPort->Tick(0.01f);
	TestEqual(TEXT("A stale hand cannot start a new contact"), NewPort->GetPortState(), ECXMRPortState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbHandHandoffTest, "CXMR.USB.HandContact.NearestHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbHandHandoffTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	ACXMRUsbPortTarget* First = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.2, 0), FRotator::ZeroRotator);
	ACXMRUsbPortTarget* Second = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.8, 0), FRotator::ZeroRotator);
	First->DispatchBeginPlay(); Second->DispatchBeginPlay();
	First->Tick(0.02f); Second->Tick(0.02f);
	TestEqual(TEXT("Touching port responds"), First->GetPortState(), ECXMRPortState::Near);
	TestEqual(TEXT("Untouched neighbour stays idle"), Second->GetPortState(), ECXMRPortState::Idle);
	First->SetActorLocation(FVector(0, 3, 0));
	Second->SetActorLocation(FVector(0, 1.1, 0));
	Second->Tick(0.02f);
	TestEqual(TEXT("Newly touching port responds immediately"), Second->GetPortState(), ECXMRPortState::Near);
	TestEqual(TEXT("Previous feedback clears despite tick order"), First->GetPortState(), ECXMRPortState::Idle);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbHandDisplayTest, "CXMR.USB.HandContact.DisplayScaleAndCorrection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbHandDisplayTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	UCXMRHandDebugComponent* Display = NewObject<UCXMRHandDebugComponent>(Fixture.Pawn);
	Display->RegisterComponent();
	Display->RadiusScale = 0.5f;
	ACXMRUsbPortTarget* Port = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.1, 0), FRotator::ZeroRotator);
	Port->DispatchBeginPlay();
	Port->Tick(0.02f);
	TestEqual(TEXT("A smaller displayed hand does not contact early"), Port->GetPortState(), ECXMRPortState::Idle);
	Display->RadiusScale = 1.f;
	Port->Tick(0.02f);
	TestEqual(TEXT("Display radius and contact radius stay consistent"), Port->GetPortState(), ECXMRPortState::Near);
	Port->SetActorLocation(FVector(0, 2.1, 0));
	Port->Tick(0.02f);
	TestEqual(TEXT("Hand is clear before correction"), Port->GetPortState(), ECXMRPortState::Idle);
	CXMRHands::SetOffset(FVector(0, 1, 0));
	FTransform Head;
	TestTrue(TEXT("The correction fixture has a head frame"), CXMRHands::GetHeadTransform(Fixture.World, Head));
	TestTrue(TEXT("Fixture head forward is world forward"), Head.GetRotation().Equals(FQuat::Identity));
	Port->Tick(0.02f);
	TestEqual(TEXT("The same hand offset moves contact with the display"), Port->GetPortState(), ECXMRPortState::Near);
	TestFalse(TEXT("Contact works even with visualization tick disabled"), Display->IsComponentTickEnabled());
	Tracker.Positions[static_cast<int32>(EHandKeypoint::IndexTip)].X = std::numeric_limits<double>::quiet_NaN();
	Port->Tick(0.3f);
	TestEqual(TEXT("Invalid hand geometry cannot create contact"), Port->GetPortState(), ECXMRPortState::Idle);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbContactSuspensionTest, "CXMR.USB.HandContact.InactiveOptions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbContactSuspensionTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	ACXMRUsbPortTarget* Old = Fixture.Port(1.05f);
	ACXMRUsbPortTarget* New = Fixture.Port(1.2f);
	Old->Tick(0.02f);
	TestEqual(TEXT("Old option has contact before replacement"), Old->GetPortState(), ECXMRPortState::Near);
	Old->SetContactEnabled(false);
	TestEqual(TEXT("Suspension clears old feedback immediately"), Old->GetPortState(), ECXMRPortState::Idle);
	TestFalse(TEXT("Inactive target no longer ticks"), Old->IsActorTickEnabled());
	New->Tick(0.02f);
	TestEqual(TEXT("A closer inactive port cannot block the new option"), New->GetPortState(), ECXMRPortState::Near);
	Old->Tick(0.02f);
	TestEqual(TEXT("Explicit ticks cannot reactivate inactive feedback"), Old->GetPortState(), ECXMRPortState::Idle);
	Old->SetContactEnabled(true);
	Old->SetActorHiddenInGame(true);
	Old->Tick(0.02f);
	TestEqual(TEXT("A staged hidden target stays idle"), Old->GetPortState(), ECXMRPortState::Idle);
	New->Tick(0.02f);
	TestEqual(TEXT("Hidden target cannot compete with visible contact"), New->GetPortState(), ECXMRPortState::Near);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbContactHandLossTest, "CXMR.USB.HandContact.OneHandLost",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbContactHandLossTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	Tracker.ActiveHand = EControllerHand::Left;
	Tracker.bOtherTracked = true;
	ACXMRUsbPortTarget* Port = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.1, 0), FRotator::ZeroRotator);
	Port->DispatchBeginPlay();
	Port->Tick(0.02f);
	TestEqual(TEXT("Left hand contacts while right remains far away"), Port->GetPortState(), ECXMRPortState::Near);
	Tracker.bTracked = false;
	Port->Tick(0.05f);
	TestEqual(TEXT("The contacting hand gets grace even while the other remains tracked"), Port->GetPortState(), ECXMRPortState::Near);
	Port->Tick(0.2f);
	TestEqual(TEXT("One-hand grace still expires"), Port->GetPortState(), ECXMRPortState::Idle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbContactTransferTest, "CXMR.USB.HandContact.TransferRequiresContact",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbContactTransferTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	Tracker.ActiveHand = EControllerHand::Left;
	Tracker.bOtherTracked = true;
	Tracker.OtherPositions.Init(FVector(0, -0.5, -1000), EHandKeypointCount);
	Tracker.OtherPositions[static_cast<int32>(EHandKeypoint::IndexTip)] = FVector(0, -0.5, 0);
	ACXMRUsbPortTarget* Port = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.1, 0), FRotator::ZeroRotator);
	Port->DispatchBeginPlay();
	Port->Tick(0.02f);
	Tracker.bTracked = false;
	Port->Tick(0.05f);
	TestEqual(TEXT("Old contact survives a brief loss"), Port->GetPortState(), ECXMRPortState::Near);
	Port->Tick(0.3f);
	TestEqual(TEXT("Another hand in the release margin cannot take over without touching"), Port->GetPortState(), ECXMRPortState::Idle);
	Tracker.OtherPositions[static_cast<int32>(EHandKeypoint::IndexTip)] = FVector::ZeroVector;
	Port->Tick(0.02f);
	TestEqual(TEXT("Fresh contact on the other hand can begin a new response"), Port->GetPortState(), ECXMRPortState::Near);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbRegistrySwapTest, "CXMR.USB.HandContact.SettingsAfterReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbRegistrySwapTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	UCXMRTuningSubsystem* Tuning = GI->GetSubsystem<UCXMRTuningSubsystem>();
	ACXMRUsbPortTarget* Old = World->SpawnActor<ACXMRUsbPortTarget>();
	Old->DispatchBeginPlay();
	ACXMRUsbPortTarget* Candidate = World->SpawnActor<ACXMRUsbPortTarget>();
	Candidate->SetContactEnabled(false);
	Candidate->SetActorHiddenInGame(true);
	Candidate->DispatchBeginPlay();
	Old->Destroy();
	TestFalse(TEXT("Staged candidate does not own settings before acceptance"), Tuning->GetTunableIds().Contains("Port.Live"));
	Candidate->SetActorHiddenInGame(false);
	Candidate->SetContactEnabled(true);
	for (const TCHAR* Id : {TEXT("Port.Live"), TEXT("Port.HandContactDistance"), TEXT("Port.HandReleaseDistance")})
	{
		TestTrue(TEXT("Accepted replacement restores USB settings"), Tuning->GetTunableIds().Contains(Id));
	}
	Candidate->Destroy();
	GI->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
