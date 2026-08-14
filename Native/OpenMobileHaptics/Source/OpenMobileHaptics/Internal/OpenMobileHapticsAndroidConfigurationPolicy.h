#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

class FOpenMobileHapticsAndroidConfigurationPolicy final
{
public:
	/** Applies the project switch after hardware discovery, since supported hardware still shouldn't expose custom vibration when you've disabled it. */
	static void ApplyCapabilityMask(
		bool bCustomVibrationEnabled,
		FOpenMobileHapticCapabilities& Capabilities
	);
};
