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
	void BeginInterruption(double NowSeconds, int32 MaximumAttempts);
	FOpenMobileHapticsRecoveryAttempt TryBeginAttempt(
		double NowSeconds,
		bool bApplicationActive,
		bool bPolicyAllowsRecovery
	);
	void CompleteAttempt(bool bRecovered, double NowSeconds);
	void Abandon();
	void Reset();

	bool IsRecovering() const { return bRecovering; }
	bool IsExhausted() const { return bExhausted; }
	bool IsAttemptInFlight() const { return bAttemptInFlight; }
	int32 GetAttemptCount() const { return AttemptCount; }
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
