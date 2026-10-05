// Copyright GMTCK CX.

#include "Modules/ModuleManager.h"
#include "CXMRDesktopPanelComponent.h"
#include "CXMRTuningWindowComponent.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "ToolMenus.h"
#include "Styling/AppStyle.h"

#define LOCTEXT_NAMESPACE "CXMREditor"

namespace
{
	template<typename T> T* FindPlayComponent()
	{
		if (GEditor && GEditor->PlayWorld)
		{
			for (TActorIterator<AActor> It(GEditor->PlayWorld); It; ++It)
			{
				if (T* Component = It->FindComponentByClass<T>())
				{
					return Component;
				}
			}
		}
		return nullptr;
	}
}

class FCXMREditorModule : public IModuleInterface
{
public:
	virtual void StartupModule() override
	{
		UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateRaw(this, &FCXMREditorModule::RegisterMenus));
	}

	virtual void ShutdownModule() override
	{
		UToolMenus::UnRegisterStartupCallback(this);
		UToolMenus::UnregisterOwner(this);
	}

private:
	void RegisterMenus()
	{
		FToolMenuOwnerScoped Owner(this);
		UToolMenu* Toolbar = UToolMenus::Get()->ExtendMenu("LevelEditor.LevelEditorToolBar.PlayToolBar");
		FToolMenuSection& Section = Toolbar->FindOrAddSection("CXMRPanels");
		Section.AddEntry(FToolMenuEntry::InitToolBarButton("CXMRUserPanel",
			FToolUIAction(FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)
			{
				if (UCXMRDesktopPanelComponent* Panel = FindPlayComponent<UCXMRDesktopPanelComponent>()) { Panel->OpenWindow(); }
			}), FToolMenuCanExecuteAction::CreateLambda([](const FToolMenuContext&) { return FindPlayComponent<UCXMRDesktopPanelComponent>() != nullptr; }), FToolMenuGetActionCheckState()),
			LOCTEXT("UserPanel", "User Panel"),
			LOCTEXT("UserPanelTip", "Open or focus the CXMR user panel. Start Play first."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Visible")));
		Section.AddEntry(FToolMenuEntry::InitToolBarButton("CXMRDeveloperPanel",
			FToolUIAction(FToolMenuExecuteAction::CreateLambda([](const FToolMenuContext&)
			{
				if (UCXMRTuningWindowComponent* Panel = FindPlayComponent<UCXMRTuningWindowComponent>()) { Panel->OpenWindow(); }
			}), FToolMenuCanExecuteAction::CreateLambda([](const FToolMenuContext&) { return FindPlayComponent<UCXMRTuningWindowComponent>() != nullptr; }), FToolMenuGetActionCheckState()),
			LOCTEXT("DeveloperPanel", "Developer Panel"),
			LOCTEXT("DeveloperPanelTip", "Open or focus CXMR calibration, settings and diagnostics. Start Play first."),
			FSlateIcon(FAppStyle::GetAppStyleSetName(), "Icons.Settings")));
	}
};

IMPLEMENT_MODULE(FCXMREditorModule, CXMREditor)

#undef LOCTEXT_NAMESPACE
