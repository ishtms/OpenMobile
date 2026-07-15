#include "OpenMobileHapticsRateLimiter.h"

#include "HAL/PlatformTime.h"
#include "Misc/ScopeLock.h"

namespace OpenMobileHapticsRateLimiterPrivate
{
	constexpr double WindowSeconds = 1.0;
	constexpr double SafeMinimumIntervalSeconds = 0.02;
	constexpr double SafeDebounceSeconds = 0.04;

	double SanitizeInterval(
		double Value,
		double Maximum,
		double SafeValue
	)
	{
		if (!FMath::IsFinite(Value) || Value < 0.0)
		{
			return SafeValue;
		}
		return FMath::Min(Value, Maximum);
	}

	bool IsInsideInterval(
		double TimeSeconds,
		double PreviousTimeSeconds,
		double IntervalSeconds
	)
	{
		return TimeSeconds - PreviousTimeSeconds + UE_DOUBLE_SMALL_NUMBER
			< IntervalSeconds;
	}

	void PruneWindow(TArray<double>& Times, double TimeSeconds)
	{
		const double Cutoff = TimeSeconds - WindowSeconds;
		Times.RemoveAll(
			[Cutoff](double PreviousTimeSeconds)
			{
				return PreviousTimeSeconds <= Cutoff;
			}
		);
	}
}

FOpenMobileHapticsRateLimiter::FOpenMobileHapticsRateLimiter()
	: FOpenMobileHapticsRateLimiter(
		[]()
		{
			return FPlatformTime::Seconds();
		}
	)
{
}

FOpenMobileHapticsRateLimiter::FOpenMobileHapticsRateLimiter(FClock InClock)
	: Clock(MoveTemp(InClock))
{
	if (!Clock)
	{
		Clock = []()
		{
			return FPlatformTime::Seconds();
		};
	}
	RecentGlobalSubmissionTimes.Reserve(
		HardMaximumGlobalSubmissionsPerSecond
	);
	RecentNonCriticalSubmissionTimes.Reserve(
		HardMaximumGlobalSubmissionsPerSecond
	);
}

FOpenMobileHapticsRateLimitDecision FOpenMobileHapticsRateLimiter::Evaluate(
	const FOpenMobileHapticsRateLimitRequest& Request,
	const FOpenMobileHapticsRateLimitPolicy& Policy
)
{
	FScopeLock Lock(&Mutex);
	return EvaluateAtLocked(Request, Policy, Clock());
}

void FOpenMobileHapticsRateLimiter::Reset()
{
	FScopeLock Lock(&Mutex);
	ResetLocked();
}

bool FOpenMobileHapticsRateLimiter::ShouldSuppress(
	FName Channel,
	bool bSelection,
	double TimeSeconds,
	double MinimumIntervalSeconds,
	double SelectionDebounceSeconds,
	int32 MaximumSubmissionsPerSecond
)
{
	FOpenMobileHapticsRateLimitRequest Request;
	Request.Channel = Channel;
	Request.Category = Channel;
	Request.Effect = bSelection ? FName(TEXT("Selection")) : NAME_None;
	Request.bCoalescible = bSelection;
	FOpenMobileHapticsRateLimitPolicy Policy;
	Policy.ChannelMinimumIntervalSeconds = MinimumIntervalSeconds;
	Policy.EffectMinimumIntervalSeconds = 0.0;
	Policy.EquivalentRequestDebounceSeconds = SelectionDebounceSeconds;
	Policy.MaximumChannelSubmissionsPerSecond =
		HardMaximumChannelSubmissionsPerSecond;
	Policy.MaximumGlobalSubmissionsPerSecond = MaximumSubmissionsPerSecond;

	FScopeLock Lock(&Mutex);
	return !EvaluateAtLocked(Request, Policy, TimeSeconds).IsAllowed();
}

FOpenMobileHapticsRateLimitDecision
FOpenMobileHapticsRateLimiter::EvaluateAtLocked(
	const FOpenMobileHapticsRateLimitRequest& Request,
	const FOpenMobileHapticsRateLimitPolicy& Policy,
	double TimeSeconds
)
{
	FOpenMobileHapticsRateLimitDecision Decision;
	if (!FMath::IsFinite(TimeSeconds))
	{
		Decision.Outcome = EOpenMobileHapticsRateLimitOutcome::InvalidClock;
		return Decision;
	}
	if (bHasObservedTime
		&& TimeSeconds + UE_DOUBLE_SMALL_NUMBER < LastObservedTimeSeconds)
	{
		ResetLocked();
		Decision.bClockReset = true;
	}
	LastObservedTimeSeconds = TimeSeconds;
	bHasObservedTime = true;

	const double ChannelMinimumIntervalSeconds =
		OpenMobileHapticsRateLimiterPrivate::SanitizeInterval(
			Policy.ChannelMinimumIntervalSeconds,
			1.0,
			OpenMobileHapticsRateLimiterPrivate::SafeMinimumIntervalSeconds
		);
	const double EffectMinimumIntervalSeconds =
		OpenMobileHapticsRateLimiterPrivate::SanitizeInterval(
			Policy.EffectMinimumIntervalSeconds,
			10.0,
			OpenMobileHapticsRateLimiterPrivate::SafeMinimumIntervalSeconds
		);
	const double EquivalentRequestDebounceSeconds =
		OpenMobileHapticsRateLimiterPrivate::SanitizeInterval(
			Policy.EquivalentRequestDebounceSeconds,
			1.0,
			OpenMobileHapticsRateLimiterPrivate::SafeDebounceSeconds
		);
	const int32 MaximumChannelSubmissions = FMath::Clamp(
		Policy.MaximumChannelSubmissionsPerSecond,
		1,
		HardMaximumChannelSubmissionsPerSecond
	);
	const int32 MaximumGlobalSubmissions = FMath::Clamp(
		Policy.MaximumGlobalSubmissionsPerSecond,
		1,
		HardMaximumGlobalSubmissionsPerSecond
	);

	OpenMobileHapticsRateLimiterPrivate::PruneWindow(
		RecentGlobalSubmissionTimes,
		TimeSeconds
	);
	OpenMobileHapticsRateLimiterPrivate::PruneWindow(
		RecentNonCriticalSubmissionTimes,
		TimeSeconds
	);
	FChannelState* ChannelState = Channels.Find(Request.Channel);
	if (ChannelState)
	{
		OpenMobileHapticsRateLimiterPrivate::PruneWindow(
			ChannelState->RecentSubmissionTimes,
			TimeSeconds
		);
	}

	FEquivalentRequestKey EquivalentKey;
	EquivalentKey.Channel = Request.Channel;
	EquivalentKey.Category = Request.Category;
	EquivalentKey.Effect = Request.Effect;
	EquivalentKey.Priority = Request.Priority;
	EquivalentKey.Signature = Request.EquivalenceHash;
	if (Request.bCoalescible && EquivalentRequestDebounceSeconds > 0.0)
	{
		if (const FEquivalentRequestState* EquivalentState =
			EquivalentRequests.Find(EquivalentKey))
		{
			if (OpenMobileHapticsRateLimiterPrivate::IsInsideInterval(
				TimeSeconds,
				EquivalentState->LastSubmissionTimeSeconds,
				EquivalentRequestDebounceSeconds
			))
			{
				Decision.Outcome =
					EOpenMobileHapticsRateLimitOutcome::EquivalentRequest;
				return Decision;
			}
		}
	}
	if (ChannelState && ChannelState->bHasSubmission
		&& OpenMobileHapticsRateLimiterPrivate::IsInsideInterval(
			TimeSeconds,
			ChannelState->LastSubmissionTimeSeconds,
			ChannelMinimumIntervalSeconds
		))
	{
		Decision.Outcome =
			EOpenMobileHapticsRateLimitOutcome::ChannelMinimumInterval;
		return Decision;
	}
	if (const FEffectState* EffectState = Effects.Find(Request.Effect))
	{
		if (OpenMobileHapticsRateLimiterPrivate::IsInsideInterval(
			TimeSeconds,
			EffectState->LastSubmissionTimeSeconds,
			EffectMinimumIntervalSeconds
		))
		{
			Decision.Outcome =
				EOpenMobileHapticsRateLimitOutcome::EffectMinimumInterval;
			return Decision;
		}
	}
	if (ChannelState
		&& ChannelState->RecentSubmissionTimes.Num()
			>= MaximumChannelSubmissions)
	{
		Decision.Outcome = EOpenMobileHapticsRateLimitOutcome::ChannelWindow;
		return Decision;
	}
	if (RecentGlobalSubmissionTimes.Num() >= MaximumGlobalSubmissions)
	{
		Decision.Outcome = EOpenMobileHapticsRateLimitOutcome::GlobalWindow;
		return Decision;
	}
	if (Request.Priority != EOpenMobileHapticChannelPriority::Critical
		&& MaximumGlobalSubmissions >= 2
		&& RecentNonCriticalSubmissionTimes.Num()
			>= MaximumGlobalSubmissions - 1)
	{
		Decision.Outcome = EOpenMobileHapticsRateLimitOutcome::GlobalWindow;
		return Decision;
	}

	ChannelState = &FindOrAddChannel(Request.Channel);
	ChannelState->LastSubmissionTimeSeconds = TimeSeconds;
	ChannelState->bHasSubmission = true;
	ChannelState->AccessSequence = ++AccessSequence;
	ChannelState->RecentSubmissionTimes.Add(TimeSeconds);
	FEffectState& EffectState = FindOrAddEffect(Request.Effect);
	EffectState.LastSubmissionTimeSeconds = TimeSeconds;
	EffectState.AccessSequence = ++AccessSequence;
	if (Request.bCoalescible && EquivalentRequestDebounceSeconds > 0.0)
	{
		FEquivalentRequestState& EquivalentState =
			FindOrAddEquivalentRequest(EquivalentKey);
		EquivalentState.LastSubmissionTimeSeconds = TimeSeconds;
		EquivalentState.AccessSequence = ++AccessSequence;
	}
	RecentGlobalSubmissionTimes.Add(TimeSeconds);
	if (Request.Priority != EOpenMobileHapticChannelPriority::Critical)
	{
		RecentNonCriticalSubmissionTimes.Add(TimeSeconds);
	}
	return Decision;
}

FOpenMobileHapticsRateLimiter::FChannelState&
FOpenMobileHapticsRateLimiter::FindOrAddChannel(FName Channel)
{
	if (FChannelState* Existing = Channels.Find(Channel))
	{
		return *Existing;
	}
	if (Channels.Num() >= MaximumTrackedChannels)
	{
		FName OldestKey;
		uint64 OldestSequence = MAX_uint64;
		for (const TPair<FName, FChannelState>& Pair : Channels)
		{
			if (Pair.Value.AccessSequence < OldestSequence)
			{
				OldestKey = Pair.Key;
				OldestSequence = Pair.Value.AccessSequence;
			}
		}
		Channels.Remove(OldestKey);
	}
	FChannelState& State = Channels.Add(Channel);
	State.RecentSubmissionTimes.Reserve(
		HardMaximumChannelSubmissionsPerSecond
	);
	return State;
}

FOpenMobileHapticsRateLimiter::FEffectState&
FOpenMobileHapticsRateLimiter::FindOrAddEffect(FName Effect)
{
	if (FEffectState* Existing = Effects.Find(Effect))
	{
		return *Existing;
	}
	if (Effects.Num() >= MaximumTrackedEffects)
	{
		FName OldestKey;
		uint64 OldestSequence = MAX_uint64;
		for (const TPair<FName, FEffectState>& Pair : Effects)
		{
			if (Pair.Value.AccessSequence < OldestSequence)
			{
				OldestKey = Pair.Key;
				OldestSequence = Pair.Value.AccessSequence;
			}
		}
		Effects.Remove(OldestKey);
	}
	return Effects.Add(Effect);
}

FOpenMobileHapticsRateLimiter::FEquivalentRequestState&
FOpenMobileHapticsRateLimiter::FindOrAddEquivalentRequest(
	const FEquivalentRequestKey& Key
)
{
	if (FEquivalentRequestState* Existing = EquivalentRequests.Find(Key))
	{
		return *Existing;
	}
	if (EquivalentRequests.Num() >= MaximumTrackedEquivalentRequests)
	{
		FEquivalentRequestKey OldestKey;
		uint64 OldestSequence = MAX_uint64;
		for (const TPair<
			FEquivalentRequestKey,
			FEquivalentRequestState
		>& Pair : EquivalentRequests)
		{
			if (Pair.Value.AccessSequence < OldestSequence)
			{
				OldestKey = Pair.Key;
				OldestSequence = Pair.Value.AccessSequence;
			}
		}
		EquivalentRequests.Remove(OldestKey);
	}
	return EquivalentRequests.Add(Key);
}

void FOpenMobileHapticsRateLimiter::ResetLocked()
{
	Channels.Reset();
	Effects.Reset();
	EquivalentRequests.Reset();
	RecentGlobalSubmissionTimes.Reset();
	RecentNonCriticalSubmissionTimes.Reset();
	LastObservedTimeSeconds = 0.0;
	bHasObservedTime = false;
}
