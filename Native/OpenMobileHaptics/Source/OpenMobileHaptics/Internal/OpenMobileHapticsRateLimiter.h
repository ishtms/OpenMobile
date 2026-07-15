#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsRateLimitOutcome : uint8
{
	Allowed,
	EquivalentRequest,
	ChannelMinimumInterval,
	EffectMinimumInterval,
	ChannelWindow,
	GlobalWindow,
	InvalidClock
};

struct FOpenMobileHapticsRateLimitRequest
{
	FName Channel;
	FName Category;
	FName Effect;
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;
	uint32 EquivalenceHash = 0;
	bool bCoalescible = false;
};

struct FOpenMobileHapticsRateLimitPolicy
{
	double ChannelMinimumIntervalSeconds = 0.02;
	double EffectMinimumIntervalSeconds = 0.0;
	double EquivalentRequestDebounceSeconds = 0.0;
	int32 MaximumChannelSubmissionsPerSecond = 30;
	int32 MaximumGlobalSubmissionsPerSecond = 30;
};

struct FOpenMobileHapticsRateLimitDecision
{
	EOpenMobileHapticsRateLimitOutcome Outcome =
		EOpenMobileHapticsRateLimitOutcome::Allowed;
	bool bClockReset = false;

	bool IsAllowed() const
	{
		return Outcome == EOpenMobileHapticsRateLimitOutcome::Allowed;
	}
};

class FOpenMobileHapticsRateLimiter final
{
public:
	using FClock = TFunction<double()>;

	enum : int32
	{
		HardMaximumChannelSubmissionsPerSecond = 30,
		HardMaximumGlobalSubmissionsPerSecond = 60,
		MaximumTrackedChannels = 128,
		MaximumTrackedEffects = 1024,
		MaximumTrackedEquivalentRequests = 256
	};

	FOpenMobileHapticsRateLimiter();
	explicit FOpenMobileHapticsRateLimiter(FClock InClock);

	FOpenMobileHapticsRateLimitDecision Evaluate(
		const FOpenMobileHapticsRateLimitRequest& Request,
		const FOpenMobileHapticsRateLimitPolicy& Policy
	);
	void Reset();

	bool ShouldSuppress(
		FName Channel,
		bool bSelection,
		double TimeSeconds,
		double MinimumIntervalSeconds,
		double SelectionDebounceSeconds,
		int32 MaximumSubmissionsPerSecond
	);

private:
	struct FChannelState
	{
		TArray<double> RecentSubmissionTimes;
		double LastSubmissionTimeSeconds = 0.0;
		uint64 AccessSequence = 0;
		bool bHasSubmission = false;
	};

	struct FEffectState
	{
		double LastSubmissionTimeSeconds = 0.0;
		uint64 AccessSequence = 0;
	};

	struct FEquivalentRequestKey
	{
		FName Channel;
		FName Category;
		FName Effect;
		EOpenMobileHapticChannelPriority Priority =
			EOpenMobileHapticChannelPriority::Normal;
		uint32 Signature = 0;

		friend bool operator==(
			const FEquivalentRequestKey& Left,
			const FEquivalentRequestKey& Right
		)
		{
			return Left.Channel == Right.Channel
				&& Left.Category == Right.Category
				&& Left.Effect == Right.Effect
				&& Left.Priority == Right.Priority
				&& Left.Signature == Right.Signature;
		}

		friend uint32 GetTypeHash(const FEquivalentRequestKey& Key)
		{
			uint32 Hash = HashCombineFast(
				GetTypeHash(Key.Channel),
				GetTypeHash(Key.Category)
			);
			Hash = HashCombineFast(Hash, GetTypeHash(Key.Effect));
			Hash = HashCombineFast(
				Hash,
				GetTypeHash(static_cast<uint8>(Key.Priority))
			);
			return HashCombineFast(Hash, Key.Signature);
		}
	};

	struct FEquivalentRequestState
	{
		double LastSubmissionTimeSeconds = 0.0;
		uint64 AccessSequence = 0;
	};

	FOpenMobileHapticsRateLimitDecision EvaluateAtLocked(
		const FOpenMobileHapticsRateLimitRequest& Request,
		const FOpenMobileHapticsRateLimitPolicy& Policy,
		double TimeSeconds
	);
	FChannelState& FindOrAddChannel(FName Channel);
	FEffectState& FindOrAddEffect(FName Effect);
	FEquivalentRequestState& FindOrAddEquivalentRequest(
		const FEquivalentRequestKey& Key
	);
	void ResetLocked();

	FClock Clock;
	FCriticalSection Mutex;
	TMap<FName, FChannelState> Channels;
	TMap<FName, FEffectState> Effects;
	TMap<FEquivalentRequestKey, FEquivalentRequestState> EquivalentRequests;
	TArray<double> RecentGlobalSubmissionTimes;
	TArray<double> RecentNonCriticalSubmissionTimes;
	double LastObservedTimeSeconds = 0.0;
	uint64 AccessSequence = 0;
	bool bHasObservedTime = false;
};
