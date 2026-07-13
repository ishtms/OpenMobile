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
	void Configure(const FOpenMobileHapticsChannelLimits& InLimits);
	FOpenMobileHapticsChannelAdmissionResult TryReserve(
		const FOpenMobileHapticsChannelAdmissionRequest& Request
	);
	void Release(uint64 RequestId);
	void Reset();

	int32 GetActiveCount() const { return ActiveCount; }
	int32 GetQueuedCount() const { return QueuedCount; }

private:
	FOpenMobileHapticsChannelLimits Limits;
	TMap<uint64, FOpenMobileHapticsChannelAdmissionRequest> Reservations;
	TMap<FName, int32> ActiveCountsByChannel;
	TMap<FName, int32> QueuedCountsByChannel;
	int32 ActiveCount = 0;
	int32 QueuedCount = 0;
};
