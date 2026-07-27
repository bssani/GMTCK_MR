// Copyright GMTCK CX.
//
// UCXMRControlPanelWidget — the Varjo control panel. Logic AND wiring live here in C++; the WBP is
// pure layout. Named widgets are bound with meta=(BindWidgetOptional): the WBP only has to contain
// widgets with the matching names, and this class binds their clicks and drives their text/colour.
//
// This is deliberate — the WBP can be generated programmatically (Unreal Python / MCP) because it
// carries no event-graph logic, only a widget tree with known names. Anything the WBP omits is simply
// skipped (every access is null-guarded), so a partial layout still works.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "CXMRControlPanelWidget.generated.h"

class UCXMRSubsystem;
class UButton;
class UTextBlock;

UCLASS(Abstract)
class CXMR_API UCXMRControlPanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	// --- Button handlers (also the OnClicked targets; still callable from BP) ---
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleMR();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleVRBackground();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleViewOffset();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleDepthTest();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleEnvDepth();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleMasking();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleMarkers();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleHands();

	// Placement actions (relayed through the subsystem to the placement component).
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void RequestRecalibrate();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void RequestPlaceVehicle();

	// Viewer cycling (relayed through the subsystem to the vehicle loader).
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void NextVehicle();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void PreviousVehicle();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void NextTrim();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void PreviousTrim();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void NextCMF();

	// Depth test range. Not cosmetic: with the range off the compositor depth-tests the whole room
	// against estimated video depth and virtual geometry flickers (see UCXMRSubsystem).
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void ToggleDepthRange();

	// Percentile manikin (Human Factors).
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void NextManikin();
	UFUNCTION(BlueprintCallable, Category = "CXMR|UI") void PreviousManikin();

	// --- State getters (still callable from BP; C++ RefreshVisuals uses them directly) ---
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsMROn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsVRBackgroundVisible() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsDepthTestOn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsEnvDepthOn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsMaskingOn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsMarkersOn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsHandsOn() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") float GetViewOffset() const;

	// --- Vehicle state ---
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetVehicleName() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetTrimName() const;
	/** "2 / 3" style position label. Empty when count is 0. */
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetVehiclePositionLabel() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetTrimPositionLabel() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") int32 GetCMFIndex() const;

	// --- Depth range readout ---
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool  IsDepthRangeOn() const;
	/** "0.00 - 0.75 m", or "unbounded" while the range is off — the state that causes flicker. */
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetDepthRangeLabel() const;

	// --- Ergonomics state ---
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetManikinName() const;
	/** "2 / 3" style position label. Empty when count is 0. */
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") FText GetManikinPositionLabel() const;

	// Support flags — disable unsupported rows.
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool IsMRSupported() const;
	UFUNCTION(BlueprintPure, Category = "CXMR|UI") bool IsMarkersSupported() const;

	/** Pushes all state onto the bound widgets. C++ does the work; a WBP may override to extend. */
	UFUNCTION(BlueprintNativeEvent, Category = "CXMR|UI") void RefreshVisuals();
	void RefreshVisuals_Implementation();

	// --- Status colours (tweakable per WBP) ---
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|UI|Style") FLinearColor OnColor  = FLinearColor(0.25f, 0.80f, 0.35f, 1.0f);
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|UI|Style") FLinearColor OffColor = FLinearColor(0.45f, 0.45f, 0.45f, 1.0f);
	/** For readouts that are a mode rather than a state (view offset EYE/CAMERA) — neither value is "off". */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CXMR|UI|Style") FLinearColor NeutralColor = FLinearColor(0.85f, 0.85f, 0.90f, 1.0f);

protected:
	UCXMRSubsystem* GetCXMR() const;

	UFUNCTION() void HandleBoolChanged(bool bNewState);
	UFUNCTION() void HandleFloatChanged(float NewValue);
	UFUNCTION() void HandleStatusChanged();

	/** Sets a status text to ON/OFF and colours it. No-op if the widget is absent. */
	void ApplyToggle(UTextBlock* Text, bool bOn);

	UPROPERTY(Transient) TObjectPtr<UCXMRSubsystem> Subsystem;

	// ============================================================================
	//  Bound widgets — the WBP only needs widgets with these exact names.
	//  Optional + null-guarded, so a partial or Python-generated layout still works.
	// ============================================================================

	// Toggle buttons
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_MR;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_VRBackground;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_ViewOffset;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_DepthTest;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_EnvDepth;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Masking;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Markers;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Hands;

	// Placement + viewer buttons
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_Recalibrate;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_PlaceVehicle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_NextVehicle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_PrevVehicle;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_NextTrim;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_PrevTrim;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_NextCMF;

	// Depth range + ergonomics buttons
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_DepthRange;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_NextManikin;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UButton> Btn_PrevManikin;

	// Toggle status texts (ON / OFF)
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_MR_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_VRBackground_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ViewOffset_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_DepthTest_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_EnvDepth_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Masking_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Markers_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_Hands_State;

	// Session readouts
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_VehicleName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_VehiclePos;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_TrimName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_TrimPos;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_CMF;

	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_DepthRange_State;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_DepthRange;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ManikinName;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> Txt_ManikinPos;
};
