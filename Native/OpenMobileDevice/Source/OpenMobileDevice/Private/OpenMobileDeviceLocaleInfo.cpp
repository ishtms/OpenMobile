#include "OpenMobileDeviceLocaleInfo.h"

namespace OpenMobileDeviceLocaleInfoPrivate
{
	bool IsAsciiAlpha(TCHAR Character)
	{
		return (Character >= TEXT('a') && Character <= TEXT('z'))
			|| (Character >= TEXT('A') && Character <= TEXT('Z'));
	}

	bool IsAsciiDigit(TCHAR Character)
	{
		return Character >= TEXT('0') && Character <= TEXT('9');
	}

	bool IsAlpha(const FString& Value)
	{
		for (const TCHAR Character : Value)
		{
			if (!IsAsciiAlpha(Character))
			{
				return false;
			}
		}
		return !Value.IsEmpty();
	}

	bool IsDigit(const FString& Value)
	{
		for (const TCHAR Character : Value)
		{
			if (!IsAsciiDigit(Character))
			{
				return false;
			}
		}
		return !Value.IsEmpty();
	}

	FString NormalizeLanguageTag(const FString& Value)
	{
		FString Normalized = Value.TrimStartAndEnd();
		Normalized.ReplaceInline(TEXT("_"), TEXT("-"));
		TArray<FString> Subtags;
		Normalized.ParseIntoArray(Subtags, TEXT("-"), false);
		if (Subtags.IsEmpty()
			|| ((Subtags[0].Len() < 2 || Subtags[0].Len() > 8)
				&& !Subtags[0].Equals(TEXT("x"), ESearchCase::IgnoreCase))
			|| !IsAlpha(Subtags[0]))
		{
			return {};
		}

		for (int32 Index = 0; Index < Subtags.Num(); ++Index)
		{
			FString& Subtag = Subtags[Index];
			if (Subtag.IsEmpty() || Subtag.Len() > 8)
			{
				return {};
			}
			for (const TCHAR Character : Subtag)
			{
				if (!IsAsciiAlpha(Character) && !IsAsciiDigit(Character))
				{
					return {};
				}
			}

			const bool bScript = Index > 0
				&& Subtag.Len() == 4
				&& IsAlpha(Subtag);
			const bool bAlphaRegion = Index > 0
				&& Subtag.Len() == 2
				&& IsAlpha(Subtag);
			const bool bNumericRegion = Index > 0
				&& Subtag.Len() == 3
				&& IsDigit(Subtag);
			Subtag.ToLowerInline();
			if (bScript)
			{
				Subtag[0] = FChar::ToUpper(Subtag[0]);
			}
			else if (bAlphaRegion)
			{
				Subtag.ToUpperInline();
			}
			else if (bNumericRegion)
			{
				continue;
			}
		}
		return FString::Join(Subtags, TEXT("-"));
	}

	FOpenMobileDeviceOptionalString MakeOptionalText(const FString& Value)
	{
		const FString Trimmed = Value.TrimStartAndEnd();
		return Trimmed.IsEmpty()
			? FOpenMobileDeviceOptionalString()
			: FOpenMobileDeviceOptionalString::MakeAvailable(Trimmed);
	}
}

FOpenMobileLocaleSnapshot FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
	TConstArrayView<FString> PreferredLanguages,
	bool bPreferredLanguagesAvailable,
	const FString& ActiveUnrealCulture
)
{
	using namespace OpenMobileDeviceLocaleInfoPrivate;
	FOpenMobileLocaleSnapshot Snapshot;
	Snapshot.bPreferredLanguagesAvailable = bPreferredLanguagesAvailable;
	if (bPreferredLanguagesAvailable)
	{
		TSet<FString> Seen;
		for (const FString& PreferredLanguage : PreferredLanguages)
		{
			FString Normalized = NormalizeLanguageTag(PreferredLanguage);
			if (!Normalized.IsEmpty() && !Seen.Contains(Normalized))
			{
				Seen.Add(Normalized);
				Snapshot.PreferredLanguages.Add(MoveTemp(Normalized));
			}
		}
	}
	const FString TrimmedCulture = ActiveUnrealCulture.TrimStartAndEnd();
	if (!TrimmedCulture.IsEmpty())
	{
		Snapshot.ActiveUnrealCulture =
			FOpenMobileDeviceOptionalString::MakeAvailable(TrimmedCulture);
	}
	return Snapshot;
}

void FOpenMobileDeviceLocaleInfo::ApplyLocale(
	FOpenMobileLocaleSnapshot& Snapshot,
	const FString& LocaleIdentifier,
	const FString& LanguageCode,
	const FString& ScriptCode,
	const FString& RegionCode,
	const FString& CurrencyCode
)
{
	using namespace OpenMobileDeviceLocaleInfoPrivate;
	Snapshot.LocaleIdentifier = MakeOptionalText(LocaleIdentifier);
	Snapshot.LanguageCode = MakeOptionalText(LanguageCode);
	Snapshot.ScriptCode = MakeOptionalText(ScriptCode);
	Snapshot.RegionCode = MakeOptionalText(RegionCode);
	Snapshot.CurrencyCode = MakeOptionalText(CurrencyCode);
}

void FOpenMobileDeviceLocaleInfo::ApplyRegionalPreferences(
	FOpenMobileLocaleSnapshot& Snapshot,
	const FString& TimeFormat,
	const FString& MeasurementSystem
)
{
	const FString NormalizedTimeFormat = TimeFormat.TrimStartAndEnd();
	if (NormalizedTimeFormat == TEXT("12"))
	{
		Snapshot.TimeFormat = EOpenMobileTimeFormatPreference::TwelveHour;
	}
	else if (NormalizedTimeFormat == TEXT("24"))
	{
		Snapshot.TimeFormat = EOpenMobileTimeFormatPreference::TwentyFourHour;
	}
	else
	{
		Snapshot.TimeFormat = EOpenMobileTimeFormatPreference::Unknown;
	}

	const FString NormalizedMeasurementSystem =
		MeasurementSystem.TrimStartAndEnd();
	if (NormalizedMeasurementSystem.Equals(
		TEXT("metric"),
		ESearchCase::IgnoreCase
	))
	{
		Snapshot.MeasurementSystem = EOpenMobileMeasurementSystem::Metric;
	}
	else if (NormalizedMeasurementSystem.Equals(
		TEXT("imperial"),
		ESearchCase::IgnoreCase
	))
	{
		Snapshot.MeasurementSystem = EOpenMobileMeasurementSystem::Imperial;
	}
	else
	{
		Snapshot.MeasurementSystem = EOpenMobileMeasurementSystem::Unknown;
	}
}
