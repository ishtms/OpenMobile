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
