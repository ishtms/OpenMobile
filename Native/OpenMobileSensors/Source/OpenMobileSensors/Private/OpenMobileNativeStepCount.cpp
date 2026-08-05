#include "OpenMobileNativeStepCount.h"

bool UOpenMobileNativeStepCountLibrary::MakeNativeStepQueryForLastDuration(
	FTimespan Duration,
	FOpenMobileNativeStepCountQuery& OutQuery)
{
	OutQuery = {};
	if (Duration <= FTimespan::Zero())
	{
		return false;
	}
	const FDateTime EndUtc = FDateTime::UtcNow();
	const FDateTime Epoch = FDateTime::FromUnixTimestamp(0);
	if (Duration > EndUtc - Epoch)
	{
		return false;
	}
	return MakeNativeStepQueryBetweenDates(
		EndUtc - Duration, EndUtc, OutQuery);
}

bool UOpenMobileNativeStepCountLibrary::MakeNativeStepQueryBetweenDates(
	FDateTime StartInclusiveUtc,
	FDateTime EndExclusiveUtc,
	FOpenMobileNativeStepCountQuery& OutQuery)
{
	OutQuery = {};
	const FDateTime Epoch = FDateTime::FromUnixTimestamp(0);
	if (StartInclusiveUtc < Epoch
		|| EndExclusiveUtc <= StartInclusiveUtc
		|| EndExclusiveUtc > FDateTime::UtcNow())
	{
		return false;
	}
	OutQuery.StartUnixTimeSeconds = static_cast<double>(
		StartInclusiveUtc.ToUnixTimestamp());
	OutQuery.EndUnixTimeSeconds = static_cast<double>(
		EndExclusiveUtc.ToUnixTimestamp());
	return true;
}
