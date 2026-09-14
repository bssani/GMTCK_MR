// Copyright GMTCK CX.
//
// UCXMRTuningSubsystem — the live values an operator may tune during a session, and their saved state.
//
// A feature registers what it wants tunable (a getter, a setter, a range) and the tuning window draws whatever is
// registered. The window knows no feature and no feature knows the window, so a project branch adds its own rows
// (virtual hands, a program-specific rig) by registering them — without touching the template.
//
// Rows marked persistent are written to Saved/CXMR/Tuning.json when changed and re-applied the moment the same row
// registers again next session. Per PC, like the marker calibration: they describe one physical setup (the depth
// range that suits this room, the exposure that suits this lighting).

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "CXMRTuningSubsystem.generated.h"

enum class ECXMRTunableKind : uint8
{
	Bool,       // checkbox; value 0 or 1
	Float,      // spin box within [Min, Max]
	Choice,     // button cycling through Options; value = option index
	Stepper,    // [-] [+] buttons calling Step(-1 / +1)
	Action,     // button calling Invoke()
	Readout     // live text from Text()
};

/** One tunable row. Plain C++ because it holds lambdas; registered by the feature that owns the value. */
struct CXMR_API FCXMRTunable
{
	/** Stable key, also the key in Tuning.json. Dotted by area: "Depth.FarZ". */
	FName Id;
	FText Category;
	FText Label;
	FText Unit;
	ECXMRTunableKind Kind = ECXMRTunableKind::Float;

	float Min = 0.0f;
	float Max = 1.0f;
	/** Spin-box drag / arrow increment. */
	float Delta = 0.01f;
	float Default = 0.0f;
	/** Saved on change and re-applied on the next registration. */
	bool bPersist = false;

	TArray<FText> Options;                  // Choice
	TFunction<float()> Get;                 // Bool, Float, Choice
	TFunction<void(float)> Set;             // Bool, Float, Choice
	TFunction<void(float)> Step;            // Stepper: direction +1 / -1
	TFunction<void()> Invoke;               // Action
	TFunction<FText()> Text;                // Readout

	/** A row lives only as long as its owner — its lambdas normally capture the owner. */
	TWeakObjectPtr<const UObject> Owner;
};

DECLARE_MULTICAST_DELEGATE(FCXMROnTunablesChanged);

UCLASS()
class CXMR_API UCXMRTuningSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;

	/** Adds a row; a persistent row gets its saved value applied right away. False if the id is already taken. */
	bool Register(FCXMRTunable Tunable);

	/** Removes every row registered by Owner. Call from the owner's EndPlay. */
	void UnregisterOwner(const UObject* Owner);

	/** Live rows in registration order. Pointers are valid until the next Register / UnregisterOwner. */
	TArray<const FCXMRTunable*> GetTunables() const;

	/** Applies a value (clamped for Bool / Float / Choice). The window passes bSave=false while a slider moves. */
	bool ApplyValue(FName Id, float Value, bool bSave);

	/** Back to the row's default, and the saved value is forgotten. */
	bool ResetToDefault(FName Id);

	/** Fires when rows are added or removed, so an open window can rebuild. */
	FCXMROnTunablesChanged OnTunablesChanged;

	// --- Scriptable surface (Blueprint / Python) ---

	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning") TArray<FName> GetTunableIds() const;
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning") float GetTunableValue(FName Id) const;
	/** Applies and saves, as a committed edit in the window would. */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning") bool SetTunableValue(FName Id, float Value);
	/** Runs an Action row, or steps a Stepper row by Direction (+1 / -1). */
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning") bool InvokeTunable(FName Id, float Direction = 1.0f);
	UFUNCTION(BlueprintCallable, Category = "CXMR|Tuning") FString GetTunableText(FName Id) const;
	UFUNCTION(BlueprintPure, Category = "CXMR|Tuning") FString GetTuningFilePath() const;

private:
	const FCXMRTunable* Find(FName Id) const;
	void LoadFromDisk();
	void SaveToDisk() const;

	TArray<FCXMRTunable> Tunables;
	TMap<FName, float> SavedValues;
};
