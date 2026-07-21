// Copyright GMTCK CX.

#include "CXMRVehicleProfile.h"

TSet<FName> UCXMRVehicleProfile::GetManagedPartTags() const
{
	TSet<FName> Tags;
	for (const FCXMRTrim& Trim : Trims)
	{
		for (const FName& Tag : Trim.VisibleParts)
		{
			if (!Tag.IsNone())
			{
				Tags.Add(Tag);
			}
		}
	}
	return Tags;
}

bool UCXMRVehicleProfile::IsValidTrim(int32 TrimIndex) const
{
	return Trims.IsValidIndex(TrimIndex);
}
