#include "CXMRDesignOption.h"
#include "CXMRPartAssembly.h"
bool UCXMRDesignOption::Validate(FString& OutError) const
{
	if (OptionId.IsNone() || SlotId.IsNone())
	{
		OutError = TEXT("Option ID and slot ID are required.");
		return false;
	}
	if (AssemblyActor.IsNull())
	{
		OutError = TEXT("Assembly class is missing.");
		return false;
	}
	if (!SlotLocalOffset.IsValid() || SlotLocalOffset.GetScale3D().GetMin() <= SMALL_NUMBER)
	{
		OutError = TEXT("Slot offset must be finite, normalized and have positive scale.");
		return false;
	}
	OutError.Reset();
	return true;
}
