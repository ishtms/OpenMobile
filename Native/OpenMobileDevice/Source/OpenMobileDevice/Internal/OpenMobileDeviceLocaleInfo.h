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
	static void ApplyLocale(
		FOpenMobileLocaleSnapshot& Snapshot,
		const FString& LocaleIdentifier,
		const FString& LanguageCode,
		const FString& ScriptCode,
		const FString& RegionCode,
		const FString& CurrencyCode
	);
};
