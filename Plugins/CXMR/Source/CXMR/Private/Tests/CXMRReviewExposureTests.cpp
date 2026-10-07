// Copyright GMTCK CX.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CXMRTuningWindowComponent.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
	struct FExposureFixture
	{
		FString Path = FPaths::ProjectSavedDir() / TEXT("CXMR/Tuning.json");
		TArray<uint8> Original;
		bool bHadFile = false;
		bool bRestoreSettings = false;
		TStrongObjectPtr<UGameInstance> GI;
		UWorld* World = nullptr;
		APostProcessVolume* Volume = nullptr;
		AActor* Owner = nullptr;
		UCXMRTuningWindowComponent* Control = nullptr;
		UCXMRTuningSubsystem* Tuning = nullptr;
		bool Initialize(const FString& Values, bool bOverride = false)
		{
			// 실제 PC 설정은 바이트 그대로 복원함.
			bHadFile = IFileManager::Get().FileExists(*Path);
			if (bHadFile && !FFileHelper::LoadFileToArray(Original, *Path)) { return false; }
			bRestoreSettings = true;
			IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
			if (!FFileHelper::SaveStringToFile(TEXT("{\"values\":{") + Values + TEXT("}}"), *Path)) { return false; }
			GI.Reset(NewObject<UGameInstance>(GEngine));
			GI->InitializeStandalone();
			World = GI->GetWorld();
			Volume = World->SpawnActor<APostProcessVolume>();
			Volume->bUnbound = true;
			Volume->Settings.bOverride_AutoExposureBias = bOverride;
			Volume->Settings.AutoExposureBias = 1.5f;
			Owner = World->SpawnActor<AActor>();
			Control = NewObject<UCXMRTuningWindowComponent>(Owner);
			Control->RegisterComponent();
			Owner->DispatchBeginPlay();
			Tuning = GI->GetSubsystem<UCXMRTuningSubsystem>();
			return true;
		}
		void Switch(bool bMR) { GI->GetSubsystem<UCXMRSubsystem>()->OnMixedRealityChanged.Broadcast(bMR); }
		~FExposureFixture()
		{
			if (Owner) { Owner->Destroy(); }
			if (GI.IsValid()) { GI->Shutdown(); }
			if (World) { World->DestroyWorld(false); GEngine->DestroyWorldContext(World); }
			if (!bRestoreSettings) { return; }
			if (bHadFile) { FFileHelper::SaveArrayToFile(Original, *Path); }
			else { IFileManager::Get().Delete(*Path); }
		}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRExposureBaselineTest, "CXMR.ReviewFix.ExposureBaseline",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRExposureBaselineTest::RunTest(const FString& Parameters)
{
	for (bool bOverride : { false, true })
	{
		FExposureFixture Fixture;
		if (!TestTrue(TEXT("Isolated settings fixture initialized"), Fixture.Initialize(TEXT(""), bOverride))) { return false; }
		TestEqual(TEXT("BeginPlay preserves authored bias"), Fixture.Volume->Settings.AutoExposureBias, 1.5f);
		TestEqual(TEXT("BeginPlay preserves authored override"), bool(Fixture.Volume->Settings.bOverride_AutoExposureBias), bOverride);
		TestEqual(TEXT("MR without a preset displays baseline"), Fixture.Tuning->GetTunableValue("Display.MRExposure"), 1.5f);
		TestEqual(TEXT("VR without a preset displays baseline"), Fixture.Tuning->GetTunableValue("Display.VRExposure"), 1.5f);
		Fixture.Switch(true); Fixture.Switch(false);
		TestEqual(TEXT("Mode toggles preserve bias without presets"), Fixture.Volume->Settings.AutoExposureBias, 1.5f);
		TestEqual(TEXT("Mode toggles preserve override without presets"), bool(Fixture.Volume->Settings.bOverride_AutoExposureBias), bOverride);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRExposureLegacyTest, "CXMR.ReviewFix.ExposureLegacy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRExposureLegacyTest::RunTest(const FString& Parameters)
{
	FExposureFixture Fixture;
	if (!TestTrue(TEXT("Legacy settings fixture initialized"), Fixture.Initialize(TEXT("\"Display.Exposure\":0.7")))) { return false; }
	TestEqual(TEXT("Legacy seeds MR preset"), Fixture.Tuning->GetTunableValue("Display.MRExposure"), 0.7f);
	TestEqual(TEXT("Legacy seeds VR preset"), Fixture.Tuning->GetTunableValue("Display.VRExposure"), 0.7f);
	Fixture.Switch(true);
	TestEqual(TEXT("Legacy MR preset applied"), Fixture.Volume->Settings.AutoExposureBias, 0.7f);
	Fixture.Switch(false);
	TestEqual(TEXT("Legacy VR preset applied"), Fixture.Volume->Settings.AutoExposureBias, 0.7f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRExposurePartialTest, "CXMR.ReviewFix.ExposurePartial",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRExposurePartialTest::RunTest(const FString& Parameters)
{
	FExposureFixture Fixture;
	if (!TestTrue(TEXT("Partial settings fixture initialized"), Fixture.Initialize(TEXT("\"Display.Exposure\":0.7,\"Display.MRExposure\":2.2")))) { return false; }
	Fixture.Switch(true);
	TestEqual(TEXT("Saved MR preset takes precedence over legacy"), Fixture.Volume->Settings.AutoExposureBias, 2.2f);
	TestTrue(TEXT("A preset overrides authored exposure"), Fixture.Volume->Settings.bOverride_AutoExposureBias);
	Fixture.Switch(false);
	TestEqual(TEXT("VR without preset restores authored bias"), Fixture.Volume->Settings.AutoExposureBias, 1.5f);
	TestFalse(TEXT("VR without preset restores authored override"), Fixture.Volume->Settings.bOverride_AutoExposureBias);
	TestEqual(TEXT("Legacy is ignored when a new key exists"), Fixture.Tuning->GetTunableValue("Display.VRExposure"), 1.5f);
	Fixture.Tuning->ApplyValue("Display.VRExposure", -0.8f, false);
	TestEqual(TEXT("An operator edit activates the VR preset"), Fixture.Volume->Settings.AutoExposureBias, -0.8f);
	Fixture.Switch(true); Fixture.Switch(false);
	TestEqual(TEXT("Edited VR preset survives mode changes"), Fixture.Volume->Settings.AutoExposureBias, -0.8f);
	return true;
}
#endif
