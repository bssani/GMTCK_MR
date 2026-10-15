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
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRSpectatorComponent.h"
#include "CXMRHandDebugComponent.h"
#include "CXMRVarjoInputComponent.h"
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
		TestTrue(TEXT("Both APIs see the control window"), UserPanel->IsWindowOpen() && DeveloperPanel->IsWindowOpen());
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
	Control->SelectPage(ECXMRControlPage::Vehicle);
	TestEqual(TEXT("Vehicle remains accessible"),Control->GetActivePage(),ECXMRControlPage::Vehicle);
	TestEqual(TEXT("Control window client size"),Control->WindowSize,FVector2D(480,820));

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
	TSharedPtr<SWidget> CXMRFindPreviewWidget(SWidget& Widget, const FString& Type)
	{
		if (Widget.GetTypeAsString() == Type) { return Widget.AsShared(); }
		if (FChildren* Children = Widget.GetChildren())
		{
			for (int32 Index = 0; Index < Children->Num(); ++Index)
			{
				if (TSharedPtr<SWidget> Found = CXMRFindPreviewWidget(*Children->GetChildAt(Index), Type)) { return Found; }
			}
		}
		return nullptr;
	}

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
	UCXMRMarkerProfile* PreviewMarkers=NewObject<UCXMRMarkerProfile>(Owner);
	PreviewMarkers->CalibrationId=TEXT("ControlWindowPreview_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
	for(int32 Id:{3,7}){FCXMRMarkerEntry Entry;Entry.MarkerId=Id;PreviewMarkers->Markers.Add(Entry);}
	Placement->MarkerProfile=PreviewMarkers;
	Placement->RegisterComponent();
	UCXMRTuningWindowComponent* Control = NewObject<UCXMRTuningWindowComponent>(Owner);
	Control->RegisterComponent();
	UCXMRHandDebugComponent* Hands=NewObject<UCXMRHandDebugComponent>(Owner);Hands->RegisterComponent();
	UCXMRSpectatorComponent* Spectator=NewObject<UCXMRSpectatorComponent>(Owner);Spectator->RegisterComponent();
	UCXMRVarjoInputComponent* Input=NewObject<UCXMRVarjoInputComponent>(Owner);Input->RegisterComponent();
	Owner->DispatchBeginPlay();
	Control->SelectPage(ECXMRControlPage::Calibration);
	Control->OpenWindow();
	UCXMRSubsystem* CXMR=GI->GetSubsystem<UCXMRSubsystem>();
	CXMR->SessionRunningForTest=false;
	UCXMRTuningSubsystem* Registry=GI->GetSubsystem<UCXMRTuningSubsystem>();
	for(const FCXMRTunable* T:Registry->GetTunables()){if(T->Set){Registry->ApplyValue(T->Id,T->Default,false);}}
	CXMR->SessionRunningForTest=true; CXMR->SetMixedReality(true);
	TSharedPtr<SWindow> Main;
	for(const auto& W:FSlateApplication::Get().GetTopLevelWindows()){if(W->GetTitle().EqualTo(Control->WindowTitle)){Main=W;break;}}
	if(TestTrue(TEXT("Main window exists"),Main.IsValid()))
	{
		Main->SetDPIScaleFactor(1.f); Main->Resize(FVector2D(480,820));
		const TCHAR* Names[]={TEXT("Starting"),TEXT("Waiting"),TEXT("World"),TEXT("Markers"),TEXT("Adjusted"),TEXT("NoHeadset")};
		for(int I=0;I<6;++I)
		{
			FCXMRControlStatus State; State.RequiredMarkers={3,7}; State.MissingMarkers={};
			State.Stage=I==0?ECXMRControlStage::Starting:I==1?ECXMRControlStage::Waiting:I==4?ECXMRControlStage::Adjusted:I==5?ECXMRControlStage::NoHeadset:ECXMRControlStage::Aligned;
			State.Restore=I==3?ECXMRControlRestore::Markers:ECXMRControlRestore::World; State.OriginMovement=I==3?1.4f:0.12f;
			if(I==1){State.MissingMarkers={7};}else if(I==0 || I==5){State.MissingMarkers={3,7};}
			Placement->ControlStatusForTest=State;
			if(I==4){Placement->AdjustmentSinceSave=FVector(1,-0.5,0); Placement->TurnSinceSave=0.5f;}
			CXMR->SetMixedReality(I!=5);
			FSlateApplication::Get().Tick(); ScrollPreviewTo(*Main,0.f);
			TestTrue(TEXT("Scenario preview saved"),SavePanelPreview(Main.ToSharedRef(),FString::Printf(TEXT("ControlWindowPreview_%s.png"),Names[I])));
		}
		FCXMRControlStatus State;State.RequiredMarkers={3,7};State.Stage=ECXMRControlStage::Adjusted;Placement->ControlStatusForTest=State;CXMR->SetMixedReality(true);
		FSlateApplication::Get().Tick(); ScrollPreviewTo(*Main,80.f); FSlateApplication::Get().Tick();
		const TSharedPtr<SWidget> ScrollWidget=CXMRFindPreviewWidget(*Main,TEXT("SScrollBox"));
		if(TestTrue(TEXT("Main content scroll exists"),ScrollWidget.IsValid()))
		{
			const TSharedPtr<SWidget> Setup=CXMRFindPreviewWidget(*ScrollWidget,TEXT("SExpandableArea"));
			SScrollBox& Scroll=static_cast<SScrollBox&>(*ScrollWidget);
			const float Offset=Scroll.GetScrollOffset();
			TestTrue(TEXT("Main content was scrolled down"),Offset>0.f);
			State.MissingMarkers={7}; Placement->ControlStatusForTest=State; FSlateApplication::Get().Tick();
			TestTrue(TEXT("Missing markers retain the scroll widget"),CXMRFindPreviewWidget(*Main,TEXT("SScrollBox"))==ScrollWidget);
			TestTrue(TEXT("Missing markers retain the content widgets"),CXMRFindPreviewWidget(*Main,TEXT("SExpandableArea"))==Setup);
			TestEqual(TEXT("Missing markers retain the scroll offset"),Scroll.GetScrollOffset(),Offset);
			State.MissingMarkers.Reset(); Placement->ControlStatusForTest=State; FSlateApplication::Get().Tick();
			TestEqual(TEXT("Reacquired markers retain the scroll offset"),Scroll.GetScrollOffset(),Offset);
			State.Stage=ECXMRControlStage::Saving; Placement->ControlStatusForTest=State; FSlateApplication::Get().Tick();
			TestTrue(TEXT("Stage change retains the scroll widget"),CXMRFindPreviewWidget(*Main,TEXT("SScrollBox"))==ScrollWidget);
			TestTrue(TEXT("Stage change rebuilds the button layout"),CXMRFindPreviewWidget(*Main,TEXT("SExpandableArea"))!=Setup);
			State.Stage=ECXMRControlStage::Adjusted; Placement->ControlStatusForTest=State;
		}
		FSlateApplication::Get().Tick(); ScrollPreviewTo(*Main,-1.f);
		TestTrue(TEXT("Control lower preview saved"),SavePanelPreview(Main.ToSharedRef(),TEXT("ControlWindowPreview_Adjusted_End.png")));
		ShowAdvancedPlacement(*Main);
		TestTrue(TEXT("First-time setup preview saved"),SavePanelPreview(Main.ToSharedRef(),TEXT("ControlWindowPreview_Setup.png")));
		Control->SelectPage(ECXMRControlPage::Vehicle); FSlateApplication::Get().Tick(); ScrollPreviewTo(*Main,0.f);
		TestTrue(TEXT("Vehicle tab preview saved"),SavePanelPreview(Main.ToSharedRef(),TEXT("ControlWindowPreview_Vehicle.png")));
		for(float Scale:{1.25f,1.5f})
		{
			Main->SetDPIScaleFactor(Scale);Main->Resize(FVector2D(480,820)*Scale); FSlateApplication::Get().Tick();
			TestTrue(TEXT("Scaled main preview saved"),SavePanelPreview(Main.ToSharedRef(),FString::Printf(TEXT("ControlWindowPreview_Main_%d.png"),FMath::RoundToInt(Scale*100))));
		}
	}
	Control->OpenAdvancedWindow();Control->OpenAdvancedWindow();
	TSharedPtr<SWindow> Advanced;int AdvancedCount=0;
	for(const auto& W:FSlateApplication::Get().GetTopLevelWindows()){if(W->GetTitle().ToString()==TEXT("CXMR Advanced settings")){Advanced=W;++AdvancedCount;}}
	TestEqual(TEXT("Advanced window is single"),AdvancedCount,1);
	if(Advanced.IsValid())
	{
		Advanced->SetDPIScaleFactor(1.f);Advanced->Resize(FVector2D(560,760));
		const TCHAR* Names[]={TEXT("MR"),TEXT("VR"),TEXT("Calibration"),TEXT("Diagnostics")};
		for(int I=0;I<4;++I)
		{
			Control->AdvancedTab=I; FSlateApplication::Get().Tick();ScrollPreviewTo(*Advanced,0.f);
			TestTrue(TEXT("Advanced tab preview saved"),SavePanelPreview(Advanced.ToSharedRef(),FString::Printf(TEXT("ControlWindowPreview_Advanced_%s.png"),Names[I])));
			ScrollPreviewTo(*Advanced,440.f);
			TestTrue(TEXT("Advanced middle preview saved"),SavePanelPreview(Advanced.ToSharedRef(),FString::Printf(TEXT("ControlWindowPreview_Advanced_%s_Middle.png"),Names[I])));
			ScrollPreviewTo(*Advanced,-1.f);
			TestTrue(TEXT("Advanced lower preview saved"),SavePanelPreview(Advanced.ToSharedRef(),FString::Printf(TEXT("ControlWindowPreview_Advanced_%s_End.png"),Names[I])));
		}
		Control->AdvancedTab=3;const auto Position=Advanced->GetPositionInScreen();
		Advanced->RequestDestroyWindow(); FSlateApplication::Get().Tick();
		TestFalse(TEXT("X hides advanced window"),Control->IsAdvancedWindowOpen());Control->OpenAdvancedWindow();
		TestEqual(TEXT("Advanced tab survives reopen"),Control->AdvancedTab,3);
		TestTrue(TEXT("Advanced position survives reopen"),Advanced->GetPositionInScreen().Equals(Position));
		for(float Scale:{1.25f,1.5f})
		{
			Advanced->SetDPIScaleFactor(Scale);Advanced->Resize(FVector2D(560,760)*Scale);ScrollPreviewTo(*Advanced,-1.f);
			TestTrue(TEXT("Scaled advanced preview saved"),SavePanelPreview(Advanced.ToSharedRef(),FString::Printf(TEXT("ControlWindowPreview_Advanced_%d.png"),FMath::RoundToInt(Scale*100))));
		}
	}
	Control->CloseWindow();TestFalse(TEXT("Closing main also closes advanced"),Control->IsAdvancedWindowOpen());
	Hands->DestroyComponent();Spectator->DestroyComponent();

	Owner->Destroy();
	GI->Shutdown();
	World->DestroyWorld(false);
	GEngine->DestroyWorldContext(World);
	return true;
}
#endif
