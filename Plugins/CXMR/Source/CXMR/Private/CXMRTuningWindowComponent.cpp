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
	CalibrationMessage = LOCTEXT("CalibrationReady", "Sit in the driver seat and face straight ahead. Align once, fine-adjust, confirm and save. Later sessions restore from markers.");
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
	CaptureExposureBaseline();
	if (UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
	{
		if (UCXMRSubsystem* CXMR = GI->GetSubsystem<UCXMRSubsystem>()) { CXMR->OnPlaceRequested.AddDynamic(this, &UCXMRTuningWindowComponent::HandleManualPlacementRequest); CXMR->OnMixedRealityChanged.AddDynamic(this, &UCXMRTuningWindowComponent::HandleDisplayModeChanged); bExposureModeMR = CXMR->IsMixedRealityOn(); }
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
		if (UCXMRSubsystem* CXMR = GI->GetSubsystem<UCXMRSubsystem>()) { CXMR->OnPlaceRequested.RemoveDynamic(this, &UCXMRTuningWindowComponent::HandleManualPlacementRequest); CXMR->OnMixedRealityChanged.RemoveDynamic(this, &UCXMRTuningWindowComponent::HandleDisplayModeChanged); }
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
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMixedRealitySupported(); };
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
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMixedRealityOn(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("MR.ViewOffset", MR, LOCTEXT("ViewOffset", "View offset 0 eye / 1 cam"), ECXMRTunableKind::Float);
		T.Min = 0.0f; T.Max = 1.0f; T.Delta = 0.05f; T.Default = 1.0f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->GetViewOffset() : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetViewOffset(V); } };
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMixedRealityOn(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("MR.ViewOffsetGlide", MR, LOCTEXT("ViewOffsetGlide", "View offset glide (K)"), ECXMRTunableKind::Float);
		T.Unit = LOCTEXT("Seconds", "s"); T.Min = 0.0f; T.Max = 2.0f; T.Delta = 0.05f; T.Default = 0.5f; T.bPersist = true;
		T.Get = [Weak] { return Weak.IsValid() ? Weak->ViewOffsetTransitionSeconds : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->ViewOffsetTransitionSeconds = V; } };
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMixedRealityOn(); };
		Tuning->Register(MoveTemp(T));
	}

	// 깊이
	const FText Depth = LOCTEXT("CatDepth", "Depth");
	{
		FCXMRTunable T = Make("Depth.Test", Depth, LOCTEXT("DepthTest", "Depth test (real in front of virtual)"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsDepthTestOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetDepthTest(V > 0.5f); } };
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMixedRealityOn(); };
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
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMixedRealityOn(); };
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
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMixedRealityOn(); };
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
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMixedRealityOn(); };
		Tuning->Register(MoveTemp(T));
	}
	{
		FCXMRTunable T = Make("Depth.EnvEstimation", Depth, LOCTEXT("EnvDepth", "Environment depth estimation"), ECXMRTunableKind::Bool);
		T.Get = [Weak] { return Weak.IsValid() ? AsValue(Weak->IsEnvironmentDepthEstimationOn()) : 0.0f; };
		T.Set = [Weak](float V) { if (Weak.IsValid()) { Weak->SetEnvironmentDepthEstimation(V > 0.5f); } };
		T.IsEnabled = [Weak] { return Weak.IsValid() && Weak->IsMixedRealityOn() && Weak->IsEnvironmentDepthEstimationSupported(); };
		Tuning->Register(MoveTemp(T));
	}

	// 모드별 가상 노출을 저장함. 렌더러 구성은 유지함.
	for (bool bMR : { true, false })
	{
		FCXMRTunable T = Make(bMR ? FName("Display.MRExposure") : FName("Display.VRExposure"), LOCTEXT("CatDisplay", "Visual quality"), bMR ? LOCTEXT("MRExposure", "MR virtual exposure") : LOCTEXT("VRExposure", "VR virtual exposure"), ECXMRTunableKind::Float);
		T.Help = LOCTEXT("ExposureHelp", "Separate virtual-scene preset for each environment. Camera exposure is controlled in Varjo Base. Requires an unbound Post Process Volume.");
		T.Unit = LOCTEXT("EV", "EV"); T.Min = -6.f; T.Max = 6.f; T.Delta = 0.1f; T.bPersist = true;
		T.Get = [this, bMR] { return GetExposurePreset(bMR); };
		T.Set = [this, bMR](float V) { SetExposurePreset(bMR, V); };
		T.IsEnabled = [this] { return FindUnboundVolume(GetWorld()) != nullptr; };
		Tuning->Register(MoveTemp(T));
	}
	HandleDisplayModeChanged(bExposureModeMR);

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

void UCXMRTuningWindowComponent::CaptureExposureBaseline()
{
	APostProcessVolume* Volume = FindUnboundVolume(GetWorld());
	if (Volume && Volume != ExposureVolume.Get())
	{
		// 레벨 기준값은 프리셋 적용 전에 보관함.
		ExposureVolume = Volume;
		BaselineExposure = Volume->Settings.AutoExposureBias;
		bBaselineExposureOverride = Volume->Settings.bOverride_AutoExposureBias;
	}
}

float UCXMRTuningWindowComponent::GetExposurePreset(bool bMR) const
{
	return (bMR ? bHasMRExposure : bHasVRExposure) ? (bMR ? MRExposure : VRExposure) : BaselineExposure;
}

void UCXMRTuningWindowComponent::SetExposurePreset(bool bMR, float Value)
{
	(bMR ? MRExposure : VRExposure) = Value;
	(bMR ? bHasMRExposure : bHasVRExposure) = true;
	if (bMR == bExposureModeMR) { HandleDisplayModeChanged(bExposureModeMR); }
}

void UCXMRTuningWindowComponent::HandleDisplayModeChanged(bool bMixedReality)
{
	bExposureModeMR = bMixedReality;
	CaptureExposureBaseline();
	if (APostProcessVolume* Volume = ExposureVolume.Get())
	{
		const bool bHasPreset = bMixedReality ? bHasMRExposure : bHasVRExposure;
		Volume->Settings.bOverride_AutoExposureBias = bHasPreset || bBaselineExposureOverride;
		Volume->Settings.AutoExposureBias = GetExposurePreset(bMixedReality);
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
	if (Page > ECXMRControlPage::Diagnostics) { return; }
	ResetDeadline = 0.0;
	ActivePage = Page == ECXMRControlPage::Vehicle || Page == ECXMRControlPage::Calibration ? Page : ECXMRControlPage::Display;
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
			|| (CalibrationPhase == 2 && (Placement->IsManualAlignment() || (!Placement->GetAlignmentGroup().IsNone() && !Placement->HasSavedAlignment()))
				&& !Placement->HasAlignmentCapture()))
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
	if (Placement && CalibrationPhase == 2 && (Placement->IsManualAlignment() || Placement->HasAlignmentCapture()))
	{
		if (bAlignmentSaveFailed) { return CalibrationMessage; }
		return FText::FromString(FString::Printf(TEXT("Not saved yet. Observe each configured marker: %d / %d captured. Press Enter or select Save Alignment when ready."),
			Placement->GetCapturedAlignmentMarkerCount(), Placement->GetRequiredAlignmentMarkerCount()));
	}
	return CalibrationMessage;
}

FText UCXMRTuningWindowComponent::GetAlignmentState(const UCXMRVehicleLoaderComponent* Loader, ECXMRAlignmentTone& OutTone) const
{
	const UCXMRPlacementComponent* Placement = FindPlacement();
	const TCHAR* State = TEXT("Alignment pending");
	OutTone = ECXMRAlignmentTone::Pending;
	if (Loader && Loader->GetSpawnedVehicle())
	{
		OutTone = ECXMRAlignmentTone::Attention;
		if (IsInitialAlignmentPending()) { State = TEXT("Alignment in progress"); }
		else if (Placement && Placement->NeedsRestoreConfirmation()) { State = TEXT("Restore needs confirmation"); }
		else if (GetCalibrationPhase() == 2) { State = TEXT("Alignment confirmed; save pending"); }
		else if (Placement && Placement->IsManualAlignment()) { State = TEXT("Manual alignment; confirmation pending"); }
		else if (Placement && Placement->bCalibrated) { State = TEXT("Aligned"); OutTone = ECXMRAlignmentTone::Aligned; }
		else { OutTone = ECXMRAlignmentTone::Pending; }
	}
	if (HasCalibrationError()) { OutTone = ECXMRAlignmentTone::Error; }
	return FText::FromString(State);
}

FText UCXMRTuningWindowComponent::GetAlignmentStatus(const UCXMRVehicleLoaderComponent* Loader) const
{
	const FString Vehicle = Loader && Loader->Profile && Loader->GetSpawnedVehicle() ? Loader->Profile->DisplayName.ToString() : TEXT("No vehicle loaded");
	ECXMRAlignmentTone Tone;
	const FString State = GetAlignmentState(Loader, Tone).ToString();
	const FString Detail = HasCalibrationError() ? TEXT("   |   ") + GetCalibrationMessage().ToString() : FString();
	return FText::FromString(FString::Printf(TEXT("%s   |   %s%s"), *Vehicle, *State, *Detail));
}


bool UCXMRTuningWindowComponent::IsSharedAlignmentResetArmed() const
{
	const UCXMRPlacementComponent* Placement = FindPlacement();
	return GetWorld() && Placement && Placement == ResetPlacement.Get() && !ResetGroup.IsNone()
		&& Placement->GetAlignmentGroup() == ResetGroup && GetWorld()->GetRealTimeSeconds() < ResetDeadline;
}

FText UCXMRTuningWindowComponent::GetSharedAlignmentResetLabel() const
{
	return IsSharedAlignmentResetArmed()
		? FText::Format(LOCTEXT("ResetGroupConfirm", "Click again to delete shared alignment for group {0}"), FText::FromName(ResetGroup))
		: LOCTEXT("ResetGroupAlignment", "Reset Group Alignment");
}

void UCXMRTuningWindowComponent::ResetSharedAlignment()
{
	UCXMRPlacementComponent* Placement = FindPlacement();
	if (!Placement || Placement->GetAlignmentGroup().IsNone()) { ResetDeadline = 0.0; return; }
	if (!IsSharedAlignmentResetArmed())
	{
		ResetPlacement = Placement;
		ResetGroup = Placement->GetAlignmentGroup();
		// 일시정지·시간 배율에 영향받지 않음.
		ResetDeadline = GetWorld()->GetRealTimeSeconds() + 5.0;
		return;
	}
	ResetDeadline = 0.0;
	if (!Placement->TryResetCalibrationToAuthored())
	{
		CalibrationMessage = Placement->GetAlignmentStorageMessage();
		return;
	}
	CancelInitialAlignment();
	CalibrationPhase = 0;
	bAlignmentSaveFailed = false;
	CalibrationMessage = LOCTEXT("GroupReset", "Shared alignment reset. Align, confirm and save this group again.");
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
	const bool bNeedsCapture = Placement->IsManualAlignment() || (!Placement->GetAlignmentGroup().IsNone() && !Placement->HasSavedAlignment());
	if (bNeedsCapture && !Placement->BeginAlignmentCapture())
	{
		CalibrationMessage = Placement->GetAlignmentSaveMessage();
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
	if (GetCalibrationPhase() != 2 || !Placement) { return false; }
	const bool bNeedsCapture = Placement->IsManualAlignment() || Placement->HasAlignmentCapture()
		|| (!Placement->GetAlignmentGroup().IsNone() && !Placement->HasSavedAlignment());
	return !bNeedsCapture || Placement->CanSaveAlignment();
}

void UCXMRTuningWindowComponent::RequestAlignmentSave()
{
	UCXMRPlacementComponent* Placement = FindPlacement();
	if (!Placement) { CalibrationMessage = LOCTEXT("SaveNoPlacement", "Not saved. Load a vehicle and marker profile first."); return; }
	CancelInitialAlignment();
	const bool bSaved = Placement->RequestAlignmentSave();
	CaptureSessionIdentity(Placement);
	const AActor* Root = Placement->VehicleRoot ? Placement->VehicleRoot.Get() : Placement->GetOwner();
	if (IsValid(Root)) { ConfirmedPose = Root->GetActorTransform(); }
	CalibrationPhase = bSaved ? 3 : Placement->HasAlignmentCapture() ? 2 : 1;
	bAlignmentSaveFailed = !bSaved && Placement->CanSaveAlignment();
	CalibrationMessage = Placement->GetAlignmentSaveMessage();
}

bool UCXMRTuningWindowComponent::SaveCalibration()
{
	if (!CanSaveCalibration()) { return false; }
	UCXMRPlacementComponent* Placement = FindPlacement();
	if (Placement->IsManualAlignment() || Placement->HasAlignmentCapture()) { Placement->SaveAlignment(); }
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
		: Placement->GetAlignmentStorageMessage().IsEmpty()
			? LOCTEXT("CalibrationSaveFailed", "Save failed. Check the profile and save location, then retry.") : Placement->GetAlignmentStorageMessage();
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
	ResetDeadline = 0.0;
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
