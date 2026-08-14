#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsConfiguration.h"

class UOpenMobileAdsSubsystem;

struct FOpenMobileAdsRetryScheduleHandle
{
	uint64 Value = 0;

	/** Keeps zero reserved as no scheduled callback. */
	bool IsValid() const
	{
		return Value != 0;
	}

	/** Clears local ownership after the scheduler cancels or fires the callback. */
	void Reset()
	{
		Value = 0;
	}

	/** Compares scheduler identity without exposing implementation details to request contexts. */
	bool operator==(const FOpenMobileAdsRetryScheduleHandle& Other) const
	{
		return Value == Other.Value;
	}
};

class IOpenMobileAdsRetryRandomSource
{
public:
	virtual ~IOpenMobileAdsRetryRandomSource() = default;
	/** Returns one unit interval sample used only for optional retry jitter. */
	virtual double NextUnit() = 0;
};

class IOpenMobileAdsRetryScheduler
{
public:
	virtual ~IOpenMobileAdsRetryScheduler() = default;
	/** Runs one callback after the resolved delay and returns a cancellable identity. */
	virtual FOpenMobileAdsRetryScheduleHandle Schedule(
		double DelaySeconds,
		TFunction<void()>&& Callback
	) = 0;
	/** Cancels only the supplied identity and resets it even when it already fired. */
	virtual void Cancel(FOpenMobileAdsRetryScheduleHandle& Handle) = 0;
};

class FOpenMobileAdsRetryDelayCalculator
{
public:
	/** Applies bounded exponential backoff and optional jitter without exceeding the policy maximum. */
	static double Calculate(
		const FOpenMobileAdsRetryPolicy& Policy,
		int32 RetryAttempt,
		IOpenMobileAdsRetryRandomSource& RandomSource
	);
};

/** Creates the ordinary random source used by live retry policy. */
TSharedRef<IOpenMobileAdsRetryRandomSource>
OpenMobileAdsCreateRetryRandomSource();

/** Creates the game-thread ticker scheduler used by live retry requests. */
TSharedRef<IOpenMobileAdsRetryScheduler>
OpenMobileAdsCreateRetryScheduler();

#if WITH_DEV_AUTOMATION_TESTS

struct FOpenMobileAdsRetryTestAccess
{
	/** Replaces scheduler and randomness together so retry tests remain deterministic. */
	static void SetDependencies(
		UOpenMobileAdsSubsystem& Subsystem,
		TSharedRef<IOpenMobileAdsRetryScheduler> Scheduler,
		TSharedRef<IOpenMobileAdsRetryRandomSource> RandomSource
	);
};

#endif
