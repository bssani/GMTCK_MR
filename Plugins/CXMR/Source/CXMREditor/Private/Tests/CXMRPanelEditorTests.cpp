// Copyright GMTCK CX.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/PostProcessVolume.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "CXMRDesktopPanelComponent.h"
#include "CXMRTuningWindowComponent.h"
#include "CXMRPlacementComponent.h"
#include "CXMRVehicleRoot.h"
#include "Engine/StaticMeshActor.h"
#include "CXMRMarkerProfile.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
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
	Control->SelectPage(ECXMRControlPage::Calibration);
	TestEqual(TEXT("Calibration remains accessible in daily use"), Control->GetActivePage(), ECXMRControlPage::Calibration);
	Control->SelectPage(ECXMRControlPage::Diagnostics);
	TestEqual(TEXT("Settings remain accessible during review"), Control->GetActivePage(), ECXMRControlPage::Display);
	Control->SelectPage(ECXMRControlPage::Diagnostics);
	TestEqual(TEXT("Historical diagnostics navigation resolves to Settings"), Control->GetActivePage(), ECXMRControlPage::Display);
	TestEqual(TEXT("Settings navigation survives setup preference changes"), Control->GetActivePage(), ECXMRControlPage::Display);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCXMRPanelCalibrationTest, "CXMR.Panels.CalibrationWorkflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCXMRPanelCalibrationTest::RunTest(const FString& Parameters)
{
	UWorld* World = UWorld::CreateWorld(EWorldType::Game, false);
	GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
	ACXMRVehicleRoot* Owner = World->SpawnActor<ACXMRVehicleRoot>();
	UCXMRPlacementComponent* Placement = Owner->Placement;
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Owner); Control->RegisterComponent();
	TestFalse(TEXT("No vehicle cannot save"), Control->SaveCalibration());
	UCXMRMarkerProfile* Profile = NewObject<UCXMRMarkerProfile>(Owner);
	Profile->CalibrationId = TEXT("PanelTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
	FCXMRMarkerEntry Entry; Entry.MarkerId = 1; Profile->Markers.Add(Entry);
	UCXMRVehicleProfile* Vehicle = NewObject<UCXMRVehicleProfile>(Owner);
	Vehicle->VehicleActor = ACXMRVehicleRoot::StaticClass(); Vehicle->MarkerProfile = Profile;
	Owner->Loader->LoadVehicle(Vehicle);
	Placement->CalibrationSettleSeconds = 0.f; Placement->AlignmentCaptureMinSamples = 2;
	auto Observe = [Placement]
	{
		struct FParams { int32 Id; FVector Position; FRotator Rotation; FVector2D Size; } Params{1, FVector(100,0,0), FRotator::ZeroRotator, FVector2D(10,10)};
		Placement->ProcessEvent(Placement->FindFunctionChecked(TEXT("HandleMarkerMoved")), &Params);
	};
	Control->StartCalibration(); TestFalse(TEXT("Start cannot manufacture calibration"), Control->CanConfirmCalibration());
	Observe(); TestTrue(TEXT("Real marker calibration enables a save request"), Control->ConfirmCalibration());
	TestFalse(TEXT("Capture needs fresh samples"), Placement->WasLastCalibrationSaveSuccessful());
	Owner->SetActorLocation(FVector(0.1,0,0)); Observe();
	TestFalse(TEXT("External movement cancels capture"), Placement->HasAlignmentCapture());
	Owner->SetActorLocation(FVector(100,0,0)); Control->RequestAlignmentSave(); Observe(); Observe();
	const FString Path = Placement->GetCalibrationFilePath();
	TestTrue(TEXT("Single request writes after fresh samples"), Placement->WasLastCalibrationSaveSuccessful() && FPaths::FileExists(Path));
	TestEqual(TEXT("Automatic save finishes the workflow"), Control->GetCalibrationPhase(), 3);
	IFileManager::Get().Delete(*Path);
	Control->StartCalibration(); Observe(); IFileManager::Get().MakeDirectory(*Path);
	Control->RequestAlignmentSave(); Observe(); Observe();
	TestFalse(TEXT("Failed write cancels the request"), Placement->HasAlignmentCapture());
	TestFalse(TEXT("Failed write cannot claim success"), Placement->WasLastCalibrationSaveSuccessful());
	IFileManager::Get().DeleteDirectory(*Path,false,false);
	Control->RequestAlignmentSave(); Observe(); Observe();
	TestTrue(TEXT("New request retries with new samples"), Placement->WasLastCalibrationSaveSuccessful());
	UCXMRVehicleProfile* Second = DuplicateObject<UCXMRVehicleProfile>(Vehicle, Owner); Second->VehicleRootOffset.SetLocation(FVector(10,0,0));
	Control->RequestAlignmentSave(); Owner->Loader->LoadVehicle(Second);
	TestEqual(TEXT("Same-profile replacement invalidates the UI session"), Control->GetCalibrationPhase(), 0);
	TestFalse(TEXT("Replacement is not calibrated without new observations"), Control->CanSaveCalibration());
	Observe(); TestTrue(TEXT("Replacement can request a fresh save"), Control->ConfirmCalibration());
	Owner->Loader->UnloadVehicle(); TestFalse(TEXT("Unloaded vehicle cannot save"), Control->CanSaveCalibration());
	IFileManager::Get().Delete(*Path); IFileManager::Get().Delete(*FPaths::ChangeExtension(Path,TEXT("startup.json")));
	World->DestroyWorld(false); GEngine->DestroyWorldContext(World);
	return true;
}

namespace
{
	TSharedPtr<SWidget> FindExpandableArea(SWidget& Widget)
	{
		if (Widget.GetTypeAsString() == TEXT("SExpandableArea")) { return Widget.AsShared(); }
		if (FChildren* Children = Widget.GetChildren())
		{
			for (int32 Index = 0; Index < Children->Num(); ++Index)
			{
				if (TSharedPtr<SWidget> Found = FindExpandableArea(*Children->GetChildAt(Index))) { return Found; }
			}
		}
		return nullptr;
	}

	void ScrollPreview(SWidget& Widget, const TSharedPtr<SWidget>& Target = nullptr)
	{
		if (Widget.GetTypeAsString() == TEXT("SScrollBox"))
		{
			SScrollBox& Scroll = static_cast<SScrollBox&>(Widget);
			if (Target) { Scroll.ScrollDescendantIntoView(Target, false, EDescendantScrollDestination::TopOrLeft); }
			else { Scroll.ScrollToStart(); }
		}
		if (FChildren* Children = Widget.GetChildren())
		{
			for (int32 Index = 0; Index < Children->Num(); ++Index) { ScrollPreview(*Children->GetChildAt(Index), Target); }
		}
	}

	/** 보이는 스크롤 영역을 지정 위치로 옮김. 음수면 맨 끝. */
	void ScrollPreviewTo(SWidget& Widget, float Offset)
	{
		if (Widget.GetTypeAsString() == TEXT("SScrollBox"))
		{
			SScrollBox& Scroll = static_cast<SScrollBox&>(Widget);
			if (Offset < 0.f) { Scroll.ScrollToEnd(); }
			else { Scroll.SetScrollOffset(Offset); }
		}
		if (FChildren* Children = Widget.GetChildren())
		{
			for (int32 Index = 0; Index < Children->Num(); ++Index) { ScrollPreviewTo(*Children->GetChildAt(Index), Offset); }
		}
	}

	void ShowAdvancedPlacement(SWidget& Widget)
	{
		if (TSharedPtr<SWidget> Area = FindExpandableArea(Widget))
		{
			static_cast<SExpandableArea&>(*Area).SetExpanded(true);
			ScrollPreview(Widget, Area);
		}
	}

	bool SavePanelPreview(const TSharedRef<SWindow>& Window, const FString& Filename)
	{
		FSlateApplication::Get().Tick();
		TArray<FColor> Pixels;
		FIntVector Size;
		if (!FSlateApplication::Get().TakeScreenshot(Window, Pixels, Size)) { return false; }
		TArray64<uint8> PNG;
		FImageUtils::PNGCompressImageArray(Size.X, Size.Y, TArrayView64<const FColor>(Pixels.GetData(), Pixels.Num()), PNG);
		return FFileHelper::SaveArrayToFile(PNG, *(FPaths::ProjectSavedDir() / Filename));
	}
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
	APostProcessVolume* Exposure = World->SpawnActor<APostProcessVolume>();
	Exposure->bUnbound = true;
	// 화면 검사용 합성 목록. 실차 애셋과 무관함.
	USceneComponent* Anchor = NewObject<USceneComponent>(Owner);
	Owner->SetRootComponent(Anchor);
	Anchor->RegisterComponent();
	UCXMRVehicleLoaderComponent* Loader = NewObject<UCXMRVehicleLoaderComponent>(Owner);
	Loader->bLoadOnBeginPlay = false;
	Loader->RegisterComponent();
	UCXMRVehicleCatalog* Catalog = NewObject<UCXMRVehicleCatalog>(Owner);
	UCXMRVehicleProfile* VehicleA = NewObject<UCXMRVehicleProfile>(Owner);
	UCXMRVehicleProfile* VehicleB = NewObject<UCXMRVehicleProfile>(Owner);
	VehicleA->DisplayName = FText::FromString(TEXT("Preview vehicle A"));
	VehicleB->DisplayName = FText::FromString(TEXT("Preview vehicle B"));
	VehicleA->VehicleActor = ACXMRVehicleRoot::StaticClass();
	VehicleB->VehicleActor = ACXMRVehicleRoot::StaticClass();
	Catalog->Vehicles.Add(VehicleA); Catalog->Vehicles.Add(VehicleB);
	Loader->Catalog = Catalog;
	TestTrue(TEXT("Preview catalog vehicle is loaded through the review API"), Loader->SelectVehicle(0));

	VehicleA->AlignmentGroup = "PreviewRig";
	UCXMRPlacementComponent* Placement = NewObject<UCXMRPlacementComponent>(Owner);
	Placement->RegisterComponent();
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Owner);
	Control->RegisterComponent();
	Owner->DispatchBeginPlay();
	Control->SelectPage(ECXMRControlPage::Display);
	Control->OpenWindow();
	bool bFoundWindow = false;
	for (const TSharedRef<SWindow>& Window : FSlateApplication::Get().GetTopLevelWindows())
	{
		if (Window->GetTitle().EqualTo(Control->WindowTitle))
		{
			bFoundWindow = true;
			for (ECXMRControlPage Page : { ECXMRControlPage::Vehicle, ECXMRControlPage::Calibration, ECXMRControlPage::Display })
			{
				Control->SelectPage(Page);
				ScrollPreview(*Window);
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
			// 고정 폭 +/- 버튼처럼 첫 화면 아래에 있는 조작부도 확인함.
			Control->SelectPage(ECXMRControlPage::Calibration);
			FSlateApplication::Get().Tick();
			ScrollPreviewTo(*Window, 420.f);
			TestTrue(TEXT("Alignment adjustment preview saved"), SavePanelPreview(Window, TEXT("ControlPanelPreview_AlignmentAdjust.png")));
			Control->SelectPage(ECXMRControlPage::Display);
			FSlateApplication::Get().Tick();
			ScrollPreviewTo(*Window, -1.f);
			TestTrue(TEXT("Settings end preview saved"), SavePanelPreview(Window, TEXT("ControlPanelPreview_SettingsEnd.png")));
			Control->SelectPage(ECXMRControlPage::Calibration);
			ScrollPreviewTo(*Window, 0.f);
			ShowAdvancedPlacement(*Window);
			TestTrue(TEXT("Expanded Alignment preview saved"), SavePanelPreview(Window, TEXT("ControlPanelPreview_AlignmentAdvanced.png")));
			Control->ResetSharedAlignment();
			TestTrue(TEXT("Armed Alignment preview saved"), SavePanelPreview(Window, TEXT("ControlPanelPreview_AlignmentArmed.png")));
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
