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
	/** Converts backend-specific failure details into the stable error contract exposed to callers. */
	static FOpenMobileHapticError Map(
		const FOpenMobileHapticsErrorContext& Context
	);
	/** Fills only missing error fields from fallback context, so a useful native reason doesn't get overwritten. */
	static FOpenMobileHapticError Complete(
		FOpenMobileHapticError Error,
		const FOpenMobileHapticsErrorContext& FallbackContext
	);
};
