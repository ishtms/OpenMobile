#pragma once

#include "CoreMinimal.h"

class UOpenMobileAdsSubsystem;

class IOpenMobileAdsClock
{
public:
	virtual ~IOpenMobileAdsClock() = default;
	virtual FDateTime UtcNow() const = 0;
	virtual double MonotonicSeconds() const = 0;
};

TSharedRef<IOpenMobileAdsClock> OpenMobileAdsCreateClock();

#if WITH_DEV_AUTOMATION_TESTS

struct FOpenMobileAdsClockTestAccess
{
	static void SetClock(
		UOpenMobileAdsSubsystem& Subsystem,
		TSharedRef<IOpenMobileAdsClock> Clock
	);
};

#endif
