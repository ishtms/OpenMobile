#include "OpenMobileDeviceWindowOrientation.h"

EOpenMobileWindowOrientation
FOpenMobileDeviceWindowOrientation::FromAndroidRotation(
	int32 Rotation,
	bool bNaturalOrientationLandscape
)
{
	if (Rotation < 0 || Rotation > 3)
	{
		return EOpenMobileWindowOrientation::Unknown;
	}
	static constexpr EOpenMobileWindowOrientation NaturalPortrait[] = {
		EOpenMobileWindowOrientation::Portrait,
		EOpenMobileWindowOrientation::LandscapeLeft,
		EOpenMobileWindowOrientation::PortraitUpsideDown,
		EOpenMobileWindowOrientation::LandscapeRight
	};
	static constexpr EOpenMobileWindowOrientation NaturalLandscape[] = {
		EOpenMobileWindowOrientation::LandscapeLeft,
		EOpenMobileWindowOrientation::PortraitUpsideDown,
		EOpenMobileWindowOrientation::LandscapeRight,
		EOpenMobileWindowOrientation::Portrait
	};
	return bNaturalOrientationLandscape
		? NaturalLandscape[Rotation]
		: NaturalPortrait[Rotation];
}

EOpenMobileWindowOrientation
FOpenMobileDeviceWindowOrientation::FromIOSInterfaceOrientation(
	int32 InterfaceOrientation
)
{
	switch (InterfaceOrientation)
	{
	case 1:
		return EOpenMobileWindowOrientation::Portrait;
	case 2:
		return EOpenMobileWindowOrientation::PortraitUpsideDown;
	case 3:
		return EOpenMobileWindowOrientation::LandscapeLeft;
	case 4:
		return EOpenMobileWindowOrientation::LandscapeRight;
	default:
		return EOpenMobileWindowOrientation::Unknown;
	}
}
