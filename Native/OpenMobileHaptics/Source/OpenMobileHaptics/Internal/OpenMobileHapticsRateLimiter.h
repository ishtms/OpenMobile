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

	/** Makes callers inspect the resolved outcome instead of assuming a default decision means success. */
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

	/** Uses the platform clock for runtime admission decisions. */
	FOpenMobileHapticsRateLimiter();
	/** Accepts a caller clock so timing behavior can be deterministic outside the live game loop. */
	explicit FOpenMobileHapticsRateLimiter(FClock InClock);

	/** Applies debounce, per-effect, per-channel, and global windows while holding one state lock. */
	FOpenMobileHapticsRateLimitDecision Evaluate(
		const FOpenMobileHapticsRateLimitRequest& Request,
		const FOpenMobileHapticsRateLimitPolicy& Policy
	);
	/** Clears observed time and request history when the subsystem lifecycle restarts. */
	void Reset();

	/** Supports the older selection-rate contract through the same bounded tracking state. */
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

		/** Compares every field that affects coalescing, including priority and caller signature. */
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

		/** Hashes the same fields as equality so equivalent requests land in the right entry. */
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

	/** Evaluates one already-sampled time while the mutex is held, clock rollback resets history before admission. */
	FOpenMobileHapticsRateLimitDecision EvaluateAtLocked(
		const FOpenMobileHapticsRateLimitRequest& Request,
		const FOpenMobileHapticsRateLimitPolicy& Policy,
		double TimeSeconds
	);
	/** Adds a channel with bounded least-recently-used eviction when tracking is full. */
	FChannelState& FindOrAddChannel(FName Channel);
	/** Adds effect timing state without allowing the map to grow past its hard cap. */
	FEffectState& FindOrAddEffect(FName Effect);
	/** Adds a coalescing key and evicts the oldest observed key when necessary. */
	FEquivalentRequestState& FindOrAddEquivalentRequest(
		const FEquivalentRequestKey& Key
	);
	/** Clears timing state while the caller already owns the mutex. */
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
