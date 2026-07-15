#include "OpenMobileHapticsBudgetPolicy.h"

int32 FOpenMobileHapticsBudgetPolicy::ResolveMaximumActiveHandles(
	int32 Configured
)
{
	return FMath::Clamp(Configured, 1, HardMaximumActiveHandles);
}

int32 FOpenMobileHapticsBudgetPolicy::ResolveMaximumQueuedHandles(
	int32 Configured
)
{
	return FMath::Clamp(Configured, 1, HardMaximumQueuedHandles);
}

int32 FOpenMobileHapticsBudgetPolicy::ResolveMaximumQueueDepthPerChannel(
	int32 Configured
)
{
	return FMath::Clamp(Configured, 1, HardMaximumQueueDepthPerChannel);
}

int32 FOpenMobileHapticsBudgetPolicy::ResolveMaximumPreparedPatterns(
	int32 Configured
)
{
	return FMath::Clamp(Configured, 1, HardMaximumPreparedPatterns);
}

int64 FOpenMobileHapticsBudgetPolicy::ResolveMaximumPreparedPatternBytes(
	int64 Configured
)
{
	return FMath::Clamp(
		Configured,
		HardMinimumPreparedPatternBytes,
		HardMaximumPreparedPatternBytes
	);
}

double FOpenMobileHapticsBudgetPolicy::ResolvePreparedIdleLifetimeSeconds(
	double Configured
)
{
	return FMath::IsFinite(Configured)
		? FMath::Clamp(
			Configured,
			HardMinimumPreparedIdleLifetimeSeconds,
			HardMaximumPreparedIdleLifetimeSeconds
		)
		: HardMinimumPreparedIdleLifetimeSeconds;
}

int32 FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternEvents(
	int32 Configured
)
{
	return FMath::Clamp(Configured, 1, HardMaximumPatternEvents);
}

int32 FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternCurves(
	int32 Configured
)
{
	return FMath::Clamp(Configured, 0, HardMaximumPatternCurves);
}

int32 FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternCurvePoints(
	int32 Configured
)
{
	return FMath::Clamp(Configured, 1, HardMaximumPatternCurvePoints);
}

int32 FOpenMobileHapticsBudgetPolicy::ResolveMaximumDiagnosticEvents(
	int32 Configured
)
{
	return FMath::Clamp(Configured, 1, HardMaximumDiagnosticEvents);
}
