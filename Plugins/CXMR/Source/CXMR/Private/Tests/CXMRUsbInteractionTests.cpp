// Copyright GMTCK CX.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CXMRUsbPortTarget.h"
#include "CXMRPlugTipComponent.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
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
		UCXMRPlugTipComponent* Plug = nullptr;
		FVector Tip, Direction;
		TArray<TPair<IConsoleVariable*, float>> Previous;

		FUsbWorld()
		{
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
			for (const TCHAR* Name : {TEXT("CXMR.PlugTip.Preview"), TEXT("CXMR.Port.EnterDistance"), TEXT("CXMR.Port.ExitDistance"), TEXT("CXMR.Port.MaxAngle"), TEXT("CXMR.Port.ApproachDistance"), TEXT("CXMR.Port.NearDistance"), TEXT("CXMR.Port.HandContactDistance"), TEXT("CXMR.Port.HandReleaseDistance")})
			{
				IConsoleVariable* Var = IConsoleManager::Get().FindConsoleVariable(Name);
				check(Var);
				Previous.Emplace(Var, Var->GetFloat());
				Var->Set(Name == FString(TEXT("CXMR.PlugTip.Preview")) ? 1.f : -1.f, ECVF_SetByConsole);
			}
			APawn* Pawn = World->SpawnActor<APawn>();
			UCameraComponent* Camera = NewObject<UCameraComponent>(Pawn);
			Pawn->SetRootComponent(Camera);
			Camera->RegisterComponent();
			APlayerController* Controller = World->SpawnActor<APlayerController>();
			World->AddController(Controller);
			Controller->Possess(Pawn);
			if (!Controller->PlayerCameraManager) { Controller->PlayerCameraManager = World->SpawnActor<APlayerCameraManager>(); }
			Plug = NewObject<UCXMRPlugTipComponent>(Pawn);
			Plug->RegisterComponent();
			check(Plug->GetPlugTip(Tip, Direction));
		}

		ACXMRUsbPortTarget* Port(float Distance, bool bAlignedDirection = false)
		{
			const FVector Axis = bAlignedDirection ? -Direction : FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
			ACXMRUsbPortTarget* Result = World->SpawnActor<ACXMRUsbPortTarget>(Tip + Direction * Distance, Axis.Rotation());
			Result->bUseHandContact = false;
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbNearTest, "CXMR.USB.NearFeedback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbNearTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	ACXMRUsbPortTarget* Port = Fixture.Port(3.f);
	Port->Tick(0.02f);
	TestEqual(TEXT("A nearby hand reacts even with the wrong insertion angle"), Port->GetPortState(), ECXMRPortState::Near);
	TestTrue(TEXT("Proximity drives visible feedback"), Port->GetApproachAmount() > 0.5f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbDwellTest, "CXMR.USB.AlignmentDwell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbDwellTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	ACXMRUsbPortTarget* Port = Fixture.Port(1.f, true);
	Port->Tick(0.02f);
	TestTrue(TEXT("One noisy frame cannot confirm alignment"), Port->GetPortState() != ECXMRPortState::Aligned);
	Port->Tick(0.2f);
	TestEqual(TEXT("A stable pose confirms alignment"), Port->GetPortState(), ECXMRPortState::Aligned);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbTrackingGraceTest, "CXMR.USB.TrackingGrace",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbTrackingGraceTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	ACXMRUsbPortTarget* Port = Fixture.Port(3.f);
	Port->Tick(0.02f);
	IConsoleManager::Get().FindConsoleVariable(TEXT("CXMR.PlugTip.Preview"))->Set(0, ECVF_SetByConsole);
	Port->Tick(0.05f);
	TestTrue(TEXT("A short tracking dropout does not flash the feedback off"), Port->GetPortState() != ECXMRPortState::Idle);
	Port->Tick(0.5f);
	TestEqual(TEXT("Expired tracking clears the state"), Port->GetPortState(), ECXMRPortState::Idle);
	TestTrue(TEXT("Expired tracking clears proximity feedback"), FMath::IsNearlyZero(Port->GetApproachAmount()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbPreservePortTest, "CXMR.USB.PreservePort",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbPreservePortTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	ACXMRUsbPortTarget* Port = Fixture.Port(1.f, true);
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
	TestTrue(TEXT("Port reaches aligned after continuing observations"), Port->IsAligned());
	TestTrue(TEXT("Aligned port is visible"), Indicator->IsVisible());
	TestEqual(TEXT("Active feedback preserves the actual USB material"), Indicator->GetMaterial(0), OriginalMaterial);
	TestTrue(TEXT("Active feedback preserves USB position and size"), Indicator->GetRelativeTransform().Equals(OriginalTransform));
	for (UStaticMeshComponent* Mesh : Meshes)
	{
		if (Mesh->GetFName() == TEXT("GuideBeam"))
		{
			TestFalse(TEXT("Realistic defaults hide the long guide beam"), Mesh->IsVisible());
		}
	}
	Port->SetActorLocation(Fixture.Tip + Fixture.Direction * 50.f);
	Port->Tick(0.2f);
	TestEqual(TEXT("Release preserves the actual USB material"), Indicator->GetMaterial(0), OriginalMaterial);
	TestTrue(TEXT("Release preserves USB position and size"), Indicator->GetRelativeTransform().Equals(OriginalTransform));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbStaleDwellTest, "CXMR.USB.StalePoseCannotConfirm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbStaleDwellTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	ACXMRUsbPortTarget* Port = Fixture.Port(1.f, true);
	Port->Tick(0.1f);
	IConsoleVariable* Preview = IConsoleManager::Get().FindConsoleVariable(TEXT("CXMR.PlugTip.Preview"));
	Preview->Set(0, ECVF_SetByConsole);
	Port->Tick(0.1f);
	TestFalse(TEXT("Grace period cannot confirm alignment from the old pose"), Port->IsAligned());
	Preview->Set(1, ECVF_SetByConsole);
	Port->Tick(0.1f);
	TestFalse(TEXT("Confirmation must restart after tracking loss"), Port->IsAligned());
	Port->Tick(0.06f);
	TestFalse(TEXT("The reacquisition frame does not count toward stability"), Port->IsAligned());
	Port->Tick(0.1f);
	TestTrue(TEXT("Fresh stable tracking can confirm alignment again"), Port->IsAligned());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbNearestTest, "CXMR.USB.NearestPortHandoff",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbNearestTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	ACXMRUsbPortTarget* First = Fixture.Port(3.f);
	ACXMRUsbPortTarget* Second = Fixture.Port(3.2f);
	First->Tick(0.02f);
	Second->Tick(0.02f);
	TestTrue(TEXT("Closest port responds"), First->GetPortState() != ECXMRPortState::Idle);
	TestEqual(TEXT("Neighbour stays idle"), Second->GetPortState(), ECXMRPortState::Idle);
	Second->SetActorLocation(Fixture.Tip + Fixture.Direction * 1.5f);
	Second->Tick(0.02f); // 새 포트가 먼저 갱신돼도 이전 반응은 종료해야 함.
	TestTrue(TEXT("Substantially closer port takes over"), Second->GetPortState() != ECXMRPortState::Idle);
	TestEqual(TEXT("Handoff releases previous feedback immediately"), First->GetPortState(), ECXMRPortState::Idle);
	First->Tick(0.02f);
	TestEqual(TEXT("Next tick keeps only one port active"), First->GetPortState(), ECXMRPortState::Idle);
	return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbFirstObservationTest, "CXMR.USB.FirstObservationDwell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbFirstObservationTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	ACXMRUsbPortTarget* Port = Fixture.Port(1.f, true);
	Port->Tick(0.2f);
	TestFalse(TEXT("A delayed first observation cannot satisfy the dwell"), Port->IsAligned());
	Port->Tick(0.1f);
	TestFalse(TEXT("Only time since the first valid observation counts"), Port->IsAligned());
	Port->Tick(0.06f);
	TestTrue(TEXT("Continuing valid observations satisfy the dwell"), Port->IsAligned());
	Port->SetActorLocation(Fixture.Tip + Fixture.Direction * 20.f);
	Port->Tick(0.02f);
	Port->SetActorLocation(Fixture.Tip + Fixture.Direction * 1.f);
	Port->Tick(0.2f);
	TestFalse(TEXT("Re-entering the valid angle-distance window starts a new dwell"), Port->IsAligned());
	IConsoleVariable* Preview = IConsoleManager::Get().FindConsoleVariable(TEXT("CXMR.PlugTip.Preview"));
	Preview->Set(0, ECVF_SetByConsole);
	Port->Tick(0.05f);
	Preview->Set(1, ECVF_SetByConsole);
	Port->Tick(0.2f);
	TestFalse(TEXT("Delayed reacquisition cannot satisfy a new dwell"), Port->IsAligned());
	Port->Tick(0.16f);
	TestTrue(TEXT("Fresh continuity after reacquisition can align"), Port->IsAligned());
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbHandSurfaceTest, "CXMR.USB.HandContact.SurfaceAndRelease",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbHandSurfaceTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	Fixture.Plug->PlugTipReach = 15.f;
	Fixture.Plug->PlugOffset.SetLocation(FVector(500, 500, 500));
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
	TestFalse(TEXT("Hand contact does not claim plug alignment"), Port->IsAligned());
	TestEqual(TEXT("Contact measures distance from the hand surface"), Port->GetPlugDistance(), 0.1f, 0.01f);
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
	UCXMRHandDebugComponent* Display = NewObject<UCXMRHandDebugComponent>(Fixture.Plug->GetOwner());
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbContactModeVisibilityTest, "CXMR.USB.HandContact.ModeVisibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbContactModeVisibilityTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	Tracker.bTracked = false;
	ACXMRUsbPortTarget* Port = Fixture.Port(50.f);
	TArray<UStaticMeshComponent*> Meshes;
	Port->GetComponents(Meshes);
	UStaticMeshComponent* Frame = nullptr;
	for (UStaticMeshComponent* Mesh : Meshes) { if (Mesh->GetFName() == TEXT("BarTop")) { Frame = Mesh; } }
	if (!TestNotNull(TEXT("Mode fixture has a feedback frame"), Frame)) { return false; }
	Port->Tick(0.02f);
	TestTrue(TEXT("Legacy idle display follows ShowWhenIdle"), Frame->IsVisible());
	Port->bUseHandContact = true;
	Port->Tick(0.02f);
	TestFalse(TEXT("Switching to contact hides idle feedback immediately"), Frame->IsVisible());
	Port->bUseHandContact = false;
	Port->Tick(0.02f);
	TestTrue(TEXT("Switching to legacy restores idle display immediately"), Frame->IsVisible());
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbMixedInputTest, "CXMR.USB.HandContact.MixedInputArbitration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbMixedInputTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	ACXMRUsbPortTarget* Legacy = Fixture.Port(3.f);
	ACXMRUsbPortTarget* Contact = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.1, 0), FRotator::ZeroRotator);
	Contact->DispatchBeginPlay();
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		if (Frame % 2) { Contact->Tick(0.02f); Legacy->Tick(0.02f); }
		else { Legacy->Tick(0.02f); Contact->Tick(0.02f); }
		TestEqual(TEXT("The closer hand contact remains the winner"), Contact->GetPortState(), ECXMRPortState::Near);
		TestEqual(TEXT("Legacy input cannot keep a second port active or retrigger contact"), Legacy->GetPortState(), ECXMRPortState::Idle);
	}
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbInactiveLegacyTest, "CXMR.USB.HandContact.InactiveLegacyCannotBlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbInactiveLegacyTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	ACXMRUsbPortTarget* Legacy = Fixture.Port(2.f);
	Legacy->ApproachDistance = 0.f;
	Legacy->NearDistance = 1.f;
	Legacy->EnterDistance = 0.2f;
	Legacy->ExitDistance = 3.f;
	ACXMRUsbPortTarget* Contact = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 3.5, 0), FRotator::ZeroRotator);
	Contact->HandContactDistance = 3.f;
	Contact->DispatchBeginPlay();
	Legacy->Tick(0.02f); Contact->Tick(0.02f);
	TestEqual(TEXT("Disabled approach and invalid near conditions keep legacy idle"), Legacy->GetPortState(), ECXMRPortState::Idle);
	TestEqual(TEXT("An ineligible legacy target cannot suppress real hand contact"), Contact->GetPortState(), ECXMRPortState::Near);
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
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRUsbGraceCannotReenterTest, "CXMR.USB.HandContact.GraceCannotReenter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRUsbGraceCannotReenterTest::RunTest(const FString& Parameters)
{
	FUsbWorld Fixture;
	FContactTracker Tracker;
	Tracker.ActiveHand = EControllerHand::Left;
	ACXMRUsbPortTarget* Legacy = Fixture.Port(0.05f);
	ACXMRUsbPortTarget* Contact = Fixture.World->SpawnActor<ACXMRUsbPortTarget>(FVector(0, 1.2, 0), FRotator::ZeroRotator);
	Contact->DispatchBeginPlay();
	Legacy->Tick(0.02f);
	TestEqual(TEXT("Legacy contact is established before loss"), Legacy->GetPortState(), ECXMRPortState::Near);
	IConsoleManager::Get().FindConsoleVariable(TEXT("CXMR.PlugTip.Preview"))->Set(0, ECVF_SetByConsole);
	for (int32 Frame = 0; Frame < 4; ++Frame)
	{
		Contact->Tick(0.02f); Legacy->Tick(0.02f);
		TestEqual(TEXT("Current hand contact wins after the old port was released"), Contact->GetPortState(), ECXMRPortState::Near);
		TestEqual(TEXT("A released port cannot re-enter from its cached plug pose"), Legacy->GetPortState(), ECXMRPortState::Idle);
	}
	return true;
}
#endif
