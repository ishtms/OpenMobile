#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsSettings.h"

struct FOpenMobileHapticsResolvedChannel
{
	EOpenMobileHapticChannelPriority EffectivePriority =
		EOpenMobileHapticChannelPriority::Normal;
	EOpenMobileHapticOverlapPolicy UnsupportedMixFallbackPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;
	int32 MaximumActiveHandles = 0;
	int32 MaximumQueueDepth = 0;
	bool bConfigured = false;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsChannelPolicy final
{
public:
	/** Combines request priority with per-channel settings and hard limits, so admission sees one resolved rule set. */
	static FOpenMobileHapticsResolvedChannel Resolve(
		FName Channel,
		EOpenMobileHapticChannelPriority RequestPriority,
		const TArray<FOpenMobileHapticChannelSettings>& Channels,
		int32 MaximumActiveHandles,
		int32 MaximumQueueDepthPerChannel
	);
};

struct FOpenMobileHapticsChannelLimits
{
	int32 MaximumActiveHandles = 1;
	int32 MaximumQueuedHandles = 1;
	int32 MaximumQueueDepthPerChannel = 1;
};

struct FOpenMobileHapticsChannelAdmissionRequest
{
	uint64 RequestId = 0;
	FName Channel;
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;
	int32 MaximumActiveHandles = 0;
	int32 MaximumQueueDepth = 0;
	bool bQueued = false;
	bool bWaitingForOverlap = false;
	bool bRepeating = false;
};

enum class EOpenMobileHapticsChannelAdmissionOutcome : uint8
{
	Admitted,
	PreemptRequired,
	ActiveCapacityReached,
	ChannelActiveCapacityReached,
	GlobalQueueCapacityReached,
	ChannelQueueCapacityReached
};

struct FOpenMobileHapticsChannelAdmissionResult
{
	EOpenMobileHapticsChannelAdmissionOutcome Outcome =
		EOpenMobileHapticsChannelAdmissionOutcome::ActiveCapacityReached;
	uint64 PreemptRequestId = 0;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsChannelArbiter final
{
public:
	/** Replaces admission limits only after clamping them to values the counters can enforce. */
	void Configure(const FOpenMobileHapticsChannelLimits& InLimits);
	/** Reserves active or queued capacity atomically and names the lower-priority owner when preemption is allowed. */
	FOpenMobileHapticsChannelAdmissionResult TryReserve(
		const FOpenMobileHapticsChannelAdmissionRequest& Request
	);
	/** Releases every counter tied to one request, regardless of whether it was active or queued. */
	void Release(uint64 RequestId);
	/** Clears reservations during backend reset so old counts can't block the new generation. */
	void Reset();

	/** Reports global active usage without exposing the reservation table. */
	int32 GetActiveCount() const { return ActiveCount; }
	/** Reports global queued usage for diagnostics and back-pressure decisions. */
	int32 GetQueuedCount() const { return QueuedCount; }
	/** Builds bounded per-channel diagnostics, returning false when the caller's output cap can't hold them all. */
	bool BuildDiagnostics(
		TArray<FOpenMobileHapticChannelDiagnostics>& OutDiagnostics,
		int32 MaximumEntries
	) const;

private:
	FOpenMobileHapticsChannelLimits Limits;
	TMap<uint64, FOpenMobileHapticsChannelAdmissionRequest> Reservations;
	TMap<FName, int32> ActiveCountsByChannel;
	TMap<FName, int32> QueuedCountsByChannel;
	int32 ActiveCount = 0;
	int32 QueuedCount = 0;
};
