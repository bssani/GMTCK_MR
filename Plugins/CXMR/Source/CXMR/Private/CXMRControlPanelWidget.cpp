// Copyright GMTCK CX.

#include "CXMRControlPanelWidget.h"
#include "CXMRSubsystem.h"
#include "Engine/GameInstance.h"

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
	}

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
	}

	Super::NativeDestruct();
}

void UCXMRControlPanelWidget::HandleBoolChanged(bool)   { RefreshVisuals(); }
void UCXMRControlPanelWidget::HandleFloatChanged(float) { RefreshVisuals(); }

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
