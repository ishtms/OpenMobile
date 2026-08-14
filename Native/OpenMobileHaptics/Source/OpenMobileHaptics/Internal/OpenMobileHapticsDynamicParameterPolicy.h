#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsDynamicParameterQueueOutcome : uint8
{
	Invalid,
	Ready,
	Coalesced
};

struct FOpenMobileHapticsScheduledDynamicParameterUpdate
{
	uint64 RequestId = 0;
	FOpenMobileHapticDynamicParameterUpdate Update;
};

class FOpenMobileHapticsDynamicParameterPolicy final
{
public:
	/** Rejects updates with no usable field or non-finite values before they enter the coalescing queue. */
	static bool IsValid(
		const FOpenMobileHapticDynamicParameterUpdate& Update
	);

	/** Creates timing state at native acceptance, earlier updates don't have a player to target. */
	void RegisterPlayback(uint64 RequestId, double SubmissionTimeSeconds);
	/** Drops queued parameters with the playback, otherwise a reused request slot could receive stale values. */
	void RemovePlayback(uint64 RequestId);
	/** Allows the subsystem tick to sleep when no rate-limited update is waiting. */
	bool HasPending() const;

	/** Returns an update immediately when the interval allows it, otherwise merges it with the pending value for that request. */
	EOpenMobileHapticsDynamicParameterQueueOutcome Queue(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update,
		double NowSeconds,
		double MinimumIntervalSeconds,
		FOpenMobileHapticDynamicParameterUpdate& OutReady
	);
	/** Collects updates whose interval has elapsed and clears only the values being submitted now. */
	void CollectReady(
		double NowSeconds,
		double MinimumIntervalSeconds,
		TArray<FOpenMobileHapticsScheduledDynamicParameterUpdate>& OutUpdates
	);
	/** Advances the rate-limit clock after an actual backend attempt, successful or not. */
	void MarkAttempted(uint64 RequestId, double AttemptTimeSeconds);

private:
	struct FPlaybackState
	{
		double LastSubmissionTimeSeconds = 0.0;
		FOpenMobileHapticDynamicParameterUpdate Pending;
		bool bHasPending = false;
	};

	/** Merges field by field so a later intensity change doesn't erase pending sharpness. */
	static void Merge(
		FOpenMobileHapticDynamicParameterUpdate& Target,
		const FOpenMobileHapticDynamicParameterUpdate& Source
	);
	/** Checks elapsed time with the stored submission clock, keeping the comparison identical for queued and collected updates. */
	static bool IsReady(
		const FPlaybackState& State,
		double NowSeconds,
		double MinimumIntervalSeconds
	);
	/** Moves one pending update out and resets its flag together, so it can't be submitted twice. */
	static FOpenMobileHapticDynamicParameterUpdate TakePending(
		FPlaybackState& State
	);

	TMap<uint64, FPlaybackState> Playbacks;
	TArray<uint64> ScratchRequestIds;
};
