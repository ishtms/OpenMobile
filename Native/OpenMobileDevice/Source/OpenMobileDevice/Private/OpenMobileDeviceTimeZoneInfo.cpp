#include "OpenMobileDeviceTimeZoneInfo.h"

void FOpenMobileDeviceTimeZoneInfo::Apply(
	FOpenMobileLocaleSnapshot& Snapshot,
	const FString& TimeZoneIdentifier,
	int64 UtcOffsetSeconds,
	bool bUtcOffsetAvailable,
	bool bIsDaylightSavingTime,
	bool bDaylightSavingTimeAvailable
)
{
	const FString TrimmedIdentifier = TimeZoneIdentifier.TrimStartAndEnd();
	if (!TrimmedIdentifier.IsEmpty())
	{
		Snapshot.TimeZoneIdentifier =
			FOpenMobileDeviceOptionalString::MakeAvailable(TrimmedIdentifier);
	}
	if (bUtcOffsetAvailable
		&& UtcOffsetSeconds >= -86400
		&& UtcOffsetSeconds <= 86400)
	{
		Snapshot.UtcOffsetSeconds =
			FOpenMobileDeviceOptionalInt32::MakeAvailable(
				static_cast<int32>(UtcOffsetSeconds)
			);
	}
	if (bDaylightSavingTimeAvailable)
	{
		Snapshot.bIsDaylightSavingTime =
			FOpenMobileDeviceOptionalBool::MakeAvailable(
				bIsDaylightSavingTime
			);
	}
}
