// Copyright GMTCK CX.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/App.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Components/SceneComponent.h"
#include "CXMRDesktopPanelComponent.h"
#include "CXMRTuningWindowComponent.h"
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
	TestTrue(TEXT("User button runs during Play"), UserButton->TryExecuteToolUIAction(Context));
	TestTrue(TEXT("Developer button runs during Play"), DeveloperButton->TryExecuteToolUIAction(Context));
	if (FApp::CanEverRender())
	{
		TestTrue(TEXT("Both panels opened"), UserPanel->IsWindowOpen() && DeveloperPanel->IsWindowOpen());
		// 창의 X를 눌렀을 때와 같은 경로로 닫음. 다시 열리는지도 확인함.
		const TArray<TSharedRef<SWindow>> Windows = FSlateApplication::Get().GetTopLevelWindows();
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
#endif
