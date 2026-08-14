#include "OpenMobileAdsRetry.h"

#include "Containers/Ticker.h"
#include "OpenMobileAdsSubsystem.h"

namespace OpenMobileAdsRetryPrivate
{
	/** Supplies ordinary runtime randomness without making policy calculations depend on global calls. */
	class FRandomSource final : public IOpenMobileAdsRetryRandomSource
	{
	public:
		/** Samples Unreal's unit random source once for each requested jitter calculation. */
		virtual double NextUnit() override
		{
			return FMath::FRand();
		}
	};

	/** Owns game-thread ticker callbacks behind cancellable retry handles. */
	class FTickerScheduler final : public IOpenMobileAdsRetryScheduler
	{
	public:
		/** Removes every outstanding ticker so callbacks can't outlive the scheduler. */
		virtual ~FTickerScheduler() override
		{
			for (const TPair<uint64, FTSTicker::FDelegateHandle>& Pair : Handles)
			{
				FTSTicker::GetCoreTicker().RemoveTicker(Pair.Value);
			}
		}

		/** Converts one delay to a ticker callback and erases its handle before invoking caller code. */
		virtual FOpenMobileAdsRetryScheduleHandle Schedule(
			double DelaySeconds,
			TFunction<void()>&& Callback
		) override
		{
			FOpenMobileAdsRetryScheduleHandle Handle;
			Handle.Value = NextHandle++;
			if (NextHandle == 0)
			{
				NextHandle = 1;
			}
			const uint64 HandleValue = Handle.Value;
			const TSharedRef<TFunction<void()>> SharedCallback =
				MakeShared<TFunction<void()>>(MoveTemp(Callback));
			const FTSTicker::FDelegateHandle TickerHandle =
				FTSTicker::GetCoreTicker().AddTicker(
					FTickerDelegate::CreateLambda(
						[this, HandleValue, SharedCallback](float DeltaTime)
						{
							static_cast<void>(DeltaTime);
							Handles.Remove(HandleValue);
							(*SharedCallback)();
							return false;
						}
					),
					static_cast<float>(FMath::Max(0.0, DelaySeconds))
				);
			Handles.Add(HandleValue, TickerHandle);
			return Handle;
		}

		/** Removes the matching ticker when present and always clears caller ownership. */
		virtual void Cancel(
			FOpenMobileAdsRetryScheduleHandle& Handle
		) override
		{
			if (const FTSTicker::FDelegateHandle* TickerHandle =
				Handles.Find(Handle.Value))
			{
				FTSTicker::GetCoreTicker().RemoveTicker(*TickerHandle);
				Handles.Remove(Handle.Value);
			}
			Handle.Reset();
		}

	private:
		TMap<uint64, FTSTicker::FDelegateHandle> Handles;
		uint64 NextHandle = 1;
	};
}

double FOpenMobileAdsRetryDelayCalculator::Calculate(
	const FOpenMobileAdsRetryPolicy& Policy,
	int32 RetryAttempt,
	IOpenMobileAdsRetryRandomSource& RandomSource
)
{
	const double MaximumDelay = FMath::Max(0.0, Policy.MaxDelaySeconds);
	double Delay = FMath::Clamp(
		Policy.InitialDelaySeconds,
		0.0,
		MaximumDelay
	);
	if (
		Delay > 0.0
		&& Policy.BackoffMultiplier > 1.0
		&& RetryAttempt > 1
	)
	{
		const double MultipliedDelay = Delay * FMath::Pow(
			Policy.BackoffMultiplier,
			static_cast<double>(RetryAttempt - 1)
		);
		if (FMath::IsFinite(MultipliedDelay))
		{
			Delay = FMath::Min(MultipliedDelay, MaximumDelay);
		}
		else
		{
			Delay = MaximumDelay;
		}
	}
	if (!Policy.bUseJitter || Delay <= 0.0)
	{
		return Delay;
	}

	const double RandomUnit = FMath::Clamp(RandomSource.NextUnit(), 0.0, 1.0);
	return Delay * (0.5 + 0.5 * RandomUnit);
}

TSharedRef<IOpenMobileAdsRetryRandomSource>
OpenMobileAdsCreateRetryRandomSource()
{
	return MakeShared<OpenMobileAdsRetryPrivate::FRandomSource>();
}

TSharedRef<IOpenMobileAdsRetryScheduler>
OpenMobileAdsCreateRetryScheduler()
{
	return MakeShared<OpenMobileAdsRetryPrivate::FTickerScheduler>();
}

#if WITH_DEV_AUTOMATION_TESTS

void FOpenMobileAdsRetryTestAccess::SetDependencies(
	UOpenMobileAdsSubsystem& Subsystem,
	TSharedRef<IOpenMobileAdsRetryScheduler> Scheduler,
	TSharedRef<IOpenMobileAdsRetryRandomSource> RandomSource
)
{
	check(IsInGameThread());
	check(!Subsystem.bRuntimeInitialized);
	Subsystem.RetryScheduler = MoveTemp(Scheduler);
	Subsystem.RetryRandomSource = MoveTemp(RandomSource);
}

#endif
