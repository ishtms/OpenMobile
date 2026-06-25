#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticNativePlaybackEvent,
	const FOpenMobileHapticPlaybackEvent&
);

/**
 * Game-thread C++ facade owned by the Haptics Game Instance subsystem.
 * Callers must release delegate bindings before the subsystem is destroyed.
 */
class OPENMOBILEHAPTICS_API IOpenMobileHaptics
{
public:
	virtual ~IOpenMobileHaptics() = default;

	virtual FOpenMobileHapticCapabilities GetCapabilitiesNative() const = 0;
	virtual FOpenMobileHapticPlaybackResult SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request
	) = 0;
	virtual FOpenMobileHapticPlaybackResult SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request
	) = 0;
	virtual FOpenMobileHapticPlaybackResult SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request
	) = 0;
	virtual FOpenMobileHapticControlResult StopPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) = 0;
	virtual FOpenMobileHapticControlResult CancelPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) = 0;
	virtual FOpenMobileHapticControlResult UpdatePlaybackParametersNative(
		FOpenMobileHapticPlaybackHandle Handle,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) = 0;
	virtual FOpenMobileHapticControlResult StopChannelNative(
		FName Channel
	) = 0;
	virtual FOpenMobileHapticControlResult StopAllNative() = 0;
	virtual EOpenMobileHapticPlaybackState GetPlaybackStateNative(
		FOpenMobileHapticPlaybackHandle Handle
	) const = 0;
	virtual FOpenMobileHapticUserPolicy GetUserPolicyNative() const = 0;
	virtual FOpenMobileHapticControlResult UpdateUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy
	) = 0;
	virtual FOpenMobileHapticsDiagnostics GetDiagnosticsNative() const = 0;
	virtual FOpenMobileHapticNativePlaybackEvent& OnPlaybackEventNative() = 0;
};
