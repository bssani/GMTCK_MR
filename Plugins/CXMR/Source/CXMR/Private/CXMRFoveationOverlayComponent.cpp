// Copyright GMTCK CX.

#include "CXMRFoveationOverlayComponent.h"
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"

#include "Components/PostProcessComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Materials/MaterialInterface.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "CXMRFoveation"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRFoveation, Log, All);

UCXMRFoveationOverlayComponent::UCXMRFoveationOverlayComponent()
{
	PrimaryComponentTick.bCanEverTick = false;

	// Plugin content referencing itself, defaulted in C++ like the IA_Varjo_* toggles so a fresh clone needs no wiring.
	static ConstructorHelpers::FObjectFinder<UMaterialInterface>
		OverlayFinder(TEXT("/CXMR/Core/Materials/PP_CXMRFoveationVisualization"));
	if (OverlayFinder.Succeeded())
	{
		OverlayMaterial = OverlayFinder.Object;
	}
}

UCXMRSubsystem* UCXMRFoveationOverlayComponent::GetCXMR() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRSubsystem>() : nullptr;
}

UCXMRTuningSubsystem* UCXMRFoveationOverlayComponent::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

void UCXMRFoveationOverlayComponent::BeginPlay()
{
	Super::BeginPlay();

	if (AActor* Owner = GetOwner())
	{
		// Created here rather than as a default subobject so a project can drop this component on any actor.
		Volume = NewObject<UPostProcessComponent>(Owner, TEXT("FoveationOverlayVolume"));
		Volume->bUnbound = true;
		Volume->bEnabled = false;
		if (OverlayMaterial)
		{
			Volume->Settings.WeightedBlendables.Array.Add(FWeightedBlendable(1.0f, OverlayMaterial));
		}
		if (USceneComponent* Root = Owner->GetRootComponent())
		{
			Volume->SetupAttachment(Root);
		}
		Volume->RegisterComponent();
	}

	if (!OverlayMaterial)
	{
		UE_LOG(LogCXMRFoveation, Warning, TEXT("Foveation overlay has no material - I will switch but show nothing."));
	}

	Subsystem = GetCXMR();
	if (Subsystem)
	{
		Subsystem->OnFoveationVisualizationChanged.AddDynamic(this, &UCXMRFoveationOverlayComponent::HandleVisualizationChanged);
		Apply(Subsystem->IsFoveationVisualizationOn());
	}

	RegisterTunables();
}

void UCXMRFoveationOverlayComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (Subsystem)
	{
		Subsystem->OnFoveationVisualizationChanged.RemoveDynamic(this, &UCXMRFoveationOverlayComponent::HandleVisualizationChanged);
	}
	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
	}
	if (Volume)
	{
		Volume->DestroyComponent();
		Volume = nullptr;
	}
	Super::EndPlay(Reason);
}

bool UCXMRFoveationOverlayComponent::IsOverlayApplied() const
{
	return Volume && Volume->bEnabled;
}

void UCXMRFoveationOverlayComponent::Apply(bool bOn)
{
	if (Volume)
	{
		Volume->bEnabled = bOn && OverlayMaterial != nullptr;
	}
}

void UCXMRFoveationOverlayComponent::HandleVisualizationChanged(bool bOn)
{
	Apply(bOn);

	if (bOn && Subsystem && !Subsystem->IsFoveatedRenderingEnabled())
	{
		UE_LOG(LogCXMRFoveation, Warning,
			TEXT("Foveated area overlay is on, but foveated rendering is not running (supported=%s), so nothing will be tinted. "
			     "It needs Rendering Mode = Quad View and Foveated Rendering on in Project Settings > Varjo OpenXR, plus eye tracking."),
			Subsystem->IsFoveatedRenderingSupported() ? TEXT("true") : TEXT("false"));
	}
}

void UCXMRFoveationOverlayComponent::RegisterTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return;
	}

	const FText Category = LOCTEXT("CatEyes", "Eyes");
	auto Make = [this, &Category](FName Id, const FText& Label, ECXMRTunableKind Kind)
	{
		FCXMRTunable Tunable;
		Tunable.Id = Id;
		Tunable.Category = Category;
		Tunable.Label = Label;
		Tunable.Kind = Kind;
		Tunable.Owner = this;
		return Tunable;
	};

	{
		FCXMRTunable T = Make("Eyes.FoveationOverlay", LOCTEXT("FoveationOverlay", "Foveated area overlay (I)"), ECXMRTunableKind::Bool);
		T.Get = [this] { return (Subsystem && Subsystem->IsFoveationVisualizationOn()) ? 1.0f : 0.0f; };
		T.Set = [this](float Value) { if (Subsystem) { Subsystem->SetFoveationVisualization(Value > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Eyes.Foveation", LOCTEXT("FoveatedRendering", "Foveated rendering"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeFoveation(); };
		Tuning->Register(MoveTemp(T));
	}
}

FText UCXMRFoveationOverlayComponent::DescribeFoveation() const
{
	if (!Subsystem)
	{
		return FText::GetEmpty();
	}
	if (Subsystem->IsFoveatedRenderingEnabled())
	{
		return LOCTEXT("Running", "running");
	}
	return Subsystem->IsFoveatedRenderingSupported()
		? LOCTEXT("Off", "off - needs Quad View + Foveated Rendering setting")
		: LOCTEXT("Unsupported", "not supported (or no XR session)");
}

#undef LOCTEXT_NAMESPACE
