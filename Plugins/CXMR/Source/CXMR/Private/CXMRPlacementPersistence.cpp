// Copyright GMTCK CX.

#include "CXMRPlacementComponent.h"
#include "CXMRMarkerProfile.h"
#include "CXMRVehicleLoaderComponent.h"
#include "CXMRVehicleProfile.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#define LOCTEXT_NAMESPACE "CXMRAlignmentStorage"

namespace
{
	bool CollectCalibrationIds(const TArray<FCXMRMarkerEntry>& Entries, TSet<int32>& Ids)
	{
		Ids.Reset();
		for (const FCXMRMarkerEntry& Entry : Entries)
		{
			if (Entry.Role != ECXMRMarkerRole::Calibration) { continue; }
			if (Entry.MarkerId <= 0 || Ids.Contains(Entry.MarkerId)) { return false; }
			Ids.Add(Entry.MarkerId);
		}
		return !Ids.IsEmpty();
	}

	bool ReadGroupIdentity(const TSharedPtr<FJsonObject>& Data, FName Group, TSet<int32>& Ids)
	{
		FString StoredGroup;
		double GroupVersion = 0, AlignmentVersion = 0;
		const TArray<TSharedPtr<FJsonValue>>* Validated = nullptr;
		if (!Data.IsValid() || !Data->TryGetStringField(TEXT("alignmentGroup"), StoredGroup)
			|| FName(*StoredGroup) != Group || !Data->TryGetNumberField(TEXT("alignmentGroupVersion"), GroupVersion) || GroupVersion != 1
			|| !Data->TryGetNumberField(TEXT("alignmentVersion"), AlignmentVersion) || AlignmentVersion != 1
			|| !Data->TryGetArrayField(TEXT("alignmentMarkers"), Validated)) { return false; }
		Ids.Reset();
		for (const TSharedPtr<FJsonValue>& Value : *Validated)
		{
			double Id = 0;
			if (!Value.IsValid() || !Value->TryGetNumber(Id) || !FMath::IsFinite(Id) || Id <= 0 || Id > MAX_int32
				|| Id != FMath::FloorToDouble(Id) || Ids.Contains(static_cast<int32>(Id))) { return false; }
			Ids.Add(static_cast<int32>(Id));
		}
		return !Ids.IsEmpty();
	}

	bool ReplaceCalibrationFile(const FString& Path, const FString& Pending)
	{
#if PLATFORM_WINDOWS
		// 기존 파일을 먼저 삭제하지 않고 교체함.
		const FString Source = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*Pending);
		const FString Target = IFileManager::Get().ConvertToAbsolutePathForExternalAppForWrite(*Path);
		return ::MoveFileExW(*Source, *Target, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
		const FString Backup = Path + TEXT(".previous");
		const bool bHadFile = IFileManager::Get().FileExists(*Path);
		if (bHadFile && IFileManager::Get().Copy(*Backup, *Path) != COPY_OK) { return false; }
		if (!IFileManager::Get().Move(*Path, *Pending, true, false, false, true))
		{
			if (bHadFile) { IFileManager::Get().Copy(*Path, *Backup); }
			return false;
		}
		IFileManager::Get().Delete(*Backup);
		return true;
#endif
	}
}

bool UCXMRPlacementComponent::WriteCalibrationData(const TArray<FCXMRMarkerEntry>& Entries, const TSet<int32>& ValidatedIds, int32 PrimaryMarker)
{
	bLastCalibrationSaveSucceeded = false;
	AlignmentStorageMessage = FText::GetEmpty();
	const FString Path = GetCalibrationFilePath();
	if (Path.IsEmpty() || !MarkerProfile) { return false; }
	const FName Group = GetAlignmentGroup();
	if (!Group.IsNone())
	{
		TSet<int32> ConfiguredIds;
		if (!CollectCalibrationIds(Entries, ConfiguredIds) || ConfiguredIds.Num() != ValidatedIds.Num() || !ConfiguredIds.Includes(ValidatedIds))
		{
			AlignmentStorageMessage = LOCTEXT("IncompleteGroup", "Not saved. Shared alignment requires a fresh, complete capture of all calibration markers.");
			return false;
		}
		// 다른 마커 구성으로 기존 그룹 파일을 덮어쓰지 않음.
		if (IFileManager::Get().FileExists(*Path))
		{
			FString Previous;
			TSharedPtr<FJsonObject> PreviousData;
			TSet<int32> PreviousIds;
			if (!FFileHelper::LoadFileToString(Previous, *Path)
				|| !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Previous), PreviousData)
				|| !ReadGroupIdentity(PreviousData, Group, PreviousIds)
				|| PreviousIds.Num() != ConfiguredIds.Num() || !PreviousIds.Includes(ConfiguredIds))
			{
				AlignmentStorageMessage = LOCTEXT("IncompatibleGroup", "Not saved. The group file has incompatible calibration markers or metadata. Reload a compatible profile, or reset this group's alignment before changing its layout.");
				return false;
			}
		}
	}
	TArray<TSharedPtr<FJsonValue>> Markers;
	for (const FCXMRMarkerEntry& Entry : Entries)
	{
		if (!Group.IsNone() && Entry.Role != ECXMRMarkerRole::Calibration) { continue; }
		if (Entry.LocalOffset.ContainsNaN()) { return false; }
		const FVector Location = Entry.LocalOffset.GetLocation();
		const FRotator Rotation = Entry.LocalOffset.Rotator();
		TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetNumberField(TEXT("id"), Entry.MarkerId);
		Item->SetStringField(TEXT("label"), Entry.Label.ToString());
		Item->SetNumberField(TEXT("x"), Location.X);
		Item->SetNumberField(TEXT("y"), Location.Y);
		Item->SetNumberField(TEXT("z"), Location.Z);
		Item->SetNumberField(TEXT("pitch"), Rotation.Pitch);
		Item->SetNumberField(TEXT("yaw"), Rotation.Yaw);
		Item->SetNumberField(TEXT("roll"), Rotation.Roll);
		Markers.Add(MakeShared<FJsonValueObject>(Item));
	}
	TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("profile"), MarkerProfile->GetName());
	Data->SetStringField(TEXT("savedAt"), FDateTime::Now().ToString());
	Data->SetStringField(TEXT("units"), TEXT("cm, degrees; marker pose in anchor-local space"));
	Data->SetArrayField(TEXT("markers"), Markers);
	const UCXMRVehicleLoaderComponent* Loader = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	if (!Group.IsNone())
	{
		Data->SetStringField(TEXT("alignmentGroup"), Group.ToString());
		Data->SetNumberField(TEXT("alignmentGroupVersion"), 1);
	}
	else if (Loader && Loader->Profile)
	{
		Data->SetStringField(TEXT("vehicleProfile"), Loader->Profile->GetPathName());
		Data->SetStringField(TEXT("vehicleOffset"), Loader->Profile->VehicleRootOffset.ToString());
		Data->SetStringField(TEXT("vehicleClass"), Loader->Profile->VehicleActor.ToSoftObjectPath().ToString());
	}
	if (!ValidatedIds.IsEmpty())
	{
		TArray<int32> Ids = ValidatedIds.Array();
		Ids.Sort();
		TArray<TSharedPtr<FJsonValue>> Validated;
		for (int32 Id : Ids) { Validated.Add(MakeShared<FJsonValueNumber>(Id)); }
		Data->SetNumberField(TEXT("alignmentVersion"), 1);
		Data->SetArrayField(TEXT("alignmentMarkers"), Validated);
		Data->SetNumberField(TEXT("primaryMarker"), ValidatedIds.Contains(PrimaryMarker) ? PrimaryMarker : Ids[0]);
		if (!Loader || !Loader->Profile) { return false; }
	}
	FString Text;
	if (!FJsonSerializer::Serialize(Data, TJsonWriterFactory<>::Create(&Text))) { return false; }
	IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
	// 임시 파일을 쓴 뒤 교체함. 실패하면 이전 저장값 유지함.
	const FString Pending = Path + TEXT(".pending");
	if (!FFileHelper::SaveStringToFile(Text, *Pending)
		|| !ReplaceCalibrationFile(Path, Pending))
	{
		IFileManager::Get().Delete(*Pending);
		return false;
	}
	bLastCalibrationSaveSucceeded = true;
	LoadedAlignmentGroup = Group;
	return true;
}

bool UCXMRPlacementComponent::SaveCalibrationToDisk()
{
	if (!MarkerProfile) { bLastCalibrationSaveSucceeded = false; return false; }
	if (bManualAlignment || bCapturingAlignment || (!GetAlignmentGroup().IsNone() && !HasSavedAlignment())) { return RequestAlignmentSave(); }
	if (GetAlignmentGroup() != LoadedAlignmentGroup)
	{
		bLastCalibrationSaveSucceeded = false;
		AlignmentStorageMessage = LOCTEXT("ScopeChanged", "Not saved. Alignment Group changed. Reload the vehicle, then confirm and save alignment again.");
		return false;
	}
	TSet<int32> CurrentIds;
	for (const FCXMRMarkerEntry& Entry : MarkerProfile->Markers)
	{
		if (Entry.Role == ECXMRMarkerRole::Calibration && Entry.MarkerId > 0) { CurrentIds.Add(Entry.MarkerId); }
	}
	const bool bSameMarkers = CurrentIds.Num() == SavedAlignmentMarkers.Num()
		&& CurrentIds.Includes(SavedAlignmentMarkers);
	// 같은 마커 구성이면 복원 정보 유지함. 바뀐 구성은 다시 확인해야 함.
	if (bSameMarkers) { return WriteCalibrationData(MarkerProfile->Markers, SavedAlignmentMarkers, SavedPrimaryMarker); }
	if (!WriteCalibrationData(MarkerProfile->Markers, {})) { return false; }
	SavedAlignmentMarkers.Reset();
	SavedPrimaryMarker = 0;
	return true;
}

bool UCXMRPlacementComponent::LoadCalibrationFromDisk()
{
	const FName Group = GetAlignmentGroup();
	AlignmentStorageMessage = Group.IsNone() ? FText::GetEmpty()
		: LOCTEXT("GroupLoadRejected", "Shared alignment not loaded. Check the group's calibration marker IDs and saved file.");
	FString Path = GetCalibrationFilePath();
	if (Path.IsEmpty() || !MarkerProfile) { return false; }
	if (!IFileManager::Get().FileExists(*Path))
	{
		if (!Group.IsNone())
		{
			AlignmentStorageMessage = LOCTEXT("NoGroupSave", "No saved alignment for this group. Align, confirm and save once.");
			return false;
		}
		// 기존 공용 파일은 차량별 저장이 없을 때만 읽음.
		Path = FPaths::ProjectSavedDir() / TEXT("CXMR")
			/ FString::Printf(TEXT("MarkerCalib_%s.json"), *MarkerProfile->GetCalibrationId());
	}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { return false; }
	TSharedPtr<FJsonObject> Data;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Data) || !Data.IsValid()) { return false; }
	TSet<int32> GroupIds;
	if (!Group.IsNone())
	{
		TSet<int32> ConfiguredIds;
		if (!CollectCalibrationIds(MarkerProfile->Markers, ConfiguredIds) || !ReadGroupIdentity(Data, Group, GroupIds)
			|| GroupIds.Num() != ConfiguredIds.Num() || !GroupIds.Includes(ConfiguredIds)) { return false; }
	}
	const UCXMRVehicleLoaderComponent* Loader = GetOwner() ? GetOwner()->FindComponentByClass<UCXMRVehicleLoaderComponent>() : nullptr;
	FString VehicleProfile, Offset;
	if (Group.IsNone() && Data->TryGetStringField(TEXT("vehicleProfile"), VehicleProfile)
		&& (!Loader || !Loader->Profile || Loader->Profile->GetPathName() != VehicleProfile)) { return false; }
	if (Group.IsNone() && Data->TryGetStringField(TEXT("vehicleOffset"), Offset)
		&& (!Loader || !Loader->Profile || Loader->Profile->VehicleRootOffset.ToString() != Offset)) { return false; }
	FString VehicleClass;
	if (Group.IsNone() && Data->TryGetStringField(TEXT("vehicleClass"), VehicleClass)
		&& (!Loader || !Loader->Profile || Loader->Profile->VehicleActor.ToSoftObjectPath().ToString() != VehicleClass)) { return false; }
	const TArray<TSharedPtr<FJsonValue>>* Markers = nullptr;
	if (!Data->TryGetArrayField(TEXT("markers"), Markers) || Markers->IsEmpty()) { return false; }
	TArray<FCXMRMarkerEntry> Entries = MarkerProfile->Markers;
	TSet<int32> LoadedIds;
	for (const TSharedPtr<FJsonValue>& Value : *Markers)
	{
		const TSharedPtr<FJsonObject>* Item = nullptr;
		double IdValue = 0;
		if (!Value.IsValid() || !Value->TryGetObject(Item) || !(*Item)->TryGetNumberField(TEXT("id"), IdValue)
			|| !FMath::IsFinite(IdValue) || IdValue < 0 || IdValue > MAX_int32 || IdValue != FMath::FloorToDouble(IdValue)) { return false; }
		const int32 Id = static_cast<int32>(IdValue);
		if (LoadedIds.Contains(Id)) { return false; }
		LoadedIds.Add(Id);
		double Values[6];
		const TCHAR* Keys[] = { TEXT("x"), TEXT("y"), TEXT("z"), TEXT("pitch"), TEXT("yaw"), TEXT("roll") };
		for (int32 Index = 0; Index < 6; ++Index)
		{
			if (!(*Item)->TryGetNumberField(Keys[Index], Values[Index]) || !FMath::IsFinite(Values[Index])) { return false; }
		}
		FCXMRMarkerEntry* Entry = Entries.FindByPredicate([Id](const FCXMRMarkerEntry& Candidate) { return Candidate.MarkerId == Id; });
		if (!Group.IsNone() && (!GroupIds.Contains(Id) || !Entry || Entry->Role != ECXMRMarkerRole::Calibration)) { return false; }
		if (!Entry)
		{
			FCXMRMarkerEntry Added;
			Added.MarkerId = Id;
			Added.Role = ECXMRMarkerRole::Calibration;
			FString Label;
			if ((*Item)->TryGetStringField(TEXT("label"), Label)) { Added.Label = FName(*Label); }
			Entry = &Entries.Add_GetRef(Added);
		}
		Entry->LocalOffset = FTransform(FRotator(Values[3], Values[4], Values[5]), FVector(Values[0], Values[1], Values[2]));
	}
	if (!Group.IsNone() && (LoadedIds.Num() != GroupIds.Num() || !LoadedIds.Includes(GroupIds))) { return false; }
	TSet<int32> ValidatedIds;
	int32 Primary = 0;
	double Version = 0;
	if (Data->TryGetNumberField(TEXT("alignmentVersion"), Version))
	{
		const TArray<TSharedPtr<FJsonValue>>* Validated = nullptr;
		double PrimaryValue = 0;
		if (Version != 1 || !Data->TryGetArrayField(TEXT("alignmentMarkers"), Validated)
			|| !Data->TryGetNumberField(TEXT("primaryMarker"), PrimaryValue) || PrimaryValue <= 0 || PrimaryValue > MAX_int32
			|| PrimaryValue != FMath::FloorToDouble(PrimaryValue)) { return false; }
		Primary = static_cast<int32>(PrimaryValue);
		for (const TSharedPtr<FJsonValue>& Value : *Validated)
		{
			double IdValue = 0;
			if (!Value.IsValid() || !Value->TryGetNumber(IdValue) || !FMath::IsFinite(IdValue) || IdValue <= 0
				|| IdValue > MAX_int32 || IdValue != FMath::FloorToDouble(IdValue)) { return false; }
			const int32 Id = static_cast<int32>(IdValue);
			const FCXMRMarkerEntry* Entry = Entries.FindByPredicate([Id](const FCXMRMarkerEntry& Candidate) { return Candidate.MarkerId == Id; });
			if (!LoadedIds.Contains(Id) || !Entry || Entry->Role != ECXMRMarkerRole::Calibration) { return false; }
			ValidatedIds.Add(Id);
		}
		if (!ValidatedIds.Contains(Primary)) { return false; }
		// 일부 마커만 새 정렬로 저장한 파일은 복원에 사용하지 않음.
		for (const FCXMRMarkerEntry& Entry : Entries)
		{
			if (Entry.Role == ECXMRMarkerRole::Calibration && Entry.MarkerId > 0 && !ValidatedIds.Contains(Entry.MarkerId)) { return false; }
		}
	}
	// 전체 파일 검증 후 한 번에 적용함.
	if (!StartupBackedUp.Contains(Path))
	{
		IFileManager::Get().Copy(*FPaths::ChangeExtension(Path, TEXT("startup.json")), *Path);
		StartupBackedUp.Add(Path);
	}
	MarkerProfile->Markers = MoveTemp(Entries);
	SavedAlignmentMarkers = MoveTemp(ValidatedIds);
	SavedPrimaryMarker = Primary;
	LoadedAlignmentGroup = Group;
	AlignmentStorageMessage = FText::GetEmpty();
	return true;
}

#undef LOCTEXT_NAMESPACE
