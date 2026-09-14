// Copyright GMTCK CX.

#include "CXMRDesktopPanelComponent.h"
#include "CXMRControlPanelWidget.h"

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

	// Same reason the pawn resolves its panel class in C++: a Blueprint class default silently
	// reverts to null when PIE reinstances the Blueprint, and a panel that fails to appear looks
	// like the feature was never built. The panel is a C++ widget, so there is no asset to find.
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
	// Must happen here. A Slate window is not garbage collected, so skipping this leaves the panel
	// floating on the editor desktop after PIE stops — and the next run opens a second one.
	CloseWindow();

	Super::EndPlay(Reason);
}

bool UCXMRDesktopPanelComponent::IsWindowOpen() const
{
	return Window.IsValid();
}

void UCXMRDesktopPanelComponent::OpenWindow()
{
	if (Window.IsValid())
	{
		Window->BringToFront();
		return;
	}

	// Commandlets and dedicated servers have no Slate. Asking for a window there is a crash.
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

	// A second instance of the same widget class. Both talk only to the subsystem, so this one and
	// the hand-held one mirror each other without any wiring between them.
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

	// Closing from the title bar must clear our handle, or IsWindowOpen lies and OpenWindow tries
	// to bring a destroyed window to the front.
	Window->SetOnWindowClosed(FOnWindowClosed::CreateWeakLambda(this,
		[this](const TSharedRef<SWindow>&)
		{
			Window.Reset();
			PanelWidget = nullptr;
		}));

	// bShowImmediately = false: let Slate place it normally rather than forcing it over the game.
	FSlateApplication::Get().AddWindow(Window.ToSharedRef(), /*bShowImmediately*/ true);

	UE_LOG(LogCXMRDesktop, Log, TEXT("Desktop panel opened (%s, %.0fx%.0f)."),
		*GetNameSafe(PanelClass), WindowSize.X, WindowSize.Y);
}

void UCXMRDesktopPanelComponent::CloseWindow()
{
	if (Window.IsValid())
	{
		Window->SetOnWindowClosed(FOnWindowClosed());   // do not re-enter the lambda while tearing down
		Window->RequestDestroyWindow();
		Window.Reset();
	}
	PanelWidget = nullptr;
}

void UCXMRDesktopPanelComponent::ToggleWindow()
{
	if (Window.IsValid())
	{
		CloseWindow();
	}
	else
	{
		OpenWindow();
	}
}
