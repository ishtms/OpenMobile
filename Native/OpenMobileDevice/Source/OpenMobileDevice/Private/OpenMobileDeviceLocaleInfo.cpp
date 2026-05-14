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
