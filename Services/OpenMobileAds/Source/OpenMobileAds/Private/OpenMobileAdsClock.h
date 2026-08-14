#pragma once

#include "CoreMinimal.h"

class UOpenMobileAdsSubsystem;

/** Pairs wall and monotonic time so persisted history and live deadlines don't use the wrong clock. */
class IOpenMobileAdsClock
{
public:
	virtual ~IOpenMobileAdsClock() = default;
	/** Supplies wall time for persisted timestamps and user-facing eligibility answers. */
	virtual FDateTime UtcNow() const = 0;
	/** Supplies monotonic time for cache, cooldown, and retry deadlines. */
	virtual double MonotonicSeconds() const = 0;
};

/** Creates the ordinary Unreal clock used outside tests. */
TSharedRef<IOpenMobileAdsClock> OpenMobileAdsCreateClock();

#if WITH_DEV_AUTOMATION_TESTS

struct FOpenMobileAdsClockTestAccess
{
	/** Replaces the subsystem clock so policy tests can move time without waiting. */
	static void SetClock(
		UOpenMobileAdsSubsystem& Subsystem,
		TSharedRef<IOpenMobileAdsClock> Clock
	);
};

#endif
