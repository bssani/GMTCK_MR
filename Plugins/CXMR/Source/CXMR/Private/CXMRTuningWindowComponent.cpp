// Copyright GMTCK CX.

#include "CXMRTuningWindowComponent.h"
#include "CXMRPanelUI.h"
#include "SCXMRControlPanel.h"
#include "CXMRControlPanelWidget.h"
#include "CXMRPlacementComponent.h"
#include "CXMRMarkerProfile.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "CXMRMarkerDebugComponent.h"
#include "CXMRSubsystem.h"
#include "CXMRTuningSubsystem.h"
#include "CXMRVarjoInputComponent.h"

#include "Engine/GameInstance.h"
#include "Engine/Engine.h"
#include "IXRTrackingSystem.h"
#include "TimerManager.h"
#include "Engine/PostProcessVolume.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/Pawn.h"
#include "HAL/IConsoleManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Widgets/SWindow.h"

#define LOCTEXT_NAMESPACE "CXMRTuning"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRTuningWindow, Log, All);

static FAutoConsoleCommandWithWorld GCXMRTuningWindow(
	TEXT("CXMR.Tuning"),
	TEXT("Open or close the tuning window."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
		{
			if (UCXMRTuningWindowComponent* Tuning = Pawn->FindComponentByClass<UCXMRTuningWindowComponent>())
			{
				Tuning->ToggleWindow();
			}
		}
	}));

namespace
{
	float AsValue(bool bOn) { return bOn ? 1.0f : 0.0f; }

	/** 노출 조정에 사용할 Unbound 볼륨 찾음. */
	APostProcessVolume* FindUnboundVolume(UWorld* World)
	{
		if (World)
		{
			for (TActorIterator<APostProcessVolume> It(World); It; ++It)
			{
				if (It->bUnbound)
				{
					return *It;
				}
			}
		}
		return nullptr;
	}
}

UCXMRTuningWindowComponent::UCXMRTuningWindowComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	WindowTitle = LOCTEXT("WindowTitle", "CXMR Control");
	CalibrationMessage = LOCTEXT("CalibrationReady", "First setup: initial alignment, fine adjustment, confirm and save. Later sessions restore from markers.");
}

UCXMRTuningSubsystem* UCXMRTuningWindowComponent::GetTuning() const
{
	const UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	return GI ? GI->GetSubsystem<UCXMRTuningSubsystem>() : nullptr;
}

void UCXMRTuningWindowComponent::BeginPlay()
{
	Super::BeginPlay();
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UCXMRSubsystem* CXMR = GI->GetSubsystem<UCXMRSubsystem>()) { CXMR->OnPlaceRequested.AddDynamic(this, &UCXMRTuningWindowComponent::HandleManualPlacementRequest); }
	}

	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		// 기능 등록이 바뀌면 열린 창도 갱신함.
		TunablesChangedHandle = Tuning->OnTunablesChanged.AddUObject(this, &UCXMRTuningWindowComponent::RebuildContent);
	}
	RegisterCoreTunables();
	RebuildContent(); // 창이 먼저 열렸어도 등록한 설정 반영함.

	if (bOpenOnBeginPlay)
	{
		OpenWindow();
	}
}

void UCXMRTuningWindowComponent::EndPlay(const EEndPlayReason::Type Reason)
{
	// PIE 종료 시 Slate 창도 닫음.
	CancelInitialAlignment();
	CloseWindow();
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UCXMRSubsystem* CXMR = GI->GetSubsystem<UCXMRSubsystem>()) { CXMR->OnPlaceRequested.RemoveDynamic(this, &UCXMRTuningWindowComponent::HandleManualPlacementRequest); }
	}

	if (UCXMRTuningSubsystem* Tuning = GetTuning())
	{
		Tuning->OnTunablesChanged.Remove(TunablesChangedHandle);
		Tuning->UnregisterOwner(this);
	}
	Super::EndPlay(Reason);
}

void UCXMRTuningWindowComponent::RegisterCoreTunables()
{
	UCXMRTuningSubsystem* Tuning = GetTuning();
	UWorld* World = GetWorld();
	const UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
	UCXMRSubsystem* CXMR = GI ? GI->GetSubsystem<UCXMRSubsystem>() : nullptr;
	if (!Tuning || !CXMR)
	{
		return;
	}

	const TWeakObjectPtr<UCXMRSubsystem> Weak(CXMR);
	auto Make = [this](FName Id, const FText& Category, const FText& Label, ECXMRTunableKind Kind)
	{
		FCXMRTunable Tunable;
		Tunable.Id = Id;
		Tunable.Category = Category;
		Tunable.Label = Label;
		Tunable.Kind = Kind;
		Tunable.Owner = this;
		return Tunable;
	};

	// 실물 화면
	const FText MR = LOCTEXT("CatMR", "Mixed reality");
	{
		FCXMRTunable T = Make("MR.MixedReality", MR, LOCTEXT("MixedReality", "Mixed reality (passthrough)"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsMixedRealityOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetMixedReality(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("MR.VRBackground", MR, LOCTEXT("VRBackground", "VR background (sky, floor)"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsVRBackgroundVisible()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetVRBackgroundVisible(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("MR.Masking", MR, LOCTEXT("Masking", "Masking (mask meshes cut through)"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsMaskingOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetMasking(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("MR.ViewOffset", MR, LOCTEXT("ViewOffset", "View offset 0 eye / 1 cam"), ECXMRTunableKind::Float);
		T.Min = 0.0f; T.Max = 1.0f; T.Delta = 0.05f; T.Default = 1.0f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->GetViewOffset() : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetViewOffset(V); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("MR.ViewOffsetGlide", MR, LOCTEXT("ViewOffsetGlide", "View offset glide (K)"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("Seconds", "s"); T.Min = 0.0f; T.Max = 2.0f; T.Delta = 0.05f; T.Default = 0.5f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->ViewOffsetTransitionSeconds : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->ViewOffsetTransitionSeconds = V; } };
		Tuning->Register(MoveTemp(T));
	}

	// 깊이
	const FText Depth = LOCTEXT("CatDepth", "Depth");
	{
		FCXMRTunable T = Make("Depth.Test", Depth, LOCTEXT("DepthTest", "Depth test (real in front of virtual)"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsDepthTestOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetDepthTest(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Depth.Range", Depth, LOCTEXT("DepthRange", "Limit depth test to range"), ECXMRTunableKind::Bool);
		T.Default = 1.0f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsDepthTestRangeOn()) : 0.0f; };
		T.Set = [Weak](float V)
		{
			if (Weak.IsValid()) { Weak->SetDepthTestRange(V > 0.5f, Weak->GetDepthTestRangeNearZ(), Weak->GetDepthTestRangeFarZ()); }
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Depth.NearZ", Depth, LOCTEXT("NearZ", "Range near"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("Metres", "m"); T.Min = 0.0f; T.Max = 2.0f; T.Delta = 0.01f; T.Default = 0.0f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->GetDepthTestRangeNearZ() : 0.0f; };
		T.Set = [Weak](float V)
		{
			if (Weak.IsValid()) { Weak->SetDepthTestRange(Weak->IsDepthTestRangeOn(), V, Weak->GetDepthTestRangeFarZ()); }
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Depth.FarZ", Depth, LOCTEXT("FarZ", "Range far"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("Metres", "m"); T.Min = 0.05f; T.Max = 10.0f; T.Delta = 0.05f; T.Default = 0.75f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->GetDepthTestRangeFarZ() : 0.0f; };
		T.Set = [Weak](float V)
		{
			if (Weak.IsValid()) { Weak->SetDepthTestRange(Weak->IsDepthTestRangeOn(), Weak->GetDepthTestRangeNearZ(), V); }
		};
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Depth.EnvEstimation", Depth, LOCTEXT("EnvDepth", "Environment depth estimation"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsEnvironmentDepthEstimationOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetEnvironmentDepthEstimation(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}

	// 화면
	{
		// 패키징에서도 동작하도록 볼륨의 노출 값을 변경함.

		FCXMRTunable T = Make("Display.Exposure", LOCTEXT("CatDisplay", "Display"), LOCTEXT("Exposure", "Exposure compensation"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("EV", "EV"); T.Min = -6.0f; T.Max = 6.0f; T.Delta = 0.1f; T.Default = 0.0f; T.bPersist = true;
		T.Get = [this]
		{
			const APostProcessVolume* Volume = FindUnboundVolume(GetWorld());
			return (Volume && Volume->Settings.bOverride_AutoExposureBias) ? Volume->Settings.AutoExposureBias : 0.0f;
		};
		T.Set = [this](float V)
		{
			if (APostProcessVolume* Volume = FindUnboundVolume(GetWorld()))
			{
				Volume->Settings.bOverride_AutoExposureBias = true;
				Volume->Settings.AutoExposureBias = V;
			}
		};
		Tuning->Register(MoveTemp(T));
	}

	// 배치 메뉴와 콘솔에서 같은 기능 사용함.
	{
		FCXMRTunable T = Make("Vehicle.PlaceInFront", LOCTEXT("CatPlacement", "Vehicle placement"), LOCTEXT("PlaceInFront", "Place vehicle in front of me"), ECXMRTunableKind::Action);
		T.Invoke = [Weak] { if (Weak.IsValid()) { Weak->RequestPlaceInFront(); } };
		Tuning->Register(MoveTemp(T));
	}
	const FText Diagnostics = LOCTEXT("CatDiagnostics", "Tracking diagnostics");
	{
		FCXMRTunable T = Make("Diagnostics.MarkerTracking", Diagnostics, LOCTEXT("MarkerTracking", "Marker tracking"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsMarkerTrackingOn()) : 0.f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetMarkerTracking(V > 0.5f); } };
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMarkerTrackingSupported(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Diagnostics.HandSkeleton", Diagnostics, LOCTEXT("HandSkeleton", "Show hand skeleton"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsHandVisualizationOn()) : 0.f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetHandVisualization(V > 0.5f); } };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Diagnostics.MarkerLabels", Diagnostics, LOCTEXT("MarkerLabels", "Show marker axes and labels"), ECXMRTunableKind::Bool);
		T.Get = [] { return AsValue(UCXMRMarkerDebugComponent::IsMarkerDrawingOn()); };
		T.Set = [](float V) { UCXMRMarkerDebugComponent::SetMarkerDrawing(V > 0.5f); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Diagnostics.MRState", Diagnostics, LOCTEXT("MRState", "Log MR state"), ECXMRTunableKind::Action);
		T.Invoke = [Weak] { if (Weak.IsValid()) { Weak->DumpMRState(); } };
		Tuning->Register(MoveTemp(T));
	}

	// 입력
	if (UCXMRVarjoInputComponent* Input = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVarjoInputComponent>() : nullptr)
	{
		const TWeakObjectPtr<UCXMRVarjoInputComponent> WeakInput(Input);
		FCXMRTunable T = Make("Input.OffsetAdjustSpeed", LOCTEXT("CatInput", "Input"), LOCTEXT("AdjustSpeed", "NumPad speed (cm, deg)"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("PerSecond", "per s"); T.Min = 0.5f; T.Max = 200.0f; T.Delta = 1.0f; T.Default = 10.0f; T.bPersist = true;
		T.Get = [WeakInput] { return WeakInput.IsValid() ? WeakInput->OffsetAdjustSpeed : 0.0f; };
		T.Set = [WeakInput](float V) { if (WeakInput.IsValid()) { WeakInput->OffsetAdjustSpeed = V; } };
		Tuning->Register(MoveTemp(T));
	}
}

bool UCXMRTuningWindowComponent::IsWindowOpen() const
{
	return Window.IsValid();
}

TSharedRef<SWidget> UCXMRTuningWindowComponent::BuildPanel()
{
	if (!ControlWidget && GetWorld() && GetWorld()->GetGameInstance())
	{
		ControlWidget = CreateWidget<UCXMRControlPanelWidget>(GetWorld(), UCXMRControlPanelWidget::StaticClass());
	}
	return SNew(SCXMRControlPanel).Control(this).Tuning(GetTuning()).Viewer(ControlWidget);
}

void UCXMRTuningWindowComponent::SelectPage(ECXMRControlPage Page)
{
	if (Page > ECXMRControlPage::Diagnostics || (!bSetupMode && Page >= ECXMRControlPage::Display)) { return; }
	ActivePage = Page;
}

void UCXMRTuningWindowComponent::SetSetupMode(bool bEnable)
{
	bSetupMode = bEnable;
	if (!bSetupMode && ActivePage >= ECXMRControlPage::Display) { ActivePage = ECXMRControlPage::Vehicle; }
}

UCXMRPlacementComponent* UCXMRTuningWindowComponent::FindPlacement() const
{
	if (GetWorld())
	{
		for (TActorIterator<AActor> It(GetWorld()); It; ++It)
		{
			if (!It->IsActorBeingDestroyed())
			{
				if (UCXMRPlacementComponent* Placement = It->FindComponentByClass<UCXMRPlacementComponent>()) { return Placement; }
			}
		}
	}
	return nullptr;
}

int32 UCXMRTuningWindowComponent::GetCalibrationPhase() const
{
	if (CalibrationPhase == 0) { return 0; }
	const UCXMRPlacementComponent* Placement = FindPlacement();
	const UCXMRVehicleLoaderComponent* Loader = Placement && Placement->GetOwner() ? Placement->GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	const TWeakObjectPtr<UCXMRVehicleProfile> CurrentVehicleProfile(Loader ? Loader->Profile.Get() : nullptr);
	const TWeakObjectPtr<AActor> CurrentVehicle(Loader ? Loader->GetSpawnedVehicle() : nullptr);
	if (!Placement || Placement != CalibrationPlacement.Get() || Placement->MarkerProfile.Get() != CalibrationProfile.Get()
		|| !CurrentVehicleProfile.HasSameIndexAndSerialNumber(CalibrationVehicleProfile)
		|| !CurrentVehicle.HasSameIndexAndSerialNumber(CalibrationVehicle))
	{
		CalibrationPhase = 0;
		CalibrationMessage = LOCTEXT("SessionChanged", "Vehicle or profile changed. Start calibration again.");
		return 0;
	}
	if (CalibrationPhase >= 2)
	{
		const AActor* Root = Placement->VehicleRoot ? Placement->VehicleRoot.Get() : Placement->GetOwner();
		if ((!Placement->bCalibrated && !Placement->IsManualAlignment()) || !IsValid(Root)
			|| !Root->GetActorTransform().Equals(ConfirmedPose, 0.001f)
			|| (CalibrationPhase == 2 && Placement->IsManualAlignment() && !Placement->HasAlignmentCapture()))
		{
			CalibrationPhase = 1;
			CalibrationMessage = LOCTEXT("PoseChanged", "Placement changed. Confirm alignment again.");
		}
	}
	return CalibrationPhase;
}

FText UCXMRTuningWindowComponent::GetCalibrationMessage() const
{
	GetCalibrationPhase();
	if (IsInitialAlignmentPending())
	{
		const float Remaining = GetWorld()->GetTimerManager().GetTimerRemaining(InitialAlignmentTimer);
		return FText::FromString(FString::Printf(TEXT("Sit naturally and face straight ahead. Aligning in %d..."), FMath::Max(1, FMath::CeilToInt(Remaining))));
	}
	const UCXMRPlacementComponent* Placement = FindPlacement();
	if (Placement && Placement->NeedsRestoreConfirmation()) { return LOCTEXT("RestoreCheck", "Restored from one marker. Check the physical reference, then use this alignment."); }
	if (Placement && Placement->bCalibrated && !Placement->IsManualAlignment() && Placement->HasSavedAlignment() && CalibrationPhase == 0)
	{
		return LOCTEXT("RestoredAutomatically", "Saved alignment restored from consistent markers. The vehicle stays fixed.");
	}
	if (Placement && CalibrationPhase == 2 && Placement->IsManualAlignment())
	{
		if (bAlignmentSaveFailed) { return CalibrationMessage; }
		return FText::FromString(FString::Printf(TEXT("Alignment confirmed. Observe each configured marker: %d / %d captured. The vehicle stays fixed."),
			Placement->GetCapturedAlignmentMarkerCount(), Placement->GetRequiredAlignmentMarkerCount()));
	}
	return CalibrationMessage;
}

void UCXMRTuningWindowComponent::StartCalibration()
{
	UCXMRPlacementComponent* Placement = FindPlacement();
	if (!Placement || !Placement->MarkerProfile)
	{
		CalibrationPhase = 0;
		CalibrationMessage = LOCTEXT("NoPlacement", "Load a vehicle and marker profile first.");
		return;
	}
	CancelInitialAlignment();
	CaptureSessionIdentity(Placement);
	CalibrationPhase = 1;
	CalibrationMessage = LOCTEXT("CalibrationStarted", "Keep the markers visible and check vehicle alignment.");
	Placement->Recalibrate();
}

void UCXMRTuningWindowComponent::HandleManualPlacementRequest()
{
	CancelInitialAlignment();
	CalibrationPhase = 0;
	CalibrationMessage = LOCTEXT("ManualPlacement", "Manual placement. Adjust the vehicle, then confirm alignment and capture the configured markers.");
}

void UCXMRTuningWindowComponent::CaptureSessionIdentity(UCXMRPlacementComponent* Placement)
{
	CalibrationPlacement = Placement;
	CalibrationProfile = Placement->MarkerProfile;
	const UCXMRVehicleLoaderComponent* Loader = Placement->GetOwner() ? Placement->GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	CalibrationVehicleProfile = Loader ? Loader->Profile.Get() : nullptr;
	CalibrationVehicle = Loader ? Loader->GetSpawnedVehicle() : nullptr;
}

bool UCXMRTuningWindowComponent::CanStartInitialAlignment() const
{
	const UCXMRPlacementComponent* Placement = FindPlacement();
	FTransform Eye;
	return Placement && Placement->Mode == ECXMRPlacementMode::MarkerAnchor && Placement->GetDriverEyeWorld(Eye)
		&& Placement->MarkerProfile && Placement->GetRequiredAlignmentMarkerCount() > 0;
}

bool UCXMRTuningWindowComponent::IsInitialAlignmentPending() const
{
	return GetWorld() && GetWorld()->GetTimerManager().IsTimerActive(InitialAlignmentTimer);
}

void UCXMRTuningWindowComponent::CancelInitialAlignment()
{
	if (GetWorld()) { GetWorld()->GetTimerManager().ClearTimer(InitialAlignmentTimer); }
}

void UCXMRTuningWindowComponent::StartInitialAlignment()
{
	if (IsInitialAlignmentPending())
	{
		CancelInitialAlignment();
		CalibrationMessage = LOCTEXT("InitialCancelled", "Initial alignment cancelled. Vehicle position kept.");
		return;
	}
	if (!CanStartInitialAlignment() || !GetWorld())
	{
		CalibrationMessage = LOCTEXT("EyeMissing", "Load a vehicle, enable its Driver Eye Reference and configure calibration marker IDs.");
		return;
	}
	UCXMRPlacementComponent* Placement = FindPlacement();
	CaptureSessionIdentity(Placement);
	InitialEyeReference = CalibrationVehicleProfile->DriverEyeReference;
	InitialModelOffset = CalibrationVehicleProfile->VehicleRootOffset;
	InitialModelClass = CalibrationVehicleProfile->VehicleActor.ToSoftObjectPath();
	const AActor* Root = Placement->VehicleRoot ? Placement->VehicleRoot.Get() : Placement->GetOwner();
	InitialVehicleRelativePose = CalibrationVehicle->GetActorTransform().GetRelativeTransform(Root->GetActorTransform());
	CalibrationPhase = 1;
	GetWorld()->GetTimerManager().SetTimer(InitialAlignmentTimer, this, &UCXMRTuningWindowComponent::FinishInitialAlignment, 3.f, false);
}

void UCXMRTuningWindowComponent::FinishInitialAlignment()
{
	const UCXMRPlacementComponent* Placement = FindPlacement();
	const AActor* Root = Placement ? (Placement->VehicleRoot ? Placement->VehicleRoot.Get() : Placement->GetOwner()) : nullptr;
	if (GetCalibrationPhase() != 1 || !CanStartInitialAlignment() || !IsValid(Root) || !CalibrationVehicleProfile.IsValid()
		|| !CalibrationVehicleProfile->DriverEyeReference.Equals(InitialEyeReference, 0.001f)
		|| !CalibrationVehicleProfile->VehicleRootOffset.Equals(InitialModelOffset, 0.001f)
		|| CalibrationVehicleProfile->VehicleActor.ToSoftObjectPath() != InitialModelClass
		|| !CalibrationVehicle.IsValid()
		|| !CalibrationVehicle->GetActorTransform().GetRelativeTransform(Root->GetActorTransform()).Equals(InitialVehicleRelativePose, 0.001f))
	{
		CalibrationMessage = LOCTEXT("InitialSessionChanged", "Vehicle or reference changed. Start initial alignment again.");
		return;
	}
	FQuat Rotation;
	FVector Position;
	if (!GEngine || !GEngine->XRSystem.IsValid() || !GEngine->XRSystem->HasValidTrackingPosition()
		|| !GEngine->XRSystem->IsTracking(IXRTrackingSystem::HMDDeviceId)
		|| !GEngine->XRSystem->GetCurrentPose(IXRTrackingSystem::HMDDeviceId, Rotation, Position))
	{
		CalibrationMessage = LOCTEXT("HeadNotTracked", "Headset position is not tracked. Vehicle position kept; retry when tracking is valid.");
		return;
	}
	const FTransform HeadWorld = FTransform(Rotation, Position) * GEngine->XRSystem->GetTrackingToWorldTransform();
	if (FindPlacement()->AlignToDriverEyePose(HeadWorld))
	{
		CalibrationMessage = LOCTEXT("EyeAligned", "Initial alignment complete. Adjust against the physical USB and console, then confirm alignment.");
	}
}

bool UCXMRTuningWindowComponent::AcceptRestoredAlignment()
{
	UCXMRPlacementComponent* Placement = FindPlacement();
	if (!Placement || !Placement->ConfirmRestoredAlignment()) { return false; }
	CaptureSessionIdentity(Placement);
	const AActor* Root = Placement->VehicleRoot ? Placement->VehicleRoot.Get() : Placement->GetOwner();
	ConfirmedPose = Root->GetActorTransform();
	CalibrationPhase = 3;
	CalibrationMessage = LOCTEXT("RestoredAccepted", "Saved alignment restored and accepted. No new calibration save is needed.");
	return true;
}

void UCXMRTuningWindowComponent::PlaceManually()
{
	if (UCXMRTuningSubsystem* Registry = GetTuning()) { Registry->InvokeTunable("Vehicle.PlaceInFront"); }
}

bool UCXMRTuningWindowComponent::CanConfirmCalibration() const
{
	const UCXMRPlacementComponent* Placement = FindPlacement();
	return Placement && !IsInitialAlignmentPending() && (GetCalibrationPhase() <= 1)
		&& (Placement->IsManualAlignment() || (CalibrationPhase == 1 && Placement->bCalibrated));
}

bool UCXMRTuningWindowComponent::ConfirmCalibration()
{
	UCXMRPlacementComponent* Placement = FindPlacement();
	if (!CanConfirmCalibration()) { return false; }
	AActor* Root = Placement->VehicleRoot ? Placement->VehicleRoot.Get() : Placement->GetOwner();
	if (!IsValid(Root)) { return false; }
	if (Placement->IsManualAlignment() && !Placement->BeginAlignmentCapture())
	{
		CalibrationMessage = LOCTEXT("NoConfiguredMarkers", "Configure calibration marker IDs in the vehicle marker profile first.");
		return false;
	}
	CaptureSessionIdentity(Placement);
	ConfirmedPose = Root->GetActorTransform();
	bAlignmentSaveFailed = false;
	CalibrationPhase = 2;
	CalibrationMessage = LOCTEXT("CalibrationConfirmed", "Alignment confirmed. Save calibration.");
	return true;
}

bool UCXMRTuningWindowComponent::CanSaveCalibration() const
{
	const UCXMRPlacementComponent* Placement = FindPlacement();
	return GetCalibrationPhase() == 2 && Placement && (!Placement->IsManualAlignment() || Placement->CanSaveAlignment());
}

bool UCXMRTuningWindowComponent::SaveCalibration()
{
	if (!CanSaveCalibration()) { return false; }
	UCXMRPlacementComponent* Placement = FindPlacement();
	if (Placement->IsManualAlignment()) { Placement->SaveAlignment(); }
	else if (!Placement->GetMarkerLocationOffset().IsNearlyZero() || !Placement->GetMarkerRotationOffset().IsNearlyZero())
	{
		Placement->SaveMarkerOffsetToProfile();
	}
	else
	{
		Placement->SaveCalibrationToDisk();
	}
	const bool bSaved = Placement->WasLastCalibrationSaveSuccessful();
	bAlignmentSaveFailed = !bSaved;
	CalibrationPhase = bSaved ? 3 : 2;
	CalibrationMessage = bSaved ? LOCTEXT("CalibrationSaved", "Calibration saved.")
		: LOCTEXT("CalibrationSaveFailed", "Save failed. Check the profile and save location, then retry.");
	return bSaved;
}

void UCXMRTuningWindowComponent::RebuildContent()
{
	if (Window.IsValid())
	{
		Window->SetContent(BuildPanel());
	}
}

void UCXMRTuningWindowComponent::OpenWindow()
{
	if (Window.IsValid())
	{
		Window->BringToFront();
		return;
	}

	// Slate를 사용할 수 없으면 창을 열지 않음.
	if (!FApp::CanEverRender() || !FSlateApplication::IsInitialized())
	{
		UE_LOG(LogCXMRTuningWindow, Log, TEXT("Tuning window skipped: this build has no Slate application."));
		return;
	}

	Window = SNew(SWindow)
		.Title(WindowTitle)
		.ClientSize(WindowSize)
		.ScreenPosition(WindowPosition)
		.AutoCenter(EAutoCenter::None)
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	Window->SetContent(BuildPanel());

	// X로 닫으면 창 참조도 비움.
	Window->SetOnWindowClosed(FOnWindowClosed::CreateWeakLambda(this, [this](const TSharedRef<SWindow>&) { Window.Reset(); ControlWidget = nullptr; }));

	FSlateApplication::Get().AddWindow(Window.ToSharedRef(), /*bShowImmediately*/ true);

	UE_LOG(LogCXMRTuningWindow, Log, TEXT("Tuning window opened (%d rows)."),
		GetTuning() ? GetTuning()->GetTunables().Num() : 0);
}

void UCXMRTuningWindowComponent::CloseWindow()
{
	if (Window.IsValid())
	{
		Window->SetOnWindowClosed(FOnWindowClosed());   // 종료 중 콜백 재진입 방지함.
		Window->RequestDestroyWindow();
		Window.Reset();
	}
	ControlWidget = nullptr;
}

void UCXMRTuningWindowComponent::ToggleWindow()
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

#undef LOCTEXT_NAMESPACE
