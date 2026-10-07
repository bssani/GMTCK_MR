// Copyright GMTCK CX.

#include "CXMRDesktopPanelComponent.h"
#include "CXMRControlPanelWidget.h"
#include "CXMRTuningWindowComponent.h"
#include "GameFramework/Actor.h"

#include "Blueprint/UserWidget.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "Misc/App.h"
#include "UObject/ConstructorHelpers.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRDesktop, Log, All);

UCXMRDesktopPanelComponent::UCXMRDesktopPanelComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	WindowTitle = NSLOCTEXT("CXMR", "DesktopPanelTitle", "CXMR Control");

	// PIE에서 BP 기본값이 비워질 수 있어 C++에서 패널 클래스를 지정함.

	PanelClass = UCXMRControlPanelWidget::StaticClass();
}

void UCXMRDesktopPanelComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bOpenOnBeginPlay)
	{
		OpenWindow();
	}
}

void UCXMRDesktopPanelComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	// PIE 종료 시 창도 닫음. Slate 창은 GC가 정리하지 않음.

	CloseWindow();

	Super::EndPlay(Reason);
}

bool UCXMRDesktopPanelComponent::IsWindowOpen() const
{
	if (const UCXMRTuningWindowComponent* Control = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRTuningWindowComponent>() : nullptr)
	{
		return Control->IsWindowOpen();
	}
	return Window.IsValid();
}

void UCXMRDesktopPanelComponent::OpenWindow()
{
	if (UCXMRTuningWindowComponent* Control = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRTuningWindowComponent>() : nullptr)
	{
		Control->OpenWindow();
		return;
	}
	if (Window.IsValid())
	{
		Window->BringToFront();
		return;
	}

	// Slate를 사용할 수 없으면 창을 열지 않음.
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized())
	{
		UE_LOG(LogCXMRDesktop, Log, TEXT("Desktop panel skipped: this build has no Slate application."));
		return;
	}

	if (!PanelClass)
	{
		UE_LOG(LogCXMRDesktop, Warning,
			TEXT("Desktop panel will NOT appear: PanelClass is unset on %s."), *GetNameSafe(GetOwner()));
		return;
	}

	// 데스크톱 패널에서 같은 상태 사용함.

	PanelWidget = CreateWidget<UUserWidget>(GetWorld(), PanelClass);
	if (!PanelWidget)
	{
		UE_LOG(LogCXMRDesktop, Warning, TEXT("Desktop panel: could not create widget of class %s."),
			*GetNameSafe(PanelClass));
		return;
	}

	Window = SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(WindowSize)
		.SupportsMaximize(false)
		.SupportsMinimize(true)
		.AutoCenter(EAutoCenter::PrimaryWorkArea);

	Window->SetContent(PanelWidget->TakeWidget());

	// X로 닫으면 창 참조도 비움. 다시 열 때 필요함.

	Window->SetOnWindowClosed(FOnWindowClosed::CreateWeakLambda(this,
		[this](const TSharedRef<SWindow>&)
		{
			Window.Reset();
			PanelWidget = nullptr;
		}));

	// 게임 창을 가리지 않게 기본 위치로 열음.
	FSlateApplication::Get().AddWindow(Window.ToSharedRef(), /*bShowImmediately*/ true);

	UE_LOG(LogCXMRDesktop, Log, TEXT("Desktop panel opened (%s, %.0fx%.0f)."),
		*GetNameSafe(PanelClass), WindowSize.X, WindowSize.Y);
}

void UCXMRDesktopPanelComponent::CloseWindow()
{
	if (UCXMRTuningWindowComponent* Control = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRTuningWindowComponent>() : nullptr)
	{
		Control->CloseWindow();
	}
	if (Window.IsValid())
	{
		Window->SetOnWindowClosed(FOnWindowClosed());   // 종료 중 콜백 재진입 방지함.
		Window->RequestDestroyWindow();
		Window.Reset();
	}
	PanelWidget = nullptr;
}

void UCXMRDesktopPanelComponent::ToggleWindow()
{
	if (IsWindowOpen())
	{
		CloseWindow();
	}
	else
	{
		OpenWindow();
	}
}
