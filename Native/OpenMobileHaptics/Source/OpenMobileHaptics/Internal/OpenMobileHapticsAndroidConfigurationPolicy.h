#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

class FOpenMobileHapticsAndroidConfigurationPolicy final
{
public:
	static void ApplyCapabilityMask(
		bool bCustomVibrationEnabled,
		FOpenMobileHapticCapabilities& Capabilities
	);
};
