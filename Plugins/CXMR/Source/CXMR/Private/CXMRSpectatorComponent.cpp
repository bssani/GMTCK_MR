// Copyright GMTCK CX.

#include "CXMRSpectatorComponent.h"
#include "CXMRHandTracking.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRVehicleRoot.h"

#include "Components/SceneCaptureComponent2D.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HeadMountedDisplayTypes.h"
#include "IHeadMountedDisplay.h"
#include "ISpectatorScreenController.h"
#include "IXRTrackingSystem.h"

#define LOCTEXT_NAMESPACE "CXMRSpectator"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRSpectator, Log, All);

namespace
{
	/** The headset's spectator screen, or null without an enabled headset (PIE on a monitor, no XR runtime). */
	ISpectatorScreenController* FindSpectatorScreenController()
	{
		if (!GEngine || !GEngine->XRSystem.IsValid())
		{
			return nullptr;
		}
		IHeadMountedDisplay* HMD = GEngine->XRSystem->GetHMDDevice();
		return (HMD && HMD->IsHMDEnabled()) ? HMD->GetSpectatorScreenController() : nullptr;
	}
}

UCXMRSpectatorComponent::UCXMRSpectatorComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;   // runs only while a camera mode is on
	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;   // after everything has moved this frame
}

UCXMRTuningSubsystem* UCXMRSpectatorComponent::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

void UCXMRSpectatorComponent::BeginPlay()
{
	Super::BeginPlay();

	// A saved mode comes back through its row, which starts the camera.
	RegisterTunables();
}

void UCXMRSpectatorComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->UnregisterOwner(this);
	}
	StopCapture();
	Super::EndPlay(Reason);
}

void UCXMRSpectatorComponent::SetMode(ECXMRSpectatorMode NewMode)
{
	if (Mode == NewMode)
	{
		return;
	}
	Mode = NewMode;

	if (Mode == ECXMRSpectatorMode::Off)
	{
		StopCapture();
	}
	else
	{
		bFollowStarted = false;

		FTransform Head;
		if (Mode == ECXMRSpectatorMode::Orbit && CXMRHands::GetHeadTransform(GetWorld(), Head))
		{
			FVector Forward = Head.GetRotation().GetForwardVector();
			Forward.Z = 0.0;
			if (!Forward.Normalize())
			{
				Forward = FVector::ForwardVector;
			}
			FallbackOrbitCentre = Head.GetLocation() + Forward * 300.0;

			// Start the orbit from where the wearer stands, so the picture does not jump to the far side.
			FVector Centre = FallbackOrbitCentre;
			double GroundZ = 0.0;
			FindOrbitTarget(Centre, GroundZ);
			OrbitAngle = static_cast<float>((Head.GetLocation() - Centre).Rotation().Yaw);
		}
		StartCapture();
	}

	SetComponentTickEnabled(Mode != ECXMRSpectatorMode::Off);

	UE_LOG(LogCXMRSpectator, Log, TEXT("Spectator camera: %s (%s)"),
		*UEnum::GetDisplayValueAsText(Mode).ToString(),
		Mode == ECXMRSpectatorMode::Off ? TEXT("headset mirror")
			: bOnSpectatorScreen ? TEXT("on the spectator screen") : TEXT("no headset spectator screen - render target only"));
}

void UCXMRSpectatorComponent::StartCapture()
{
	AActor* Owner = GetOwner();
	if (!Owner)
	{
		return;
	}

	if (!Capture)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);
		RenderTarget->RenderTargetFormat = RTF_RGBA8;
		// A new render target forces linear gamma and InitAutoFormat does not undo it, so the tonemapper would write linear
		// values into 8 bits and the monitor picture came out as dark as viewport^2.2 (measured). Encode like the back buffer.
		RenderTarget->TargetGamma = 2.2f;
		RenderTarget->ClearColor = FLinearColor::Black;
		RenderTarget->InitAutoFormat(FMath::Max(Resolution.X, 64), FMath::Max(Resolution.Y, 64));
		RenderTarget->UpdateResourceImmediate(true);

		Capture = NewObject<USceneCaptureComponent2D>(Owner);
		if (USceneComponent* Root = Owner->GetRootComponent())
		{
			Capture->SetupAttachment(Root);
		}
		// Placed in the world every frame; it must not ride along with the pawn on top of that.
		Capture->SetUsingAbsoluteLocation(true);
		Capture->SetUsingAbsoluteRotation(true);
		Capture->SetUsingAbsoluteScale(true);
		Capture->CaptureSource = ESceneCaptureSource::SCS_FinalColorLDR;
		Capture->TextureTarget = RenderTarget;
		Capture->FOVAngle = FieldOfView;
		Capture->bCaptureEveryFrame = true;   // before registering: that is when the scene picks it up
		Capture->bCaptureOnMovement = false;
		Capture->RegisterComponent();
	}

	if (ISpectatorScreenController* Screen = FindSpectatorScreenController())
	{
		if (!bOnSpectatorScreen)
		{
			PreviousScreenMode = static_cast<uint8>(Screen->GetSpectatorScreenMode());
		}
		Screen->SetSpectatorScreenTexture(RenderTarget);
		Screen->SetSpectatorScreenMode(ESpectatorScreenMode::Texture);
		bOnSpectatorScreen = true;
	}
}

void UCXMRSpectatorComponent::StopCapture()
{
	if (bOnSpectatorScreen)
	{
		if (ISpectatorScreenController* Screen = FindSpectatorScreenController())
		{
			Screen->SetSpectatorScreenMode(static_cast<ESpectatorScreenMode>(PreviousScreenMode));
			Screen->SetSpectatorScreenTexture(nullptr);
		}
		bOnSpectatorScreen = false;
	}

	if (Capture)
	{
		Capture->DestroyComponent();
		Capture = nullptr;
	}
	RenderTarget = nullptr;   // left to garbage collection; the next start makes a new one at the current resolution
}

void UCXMRSpectatorComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	if (!Capture)
	{
		return;
	}
	Capture->FOVAngle = FieldOfView;

	if (Mode == ECXMRSpectatorMode::FirstPerson)
	{
		UpdateFirstPerson(DeltaTime);
	}
	else if (Mode == ECXMRSpectatorMode::Orbit)
	{
		UpdateOrbit(DeltaTime);
	}
}

void UCXMRSpectatorComponent::UpdateFirstPerson(float DeltaTime)
{
	FTransform Head;
	if (!CXMRHands::GetHeadTransform(GetWorld(), Head))
	{
		return;
	}

	// Roll dropped: a tilting horizon is what makes a mirrored headset view hard to watch.
	FRotator Level = Head.Rotator();
	Level.Roll = 0.0;

	if (!bFollowStarted)
	{
		Capture->SetWorldLocationAndRotation(Head.GetLocation(), Level);
		bFollowStarted = true;
		return;
	}

	// Exponential smoothing: the same calm at any frame rate.
	const float Alpha = 1.0f - FMath::Exp(-FollowSpeed * DeltaTime);
	const FVector Location = FMath::Lerp(Capture->GetComponentLocation(), Head.GetLocation(), static_cast<double>(Alpha));
	const FQuat Rotation = FQuat::Slerp(Capture->GetComponentQuat(), Level.Quaternion(), Alpha);
	Capture->SetWorldLocationAndRotation(Location, Rotation);
}

void UCXMRSpectatorComponent::UpdateOrbit(float DeltaTime)
{
	FVector Centre;
	double GroundZ = 0.0;
	if (!FindOrbitTarget(Centre, GroundZ))
	{
		Centre = FallbackOrbitCentre;
		GroundZ = FallbackOrbitCentre.Z - OrbitHeight;   // keeps the camera at the height the orbit started from
	}

	OrbitAngle = FMath::Fmod(OrbitAngle + OrbitDegreesPerSecond * DeltaTime, 360.0f);

	FVector Location = Centre + FRotator(0.0, OrbitAngle, 0.0).Vector() * OrbitDistance;
	Location.Z = GroundZ + OrbitHeight;
	Capture->SetWorldLocationAndRotation(Location, (Centre - Location).Rotation());
}

bool UCXMRSpectatorComponent::FindOrbitTarget(FVector& OutCentre, double& OutGroundZ) const
{
	const UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	for (TActorIterator<ACXMRVehicleRoot> It(World); It; ++It)
	{
		FVector Origin;
		FVector Extent;
		It->GetActorBounds(/*bOnlyCollidingComponents*/ false, Origin, Extent, /*bIncludeFromChildActors*/ true);
		if (Extent.IsNearlyZero())
		{
			Origin = It->GetActorLocation();
		}
		OutCentre = Origin;
		OutGroundZ = Origin.Z - Extent.Z;
		return true;
	}
	return false;
}

// ---- Tuning window rows ----

void UCXMRSpectatorComponent::RegisterTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	if (!Tuning)
	{
		return;
	}

	const FText Category = LOCTEXT("CatMonitor", "Monitor");
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

	// Sizes first: a saved mode starts the camera the moment its row registers.
	{
		FCXMRTunable T = Make("Spectator.FieldOfView", LOCTEXT("FieldOfView", "Monitor field of view"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("Degrees", "deg"); T.Min = 40.0f; T.Max = 120.0f; T.Delta = 1.0f; T.Default = 90.0f; T.bPersist = true;
		T.Get = [this] { return FieldOfView; };
		T.Set = [this](float Value) { FieldOfView = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Spectator.FollowSpeed", LOCTEXT("FollowSpeed", "Smoothing (lower = calmer)"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("PerSecond", "per s"); T.Min = 0.5f; T.Max = 20.0f; T.Delta = 0.1f; T.Default = 4.0f; T.bPersist = true;
		T.Get = [this] { return FollowSpeed; };
		T.Set = [this](float Value) { FollowSpeed = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Spectator.OrbitDistance", LOCTEXT("OrbitDistance", "Orbit distance"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("cm", "cm"); T.Min = 150.0f; T.Max = 2000.0f; T.Delta = 10.0f; T.Default = 600.0f; T.bPersist = true;
		T.Get = [this] { return OrbitDistance; };
		T.Set = [this](float Value) { OrbitDistance = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Spectator.OrbitSpeed", LOCTEXT("OrbitSpeed", "Orbit speed"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("DegreesPerSecond", "deg/s"); T.Min = -45.0f; T.Max = 45.0f; T.Delta = 0.5f; T.Default = 8.0f; T.bPersist = true;
		T.Get = [this] { return OrbitDegreesPerSecond; };
		T.Set = [this](float Value) { OrbitDegreesPerSecond = Value; };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Spectator.Mode", LOCTEXT("Mode", "Monitor shows"), ECXMRTunableKind::Choice);
		T.Options = { LOCTEXT("ModeOff", "Headset mirror"), LOCTEXT("ModeFirstPerson", "Wearer's view, smoothed"), LOCTEXT("ModeOrbit", "Orbit the vehicle") };
		T.Default = 0.0f; T.bPersist = true;
		T.Get = [this] { return static_cast<float>(Mode); };
		T.Set = [this](float Value) { SetMode(static_cast<ECXMRSpectatorMode>(FMath::Clamp(FMath::RoundToInt(Value), 0, 2))); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Spectator.Output", LOCTEXT("Output", "Monitor picture"), ECXMRTunableKind::Readout);
		T.Text = [this] { return DescribeOutput(); };
		Tuning->Register(MoveTemp(T));
	}
}

FText UCXMRSpectatorComponent::DescribeOutput() const
{
	if (Mode == ECXMRSpectatorMode::Off || !RenderTarget)
	{
		return LOCTEXT("OutputMirror", "headset mirror (no extra rendering)");
	}
	if (bOnSpectatorScreen)
	{
		return FText::FromString(FString::Printf(TEXT("camera on the monitor, %d x %d"), RenderTarget->SizeX, RenderTarget->SizeY));
	}
	return LOCTEXT("OutputNoScreen", "rendering, but no headset spectator screen to show it on");
}

#undef LOCTEXT_NAMESPACE
