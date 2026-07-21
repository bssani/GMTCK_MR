// Copyright GMTCK CX.

#include "CXMRMarkerProfile.h"

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

ECXMRMarkerRole UCXMRMarkerProfile::GetRole(int32 MarkerId) const
{
	FCXMRMarkerEntry Entry;
	return FindEntry(MarkerId, Entry) ? Entry.Role : ECXMRMarkerRole::Reserved;
}
