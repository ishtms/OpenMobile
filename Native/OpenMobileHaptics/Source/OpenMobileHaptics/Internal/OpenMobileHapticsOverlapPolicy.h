#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

struct FOpenMobileHapticsOverlapConflict
{
	uint64 RequestId = 0;
	FName Channel;
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;
	bool bQueued = false;
};

struct FOpenMobileHapticsOverlapRequest
{
	FName Channel;
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;
	EOpenMobileHapticOverlapPolicy Policy =
		EOpenMobileHapticOverlapPolicy::Replace;
	EOpenMobileHapticSupportState Mixing =
		EOpenMobileHapticSupportState::Unsupported;
	EOpenMobileHapticOverlapPolicy UnsupportedMixFallback =
		EOpenMobileHapticOverlapPolicy::Replace;
};

enum class EOpenMobileHapticsOverlapOutcome : uint8
{
	Submit,
	Suppress,
	Queue,
	InterruptThenSubmit
};

struct FOpenMobileHapticsOverlapResolution
{
	EOpenMobileHapticsOverlapOutcome Outcome =
		EOpenMobileHapticsOverlapOutcome::Submit;
	EOpenMobileHapticOverlapPolicy ResolvedPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;
	TArray<uint64> TerminalRequestIds;
	bool bUsedMixFallback = false;
};

struct FOpenMobileHapticsOverlapQueueEntry
{
	uint64 RequestId = 0;
	FName Channel;
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;
	double EnqueuedAtSeconds = 0.0;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsOverlapPolicy final
{
public:
	static FOpenMobileHapticsOverlapResolution Resolve(
		const FOpenMobileHapticsOverlapRequest& Request,
		const TArray<FOpenMobileHapticsOverlapConflict>& Conflicts
	);

	static uint64 SelectNext(
		FName Channel,
		const TArray<FOpenMobileHapticsOverlapQueueEntry>& Queue
	);

	static bool IsExpired(
		double EnqueuedAtSeconds,
		double NowSeconds,
		double MaximumAgeSeconds
	);
};
