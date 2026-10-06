// Copyright GMTCK CX.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "CXMRUsbPortTarget.h"
#include "CXMRPlugTipComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

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
			for (const TCHAR* Name : {TEXT("CXMR.PlugTip.Preview"), TEXT("CXMR.Port.EnterDistance"), TEXT("CXMR.Port.ExitDistance"), TEXT("CXMR.Port.MaxAngle"), TEXT("CXMR.Port.ApproachDistance"), TEXT("CXMR.Port.NearDistance")})
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
			Plug = NewObject<UCXMRPlugTipComponent>(Pawn);
			Plug->RegisterComponent();
			check(Plug->GetPlugTip(Tip, Direction));
		}

		ACXMRUsbPortTarget* Port(float Distance, bool bAlignedDirection = false)
		{
			const FVector Axis = bAlignedDirection ? -Direction : FVector::CrossProduct(Direction, FVector::UpVector).GetSafeNormal();
			ACXMRUsbPortTarget* Result = World->SpawnActor<ACXMRUsbPortTarget>(Tip + Direction * Distance, Axis.Rotation());
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

#endif
