#pragma once

#include "OpenMobileDeviceLocaleTypes.h"

class OPENMOBILEDEVICE_API FOpenMobileDeviceLocaleInfo final
{
public:
	static FOpenMobileLocaleSnapshot BuildPreferredLanguages(
		TConstArrayView<FString> PreferredLanguages,
		bool bPreferredLanguagesAvailable,
		const FString& ActiveUnrealCulture
	);
};
