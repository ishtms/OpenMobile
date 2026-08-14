#pragma once

#include "CoreMinimal.h"

enum class EOpenMobileHapticsRecoveryAttemptOutcome : uint8
{
	Started,
	NotRecovering,
	Inactive,
	PolicyBlocked,
	Deferred,
	Exhausted
};

struct FOpenMobileHapticsRecoveryAttempt
{
	EOpenMobileHapticsRecoveryAttemptOutcome Outcome =
		EOpenMobileHapticsRecoveryAttemptOutcome::NotRecovering;
	int32 AttemptNumber = 0;
	double RetryDelaySeconds = 0.0;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsRecoveryPolicy final
{
public:
	/** Starts a fresh recovery window at interruption time and clamps attempts to a non-negative count. */
	void BeginInterruption(double NowSeconds, int32 MaximumAttempts);
	/** Starts one retry only when app state, policy, backoff, and attempt count all allow it. */
	FOpenMobileHapticsRecoveryAttempt TryBeginAttempt(
		double NowSeconds,
		bool bApplicationActive,
		bool bPolicyAllowsRecovery
	);
	/** Clears recovery after success or schedules the next bounded backoff after failure. */
	void CompleteAttempt(bool bRecovered, double NowSeconds);
	/** Stops retrying after a terminal native result, even if the configured attempt count remains. */
	void Abandon();
	/** Restores the idle state when backend lifecycle is rebuilt from scratch. */
	void Reset();

	/** Tells the subsystem whether interruption recovery still owns the lifecycle flow. */
	bool IsRecovering() const { return bRecovering; }
	/** Separates an exhausted retry window from an interruption that was never started. */
	bool IsExhausted() const { return bExhausted; }
	/** Prevents overlapping native recovery attempts while a callback is still outstanding. */
	bool IsAttemptInFlight() const { return bAttemptInFlight; }
	/** Exposes the attempts already started, handy for diagnostics and backoff reporting. */
	int32 GetAttemptCount() const { return AttemptCount; }
	/** Gives the tick loop the exact wake-up time without duplicating backoff calculation. */
	double GetNextAttemptTimeSeconds() const
	{
		return NextAttemptTimeSeconds;
	}

private:
	static constexpr double InitialDelaySeconds = 0.1;
	static constexpr double FirstRetryDelaySeconds = 0.25;
	static constexpr double MaximumRetryDelaySeconds = 2.0;

	bool bRecovering = false;
	bool bExhausted = false;
	bool bAttemptInFlight = false;
	int32 AttemptCount = 0;
	int32 MaximumAttemptCount = 0;
	double NextAttemptTimeSeconds = 0.0;
};
