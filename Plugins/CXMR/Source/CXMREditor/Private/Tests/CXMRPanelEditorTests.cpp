// Copyright GMTCK CX.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "CXMRDesktopPanelComponent.h"
#include "CXMRTuningWindowComponent.h"
#include "CXMRPlacementComponent.h"
#include "CXMRMarkerProfile.h"
#include "CXMRPlugTipComponent.h"
#include "CXMRUsbPortTarget.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ToolMenus.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPanelRecoveryTest, "CXMR.Panels.EditorRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPanelRecoveryTest::RunTest(const FString& Parameters)
{
	UToolMenu* Menu = UToolMenus::Get()->FindMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
	if (!TestNotNull(TEXT("Editor toolbar exists"), Menu)) { return false; }
	FToolMenuSection* Section = Menu->FindSection("CXMRPanels");
	if (!TestNotNull(TEXT("CXMR toolbar section exists"), Section)) { return false; }
	FToolMenuEntry* UserButton = Section->FindEntry("CXMRUserPanel");
	FToolMenuEntry* DeveloperButton = Section->FindEntry("CXMRDeveloperPanel");
	if (!TestNotNull(TEXT("User panel button registered"), UserButton) || !TestNotNull(TEXT("Developer panel button registered"), DeveloperButton)) { return false; }
	const FToolMenuContext Context;
	UWorld* PreviousPlayWorld = GEditor->PlayWorld;
	GEditor->PlayWorld = nullptr;
	TestFalse(TEXT("User button disabled outside Play"), UserButton->TryExecuteToolUIAction(Context));
	TestFalse(TEXT("Developer button disabled outside Play"), DeveloperButton->TryExecuteToolUIAction(Context));

	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	GEditor->PlayWorld = World;
	AActor* Owner = World->SpawnActor<AActor>();
	UCXMRDesktopPanelComponent* UserPanel = NewObject<UCXMRDesktopPanelComponent>(Owner);
	UCXMRTuningWindowComponent* DeveloperPanel = NewObject<UCXMRTuningWindowComponent>(Owner);
	UserPanel->RegisterComponent();
	DeveloperPanel->RegisterComponent();
	TestFalse(TEXT("Settings registration does not automatically open a second window"), DeveloperPanel->bOpenOnBeginPlay);
	TestTrue(TEXT("User button runs during Play"), UserButton->TryExecuteToolUIAction(Context));
	TestTrue(TEXT("Developer button runs during Play"), DeveloperButton->TryExecuteToolUIAction(Context));
	if (FApp::CanEverRender())
	{
		TestTrue(TEXT("Both APIs see the shared window"), UserPanel->IsWindowOpen() && DeveloperPanel->IsWindowOpen());
		// X로 닫은 경로와 재열기 확인함.
		const TArray<TSharedRef<SWindow>> Windows = FSlateApplication::Get().GetTopLevelWindows();
		int32 ControlWindows = 0;
		for (const TSharedRef<SWindow>& Window : Windows)
		{
			if (Window->GetTitle().EqualTo(DeveloperPanel->WindowTitle)) { ++ControlWindows; }
		}
		TestEqual(TEXT("Two toolbar actions create only one control window"), ControlWindows, 1);
		for (const TSharedRef<SWindow>& Window : Windows)
		{
			if (Window->GetTitle().EqualTo(UserPanel->WindowTitle) || Window->GetTitle().EqualTo(DeveloperPanel->WindowTitle))
			{
				FSlateApplication::Get().DestroyWindowImmediately(Window);
			}
		}
		TestFalse(TEXT("X clears user window handle"), UserPanel->IsWindowOpen());
		TestFalse(TEXT("X clears developer window handle"), DeveloperPanel->IsWindowOpen());
		UserButton->TryExecuteToolUIAction(Context);
		DeveloperButton->TryExecuteToolUIAction(Context);
		TestTrue(TEXT("Toolbar reopens both closed windows"), UserPanel->IsWindowOpen() && DeveloperPanel->IsWindowOpen());
		UserButton->TryExecuteToolUIAction(Context);
		DeveloperButton->TryExecuteToolUIAction(Context);
		TestTrue(TEXT("Repeated clicks keep the panels open"), UserPanel->IsWindowOpen() && DeveloperPanel->IsWindowOpen());
		UserPanel->CloseWindow();
		TestFalse(TEXT("Desktop close releases the shared window"), DeveloperPanel->IsWindowOpen());
		DeveloperButton->TryExecuteToolUIAction(Context);
		TestTrue(TEXT("Settings action also restores the desktop API"), UserPanel->IsWindowOpen());
	}
	else
	{
		AddInfo(TEXT("Window rendering checks require a render-capable run; toolbar registration and enablement checked."));
	}
	UserPanel->CloseWindow();
	DeveloperPanel->CloseWindow();
	GEditor->PlayWorld = PreviousPlayWorld;
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPanelModesTest, "CXMR.Panels.ModeNavigation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPanelModesTest::RunTest(const FString& Parameters)
{
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>();
	TestFalse(TEXT("Daily use is the initial mode"), Control->IsSetupMode());
	Control->SelectPage(ECXMRControlPage::Calibration);
	TestEqual(TEXT("Calibration remains accessible in daily use"), Control->GetActivePage(), ECXMRControlPage::Calibration);
	Control->SelectPage(ECXMRControlPage::USB);
	TestEqual(TEXT("Daily use does not enter hidden advanced pages"), Control->GetActivePage(), ECXMRControlPage::Calibration);
	Control->SetSetupMode(true);
	Control->SelectPage(ECXMRControlPage::USB);
	TestEqual(TEXT("Setup exposes USB tuning"), Control->GetActivePage(), ECXMRControlPage::USB);
	Control->SetSetupMode(false);
	TestEqual(TEXT("Leaving setup returns from advanced content to daily use"), Control->GetActivePage(), ECXMRControlPage::Vehicle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPanelCalibrationTest, "CXMR.Panels.CalibrationWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPanelCalibrationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	AActor* Owner = World->SpawnActor<AActor>();
	USceneComponent* Root = NewObject<USceneComponent>(Owner);
	Owner->SetRootComponent(Root);
	Root->RegisterComponent();
	UCXMRPlacementComponent* Placement = NewObject<UCXMRPlacementComponent>(Owner);
	Placement->RegisterComponent();
	UCXMRMarkerProfile* Profile = NewObject<UCXMRMarkerProfile>(Owner);
	Profile->CalibrationId = TEXT("PanelTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	Placement->MarkerProfile = Profile;
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Owner);
	Control->RegisterComponent();
	TestFalse(TEXT("Save requires an explicit alignment confirmation"), Control->SaveCalibration());
	Control->StartCalibration();
	TestFalse(TEXT("Start cannot manufacture successful marker alignment"), Control->CanConfirmCalibration());
	Placement->bCalibrated = true; // 계산 품질은 배치 검사에서 확인함. 여기서는 UI 연결만 검사함.
	TestTrue(TEXT("Actual completed calibration unlocks confirmation"), Control->ConfirmCalibration());
	Owner->SetActorLocation(FVector(0.1f, 0.f, 0.f));
	TestFalse(TEXT("Moving the vehicle invalidates the prior confirmation"), Control->CanSaveCalibration());
	Owner->SetActorLocation(FVector::ZeroVector);
	TestFalse(TEXT("Returning to the old pose cannot restore an invalidated confirmation"), Control->CanSaveCalibration());
	TestTrue(TEXT("Moved pose can be explicitly confirmed again"), Control->ConfirmCalibration());
	const FString Path = Placement->GetCalibrationFilePath();
	TestTrue(TEXT("Confirmed calibration can be saved"), Control->SaveCalibration());
	TestTrue(TEXT("Success corresponds to an actual saved file"), FPaths::FileExists(Path));
	TestEqual(TEXT("Successful save finishes the workflow"), Control->GetCalibrationPhase(), 3);
	IFileManager::Get().Delete(*Path);

	// 테스트 GUID 경로에 폴더를 만들어 저장 실패 재현함.
	Control->StartCalibration();
	Placement->bCalibrated = true;
	Control->ConfirmCalibration();
	IFileManager::Get().MakeDirectory(*Path);
	TestFalse(TEXT("Failed file write cannot report success"), Control->SaveCalibration());
	TestEqual(TEXT("Save failure stays on the confirmation step for retry"), Control->GetCalibrationPhase(), 2);
	IFileManager::Get().DeleteDirectory(*Path, false, false);
	Placement->MarkerProfile = NewObject<UCXMRMarkerProfile>(Owner);
	TestEqual(TEXT("Changing profile invalidates an old workflow"), Control->GetCalibrationPhase(), 0);
	TestFalse(TEXT("Old profile confirmation cannot save the new one"), Control->SaveCalibration());

	UCXMRVehicleLoaderComponent* Loader = NewObject<UCXMRVehicleLoaderComponent>(Owner);
	Loader->RegisterComponent();
	UCXMRVehicleProfile* FirstVehicle = NewObject<UCXMRVehicleProfile>(Owner);
	UCXMRVehicleProfile* SecondVehicle = NewObject<UCXMRVehicleProfile>(Owner);
	FirstVehicle->VehicleActor = ACXMRUsbPortTarget::StaticClass();
	SecondVehicle->VehicleActor = ACXMRUsbPortTarget::StaticClass();
	FirstVehicle->MarkerProfile = Placement->MarkerProfile.Get();
	SecondVehicle->MarkerProfile = Placement->MarkerProfile.Get();
	SecondVehicle->VehicleRootOffset.SetLocation(FVector(10.f, 0.f, 0.f));
	Loader->LoadVehicle(FirstVehicle);
	Control->StartCalibration();
	Placement->bCalibrated = true;
	Control->ConfirmCalibration();
	Loader->LoadVehicle(SecondVehicle);
	TestNotNull(TEXT("Replacement vehicle spawned"), Loader->GetSpawnedVehicle());
	TestEqual(TEXT("Vehicles using the same marker profile still invalidate confirmation"), Control->GetCalibrationPhase(), 0);
	TestFalse(TEXT("A replacement vehicle requires a new alignment confirmation"), Control->CanSaveCalibration());
	Loader->LoadVehicle(FirstVehicle);
	TestFalse(TEXT("Switching back cannot restore the previous confirmation"), Control->CanSaveCalibration());
	Control->StartCalibration();
	Placement->bCalibrated = true;
	TestTrue(TEXT("Reloaded vehicle can receive a fresh confirmation"), Control->ConfirmCalibration());
	Loader->UnloadVehicle();
	TestFalse(TEXT("Unloading the confirmed vehicle invalidates its confirmation"), Control->CanSaveCalibration());
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPanelRenderTest, "CXMR.Panels.RenderPreview",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPanelRenderTest::RunTest(const FString& Parameters)
{
	if (!FApp::CanEverRender()) { AddInfo(TEXT("Slate screenshot requires a render-capable run.")); return true; }
	UGameInstance* GI = NewObject<UGameInstance>(GEngine);
	GI->InitializeStandalone();
	UWorld* World = GI->GetWorld();
	AActor* Owner = World->SpawnActor<AActor>();
	UCXMRPlacementComponent* Placement = NewObject<UCXMRPlacementComponent>(Owner);
	Placement->RegisterComponent();
	UCXMRPlugTipComponent* Plug = NewObject<UCXMRPlugTipComponent>(Owner);
	Plug->RegisterComponent();
	ACXMRUsbPortTarget* Port = World->SpawnActor<ACXMRUsbPortTarget>();
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Owner);
	Control->RegisterComponent();
	Owner->DispatchBeginPlay();
	Port->DispatchBeginPlay();
	Control->SetSetupMode(true);
	Control->SelectPage(ECXMRControlPage::Display);
	Control->OpenWindow();
	bool bFoundWindow = false;
	for (const TSharedRef<SWindow>& Window : FSlateApplication::Get().GetTopLevelWindows())
	{
		if (Window->GetTitle().EqualTo(Control->WindowTitle))
		{
			bFoundWindow = true;
			for (ECXMRControlPage Page : { ECXMRControlPage::Display, ECXMRControlPage::Calibration, ECXMRControlPage::USB, ECXMRControlPage::Vehicle, ECXMRControlPage::View, ECXMRControlPage::Diagnostics })
			{
				Control->SelectPage(Page);
				FSlateApplication::Get().Tick();
				TArray<FColor> Pixels;
				FIntVector Size;
				if (TestTrue(TEXT("Slate control page renders"), FSlateApplication::Get().TakeScreenshot(Window, Pixels, Size)))
				{
					TArray64<uint8> PNG;
					FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), PNG);
					const FString Filename = Page == ECXMRControlPage::Display ? TEXT("ControlPanelPreview.png") : FString::Printf(TEXT("ControlPanelPreview_%d.png"), static_cast<int32>(Page));
					TestTrue(TEXT("Preview image is saved"), FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir() / Filename)));
				}
			}
			break;
		}
	}
	TestTrue(TEXT("The control window was found for rendering"), bFoundWindow);
	Control->CloseWindow();
	Owner->Destroy();
	GI->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
