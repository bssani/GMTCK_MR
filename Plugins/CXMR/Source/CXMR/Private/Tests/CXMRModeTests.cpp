// Copyright GMTCK CX.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "CXMRTuningWindowComponent.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRSubsystem.h"
#include "CXMRControlPanelWidget.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRDesignOption.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/SWidget.h"
#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/PostProcessVolume.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRModeExposureTest, "CXMR.Panels.ModeExposureIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRModeExposureTest::RunTest(const FString& Parameters)
{
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	APostProcessVolume* Volume = World->SpawnActor<APostProcessVolume>();
	Volume->bUnbound = true;
	AActor* Owner = World->SpawnActor<AActor>();
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Owner);
	Control->RegisterComponent();
	Owner->DispatchBeginPlay();
	UCXMRTuningSubsystem* Tuning = GI->GetSubsystem<UCXMRTuningSubsystem>();
	const TArray<FName> Ids = Tuning->GetTunableIds();
	const bool bMR = TestTrue(TEXT("MR exposure has its own persisted registry identity"), Ids.Contains("Display.MRExposure"));
	const bool bVR = TestTrue(TEXT("VR exposure has its own persisted registry identity"), Ids.Contains("Display.VRExposure"));
	if (bMR && bVR)
	{
		for (const FCXMRTunable* Row : Tuning->GetTunables())
		{
			if (Row->Id == "Display.MRExposure" || Row->Id == "Display.VRExposure") { TestTrue(TEXT("Each exposure preset participates in registry persistence"), Row->bPersist); }
		}
		const float OldMR = Tuning->GetTunableValue("Display.MRExposure");
		const float OldVR = Tuning->GetTunableValue("Display.VRExposure");
		Tuning->ApplyValue("Display.MRExposure", 1.7f, false);
		Tuning->ApplyValue("Display.VRExposure", -0.8f, false);
		TestEqual(TEXT("Editing VR does not replace MR preset"), Tuning->GetTunableValue("Display.MRExposure"), 1.7f);
		TestEqual(TEXT("Editing MR does not replace VR preset"), Tuning->GetTunableValue("Display.VRExposure"), -0.8f);
		UCXMRSubsystem* CXMR = GI->GetSubsystem<UCXMRSubsystem>();
		if (TestNotNull(TEXT("Display mode event source is available"), CXMR))
		{
			CXMR->OnMixedRealityChanged.Broadcast(true);
			TestEqual(TEXT("MR applies MR preset to the virtual scene"), Volume->Settings.AutoExposureBias, 1.7f);
			CXMR->OnMixedRealityChanged.Broadcast(false);
			TestEqual(TEXT("VR applies VR preset to the virtual scene"), Volume->Settings.AutoExposureBias, -0.8f);
			CXMR->OnMixedRealityChanged.Broadcast(true);
			TestEqual(TEXT("Returning to MR retains its exposure"), Volume->Settings.AutoExposureBias, 1.7f);
		}
		Tuning->ApplyValue("Display.MRExposure", OldMR, false);
		Tuning->ApplyValue("Display.VRExposure", OldVR, false);
	}
	Owner->Destroy();
	GI->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRReviewAssetLifetimeTest, "CXMR.Panels.ReviewAssetLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRReviewAssetLifetimeTest::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UGameInstance> GI(NewObject<UGameInstance>(GEngine));
	GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	TStrongObjectPtr<AActor> Owner(World->SpawnActor<AActor>());
	UCXMRVehicleLoaderComponent* Loader = NewObject<UCXMRVehicleLoaderComponent>(Owner.Get());
	Loader->bLoadOnBeginPlay = false;
	Loader->RegisterComponent();
	Loader->Catalog = NewObject<UCXMRVehicleCatalog>(Owner.Get());
	Loader->Profile = NewObject<UCXMRVehicleProfile>(Owner.Get());
	UCXMRVehicleProfile* InactiveVehicle = NewObject<UCXMRVehicleProfile>(Owner.Get());
	UCXMRDesignOption* InactiveOption = NewObject<UCXMRDesignOption>(Owner.Get());
	InactiveVehicle->DisplayName = FText::FromString(TEXT("Inactive vehicle"));
	InactiveOption->DisplayName = FText::FromString(TEXT("Inactive option"));
	InactiveOption->OptionId = "Alternative"; InactiveOption->SlotId = "Console";
	Loader->Catalog->Vehicles.Add(Loader->Profile.Get());
	Loader->Catalog->Vehicles.Add(InactiveVehicle);
	Loader->Profile->DesignOptions.Add(InactiveOption);
	const TWeakObjectPtr<UCXMRVehicleProfile> WeakVehicle(InactiveVehicle);
	const TWeakObjectPtr<UCXMRDesignOption> WeakOption(InactiveOption);
	TStrongObjectPtr<UCXMRControlPanelWidget> Viewer(CreateWidget<UCXMRControlPanelWidget>(World, UCXMRControlPanelWidget::StaticClass()));
	if (TestNotNull(TEXT("Review helper is created"), Viewer.Get()))
	{
		const TSharedRef<SWidget> Page = Viewer->BuildViewerPage();
		// 소프트 참조만 남은 비활성 행의 GC 생존을 검사함.
		InactiveVehicle = nullptr; InactiveOption = nullptr;
		CollectGarbage(RF_NoFlags);
		TestTrue(TEXT("Visible inactive vehicle survives garbage collection"), WeakVehicle.IsValid());
		TestTrue(TEXT("Visible inactive design option survives garbage collection"), WeakOption.IsValid());
		TestEqual(TEXT("Review still resolves the current loader after GC"), Viewer->FindLoader(), Loader);
		Loader->DestroyComponent();
		UCXMRVehicleLoaderComponent* Replacement = NewObject<UCXMRVehicleLoaderComponent>(Owner.Get());
		Replacement->bLoadOnBeginPlay = false;
		Replacement->RegisterComponent();
		TestEqual(TEXT("Invalid cached loader is rediscovered"), Viewer->FindLoader(), Replacement);
	}
	Viewer.Reset();
	Owner->Destroy();
	Owner.Reset();
	GI->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
