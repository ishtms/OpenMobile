#include "OpenMobileAdsClock.h"

#include "HAL/PlatformTime.h"
#include "OpenMobileAdsSubsystem.h"

namespace OpenMobileAdsClockPrivate
{
	class FSystemClock final : public IOpenMobileAdsClock
	{
	public:
		virtual FDateTime UtcNow() const override
		{
			return FDateTime::UtcNow();
		}

		virtual double MonotonicSeconds() const override
		{
			return FPlatformTime::Seconds();
		}
	};
}

TSharedRef<IOpenMobileAdsClock> OpenMobileAdsCreateClock()
{
	return MakeShared<OpenMobileAdsClockPrivate::FSystemClock>();
}

#if WITH_DEV_AUTOMATION_TESTS

void FOpenMobileAdsClockTestAccess::SetClock(
	UOpenMobileAdsSubsystem& Subsystem,
	TSharedRef<IOpenMobileAdsClock> Clock
)
{
	check(IsInGameThread());
	check(!Subsystem.bRuntimeInitialized);
	Subsystem.CacheClock = MoveTemp(Clock);
}

#endif
