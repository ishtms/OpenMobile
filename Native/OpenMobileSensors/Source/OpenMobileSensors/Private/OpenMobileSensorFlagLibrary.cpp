#include "OpenMobileSensorFlagLibrary.h"

namespace
{
	template <typename FlagType>
	bool HasFlag(int32 Flags, FlagType Flag)
	{
		const int32 Requested = static_cast<int32>(Flag);
		return Requested == 0
			? Flags == 0
			: (Flags & Requested) == Requested;
	}
}

bool UOpenMobileSensorFlagLibrary::HasSensorSource(
	int32 SourceFlags,
	EOpenMobileSensorSourceFlags Source)
{
	return HasFlag(SourceFlags, Source);
}

bool UOpenMobileSensorFlagLibrary::HasTimestampIssue(
	int32 TimestampIssueFlags,
	EOpenMobileSensorTimestampIssue Issue)
{
	return HasFlag(TimestampIssueFlags, Issue);
}

bool UOpenMobileSensorFlagLibrary::HasAttitudeRepresentation(
	int32 AttitudeRepresentations,
	EOpenMobileAttitudeRepresentation Representation)
{
	return HasFlag(AttitudeRepresentations, Representation);
}

bool UOpenMobileSensorFlagLibrary::HasAltitudeLimitation(
	int32 QualityLimitationFlags,
	EOpenMobileRelativeAltitudeQualityLimitation Limitation)
{
	return HasFlag(QualityLimitationFlags, Limitation);
}

bool UOpenMobileSensorFlagLibrary::HasUnsupportedFallbackCondition(
	int32 UnsupportedConditionFlags,
	EOpenMobileSensorFallbackUnsupportedCondition Condition)
{
	return HasFlag(UnsupportedConditionFlags, Condition);
}
