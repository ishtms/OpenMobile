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

	/** Clamps the configured active handle count to the hard cap, so a bad ini value can't grow runtime state forever. */
	static int32 ResolveMaximumActiveHandles(int32 Configured);
	/** Keeps the global waiting list within the memory limit even when settings contain a wild value. */
	static int32 ResolveMaximumQueuedHandles(int32 Configured);
	/** Caps one channel separately, otherwise a noisy caller can consume the whole queue. */
	static int32 ResolveMaximumQueueDepthPerChannel(int32 Configured);
	/** Limits cached native preparations before least-recently-used eviction has to start. */
	static int32 ResolveMaximumPreparedPatterns(int32 Configured);
	/** Normalizes the prepared byte budget and prevents zero from disabling accounting by accident. */
	static int64 ResolveMaximumPreparedPatternBytes(int64 Configured);
	/** Restricts idle lifetime to a useful range, including non-finite configuration values. */
	static double ResolvePreparedIdleLifetimeSeconds(double Configured);
	/** Caps events before platform compilation, because accepting more here only postpones the same rejection. */
	static int32 ResolveMaximumPatternEvents(int32 Configured);
	/** Caps parameter curves before their point arrays are inspected. */
	static int32 ResolveMaximumPatternCurves(int32 Configured);
	/** Caps points across one curve so imported data can't force oversized allocations. */
	static int32 ResolveMaximumPatternCurvePoints(int32 Configured);
	/** Bounds the in-memory diagnostic history, old entries can go once this count is reached. */
	static int32 ResolveMaximumDiagnosticEvents(int32 Configured);
};
