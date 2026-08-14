#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticNativePlaybackEvent,
	const FOpenMobileHapticPlaybackEvent&
);

/**
 * Native callers use this Game Instance owned interface when Blueprint nodes aren't involved. Calls belong on the game thread, and you've got to release event bindings before the subsystem goes away.
 */
class OPENMOBILEHAPTICS_API IOpenMobileHaptics
{
public:
	/** Allows deletion through the interface without skipping the subsystem implementation's cleanup. */
	virtual ~IOpenMobileHaptics() = default;

	/** Returns the latest runtime capability snapshot, Unknown stays Unknown till the backend proves support. */
	virtual FOpenMobileHapticCapabilities GetCapabilitiesNative() const = 0;
	/** Submits portable meaning instead of platform names, which lets policy choose the available native or fallback path. */
	virtual FOpenMobileHapticPlaybackResult SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request
	) = 0;
	/** Requests bounded vibration and reports clamping in the result, native duration limits aren't hidden from you. */
	virtual FOpenMobileHapticPlaybackResult SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request
	) = 0;
	/** Plays a prepared configured alias only, this low-level call won't synchronously load its library. */
	virtual FOpenMobileHapticPlaybackResult SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request
	) = 0;
	/** Ends accepted playback normally when its resolved path supports stopping. */
	virtual FOpenMobileHapticControlResult StopPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) = 0;
	/** Abandons pending or active playback and reports stale handles instead of touching recycled state. */
	virtual FOpenMobileHapticControlResult CancelPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) = 0;
	/** Applies only the fields selected in the update, so changing intensity won't reset sharpness also. */
	virtual FOpenMobileHapticControlResult UpdatePlaybackParametersNative(
		FOpenMobileHapticPlaybackHandle Handle,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) = 0;
	/** Pauses only when the backend can preserve a resumable position for this request. */
	virtual FOpenMobileHapticControlResult PausePlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) = 0;
	/** Continues a successfully paused request and rejects terminal or never-paused handles. */
	virtual FOpenMobileHapticControlResult ResumePlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) = 0;
	/** Seeks to a non-negative position and returns the backend's resolved timing in the control result. */
	virtual FOpenMobileHapticControlResult SeekPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle,
		double PositionSeconds
	) = 0;
	/** Stops requests owned by one resolved channel without disturbing independent feedback. */
	virtual FOpenMobileHapticControlResult StopChannelNative(
		FName Channel
	) = 0;
	/** Stops every request owned by this Game Instance, platform-global vibration outside the plugin isn't claimed here. */
	virtual FOpenMobileHapticControlResult StopAllNative() = 0;
	/** Reads tracked lifecycle state without asking the native actuator for an observation it can't provide. */
	virtual EOpenMobileHapticPlaybackState GetPlaybackStateNative(
		FOpenMobileHapticPlaybackHandle Handle
	) const = 0;
	/** Returns this Game Instance's player policy switch, it isn't a hardware availability query. */
	virtual bool IsHapticsEnabledNative() const = 0;
	/** Changes the runtime player switch only, persistence still belongs to the game. */
	virtual FOpenMobileHapticControlResult SetHapticsEnabledNative(
		bool bEnabled
	) = 0;
	/** Returns the normalized player scale currently applied before backend submission. */
	virtual float GetMasterIntensityNative() const = 0;
	/** Updates and clamps the runtime player scale without saving it to project or user settings. */
	virtual FOpenMobileHapticControlResult SetMasterIntensityNative(
		float MasterIntensity
	) = 0;
	/** Copies the full runtime policy when native code needs category and effect overrides also. */
	virtual FOpenMobileHapticUserPolicy GetUserPolicyNative() const = 0;
	/** Replaces runtime policy as one validated value, avoiding partially applied accessibility settings. */
	virtual FOpenMobileHapticControlResult UpdateUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy
	) = 0;
	/** Returns bounded sanitized diagnostics suitable for development tools, no raw native payload is exposed. */
	virtual FOpenMobileHapticsDiagnostics GetDiagnosticsNative() const = 0;
	/** Provides the subsystem-wide native event stream, bind by handle and unbind before subsystem teardown. */
	virtual FOpenMobileHapticNativePlaybackEvent& OnPlaybackEventNative() = 0;
};
