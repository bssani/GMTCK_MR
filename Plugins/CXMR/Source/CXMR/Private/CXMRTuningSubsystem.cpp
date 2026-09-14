// Copyright GMTCK CX.

#include "CXMRTuningSubsystem.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogCXMRTuning, Log, All);

namespace
{
	bool IsLive(const FCXMRTunable& Tunable)
	{
		return Tunable.Owner.IsExplicitlyNull() || Tunable.Owner.IsValid();
	}

	bool HasRange(ECXMRTunableKind Kind)
	{
		return Kind == ECXMRTunableKind::Bool || Kind == ECXMRTunableKind::Float || Kind == ECXMRTunableKind::Choice;
	}
}

void UCXMRTuningSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	LoadFromDisk();
}

bool UCXMRTuningSubsystem::Register(FCXMRTunable Tunable)
{
	if (Tunable.Id.IsNone())
	{
		return false;
	}

	// A component destroyed without EndPlay leaves rows behind; drop them before checking for duplicates.
	Tunables.RemoveAll([](const FCXMRTunable& Existing) { return !IsLive(Existing); });

	if (const FCXMRTunable* Existing = Find(Tunable.Id))
	{
		UE_LOG(LogCXMRTuning, Warning, TEXT("Tunable '%s' is already registered by %s; ignoring the one from %s."),
			*Tunable.Id.ToString(), *GetNameSafe(Existing->Owner.Get()), *GetNameSafe(Tunable.Owner.Get()));
		return false;
	}

	if (Tunable.Kind == ECXMRTunableKind::Bool)
	{
		Tunable.Min = 0.0f;
		Tunable.Max = 1.0f;
	}
	else if (Tunable.Kind == ECXMRTunableKind::Choice)
	{
		Tunable.Min = 0.0f;
		Tunable.Max = static_cast<float>(FMath::Max(0, Tunable.Options.Num() - 1));
	}

	// The saved value goes in before the row is visible, so the feature starts where the operator left it.
	if (Tunable.bPersist && Tunable.Set)
	{
		if (const float* Saved = SavedValues.Find(Tunable.Id))
		{
			Tunable.Set(FMath::Clamp(*Saved, Tunable.Min, Tunable.Max));
			UE_LOG(LogCXMRTuning, Log, TEXT("Tunable '%s' restored to %g"), *Tunable.Id.ToString(), *Saved);
		}
	}

	Tunables.Add(MoveTemp(Tunable));
	OnTunablesChanged.Broadcast();
	return true;
}

void UCXMRTuningSubsystem::UnregisterOwner(const UObject* Owner)
{
	const int32 Removed = Tunables.RemoveAll([Owner](const FCXMRTunable& Tunable)
	{
		return Tunable.Owner.Get() == Owner || !IsLive(Tunable);
	});
	if (Removed > 0)
	{
		OnTunablesChanged.Broadcast();
	}
}

TArray<const FCXMRTunable*> UCXMRTuningSubsystem::GetTunables() const
{
	TArray<const FCXMRTunable*> Live;
	for (const FCXMRTunable& Tunable : Tunables)
	{
		if (IsLive(Tunable))
		{
			Live.Add(&Tunable);
		}
	}
	return Live;
}

const FCXMRTunable* UCXMRTuningSubsystem::Find(FName Id) const
{
	for (const FCXMRTunable& Tunable : Tunables)
	{
		if (Tunable.Id == Id && IsLive(Tunable))
		{
			return &Tunable;
		}
	}
	return nullptr;
}

bool UCXMRTuningSubsystem::ApplyValue(FName Id, float Value, bool bSave)
{
	const FCXMRTunable* Tunable = Find(Id);
	if (!Tunable || !Tunable->Set)
	{
		return false;
	}

	const float Applied = HasRange(Tunable->Kind) ? FMath::Clamp(Value, Tunable->Min, Tunable->Max) : Value;
	Tunable->Set(Applied);

	if (bSave && Tunable->bPersist)
	{
		SavedValues.Add(Id, Applied);
		SaveToDisk();
	}
	return true;
}

bool UCXMRTuningSubsystem::ResetToDefault(FName Id)
{
	const FCXMRTunable* Tunable = Find(Id);
	if (!Tunable)
	{
		return false;
	}
	const float Default = Tunable->Default;
	const bool bApplied = ApplyValue(Id, Default, /*bSave*/ false);
	if (SavedValues.Remove(Id) > 0)
	{
		SaveToDisk();
	}
	return bApplied;
}

bool UCXMRTuningSubsystem::IsTunableEnabled(FName Id) const
{
	const FCXMRTunable* Tunable = Find(Id);
	return !Tunable || !Tunable->IsEnabled || Tunable->IsEnabled();
}

TArray<FName> UCXMRTuningSubsystem::GetTunableIds() const
{
	TArray<FName> Ids;
	for (const FCXMRTunable* Tunable : GetTunables())
	{
		Ids.Add(Tunable->Id);
	}
	return Ids;
}

float UCXMRTuningSubsystem::GetTunableValue(FName Id) const
{
	const FCXMRTunable* Tunable = Find(Id);
	return (Tunable && Tunable->Get) ? Tunable->Get() : 0.0f;
}

bool UCXMRTuningSubsystem::SetTunableValue(FName Id, float Value)
{
	return ApplyValue(Id, Value, /*bSave*/ true);
}

bool UCXMRTuningSubsystem::InvokeTunable(FName Id, float Direction)
{
	const FCXMRTunable* Tunable = Find(Id);
	if (!Tunable)
	{
		return false;
	}
	if (Tunable->Kind == ECXMRTunableKind::Action && Tunable->Invoke)
	{
		Tunable->Invoke();
		return true;
	}
	if (Tunable->Kind == ECXMRTunableKind::Stepper && Tunable->Step)
	{
		Tunable->Step(Direction);
		return true;
	}
	return false;
}

FString UCXMRTuningSubsystem::GetTunableText(FName Id) const
{
	const FCXMRTunable* Tunable = Find(Id);
	if (!Tunable)
	{
		return FString();
	}
	if (Tunable->Text)
	{
		return Tunable->Text().ToString();
	}
	return Tunable->Get ? FString::SanitizeFloat(Tunable->Get()) : FString();
}

FString UCXMRTuningSubsystem::GetTuningFilePath() const
{
	return FPaths::ProjectSavedDir() / TEXT("CXMR") / TEXT("Tuning.json");
}

void UCXMRTuningSubsystem::LoadFromDisk()
{
	SavedValues.Reset();

	const FString Path = GetTuningFilePath();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path))
	{
		return;   // first run on this PC
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogCXMRTuning, Warning, TEXT("Could not parse %s; starting from defaults."), *Path);
		return;
	}

	const TSharedPtr<FJsonObject>* Values = nullptr;
	if (Root->TryGetObjectField(TEXT("values"), Values) && Values && Values->IsValid())
	{
		for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*Values)->Values)
		{
			double Number = 0.0;
			if (Pair.Value.IsValid() && Pair.Value->TryGetNumber(Number))
			{
				SavedValues.Add(FName(*Pair.Key), static_cast<float>(Number));
			}
		}
	}
	UE_LOG(LogCXMRTuning, Log, TEXT("Loaded %d tuned value(s) from %s"), SavedValues.Num(), *Path);
}

void UCXMRTuningSubsystem::SaveToDisk() const
{
	TSharedRef<FJsonObject> Values = MakeShared<FJsonObject>();
	for (const TPair<FName, float>& Pair : SavedValues)
	{
		Values->SetNumberField(Pair.Key.ToString(), Pair.Value);
	}

	TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetStringField(TEXT("savedAt"), FDateTime::Now().ToString());
	Root->SetStringField(TEXT("note"), TEXT("Session tuning for this PC. Delete a line, or the file, to go back to defaults."));
	Root->SetObjectField(TEXT("values"), Values);

	FString Text;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Text);
	if (!FJsonSerializer::Serialize(Root, Writer))
	{
		return;
	}

	const FString Path = GetTuningFilePath();
	// SaveStringToFile does not create directories, and Saved/CXMR may not exist yet.
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), /*Tree*/ true);
	if (!FFileHelper::SaveStringToFile(Text, *Path))
	{
		UE_LOG(LogCXMRTuning, Warning, TEXT("Failed to write %s"), *Path);
	}
}
