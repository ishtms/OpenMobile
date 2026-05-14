#include "OpenMobileDeviceIOSLocale.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "OpenMobileDeviceLocaleInfo.h"

#import <Foundation/Foundation.h>

FOpenMobileLocaleSnapshot GetOpenMobileDeviceIOSLocaleSnapshot()
{
	@autoreleasepool
	{
		NSArray<NSString*>* NativeLanguages = [NSLocale preferredLanguages];
		TArray<FString> PreferredLanguages;
		PreferredLanguages.Reserve([NativeLanguages count]);
		for (NSString* NativeLanguage in NativeLanguages)
		{
			PreferredLanguages.Add(FString(NativeLanguage));
		}
		return FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
			PreferredLanguages,
			true,
			FInternationalization::Get().GetCurrentCulture()->GetName()
		);
	}
}
