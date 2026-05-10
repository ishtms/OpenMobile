#include "OpenMobileDeviceFormFactor.h"

EOpenMobileDeviceFormFactor FOpenMobileDeviceFormFactor::Classify(
	const FOpenMobileDeviceFormFactorTraits& Traits
)
{
	if (Traits.bHasFoldableHardware || Traits.bSeparatingPosture)
	{
		return EOpenMobileDeviceFormFactor::Foldable;
	}
	if (Traits.bPhoneIdiom != Traits.bTabletIdiom)
	{
		return Traits.bPhoneIdiom
			? EOpenMobileDeviceFormFactor::Phone
			: EOpenMobileDeviceFormFactor::Tablet;
	}
	if (Traits.SmallestWindowWidthDp >= 600
		&& Traits.WindowSizeClass == EOpenMobileDeviceWindowSizeClass::Large)
	{
		return EOpenMobileDeviceFormFactor::Tablet;
	}
	if (Traits.SmallestWindowWidthDp > 0
		&& Traits.SmallestWindowWidthDp < 600
		&& Traits.WindowSizeClass == EOpenMobileDeviceWindowSizeClass::Compact)
	{
		return EOpenMobileDeviceFormFactor::Phone;
	}
	return EOpenMobileDeviceFormFactor::Unknown;
}
