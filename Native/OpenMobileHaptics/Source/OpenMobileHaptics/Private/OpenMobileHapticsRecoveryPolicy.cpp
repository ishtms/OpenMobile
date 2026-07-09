#include "OpenMobileHapticsRecoveryPolicy.h"

void FOpenMobileHapticsRecoveryPolicy::BeginInterruption(
	double NowSeconds,
	int32 MaximumAttempts
)
{
	if (bRecovering)
	{
		return;
	}
	bRecovering = true;
	bExhausted = MaximumAttempts <= 0;
	bAttemptInFlight = false;
	AttemptCount = 0;
	MaximumAttemptCount = FMath::Clamp(MaximumAttempts, 0, 8);
	const double ValidNow = FMath::IsFinite(NowSeconds) ? NowSeconds : 0.0;
	NextAttemptTimeSeconds = ValidNow + InitialDelaySeconds;
}

FOpenMobileHapticsRecoveryAttempt
FOpenMobileHapticsRecoveryPolicy::TryBeginAttempt(
	double NowSeconds,
	bool bApplicationActive,
	bool bPolicyAllowsRecovery
)
{
	FOpenMobileHapticsRecoveryAttempt Result;
	if (!bRecovering)
	{
		return Result;
	}
	if (!bApplicationActive)
	{
		Result.Outcome = EOpenMobileHapticsRecoveryAttemptOutcome::Inactive;
		return Result;
	}
	if (!bPolicyAllowsRecovery)
	{
		Result.Outcome =
			EOpenMobileHapticsRecoveryAttemptOutcome::PolicyBlocked;
		return Result;
	}
	if (bExhausted || AttemptCount >= MaximumAttemptCount)
	{
		bExhausted = true;
		Result.Outcome = EOpenMobileHapticsRecoveryAttemptOutcome::Exhausted;
		return Result;
	}
	if (bAttemptInFlight)
	{
		Result.Outcome = EOpenMobileHapticsRecoveryAttemptOutcome::Deferred;
		return Result;
	}
	const double ValidNow = FMath::IsFinite(NowSeconds)
		? NowSeconds
		: NextAttemptTimeSeconds;
	if (ValidNow + UE_DOUBLE_SMALL_NUMBER < NextAttemptTimeSeconds)
	{
		Result.Outcome = EOpenMobileHapticsRecoveryAttemptOutcome::Deferred;
		Result.RetryDelaySeconds = NextAttemptTimeSeconds - ValidNow;
		return Result;
	}

	bAttemptInFlight = true;
	++AttemptCount;
	Result.Outcome = EOpenMobileHapticsRecoveryAttemptOutcome::Started;
	Result.AttemptNumber = AttemptCount;
	return Result;
}

void FOpenMobileHapticsRecoveryPolicy::CompleteAttempt(
	bool bRecovered,
	double NowSeconds
)
{
	if (!bRecovering || !bAttemptInFlight)
	{
		return;
	}
	bAttemptInFlight = false;
	if (bRecovered)
	{
		Reset();
		return;
	}
	if (AttemptCount >= MaximumAttemptCount)
	{
		bExhausted = true;
		return;
	}
	const double Exponent = static_cast<double>(FMath::Max(0, AttemptCount - 1));
	const double DelaySeconds = FMath::Min(
		FirstRetryDelaySeconds * FMath::Pow(2.0, Exponent),
		MaximumRetryDelaySeconds
	);
	const double ValidNow = FMath::IsFinite(NowSeconds) ? NowSeconds : 0.0;
	NextAttemptTimeSeconds = ValidNow + DelaySeconds;
}

void FOpenMobileHapticsRecoveryPolicy::Abandon()
{
	if (bRecovering)
	{
		bAttemptInFlight = false;
		bExhausted = true;
	}
}

void FOpenMobileHapticsRecoveryPolicy::Reset()
{
	bRecovering = false;
	bExhausted = false;
	bAttemptInFlight = false;
	AttemptCount = 0;
	MaximumAttemptCount = 0;
	NextAttemptTimeSeconds = 0.0;
}
