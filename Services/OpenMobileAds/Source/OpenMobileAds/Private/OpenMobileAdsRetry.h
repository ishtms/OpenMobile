#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsConfiguration.h"

class UOpenMobileAdsSubsystem;

struct FOpenMobileAdsRetryScheduleHandle
{
	uint64 Value = 0;

	bool IsValid() const
	{
		return Value != 0;
	}

	void Reset()
	{
		Value = 0;
	}

	bool operator==(const FOpenMobileAdsRetryScheduleHandle& Other) const
	{
		return Value == Other.Value;
	}
};

class IOpenMobileAdsRetryRandomSource
{
public:
	virtual ~IOpenMobileAdsRetryRandomSource() = default;
	virtual double NextUnit() = 0;
};

class IOpenMobileAdsRetryScheduler
{
public:
	virtual ~IOpenMobileAdsRetryScheduler() = default;
	virtual FOpenMobileAdsRetryScheduleHandle Schedule(
		double DelaySeconds,
		TFunction<void()>&& Callback
	) = 0;
	virtual void Cancel(FOpenMobileAdsRetryScheduleHandle& Handle) = 0;
};

class FOpenMobileAdsRetryDelayCalculator
{
public:
	static double Calculate(
		const FOpenMobileAdsRetryPolicy& Policy,
		int32 RetryAttempt,
		IOpenMobileAdsRetryRandomSource& RandomSource
	);
};

TSharedRef<IOpenMobileAdsRetryRandomSource>
OpenMobileAdsCreateRetryRandomSource();

TSharedRef<IOpenMobileAdsRetryScheduler>
OpenMobileAdsCreateRetryScheduler();

#if WITH_DEV_AUTOMATION_TESTS

struct FOpenMobileAdsRetryTestAccess
{
	static void SetDependencies(
		UOpenMobileAdsSubsystem& Subsystem,
		TSharedRef<IOpenMobileAdsRetryScheduler> Scheduler,
		TSharedRef<IOpenMobileAdsRetryRandomSource> RandomSource
	);
};

#endif
