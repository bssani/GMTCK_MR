// Copyright GMTCK CX.

#include "CXMRControlPanelWidget.h"
#include "CXMRSubsystem.h"
#include "Engine/GameInstance.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"

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
		Subsystem->OnViewOffsetChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleFloatChanged);
		Subsystem->OnVehicleStatusChanged.AddDynamic(this, &UCXMRControlPanelWidget::HandleStatusChanged);
	}

	// Wire whatever buttons the WBP layout provided. Missing ones are simply skipped.
	CXMR_BIND_BUTTON(Btn_MR,          ToggleMR);
	CXMR_BIND_BUTTON(Btn_VRBackground, ToggleVRBackground);
	CXMR_BIND_BUTTON(Btn_ViewOffset,  ToggleViewOffset);
	CXMR_BIND_BUTTON(Btn_DepthTest,   ToggleDepthTest);
	CXMR_BIND_BUTTON(Btn_EnvDepth,    ToggleEnvDepth);
	CXMR_BIND_BUTTON(Btn_Masking,     ToggleMasking);
	CXMR_BIND_BUTTON(Btn_Markers,     ToggleMarkers);
	CXMR_BIND_BUTTON(Btn_Recalibrate, RequestRecalibrate);
	CXMR_BIND_BUTTON(Btn_PlaceVehicle, RequestPlaceVehicle);
	CXMR_BIND_BUTTON(Btn_NextVehicle, NextVehicle);
	CXMR_BIND_BUTTON(Btn_PrevVehicle, PreviousVehicle);
	CXMR_BIND_BUTTON(Btn_NextTrim,    NextTrim);
	CXMR_BIND_BUTTON(Btn_PrevTrim,    PreviousTrim);
	CXMR_BIND_BUTTON(Btn_NextCMF,     NextCMF);

	RefreshVisuals();
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
		Subsystem->OnViewOffsetChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleFloatChanged);
		Subsystem->OnVehicleStatusChanged.RemoveDynamic(this, &UCXMRControlPanelWidget::HandleStatusChanged);
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
	ApplyToggle(Txt_ViewOffset_State,   GetViewOffset() > 0.5f);
	ApplyToggle(Txt_DepthTest_State,    IsDepthTestOn());
	ApplyToggle(Txt_EnvDepth_State,     IsEnvDepthOn());
	ApplyToggle(Txt_Masking_State,      IsMaskingOn());
	ApplyToggle(Txt_Markers_State,      IsMarkersOn());

	// Session readouts
	if (Txt_VehicleName) { Txt_VehicleName->SetText(GetVehicleName()); }
	if (Txt_VehiclePos)  { Txt_VehiclePos->SetText(GetVehiclePositionLabel()); }
	if (Txt_TrimName)    { Txt_TrimName->SetText(GetTrimName()); }
	if (Txt_TrimPos)     { Txt_TrimPos->SetText(GetTrimPositionLabel()); }
	if (Txt_CMF)         { Txt_CMF->SetText(FText::AsNumber(GetCMFIndex())); }

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

void UCXMRControlPanelWidget::RequestRecalibrate()   { if (Subsystem) { Subsystem->RequestRecalibrate(); } }
void UCXMRControlPanelWidget::RequestPlaceVehicle()  { if (Subsystem) { Subsystem->RequestPlaceInFront(); } }

// Viewer cycling — relayed to the loader via the subsystem, so the panel needs no loader reference.
void UCXMRControlPanelWidget::NextVehicle()     { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::NextVehicle); } }
void UCXMRControlPanelWidget::PreviousVehicle() { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::PreviousVehicle); } }
void UCXMRControlPanelWidget::NextTrim()        { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::NextTrim); } }
void UCXMRControlPanelWidget::PreviousTrim()    { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::PreviousTrim); } }
void UCXMRControlPanelWidget::NextCMF()         { if (Subsystem) { Subsystem->RequestViewerAction(ECXMRViewerAction::NextCMF); } }

// --- Getters ---
bool  UCXMRControlPanelWidget::IsMROn() const          { return Subsystem && Subsystem->IsMixedRealityOn(); }
bool  UCXMRControlPanelWidget::IsVRBackgroundVisible() const { return Subsystem && Subsystem->IsVRBackgroundVisible(); }
bool  UCXMRControlPanelWidget::IsDepthTestOn() const   { return Subsystem && Subsystem->IsDepthTestOn(); }
bool  UCXMRControlPanelWidget::IsEnvDepthOn() const    { return Subsystem && Subsystem->IsEnvironmentDepthEstimationOn(); }
bool  UCXMRControlPanelWidget::IsMaskingOn() const     { return Subsystem && Subsystem->IsMaskingOn(); }
bool  UCXMRControlPanelWidget::IsMarkersOn() const     { return Subsystem && Subsystem->IsMarkerTrackingOn(); }
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
