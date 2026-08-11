// Copyright GMTCK CX.

#include "CXMRControlPanelWidget.h"
#include "CXMRSubsystem.h"
#include "Engine/GameInstance.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"

// Binds a button's OnClicked to a handler, if the WBP provided that button.
#define CXMR_BIND_BUTTON(Btn, Handler) if (Btn) { (Btn)->OnClicked.AddDynamic(this, &UCXMRControlPanelWidget::Handler); }

UCXMRSubsystem* UCXMRControlPanelWidget::GetCXMR() const
{
	if (const UWorld* World = GetWorld())
	{
		if (UGameInstance* GI = World->GetGameInstance())
		{
			return GI->GetSubsystem<UCXMRSubsystem>();
		}
	}
	return nullptr;
}

void UCXMRControlPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();

	Subsystem = GetCXMR();
	if (Subsystem)
	{
		Subsystem->OnMixedRealityChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnVRBackgroundChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnDepthTestChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnEnvironmentDepthEstimationChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnMaskingChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnMarkerTrackingChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		// Hand visualization shipped without this line, so the panel's Hands row could never change —
		// pressing H drew the skeleton but the readout stayed at whatever it was built with.
		Subsystem->OnHandVisualizationChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnDepthTestRangeChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnViewOffsetChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleFloatChanged);
		Subsystem->OnVehicleStatusChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleStatusChanged);
		Subsystem->OnManikinChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleStatusChanged);
	}

	// Wire whatever buttons the WBP layout provided. Missing ones are simply skipped.
	CXMR_BIND_BUTTON(Btn_MR,          ToggleMR);
	CXMR_BIND_BUTTON(Btn_VRBackground, ToggleVRBackground);
	CXMR_BIND_BUTTON(Btn_ViewOffset,  ToggleViewOffset);
	CXMR_BIND_BUTTON(Btn_DepthTest,   ToggleDepthTest);
	CXMR_BIND_BUTTON(Btn_EnvDepth,    ToggleEnvDepth);
	CXMR_BIND_BUTTON(Btn_Masking,     ToggleMasking);
	CXMR_BIND_BUTTON(Btn_Markers,     ToggleMarkers);
	CXMR_BIND_BUTTON(Btn_Hands,       ToggleHands);
	CXMR_BIND_BUTTON(Btn_Recalibrate, RequestRecalibrate);
	CXMR_BIND_BUTTON(Btn_PlaceVehicle, RequestPlaceVehicle);
	CXMR_BIND_BUTTON(Btn_NextVehicle, NextVehicle);
	CXMR_BIND_BUTTON(Btn_PrevVehicle, PreviousVehicle);
	CXMR_BIND_BUTTON(Btn_NextTrim,    NextTrim);
	CXMR_BIND_BUTTON(Btn_PrevTrim,    PreviousTrim);
	CXMR_BIND_BUTTON(Btn_NextCMF,     NextCMF);
	CXMR_BIND_BUTTON(Btn_DepthRange,  ToggleDepthRange);
	CXMR_BIND_BUTTON(Btn_NextManikin, NextManikin);
	CXMR_BIND_BUTTON(Btn_PrevManikin, PreviousManikin);

	CXMR_BIND_BUTTON(Btn_Tab_Display, ShowDisplayTab);
	CXMR_BIND_BUTTON(Btn_Tab_Calib,   ShowCalibTab);
	CXMR_BIND_BUTTON(Btn_Tab_Viewer,  ShowViewerTab);
	SetActiveTab(0);

	RefreshVisuals();
}

void UCXMRControlPanelWidget::ShowDisplayTab() { SetActiveTab(0); }
void UCXMRControlPanelWidget::ShowCalibTab()   { SetActiveTab(1); }
void UCXMRControlPanelWidget::ShowViewerTab()  { SetActiveTab(2); }

void UCXMRControlPanelWidget::SetActiveTab(int32 TabIndex)
{
	if (Switcher_Pages)
	{
		const int32 PageCount = Switcher_Pages->GetNumWidgets();
		if (PageCount > 0)
		{
			TabIndex = FMath::Clamp(TabIndex, 0, PageCount - 1);
		}
		Switcher_Pages->SetActiveWidgetIndex(TabIndex);
	}

	// Dim the captions of the pages you are not on — otherwise the panel just looks like it lost rows.
	UTextBlock* const Captions[] = { Cap_Btn_Tab_Display, Cap_Btn_Tab_Calib, Cap_Btn_Tab_Viewer };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Captions); ++Index)
	{
		if (Captions[Index])
		{
			Captions[Index]->SetColorAndOpacity(FSlateColor(Index == TabIndex ? ActiveTabColor : InactiveTabColor));
		}
	}
}

void UCXMRControlPanelWidget::NativeTick(const FGeometry& Geometry, float DeltaSeconds)
{
	Super::NativeTick(Geometry, DeltaSeconds);

	// The marker offset changes continuously while an adjust key is held and the placement component
	// broadcasts nothing, so the readout has to be polled — there is no event to hang it off.
	if (!Txt_OffsetX && !Txt_OffsetY && !Txt_OffsetZ && !Txt_OffsetYaw)
	{
		return;
	}

	// Read it off the subsystem: the placement component is on the vehicle rig, not the pawn.
	const UCXMRSubsystem* CXMR = Subsystem ? Subsystem.Get() : GetCXMR();
	if (!CXMR)
	{
		return;
	}

	// Only touch the text when the value actually moved. This runs every frame in VR, and each
	// SetText is a string build plus a Slate invalidation.
	const FVector  Location = CXMR->GetMarkerLocationOffset();
	const FRotator Rotation = CXMR->GetMarkerRotationOffset();
	if (bOffsetReadoutValid && Location.Equals(ShownOffsetLocation) && Rotation.Equals(ShownOffsetRotation))
	{
		return;
	}
	ShownOffsetLocation  = Location;
	ShownOffsetRotation  = Rotation;
	bOffsetReadoutValid  = true;

	if (Txt_OffsetX)   { Txt_OffsetX->SetText(FText::FromString(FString::Printf(TEXT("%.1f cm"),  Location.X))); }
	if (Txt_OffsetY)   { Txt_OffsetY->SetText(FText::FromString(FString::Printf(TEXT("%.1f cm"),  Location.Y))); }
	if (Txt_OffsetZ)   { Txt_OffsetZ->SetText(FText::FromString(FString::Printf(TEXT("%.1f cm"),  Location.Z))); }
	if (Txt_OffsetYaw) { Txt_OffsetYaw->SetText(FText::FromString(FString::Printf(TEXT("%.1f deg"), Rotation.Yaw))); }
}

void UCXMRControlPanelWidget::NativeDestruct()
{
	if (Subsystem)
	{
		Subsystem->OnMixedRealityChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnVRBackgroundChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnDepthTestChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnEnvironmentDepthEstimationChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnMaskingChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnMarkerTrackingChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnHandVisualizationChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnDepthTestRangeChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleBoolChanged);
		Subsystem->OnViewOffsetChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleFloatChanged);
		Subsystem->OnVehicleStatusChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleStatusChanged);
		Subsystem->OnManikinChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleStatusChanged);
	}

	Super::NativeDestruct();
}

void UCXMRControlPanelWidget::HandleBoolChanged(bool)   { RefreshVisuals(); }
void UCXMRControlPanelWidget::HandleFloatChanged(float) { RefreshVisuals(); }
void UCXMRControlPanelWidget::HandleStatusChanged()     { RefreshVisuals(); }

// --- Visuals ---

void UCXMRControlPanelWidget::ApplyToggle(UTextBlock* Text, bool bOn)
{
	if (!Text)
	{
		return;
	}
	Text->SetText(bOn ? NSLOCTEXT("CXMR", "On", "ON") : NSLOCTEXT("CXMR", "Off", "OFF"));
	Text->SetColorAndOpacity(FSlateColor(bOn ? OnColor : OffColor));
}

void UCXMRControlPanelWidget::RefreshVisuals_Implementation()
{
	// Toggle states
	ApplyToggle(Txt_MR_State,           IsMROn());
	ApplyToggle(Txt_VRBackground_State, IsVRBackgroundVisible());
	ApplyToggle(Txt_DepthTest_State,    IsDepthTestOn());
	ApplyToggle(Txt_EnvDepth_State,     IsEnvDepthOn());
	ApplyToggle(Txt_Masking_State,      IsMaskingOn());
	ApplyToggle(Txt_Markers_State,      IsMarkersOn());
	ApplyToggle(Txt_Hands_State,        IsHandsOn());
	ApplyToggle(Txt_DepthRange_State,   IsDepthRangeOn());

	// View offset is NOT on/off — it picks which viewpoint the frame renders from (0 = eye, 1 = camera).
	// Showing ON/OFF here reads as "feature enabled", which is wrong: OFF is a valid, deliberate mode.
	if (Txt_ViewOffset_State)
	{
		const bool bCamera = GetViewOffset() > 0.5f;
		Txt_ViewOffset_State->SetText(FText::FromString(bCamera ? TEXT("CAMERA") : TEXT("EYE")));
		Txt_ViewOffset_State->SetColorAndOpacity(FSlateColor(NeutralColor));
	}

	// Session readouts. Empty means "nothing loaded" — show a dash, otherwise the cycling row renders as
	// bare < > arrows with a void between them and looks broken rather than empty.
	// ASCII only: the default font has no glyph for em-dash/arrows and draws an empty box instead.
	auto OrDash = [](const FText& In) { return In.IsEmpty() ? FText::FromString(TEXT("-")) : In; };

	if (Txt_VehicleName) { Txt_VehicleName->SetText(OrDash(GetVehicleName())); }
	if (Txt_VehiclePos)  { Txt_VehiclePos->SetText(OrDash(GetVehiclePositionLabel())); }
	if (Txt_TrimName)    { Txt_TrimName->SetText(OrDash(GetTrimName())); }
	if (Txt_TrimPos)     { Txt_TrimPos->SetText(OrDash(GetTrimPositionLabel())); }
	if (Txt_CMF)         { Txt_CMF->SetText(FText::AsNumber(GetCMFIndex())); }
	if (Txt_DepthRange)  { Txt_DepthRange->SetText(GetDepthRangeLabel()); }
	if (Txt_ManikinName) { Txt_ManikinName->SetText(OrDash(GetManikinName())); }
	if (Txt_ManikinPos)  { Txt_ManikinPos->SetText(OrDash(GetManikinPositionLabel())); }

	// Grey out rows the headset does not support.
	if (Btn_MR)      { Btn_MR->SetIsEnabled(IsMRSupported()); }
	if (Btn_Markers) { Btn_Markers->SetIsEnabled(IsMarkersSupported()); }
}

// --- Buttons ---
void UCXMRControlPanelWidget::ToggleMR()         { if (Subsystem) { Subsystem->ToggleMixedReality(); } }
void UCXMRControlPanelWidget::ToggleVRBackground() { if (Subsystem) { Subsystem->ToggleVRBackground(); } }
void UCXMRControlPanelWidget::ToggleViewOffset() { if (Subsystem) { Subsystem->ToggleViewOffset(); } }
void UCXMRControlPanelWidget::ToggleDepthTest()  { if (Subsystem) { Subsystem->ToggleDepthTest(); } }
void UCXMRControlPanelWidget::ToggleEnvDepth()   { if (Subsystem) { Subsystem->ToggleEnvironmentDepthEstimation(); } }
void UCXMRControlPanelWidget::ToggleMasking()    { if (Subsystem) { Subsystem->ToggleMasking(); } }
void UCXMRControlPanelWidget::ToggleMarkers()    { if (Subsystem) { Subsystem->ToggleMarkerTracking(); } }
void UCXMRControlPanelWidget::ToggleHands()      { if (Subsystem) { Subsystem->ToggleHandVisualization(); } }

void UCXMRControlPanelWidget::RequestRecalibrate()   { if (Subsystem) { Subsystem->RequestRecalibrate(); } }
void UCXMRControlPanelWidget::RequestPlaceVehicle()  { if (Subsystem) { Subsystem->RequestPlaceInFront(); } }

// Viewer cycling — relayed to the loader via the subsystem, so the panel needs no loader reference.
void UCXMRControlPanelWidget::NextVehicle()     { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::NextVehicle); } }
void UCXMRControlPanelWidget::PreviousVehicle() { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::PreviousVehicle); } }
void UCXMRControlPanelWidget::NextTrim()        { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::NextTrim); } }
void UCXMRControlPanelWidget::PreviousTrim()    { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::PreviousTrim); } }
void UCXMRControlPanelWidget::NextCMF()         { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::NextCMF); } }

void UCXMRControlPanelWidget::ToggleDepthRange() { if (Subsystem) { Subsystem->ToggleDepthTestRange(); } }

// Ergonomics uses its own step relay rather than ECXMRViewerAction — same rendezvous, different verb.
void UCXMRControlPanelWidget::NextManikin()     { if (Subsystem) { Subsystem->RequestErgonomicsStep(1); } }
void UCXMRControlPanelWidget::PreviousManikin() { if (Subsystem) { Subsystem->RequestErgonomicsStep(-1); } }

// --- Getters ---
bool  UCXMRControlPanelWidget::IsMROn() const          { return Subsystem && Subsystem->IsMixedRealityOn(); }
bool  UCXMRControlPanelWidget::IsVRBackgroundVisible() const { return Subsystem && Subsystem->IsVRBackgroundVisible(); }
bool  UCXMRControlPanelWidget::IsDepthTestOn() const   { return Subsystem && Subsystem->IsDepthTestOn(); }
bool  UCXMRControlPanelWidget::IsEnvDepthOn() const    { return Subsystem && Subsystem->IsEnvironmentDepthEstimationOn(); }
bool  UCXMRControlPanelWidget::IsMaskingOn() const     { return Subsystem && Subsystem->IsMaskingOn(); }
bool  UCXMRControlPanelWidget::IsMarkersOn() const     { return Subsystem && Subsystem->IsMarkerTrackingOn(); }
bool  UCXMRControlPanelWidget::IsHandsOn() const       { return Subsystem && Subsystem->IsHandVisualizationOn(); }
float UCXMRControlPanelWidget::GetViewOffset() const   { return Subsystem ? Subsystem->GetViewOffset() : 0.0f; }

bool UCXMRControlPanelWidget::IsMRSupported() const      { return Subsystem && Subsystem->IsMixedRealitySupported(); }
bool UCXMRControlPanelWidget::IsMarkersSupported() const { return Subsystem && Subsystem->IsMarkerTrackingSupported(); }

// --- Vehicle state ---
FText UCXMRControlPanelWidget::GetVehicleName() const { return Subsystem ? Subsystem->GetVehicleName() : FText::GetEmpty(); }
FText UCXMRControlPanelWidget::GetTrimName() const    { return Subsystem ? Subsystem->GetTrimName() : FText::GetEmpty(); }
int32 UCXMRControlPanelWidget::GetCMFIndex() const    { return Subsystem ? Subsystem->GetCMFIndex() : 0; }

// A "2 / 3" readout only makes sense with something to count — an empty label reads as "not applicable".
FText UCXMRControlPanelWidget::GetVehiclePositionLabel() const
{
	if (!Subsystem || Subsystem->GetVehicleCount() <= 0)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("%d / %d"),
		Subsystem->GetVehicleIndex() + 1, Subsystem->GetVehicleCount()));
}

FText UCXMRControlPanelWidget::GetTrimPositionLabel() const
{
	if (!Subsystem || Subsystem->GetTrimCount() <= 0)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("%d / %d"),
		Subsystem->GetTrimIndex() + 1, Subsystem->GetTrimCount()));
}

// --- Depth range ---

bool UCXMRControlPanelWidget::IsDepthRangeOn() const { return Subsystem && Subsystem->IsDepthTestRangeOn(); }

FText UCXMRControlPanelWidget::GetDepthRangeLabel() const
{
	if (!Subsystem)
	{
		return FText::GetEmpty();
	}
	// "unbounded" is the honest word for what the compositor actually gets when the range is off
	// (farZ = HUGE_VALF) — and it is the state that makes the room flicker, so it is worth naming.
	// ASCII only: the default font draws a box for anything else.
	if (!Subsystem->IsDepthTestRangeOn())
	{
		return NSLOCTEXT("CXMR", "DepthRangeUnbounded", "unbounded");
	}
	return FText::FromString(FString::Printf(TEXT("%.2f - %.2f m"),
		Subsystem->GetDepthTestRangeNearZ(), Subsystem->GetDepthTestRangeFarZ()));
}

// --- Ergonomics ---

FText UCXMRControlPanelWidget::GetManikinName() const { return Subsystem ? Subsystem->GetManikinName() : FText::GetEmpty(); }

FText UCXMRControlPanelWidget::GetManikinPositionLabel() const
{
	if (!Subsystem || Subsystem->GetManikinCount() <= 0)
	{
		return FText::GetEmpty();
	}
	return FText::FromString(FString::Printf(TEXT("%d / %d"),
		Subsystem->GetManikinIndex() + 1, Subsystem->GetManikinCount()));
}
