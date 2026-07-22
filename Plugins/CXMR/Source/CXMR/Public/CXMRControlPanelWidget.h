// Copyright GMTCK CX.
//
// UCXMRControlPanelWidget — base for the Varjo control panel.
// Subscribes to the subsystem's state delegates and calls RefreshVisuals() on any change.
// The WBP subclass provides the layout (buttons -> Toggle*, text -> Is*On getters).

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CXMRControlPanelWidget.generated.h"

class UCXMRSubsystem;

UCLASS(Abstract)
class CXMR_API UCXMRControlPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- Button handlers (bind Button OnClicked to these in the WBP) ---
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleMR();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleVRBackground();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleViewOffset();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleDepthTest();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleEnvDepth();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleMasking();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleMarkers();

	// Placement actions (relayed through the subsystem to the placement component).
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void RequestRecalibrate();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void RequestPlaceVehicle();

	// Viewer cycling (relayed through the subsystem to the vehicle loader).
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void NextVehicle();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void PreviousVehicle();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void NextTrim();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void PreviousTrim();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void NextCMF();

	// --- State getters (bind text / color to these) ---
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsMROn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsVRBackgroundVisible() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsDepthTestOn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsEnvDepthOn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsMaskingOn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsMarkersOn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") float GetViewOffset() const;

	// --- Vehicle state (bind Viewer readouts to these) ---
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetVehicleName() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetTrimName() const;
	/** "2 / 3" style position label. Empty when count is 0. */
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetVehiclePositionLabel() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetTrimPositionLabel() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") int32 GetCMFIndex() const;

	// Support flags — gray out unsupported rows.
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool IsMRSupported() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool IsMarkersSupported() const;

	/** Implement in the WBP to refresh all visuals. Called once on construct + on any state change. */
	UFUNCTION(BlueprintImplementableEvent, Category = "CXMR|UI") void RefreshVisuals();

protected:
	UCXMRSubsystem* GetCXMR() const;

	UFUNCTION() void HandleBoolChanged(bool bNewState);
	UFUNCTION() void HandleFloatChanged(float NewValue);
	UFUNCTION() void HandleStatusChanged();

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;
};
