#include "OpenMobileDeviceIOSLocale.h"

#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "OpenMobileDeviceLocaleInfo.h"
#include "OpenMobileDeviceTimeZoneInfo.h"

#import <Foundation/Foundation.h>

FOpenMobileLocaleSnapshot GetOpenMobileDeviceIOSLocaleSnapshot(
	const FDateTime& UtcInstant
)
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
		FOpenMobileLocaleSnapshot Snapshot =
			FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
				PreferredLanguages,
				true,
				FInternationalization::Get().GetCurrentCulture()->GetName()
			);
		NSLocale* Locale = [NSLocale currentLocale];
		FOpenMobileDeviceLocaleInfo::ApplyLocale(
			Snapshot,
			FString([Locale localeIdentifier]),
			FString([Locale languageCode]),
			FString([Locale scriptCode]),
			FString([Locale countryCode]),
			FString([Locale currencyCode])
		);
		NSTimeZone* TimeZone = [NSTimeZone localTimeZone];
		NSDate* Instant = [NSDate dateWithTimeIntervalSince1970:
			static_cast<NSTimeInterval>(UtcInstant.ToUnixTimestamp())];
		FOpenMobileDeviceTimeZoneInfo::Apply(
			Snapshot,
			FString([TimeZone name]),
			[TimeZone secondsFromGMTForDate:Instant],
			true,
			[TimeZone isDaylightSavingTimeForDate:Instant],
			true
		);
		NSString* HourPattern = [NSDateFormatter
			dateFormatFromTemplate:@"j"
			options:0
			locale:Locale];
		FString TimeFormat;
		if ([HourPattern rangeOfCharacterFromSet:
			[NSCharacterSet characterSetWithCharactersInString:@"Hk"]].location
			!= NSNotFound)
		{
			TimeFormat = TEXT("24");
		}
		else if ([HourPattern rangeOfCharacterFromSet:
			[NSCharacterSet characterSetWithCharactersInString:@"hKa"]].location
			!= NSNotFound)
		{
			TimeFormat = TEXT("12");
		}
		NSNumber* UsesMetricSystem = [Locale countryCode].length > 0
			? [Locale objectForKey:NSLocaleUsesMetricSystem]
			: nil;
		FOpenMobileDeviceLocaleInfo::ApplyRegionalPreferences(
			Snapshot,
			TimeFormat,
			UsesMetricSystem
				? ([UsesMetricSystem boolValue] ? TEXT("metric") : TEXT("imperial"))
				: FString()
		);
		return Snapshot;
	}
}
