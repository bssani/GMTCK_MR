// Copyright GMTCK CX.

#include "CXMRMarkerProfile.h"

#include "Misc/Paths.h"

FString UCXMRMarkerProfile::GetCalibrationId() const
{
	FString Id = CalibrationId.TrimStartAndEnd();
	if (Id.IsEmpty())
	{
		Id = GetName();
	}

	// The id ends up in a path, and it is authored by hand — a space or a slash would silently
	// produce a file nobody can find.
	return FPaths::MakeValidFileName(Id, TEXT('_'));
}

bool UCXMRMarkerProfile::FindEntry(int32 MarkerId, FCXMRMarkerEntry& OutEntry) const
{
	for (const FCXMRMarkerEntry& Entry : Markers)
	{
		if (Entry.MarkerId == MarkerId)
		{
			OutEntry = Entry;
			return true;
		}
	}
	return false;
}

FCXMRMarkerEntry* UCXMRMarkerProfile::GetEntryMutable(int32 MarkerId)
{
	for (FCXMRMarkerEntry& Entry : Markers)
	{
		if (Entry.MarkerId == MarkerId)
		{
			return &Entry;
		}
	}
	return nullptr;
}

ECXMRMarkerRole UCXMRMarkerProfile::GetRole(int32 MarkerId) const
{
	FCXMRMarkerEntry Entry;
	return FindEntry(MarkerId, Entry) ? Entry.Role : ECXMRMarkerRole::Reserved;
}
