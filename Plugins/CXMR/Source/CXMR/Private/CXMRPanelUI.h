// Copyright GMTCK CX.
//
// CXMRPanelUI — the look shared by the operator windows: the tuning window and the control panel.
//
// A row is drawn from an FCXMRTunable (what it is: label, kind, range, options) plus a binding (where its value
// lives). The tuning window binds rows to the registry, so edits are clamped and saved; the control panel binds
// its rows straight to the subsystem. Same widgets and palette in both, changed in one place.

#pragma once

#include "CoreMinimal.h"
#include "CXMRTuningSubsystem.h"

class SWidget;

namespace CXMRPanelUI
{
	/** Where a row reads and writes. The renderer only ever calls these — never the tunable's own lambdas. */
	struct FRowBinding
	{
		TFunction<float()> Get;
		/** bCommit is false while a slider is being dragged, true once the edit is done. */
		TFunction<void(float Value, bool bCommit)> Apply;
		TFunction<void(float Direction)> Step;
		TFunction<void()> Invoke;
		TFunction<FText()> Text;
		/** Greys the row out while false. Unset = always enabled. */
		TFunction<bool()> IsEnabled;
		/** Adds a Default button. Unset = no button. */
		TFunction<void()> Reset;
	};

	struct FRowSpec
	{
		FCXMRTunable Tunable;
		FRowBinding Binding;
	};

	FLinearColor BackgroundColor();
	FLinearColor HeaderColor();
	FLinearColor ReadoutColor();

	/** Through the registry by id: clamped, saved on commit, Default button on saved rows. */
	FRowBinding BindToRegistry(UCXMRTuningSubsystem* Tuning, const FCXMRTunable& Tunable);

	/** Straight to the tunable's own lambdas, clamped to its range. For rows that are not registered. */
	FRowBinding BindDirect(const FCXMRTunable& Tunable);

	TSharedRef<SWidget> MakeHeader(const FText& Category);
	TSharedRef<SWidget> MakeRow(const FCXMRTunable& Tunable, const FRowBinding& Binding);

	/** Rows under their category headers, categories in order of first appearance. */
	TSharedRef<SWidget> MakeRowList(const TArray<FRowSpec>& Rows);

	/** A row of tab buttons; the active one is highlighted. */
	TSharedRef<SWidget> MakeTabBar(const TArray<FText>& Labels, TFunction<int32()> GetActive, TFunction<void(int32)> Select);

	TSharedRef<SWidget> MakeScroll(const TSharedRef<SWidget>& Content);

	/** The dark panel ground both windows sit on. */
	TSharedRef<SWidget> MakeBackground(const TSharedRef<SWidget>& Content);
}
