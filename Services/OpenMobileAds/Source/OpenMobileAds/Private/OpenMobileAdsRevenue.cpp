#include "OpenMobileAdsRevenue.h"

bool FOpenMobileAdsRevenue::TryConvertMajorUnitsToMicros(
	double MajorUnits,
	int64& OutValueMicros
)
{
	OutValueMicros = 0;
	if (!FMath::IsFinite(MajorUnits) || MajorUnits < 0.0)
	{
		return false;
	}

	const double ScaledValue = MajorUnits * static_cast<double>(MicrosPerMajorUnit);
	constexpr double MaximumExclusive = 9223372036854775808.0;
	if (!FMath::IsFinite(ScaledValue) || ScaledValue >= MaximumExclusive)
	{
		return false;
	}

	const double RoundedValue = FMath::RoundToDouble(ScaledValue);
	if (RoundedValue >= MaximumExclusive)
	{
		return false;
	}

	OutValueMicros = static_cast<int64>(RoundedValue);
	return true;
}

bool FOpenMobileAdsRevenue::TryScaleToMicros(
	int64 ProviderValue,
	int64 MicrosPerProviderUnit,
	int64& OutValueMicros
)
{
	OutValueMicros = 0;
	if (
		ProviderValue < 0
		|| MicrosPerProviderUnit <= 0
		|| ProviderValue > MAX_int64 / MicrosPerProviderUnit
	)
	{
		return false;
	}

	OutValueMicros = ProviderValue * MicrosPerProviderUnit;
	return true;
}

bool FOpenMobileAdsRevenue::TryNormalizeCurrencyCode(
	const FString& ProviderCurrencyCode,
	FString& OutCurrencyCode
)
{
	if (ProviderCurrencyCode.Len() != 3)
	{
		OutCurrencyCode.Reset();
		return false;
	}

	TCHAR NormalizedCode[3];
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(NormalizedCode); ++Index)
	{
		const TCHAR Character = ProviderCurrencyCode[Index];
		if (Character >= TEXT('A') && Character <= TEXT('Z'))
		{
			NormalizedCode[Index] = Character;
		}
		else if (Character >= TEXT('a') && Character <= TEXT('z'))
		{
			NormalizedCode[Index] = Character - TEXT('a') + TEXT('A');
		}
		else
		{
			OutCurrencyCode.Reset();
			return false;
		}
	}

	OutCurrencyCode.Reset();
	OutCurrencyCode.AppendChars(NormalizedCode, UE_ARRAY_COUNT(NormalizedCode));
	return true;
}

EOpenMobileAdsRevenuePrecision FOpenMobileAdsRevenue::NormalizePrecision(
	EOpenMobileAdsRevenuePrecision ProviderPrecision
)
{
	switch (ProviderPrecision)
	{
	case EOpenMobileAdsRevenuePrecision::Estimated:
	case EOpenMobileAdsRevenuePrecision::PublisherProvided:
	case EOpenMobileAdsRevenuePrecision::Precise:
		return ProviderPrecision;
	default:
		return EOpenMobileAdsRevenuePrecision::Unknown;
	}
}
