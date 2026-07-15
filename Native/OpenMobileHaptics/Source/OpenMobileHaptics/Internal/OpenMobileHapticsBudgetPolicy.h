#pragma once

#include "CoreMinimal.h"

class FOpenMobileHapticsBudgetPolicy final
{
public:
	enum : int32
	{
		HardMaximumActiveHandles = 128,
		HardMaximumQueuedHandles = 256,
		HardMaximumQueueDepthPerChannel = 64,
		HardMaximumPreparedPatterns = 128,
		HardMaximumPatternEvents = 4096,
		HardMaximumPatternCurves = 128,
		HardMaximumPatternCurvePoints = 4096,
		HardMaximumDiagnosticEvents = 512
	};

	static constexpr int64 HardMinimumPreparedPatternBytes = 1;
	static constexpr int64 HardMaximumPreparedPatternBytes =
		64 * 1024 * 1024;
	static constexpr double HardMinimumPreparedIdleLifetimeSeconds = 1.0;
	static constexpr double HardMaximumPreparedIdleLifetimeSeconds = 300.0;

	static int32 ResolveMaximumActiveHandles(int32 Configured);
	static int32 ResolveMaximumQueuedHandles(int32 Configured);
	static int32 ResolveMaximumQueueDepthPerChannel(int32 Configured);
	static int32 ResolveMaximumPreparedPatterns(int32 Configured);
	static int64 ResolveMaximumPreparedPatternBytes(int64 Configured);
	static double ResolvePreparedIdleLifetimeSeconds(double Configured);
	static int32 ResolveMaximumPatternEvents(int32 Configured);
	static int32 ResolveMaximumPatternCurves(int32 Configured);
	static int32 ResolveMaximumPatternCurvePoints(int32 Configured);
	static int32 ResolveMaximumDiagnosticEvents(int32 Configured);
};
