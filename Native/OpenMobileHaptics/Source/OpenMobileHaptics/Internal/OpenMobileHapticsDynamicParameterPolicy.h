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
	static bool IsValid(
		const FOpenMobileHapticDynamicParameterUpdate& Update
	);

	void RegisterPlayback(uint64 RequestId, double SubmissionTimeSeconds);
	void RemovePlayback(uint64 RequestId);
	bool HasPending() const;

	EOpenMobileHapticsDynamicParameterQueueOutcome Queue(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update,
		double NowSeconds,
		double MinimumIntervalSeconds,
		FOpenMobileHapticDynamicParameterUpdate& OutReady
	);
	void CollectReady(
		double NowSeconds,
		double MinimumIntervalSeconds,
		TArray<FOpenMobileHapticsScheduledDynamicParameterUpdate>& OutUpdates
	);
	void MarkAttempted(uint64 RequestId, double AttemptTimeSeconds);

private:
	struct FPlaybackState
	{
		double LastSubmissionTimeSeconds = 0.0;
		FOpenMobileHapticDynamicParameterUpdate Pending;
		bool bHasPending = false;
	};

	static void Merge(
		FOpenMobileHapticDynamicParameterUpdate& Target,
		const FOpenMobileHapticDynamicParameterUpdate& Source
	);
	static bool IsReady(
		const FPlaybackState& State,
		double NowSeconds,
		double MinimumIntervalSeconds
	);
	static FOpenMobileHapticDynamicParameterUpdate TakePending(
		FPlaybackState& State
	);

	TMap<uint64, FPlaybackState> Playbacks;
};
