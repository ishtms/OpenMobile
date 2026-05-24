#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

enum class EOpenMobileHapticsFailureReason : uint8
{
	UnsupportedHardware,
	UnsupportedFeature,
	DisabledPolicy,
	InvalidPattern,
	RateLimited,
	BusyChannel,
	LifecycleRestricted,
	NotConfigured,
	NativeEngineFailure,
	Interrupted,
	Cancelled,
	InvalidRequest,
	BackendUnavailable,
	Internal
};

struct FOpenMobileHapticsErrorContext
{
	EOpenMobileHapticsFailureReason Reason =
		EOpenMobileHapticsFailureReason::Internal;
	EOpenMobileHapticFailureStage Stage =
		EOpenMobileHapticFailureStage::None;
	FString NativeDomain;
	FString NativeCode;
	FName FailedItem;
	FName Channel;
	FOpenMobileHapticPlaybackHandle Handle;
	TArray<FName> FallbackAttempts;
	bool bAfterAcceptance = false;
};

class OPENMOBILEHAPTICS_API FOpenMobileHapticsErrorMapper final
{
public:
	static FOpenMobileHapticError Map(
		const FOpenMobileHapticsErrorContext& Context
	);
	static FOpenMobileHapticError Complete(
		FOpenMobileHapticError Error,
		const FOpenMobileHapticsErrorContext& FallbackContext
	);
};
