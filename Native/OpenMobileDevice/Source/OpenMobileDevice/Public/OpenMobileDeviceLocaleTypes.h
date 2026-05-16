#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceLocaleTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileTimeFormatPreference : uint8
{
	Unknown,
	TwelveHour,
	TwentyFourHour
};

UENUM(BlueprintType)
enum class EOpenMobileMeasurementSystem : uint8
{
	Unknown,
	Metric,
	Imperial
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileLocaleSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceSnapshotMetadata Metadata;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bPreferredLanguagesAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	TArray<FString> PreferredLanguages;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString ActiveUnrealCulture;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString LocaleIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString LanguageCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString ScriptCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString RegionCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString CurrencyCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalString TimeZoneIdentifier;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalInt32 UtcOffsetSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileDeviceOptionalBool bIsDaylightSavingTime;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileTimeFormatPreference TimeFormat = EOpenMobileTimeFormatPreference::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileMeasurementSystem MeasurementSystem = EOpenMobileMeasurementSystem::Unknown;

	bool operator==(const FOpenMobileLocaleSnapshot& Other) const
	{
		return Metadata == Other.Metadata
			&& bPreferredLanguagesAvailable == Other.bPreferredLanguagesAvailable
			&& PreferredLanguages == Other.PreferredLanguages
			&& ActiveUnrealCulture == Other.ActiveUnrealCulture
			&& LocaleIdentifier == Other.LocaleIdentifier
			&& LanguageCode == Other.LanguageCode
			&& ScriptCode == Other.ScriptCode
			&& RegionCode == Other.RegionCode
			&& CurrencyCode == Other.CurrencyCode
			&& TimeZoneIdentifier == Other.TimeZoneIdentifier
			&& UtcOffsetSeconds == Other.UtcOffsetSeconds
			&& bIsDaylightSavingTime == Other.bIsDaylightSavingTime
			&& TimeFormat == Other.TimeFormat
			&& MeasurementSystem == Other.MeasurementSystem;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileLocaleSnapshotChange
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileLocaleSnapshot PreviousSnapshot;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileLocaleSnapshot CurrentSnapshot;
};
