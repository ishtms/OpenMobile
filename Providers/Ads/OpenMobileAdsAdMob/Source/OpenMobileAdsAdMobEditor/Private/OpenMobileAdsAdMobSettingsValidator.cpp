#include "OpenMobileAdsAdMobSettingsValidator.h"

#include "OpenMobileAdsAdMobSettings.h"

namespace OpenMobileAdsAdMobEditorPrivate
{
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
	Errors.Reserve(4);
	OpenMobileAdsAdMobEditorPrivate::ValidateIdentifier(
		TEXT("Android app ID"),
		Settings.AndroidAppId,
		TEXT('~'),
		Errors
	);
	OpenMobileAdsAdMobEditorPrivate::ValidateIdentifier(
		TEXT("Android rewarded ad-unit ID"),
		Settings.AndroidRewardedAdUnitId,
		TEXT('/'),
		Errors
	);
	OpenMobileAdsAdMobEditorPrivate::ValidateIdentifier(
		TEXT("iOS app ID"),
		Settings.IOSAppId,
		TEXT('~'),
		Errors
	);
	OpenMobileAdsAdMobEditorPrivate::ValidateIdentifier(
		TEXT("iOS rewarded ad-unit ID"),
		Settings.IOSRewardedAdUnitId,
		TEXT('/'),
		Errors
	);
	if (
		bForShipping
		&& (
			Settings.AndroidAppId == TEXT("ca-app-pub-3940256099942544~3347511713")
			|| Settings.AndroidRewardedAdUnitId == TEXT("ca-app-pub-3940256099942544/5224354917")
			|| Settings.IOSAppId == TEXT("ca-app-pub-3940256099942544~1458002511")
			|| Settings.IOSRewardedAdUnitId == TEXT("ca-app-pub-3940256099942544/1712485313")
		)
	)
	{
		Errors.Add(TEXT("Google sample IDs are not allowed in shipping builds."));
	}
	return Errors;
}
