#include "OpenMobileAdsAdMobSettingsValidator.h"

#include "OpenMobileAdsAdMobSettings.h"

namespace OpenMobileAdsAdMobEditorPrivate
{
	/** Rejects spacing that Google app and ad-unit identifiers never permit. */
	bool ContainsWhitespace(const FString& Value)
	{
		for (const TCHAR Character : Value)
		{
			if (FChar::IsWhitespace(Character))
			{
				return true;
			}
		}
		return false;
	}

	/** Checks one Google identifier for presence, spacing, and its required app or unit separator. */
	void ValidateIdentifier(
		const TCHAR* FieldName,
		const FString& Value,
		const TCHAR RequiredSeparator,
		TArray<FString>& OutErrors
	)
	{
		if (Value.IsEmpty())
		{
			OutErrors.Add(FString::Printf(TEXT("%s is required."), FieldName));
			return;
		}

		if (ContainsWhitespace(Value))
		{
			OutErrors.Add(FString::Printf(TEXT("%s must not contain whitespace."), FieldName));
		}
		if (!Value.StartsWith(TEXT("ca-app-pub-")))
		{
			OutErrors.Add(FString::Printf(TEXT("%s must start with 'ca-app-pub-'."), FieldName));
		}
		if (!Value.Contains(FString::Chr(RequiredSeparator)))
		{
			OutErrors.Add(FString::Printf(
				TEXT("%s must contain '%c'."),
				FieldName,
				RequiredSeparator
			));
		}
	}
}

TArray<FString> FOpenMobileAdsAdMobSettingsValidator::Validate(
	const UOpenMobileAdsAdMobSettings& Settings,
	bool bForShipping
)
{
	TArray<FString> Errors;
	Errors.Reserve(8 + Settings.TestDeviceIdentifiers.Num());
	OpenMobileAdsAdMobEditorPrivate::ValidateIdentifier(
		TEXT("Android app ID"),
		Settings.AndroidAppId,
		TEXT('~'),
		Errors
	);
	OpenMobileAdsAdMobEditorPrivate::ValidateIdentifier(
		TEXT("iOS app ID"),
		Settings.IOSAppId,
		TEXT('~'),
		Errors
	);
	TSet<FString> SeenTestDeviceIdentifiers;
	for (int32 Index = 0; Index < Settings.TestDeviceIdentifiers.Num(); ++Index)
	{
		const FString& Identifier = Settings.TestDeviceIdentifiers[Index];
		const FString IdentifierKey = Identifier.ToLower();
		if (
			!FOpenMobileAdsDevelopmentConfiguration::IsValidTestDeviceIdentifier(Identifier)
			|| SeenTestDeviceIdentifiers.Contains(IdentifierKey)
		)
		{
			Errors.Add(FString::Printf(
				TEXT("AdMob test-device identifier at index %d is invalid."),
				Index
			));
		}
		SeenTestDeviceIdentifiers.Add(IdentifierKey);
	}
	if (bForShipping && !Settings.TestDeviceIdentifiers.IsEmpty())
	{
		Errors.Add(TEXT("AdMob test-device identifiers are not allowed in shipping builds."));
	}
	if (
		bForShipping
		&& (
			UOpenMobileAdsAdMobSettings::IsGoogleSampleIdentifier(Settings.AndroidAppId)
			|| UOpenMobileAdsAdMobSettings::IsGoogleSampleIdentifier(Settings.IOSAppId)
		)
	)
	{
		Errors.Add(TEXT("Google sample IDs are not allowed in shipping builds."));
	}
	return Errors;
}
