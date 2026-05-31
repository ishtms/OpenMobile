#include "OpenMobileDeviceWindowMode.h"

EOpenMobileWindowMode FOpenMobileDeviceWindowMode::Normalize(
	EOpenMobileWindowMode Mode
)
{
	switch (Mode)
	{
	case EOpenMobileWindowMode::Unknown:
	case EOpenMobileWindowMode::FullScreen:
	case EOpenMobileWindowMode::Split:
	case EOpenMobileWindowMode::Floating:
	case EOpenMobileWindowMode::Freeform:
		return Mode;
	}
	return EOpenMobileWindowMode::Unknown;
}

EOpenMobileWindowMode FOpenMobileDeviceWindowMode::FromAndroid(
	bool bIsInMultiWindowMode,
	bool bIsInPictureInPictureMode
)
{
	if (bIsInPictureInPictureMode)
	{
		return EOpenMobileWindowMode::Floating;
	}
	return bIsInMultiWindowMode
		? EOpenMobileWindowMode::Unknown
		: EOpenMobileWindowMode::FullScreen;
}

EOpenMobileWindowMode FOpenMobileDeviceWindowMode::FromIOS(
	bool bMatchesScreenBounds
)
{
	return bMatchesScreenBounds
		? EOpenMobileWindowMode::FullScreen
		: EOpenMobileWindowMode::Unknown;
}
