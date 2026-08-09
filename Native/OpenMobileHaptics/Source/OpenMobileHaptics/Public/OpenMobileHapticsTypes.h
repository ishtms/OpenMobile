#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileHapticsTypes.generated.h"

class UOpenMobileHapticPreparationLease;

UENUM(BlueprintType, meta = (ToolTip = "Current categorical Haptics availability. Never compare these values by ordinal."))
enum class EOpenMobileHapticAvailability : uint8
{
	UnsupportedPlatform UMETA(DisplayName = "Unsupported Platform", ToolTip = "This platform has no OpenMobile Haptics backend."),
	NoActuator UMETA(DisplayName = "No Haptic Actuator", ToolTip = "The platform is supported, but this device reports no Haptic actuator."),
	BasicVibration UMETA(DisplayName = "Basic Vibration", ToolTip = "Only basic phone vibration is currently available."),
	SemanticFeedback UMETA(DisplayName = "Semantic Haptics", ToolTip = "Portable selection, impact, and notification feedback is available."),
	RichHaptics UMETA(DisplayName = "Rich Haptics", ToolTip = "Custom prepared Haptic patterns are available."),
	DisabledByPolicy UMETA(DisplayName = "Disabled By Player Policy", ToolTip = "Hardware may be supported, but the current player policy disables normal output."),
	TemporarilyUnavailable UMETA(DisplayName = "Temporarily Unavailable", ToolTip = "Haptics are temporarily unavailable during lifecycle or backend recovery.")
};

UENUM(BlueprintType, meta = (ToolTip = "Tri-state device support. Unknown is never proof of support."))
enum class EOpenMobileHapticSupportState : uint8
{
	Unknown UMETA(DisplayName = "Unknown", ToolTip = "Support has not been determined and must not be treated as supported."),
	Supported UMETA(DisplayName = "Supported", ToolTip = "The current device explicitly reports support."),
	Unsupported UMETA(DisplayName = "Unsupported", ToolTip = "The current device explicitly reports no support.")
};

UENUM(BlueprintType, meta = (ToolTip = "Portable semantic Haptic intent. Prefer the narrower common nodes for impact, notification, and game feedback."))
enum class EOpenMobileHapticSemanticEffect : uint8
{
	Selection UMETA(DisplayName = "Selection", ToolTip = "A small UI selection or picker step."),
	ImpactLight UMETA(DisplayName = "Light Impact", ToolTip = "A light portable impact. Prefer Play Impact Haptic in common graphs."),
	ImpactMedium UMETA(DisplayName = "Medium Impact", ToolTip = "A medium portable impact. Prefer Play Impact Haptic in common graphs."),
	ImpactHeavy UMETA(DisplayName = "Heavy Impact", ToolTip = "A heavy portable impact. Prefer Play Impact Haptic in common graphs."),
	ImpactSoft UMETA(DisplayName = "Soft Impact", ToolTip = "A rounded soft impact. Prefer Play Impact Haptic in common graphs."),
	ImpactRigid UMETA(DisplayName = "Rigid Impact", ToolTip = "A crisp rigid impact. Prefer Play Impact Haptic in common graphs."),
	NotificationSuccess UMETA(DisplayName = "Notification Success", ToolTip = "Success feedback. Prefer Play Notification Haptic in common graphs."),
	NotificationWarning UMETA(DisplayName = "Notification Warning", ToolTip = "Warning feedback. Prefer Play Notification Haptic in common graphs."),
	NotificationError UMETA(DisplayName = "Notification Error", ToolTip = "Error feedback. Prefer Play Notification Haptic in common graphs."),
	Confirm UMETA(DisplayName = "Confirm", ToolTip = "A portable confirmed-action game intent."),
	Reject UMETA(DisplayName = "Reject", ToolTip = "A portable rejected-action game intent."),
	Tick UMETA(DisplayName = "Tick", ToolTip = "A small discrete gameplay step."),
	Click UMETA(DisplayName = "Click", ToolTip = "A crisp discrete gameplay click."),
	Bump UMETA(DisplayName = "Bump", ToolTip = "A brief gameplay bump or contact."),
	Damage UMETA(DisplayName = "Damage", ToolTip = "A gameplay damage event."),
	Pickup UMETA(DisplayName = "Pickup", ToolTip = "A gameplay pickup event."),
	Achievement UMETA(DisplayName = "Achievement", ToolTip = "A notable reward or achievement event.")
};

UENUM(BlueprintType, meta = (ToolTip = "Portable impact character for Play Impact Haptic."))
enum class EOpenMobileHapticImpactStyle : uint8
{
	Light UMETA(DisplayName = "Light", ToolTip = "A subtle light impact."),
	Medium UMETA(DisplayName = "Medium", ToolTip = "A balanced medium impact."),
	Heavy UMETA(DisplayName = "Heavy", ToolTip = "A strong heavy impact."),
	Soft UMETA(DisplayName = "Soft", ToolTip = "A rounded soft impact when the platform supports it."),
	Rigid UMETA(DisplayName = "Rigid", ToolTip = "A crisp rigid impact when the platform supports it.")
};

UENUM(BlueprintType, meta = (ToolTip = "Portable in-app notification meaning. This does not create an operating-system notification."))
enum class EOpenMobileHapticNotificationType : uint8
{
	Success UMETA(DisplayName = "Success", ToolTip = "Positive completion or confirmation feedback."),
	Warning UMETA(DisplayName = "Warning", ToolTip = "Important warning feedback."),
	Error UMETA(DisplayName = "Error", ToolTip = "Failure or invalid-action feedback.")
};

UENUM(BlueprintType, meta = (ToolTip = "Stable game-facing Haptic preset. Configured prepared-pattern overrides are preferred when available."))
enum class EOpenMobileHapticGamePreset : uint8
{
	Confirm UMETA(DisplayName = "Confirm", ToolTip = "A confirmed player action."),
	Reject UMETA(DisplayName = "Reject", ToolTip = "A rejected or unavailable player action."),
	Tick UMETA(DisplayName = "Tick", ToolTip = "A small repeated gameplay step."),
	Click UMETA(DisplayName = "Click", ToolTip = "A crisp gameplay click."),
	Bump UMETA(DisplayName = "Bump", ToolTip = "A brief collision or surface bump."),
	Damage UMETA(DisplayName = "Damage", ToolTip = "A player or controlled-object damage event."),
	Pickup UMETA(DisplayName = "Pickup", ToolTip = "An item pickup event."),
	Achievement UMETA(DisplayName = "Achievement", ToolTip = "A notable reward or achievement event.")
};

UENUM(BlueprintType, meta = (ToolTip = "Authored portable pattern event type."))
enum class EOpenMobileHapticPatternEventType : uint8
{
	Transient UMETA(DisplayName = "Transient", ToolTip = "A short instantaneous Haptic event."),
	Continuous UMETA(DisplayName = "Continuous", ToolTip = "A Haptic event with an authored duration."),
	Silence UMETA(DisplayName = "Silence", ToolTip = "An explicit silent interval in the portable timeline.")
};

UENUM(BlueprintType, meta = (ToolTip = "Portable parameter controlled by an authored curve."))
enum class EOpenMobileHapticCurveParameter : uint8
{
	IntensityControl UMETA(DisplayName = "Intensity", ToolTip = "Controls normalized Haptic strength over time."),
	SharpnessControl UMETA(DisplayName = "Sharpness", ToolTip = "Controls normalized Haptic texture over time.")
};

UENUM(BlueprintType, meta = (ToolTip = "Relative project priority used for channel admission and interruption."))
enum class EOpenMobileHapticChannelPriority : uint8
{
	Low UMETA(DisplayName = "Low", ToolTip = "Background or cosmetic feedback that should yield first."),
	Normal UMETA(DisplayName = "Normal", ToolTip = "Normal UI and gameplay feedback."),
	High UMETA(DisplayName = "High", ToolTip = "Important feedback that can outrank normal work."),
	Critical UMETA(DisplayName = "Critical", ToolTip = "Critical alert feedback, still subject to explicit lifecycle and accessibility policy.")
};

UENUM(BlueprintType, meta = (ToolTip = "How a new request interacts with plugin-owned playback on the same channel."))
enum class EOpenMobileHapticOverlapPolicy : uint8
{
	Replace UMETA(DisplayName = "Replace Existing", ToolTip = "Interrupts existing plugin-owned work on the same channel before submission."),
	Ignore UMETA(DisplayName = "Ignore New Request", ToolTip = "Suppresses the new request while the channel is occupied."),
	Queue UMETA(DisplayName = "Queue", ToolTip = "Queues the new request within bounded project limits."),
	InterruptLowerPriority UMETA(DisplayName = "Interrupt Lower Priority", ToolTip = "Interrupts conflicts only when every conflict has lower effective priority."),
	MixWhenSupported UMETA(DisplayName = "Mix When Supported", ToolTip = "Mixes only when the selected backend supports it, otherwise uses the configured fallback policy.")
};

UENUM(BlueprintType, meta = (ToolTip = "How the runtime may reduce Haptic quality when the requested path is unavailable."))
enum class EOpenMobileHapticFallbackPolicy : uint8
{
	Automatic UMETA(DisplayName = "Automatic", ToolTip = "Allows the configured portable fallback ladder, including basic vibration."),
	NoBasicVibration UMETA(DisplayName = "No Basic Vibration", ToolTip = "Allows rich, primitive, preset, or semantic fallback but not generic basic vibration."),
	ExactOnly UMETA(DisplayName = "Exact Only", ToolTip = "Rejects the request unless the exact requested path is available."),
	NoEffectAllowed UMETA(DisplayName = "No Effect Allowed", ToolTip = "Allows intentional silence when no suitable output path is available.")
};

UENUM(BlueprintType, meta = (ToolTip = "What happens to eligible named playback after a native interruption."))
enum class EOpenMobileHapticInterruptionPolicy : uint8
{
	Stop UMETA(DisplayName = "Stop", ToolTip = "Ends playback after interruption and does not restart it."),
	Restart UMETA(DisplayName = "Restart If Eligible", ToolTip = "May restart an eligible named pattern with a new handle after recovery when project policy allows it.")
};

UENUM(BlueprintType, meta = (ToolTip = "Clock interpretation for a Haptic playback schedule."))
enum class EOpenMobileHapticScheduleMode : uint8
{
	Immediate UMETA(DisplayName = "Immediate", ToolTip = "Submits without an intentional delay."),
	Relative UMETA(DisplayName = "After Delay", ToolTip = "Starts after a relative delay in seconds."),
	AbsoluteGameTime UMETA(DisplayName = "At Game Time", ToolTip = "Targets an absolute calibrated Unreal game-clock time."),
	AbsoluteAudioTime UMETA(DisplayName = "At Audio Time", ToolTip = "Targets an absolute calibrated Unreal audio-clock time.")
};

UENUM(BlueprintType, meta = (ToolTip = "Clock used for Haptic timing calibration."))
enum class EOpenMobileHapticTimingClock : uint8
{
	None UMETA(DisplayName = "No Clock", ToolTip = "No absolute timing clock is selected."),
	Game UMETA(DisplayName = "Game Clock", ToolTip = "Unreal game time supplied by the caller."),
	Audio UMETA(DisplayName = "Audio Clock", ToolTip = "Unreal audio time supplied by the caller.")
};

UENUM(BlueprintType, meta = (ToolTip = "Result of sampling a Haptic timing clock."))
enum class EOpenMobileHapticTimingCalibrationStatus : uint8
{
	Rejected UMETA(DisplayName = "Rejected", ToolTip = "The clock sample or precision was invalid."),
	Accepted UMETA(DisplayName = "Calibrated", ToolTip = "The clock sample was accepted for absolute scheduling."),
	ClockDiscontinuity UMETA(DisplayName = "Clock Reset", ToolTip = "The clock moved discontinuously, so its previous calibration was cleared.")
};

UENUM(BlueprintType, meta = (ToolTip = "Timing quality used by the resolved native playback path."))
enum class EOpenMobileHapticSynchronizationMode : uint8
{
	None UMETA(DisplayName = "No Synchronization", ToolTip = "No audio or absolute-time synchronization was requested."),
	NativeAudioAndHaptics UMETA(DisplayName = "Native Audio And Haptics", ToolTip = "The platform owns a shared native audio and Haptics timeline."),
	BestEffort UMETA(DisplayName = "Best Effort", ToolTip = "The runtime schedules against calibrated clocks without claiming sample-accurate output.")
};

UENUM(BlueprintType, meta = (ToolTip = "Latest lifecycle state for one accepted Haptic request."))
enum class EOpenMobileHapticPlaybackState : uint8
{
	Invalid UMETA(DisplayName = "Invalid", ToolTip = "No live request is represented by this state."),
	Accepted UMETA(DisplayName = "Accepted", ToolTip = "The runtime accepted the request."),
	Scheduled UMETA(DisplayName = "Scheduled", ToolTip = "The request owns a future start."),
	Started UMETA(DisplayName = "Started", ToolTip = "The request reached its resolved start."),
	Paused UMETA(DisplayName = "Paused", ToolTip = "A supported native playback path is paused."),
	Resumed UMETA(DisplayName = "Resumed", ToolTip = "A paused native playback path resumed."),
	Stopped UMETA(DisplayName = "Stopped", ToolTip = "Playback ended after a graceful stop request."),
	Cancelled UMETA(DisplayName = "Cancelled", ToolTip = "Pending or active playback was cancelled."),
	Completed UMETA(DisplayName = "Completed", ToolTip = "Playback reached its natural terminal end."),
	Interrupted UMETA(DisplayName = "Interrupted", ToolTip = "Lifecycle or a native engine interruption ended playback."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "Playback failed before or after native submission.")
};

UENUM(BlueprintType, meta = (ToolTip = "Detailed immediate runtime outcome. Common nodes map fallback into Accepted plus Used Fallback."))
enum class EOpenMobileHapticPlaybackOutcome : uint8
{
	Rejected UMETA(DisplayName = "Rejected", ToolTip = "The request was invalid, unsupported, unavailable, or failed submission."),
	Accepted UMETA(DisplayName = "Accepted", ToolTip = "The requested path was accepted."),
	Suppressed UMETA(DisplayName = "Suppressed", ToolTip = "The request was intentionally silent and has no active playback."),
	Fallback UMETA(DisplayName = "Accepted With Fallback", ToolTip = "A lower-quality allowed output path was accepted.")
};

UENUM(BlueprintType, meta = (ToolTip = "Immediate outcome for a Blueprint-first Haptics request. Fallback is reported as accepted with a separate flag."))
enum class EOpenMobileHapticRequestOutcome : uint8
{
	Accepted UMETA(
		DisplayName = "Accepted",
		ToolTip = "The request was accepted for playback. This does not prove that a person felt the actuator output."
	),
	Suppressed UMETA(
		DisplayName = "Suppressed",
		ToolTip = "The request was intentionally silent because of policy, lifecycle, rate limiting, overlap, or zero output."
	),
	Rejected UMETA(
		DisplayName = "Rejected",
		ToolTip = "The request was invalid, unavailable, unsupported, or could not be submitted."
	)
};

UENUM(BlueprintType, meta = (ToolTip = "Why one accepted Haptics playback reached its terminal state."))
enum class EOpenMobileHapticTerminalReason : uint8
{
	None UMETA(Hidden),
	Completed UMETA(
		DisplayName = "Completed",
		ToolTip = "Playback reached its natural end."
	),
	Stopped UMETA(
		DisplayName = "Stopped",
		ToolTip = "Playback ended because an owner requested a graceful stop."
	),
	Cancelled UMETA(
		DisplayName = "Cancelled",
		ToolTip = "Pending or active playback was cancelled."
	),
	Interrupted UMETA(
		DisplayName = "Interrupted",
		ToolTip = "The operating system or native Haptics engine interrupted playback."
	),
	Failed UMETA(
		DisplayName = "Failed",
		ToolTip = "Playback failed after the request had been accepted."
	)
};

UENUM(BlueprintType, meta = (ToolTip = "Reason a valid Haptic request intentionally produced no output."))
enum class EOpenMobileHapticSuppressionReason : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "The request was not suppressed."),
	PlayerPolicy UMETA(DisplayName = "Player Policy", ToolTip = "The current player enable or intensity policy suppressed output."),
	Lifecycle UMETA(DisplayName = "Application Lifecycle", ToolTip = "Inactive, background, or shutdown policy suppressed output."),
	ZeroOutput UMETA(DisplayName = "Zero Output", ToolTip = "The resolved duration or intensity was zero."),
	Unavailable UMETA(DisplayName = "Unavailable", ToolTip = "No allowed output path is currently available and silence is permitted."),
	OverlapPolicy UMETA(DisplayName = "Overlap Policy", ToolTip = "The channel overlap rule intentionally ignored the new request."),
	EquivalentRequest UMETA(DisplayName = "Equivalent Request", ToolTip = "A recent equivalent UI request was coalesced."),
	ChannelMinimumInterval UMETA(DisplayName = "Channel Minimum Interval", ToolTip = "The request arrived before the channel comfort interval elapsed."),
	EffectMinimumInterval UMETA(DisplayName = "Effect Minimum Interval", ToolTip = "The effect arrived before its comfort interval elapsed."),
	ChannelWindow UMETA(DisplayName = "Channel Rate Window", ToolTip = "The channel reached its bounded one-second request window."),
	GlobalWindow UMETA(DisplayName = "Global Rate Window", ToolTip = "The Game Instance reached its bounded one-second request window."),
	InvalidClock UMETA(DisplayName = "Invalid Clock", ToolTip = "A nonfinite or invalid monotonic clock reading failed closed."),
	Other UMETA(DisplayName = "Other", ToolTip = "Another documented policy intentionally suppressed output.")
};

UENUM(BlueprintType, meta = (ToolTip = "Readiness of one configured named Haptic pattern."))
enum class EOpenMobileHapticNamedPatternStatus : uint8
{
	Unprepared UMETA(DisplayName = "Unprepared", ToolTip = "The configured pattern has not been prepared in this Game Instance."),
	Loading UMETA(DisplayName = "Loading", ToolTip = "The configured pattern is being loaded or prepared."),
	Loaded UMETA(DisplayName = "Ready", ToolTip = "The configured pattern is loaded and ready for strict named submission."),
	Missing UMETA(DisplayName = "Missing", ToolTip = "No configured pattern matches this identifier."),
	Invalid UMETA(DisplayName = "Invalid", ToolTip = "The configured pattern or one of its resources failed validation or preparation.")
};

UENUM(BlueprintType, meta = (ToolTip = "Aggregate state of configured Haptic content preparation."))
enum class EOpenMobileHapticPreparationState : uint8
{
	Unprepared UMETA(DisplayName = "Unprepared", ToolTip = "Configured content is not prepared."),
	Preparing UMETA(DisplayName = "Preparing", ToolTip = "Configured content or native resources are preparing asynchronously."),
	Prepared UMETA(DisplayName = "Prepared", ToolTip = "Configured content and required native resources are ready."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "The latest preparation attempt failed.")
};

struct OPENMOBILEHAPTICS_API FOpenMobileHapticsPreparedResourceLimits
{
	int32 MaximumCount = 32;
	int64 MaximumBytes = 4 * 1024 * 1024;
	double IdleLifetimeSeconds = 30.0;
};

UENUM(BlueprintType, meta = (ToolTip = "Lowest-quality output path an authored pattern allows."))
enum class EOpenMobileHapticFallbackFloor : uint8
{
	PortableRich UMETA(DisplayName = "Portable Rich Pattern", ToolTip = "Do not fall below the portable rich timeline."),
	PrimitiveOrPredefined UMETA(DisplayName = "Primitive Or Predefined", ToolTip = "Allow a primitive composition or native predefined effect."),
	Semantic UMETA(DisplayName = "Semantic Feedback", ToolTip = "Allow portable semantic feedback."),
	BasicVibration UMETA(DisplayName = "Basic Vibration", ToolTip = "Allow generic basic phone vibration as the final fallback.")
};

UENUM(BlueprintType, meta = (ToolTip = "Detailed outcome of an advanced Haptic control request."))
enum class EOpenMobileHapticControlOutcome : uint8
{
	Rejected UMETA(DisplayName = "Rejected", ToolTip = "The control request was invalid or failed."),
	Accepted UMETA(DisplayName = "Accepted", ToolTip = "The control request was accepted."),
	Unsupported UMETA(DisplayName = "Unsupported", ToolTip = "The resolved playback path cannot perform this control."),
	StaleHandle UMETA(DisplayName = "Stale Handle", ToolTip = "The raw handle no longer identifies active playback.")
};

UENUM(BlueprintType, meta = (ToolTip = "Compact Blueprint outcome for a Haptics playback control request."))
enum class EOpenMobileHapticControlBranch : uint8
{
	Succeeded UMETA(
		DisplayName = "Succeeded",
		ToolTip = "The control request was accepted."
	),
	Unsupported UMETA(
		DisplayName = "Unsupported",
		ToolTip = "The resolved playback path cannot perform this control operation."
	),
	Stale UMETA(
		DisplayName = "Stale",
		ToolTip = "The playback object no longer owns an active handle."
	),
	Failed UMETA(
		DisplayName = "Failed",
		ToolTip = "The control request was rejected for another reason."
	)
};

UENUM(BlueprintType, meta = (ToolTip = "Ordered portable capability tier. Use this instead of comparing availability enum ordinals."))
enum class EOpenMobileHapticCapabilityTier : uint8
{
	None UMETA(
		DisplayName = "None",
		ToolTip = "No Haptics output can currently be produced."
	),
	Basic UMETA(
		DisplayName = "Basic Vibration",
		ToolTip = "Short basic phone vibration is available."
	),
	Semantic UMETA(
		DisplayName = "Semantic Haptics",
		ToolTip = "Portable semantic selection, impact, and notification feedback is available."
	),
	Rich UMETA(
		DisplayName = "Rich Haptics",
		ToolTip = "Prepared custom pattern playback is available."
	)
};

UENUM(BlueprintType, meta = (ToolTip = "Recommended next step for a stable Haptics error."))
enum class EOpenMobileHapticRecoveryAction : uint8
{
	None UMETA(DisplayName = "No Action", ToolTip = "No recovery action is suggested."),
	CheckConfiguration UMETA(DisplayName = "Check Project Configuration", ToolTip = "Review plugin settings, packaged resources, and configured assets."),
	PrepareContent UMETA(DisplayName = "Prepare Haptic Content", ToolTip = "Run the owned Haptics preparation task before named playback."),
	RetryWhenAvailable UMETA(DisplayName = "Retry When Available", ToolTip = "Retry after temporary backend recovery or device availability changes."),
	WaitForForeground UMETA(DisplayName = "Wait For Foreground", ToolTip = "Retry after the application returns to an active foreground state."),
	UseFallback UMETA(DisplayName = "Allow A Fallback", ToolTip = "Use a less strict fallback policy if the design permits lower-quality output."),
	FixInput UMETA(DisplayName = "Fix The Request", ToolTip = "Correct an invalid name, range, timing value, asset, or option."),
	ReportNativeFailure UMETA(DisplayName = "Report Native Failure", ToolTip = "Retain sanitized diagnostics and report a repeatable native engine failure.")
};

UENUM(BlueprintType, meta = (ToolTip = "How an advanced playback control was performed."))
enum class EOpenMobileHapticControlImplementation : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No control implementation was used."),
	Native UMETA(DisplayName = "Native", ToolTip = "The selected platform performed the control directly."),
	Emulated UMETA(DisplayName = "Emulated", ToolTip = "OpenMobile emulated the control while preserving the documented contract."),
	Unsupported UMETA(DisplayName = "Unsupported", ToolTip = "The resolved playback path cannot perform this control.")
};

UENUM(BlueprintType, meta = (ToolTip = "Evidence source for a playback lifecycle event."))
enum class EOpenMobileHapticEventEvidence : uint8
{
	Estimated UMETA(DisplayName = "Estimated", ToolTip = "The platform exposes no reliable callback, so timing is estimated."),
	SchedulerConfirmed UMETA(DisplayName = "Scheduler Confirmed", ToolTip = "OpenMobile or its native scheduler confirmed the transition without claiming actuator observation."),
	NativeConfirmed UMETA(DisplayName = "Native Confirmed", ToolTip = "The native player or API explicitly reported the transition.")
};

UENUM(BlueprintType, meta = (ToolTip = "Stable Haptics error code for gameplay and recovery decisions."))
enum class EOpenMobileHapticErrorCode : uint8
{
	None UMETA(DisplayName = "No Error", ToolTip = "No Haptics error is present."),
	UnsupportedHardware UMETA(DisplayName = "Unsupported Hardware", ToolTip = "The device has no compatible Haptic actuator."),
	UnsupportedFeature UMETA(DisplayName = "Unsupported Feature", ToolTip = "The requested Haptic feature is not supported by the current path."),
	DisabledByPolicy UMETA(DisplayName = "Disabled By Policy", ToolTip = "Player or project policy disallows the request."),
	InvalidPattern UMETA(DisplayName = "Invalid Pattern", ToolTip = "The authored or prepared pattern is invalid."),
	RateLimited UMETA(DisplayName = "Rate Limited", ToolTip = "A bounded comfort rate limit rejected the advanced request."),
	ChannelBusy UMETA(DisplayName = "Channel Busy", ToolTip = "Channel capacity or overlap policy could not admit the request."),
	LifecycleRestricted UMETA(DisplayName = "Lifecycle Restricted", ToolTip = "Application lifecycle policy disallows the request."),
	NotConfigured UMETA(DisplayName = "Not Configured", ToolTip = "Required plugin settings, packaging, or content are not configured."),
	NativeEngineFailure UMETA(DisplayName = "Native Engine Failure", ToolTip = "The selected native Haptics API reported a failure."),
	Interrupted UMETA(DisplayName = "Interrupted", ToolTip = "An accepted request was interrupted by lifecycle or the native engine."),
	Cancelled UMETA(DisplayName = "Cancelled", ToolTip = "The request or preparation task was cancelled."),
	InvalidRequest UMETA(DisplayName = "Invalid Request", ToolTip = "One or more request values are invalid."),
	BackendUnavailable UMETA(DisplayName = "Backend Unavailable", ToolTip = "The selected backend is absent or temporarily recovering."),
	Internal UMETA(DisplayName = "Internal Error", ToolTip = "An internal contract failure occurred.")
};

UENUM(BlueprintType, meta = (ToolTip = "Stage where a Haptics request or task failed."))
enum class EOpenMobileHapticFailureStage : uint8
{
	None UMETA(DisplayName = "None", ToolTip = "No failure stage is present."),
	Validation UMETA(DisplayName = "Validation", ToolTip = "Input or authored-data validation failed."),
	Policy UMETA(DisplayName = "Policy", ToolTip = "Player or project policy rejected the request."),
	Capability UMETA(DisplayName = "Capability", ToolTip = "The required device capability is unavailable."),
	Channel UMETA(DisplayName = "Channel", ToolTip = "Channel admission or overlap resolution failed."),
	RateLimit UMETA(DisplayName = "Rate Limit", ToolTip = "A comfort rate limit rejected the request."),
	Lifecycle UMETA(DisplayName = "Lifecycle", ToolTip = "Application lifecycle policy rejected the request."),
	Preparation UMETA(DisplayName = "Preparation", ToolTip = "Asset loading or native resource preparation failed."),
	Compilation UMETA(DisplayName = "Compilation", ToolTip = "Portable pattern compilation or translation failed."),
	NativeSubmission UMETA(DisplayName = "Native Submission", ToolTip = "The selected platform rejected native submission."),
	Playback UMETA(DisplayName = "Playback", ToolTip = "Accepted playback later failed."),
	Interruption UMETA(DisplayName = "Interruption", ToolTip = "Lifecycle or native interruption ended the request."),
	Shutdown UMETA(DisplayName = "Shutdown", ToolTip = "The owning world or Game Instance ended.")
};

UENUM(BlueprintType, meta = (ToolTip = "Standard project channel used to create a typed Haptic channel identifier."))
enum class EOpenMobileHapticStandardChannel : uint8
{
	UI UMETA(DisplayName = "UI", ToolTip = "The standard low-latency user-interface channel."),
	Gameplay UMETA(DisplayName = "Gameplay", ToolTip = "The standard gameplay feedback channel."),
	Alerts UMETA(DisplayName = "Alerts", ToolTip = "The standard important in-app alert channel."),
	Accessibility UMETA(DisplayName = "Accessibility", ToolTip = "The standard accessibility feedback channel."),
	Cinematic UMETA(DisplayName = "Cinematic", ToolTip = "The standard cinematic presentation channel."),
	Critical UMETA(DisplayName = "Critical", ToolTip = "The standard critical alert channel, still subject to explicit policy.")
};

UENUM(BlueprintType, meta = (ToolTip = "Portable feature queried by Supports Haptic Feature."))
enum class EOpenMobileHapticFeature : uint8
{
	BasicVibration UMETA(DisplayName = "Basic Vibration", ToolTip = "Generic phone vibration output."),
	SemanticFeedback UMETA(DisplayName = "Semantic Feedback", ToolTip = "Portable selection, impact, and notification feedback."),
	RichPatterns UMETA(DisplayName = "Rich Patterns", ToolTip = "Custom prepared pattern playback."),
	AmplitudeControl UMETA(DisplayName = "Amplitude Control", ToolTip = "Per-request or per-step vibration amplitude control."),
	Looping UMETA(DisplayName = "Looping", ToolTip = "Bounded repeated Haptic pattern playback."),
	DynamicParameters UMETA(DisplayName = "Dynamic Parameters", ToolTip = "Runtime intensity or sharpness updates."),
	Scheduling UMETA(DisplayName = "Scheduling", ToolTip = "Delayed or calibrated absolute playback scheduling."),
	Pause UMETA(DisplayName = "Pause", ToolTip = "Request-scoped playback pause."),
	Resume UMETA(DisplayName = "Resume", ToolTip = "Request-scoped playback resume."),
	Seek UMETA(DisplayName = "Seek", ToolTip = "Request-scoped playback seek.")
};

UENUM(BlueprintType, meta = (ToolTip = "Outcome of a Blueprint-first Haptic clock calibration request."))
enum class EOpenMobileHapticCalibrationOutcome : uint8
{
	Calibrated UMETA(DisplayName = "Calibrated", ToolTip = "The supplied clock sample is ready for absolute scheduling."),
	ClockReset UMETA(DisplayName = "Clock Reset", ToolTip = "A discontinuity cleared the previous calibration. Supply a new stable sample."),
	Rejected UMETA(DisplayName = "Rejected", ToolTip = "The clock, time, precision, world, or subsystem is invalid.")
};

USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.MakeHapticPatternIdentifier", HasNativeBreak = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.BreakHapticPatternIdentifier"))
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPatternIdentifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Identifiers", meta = (ToolTip = "Configured stable pattern alias. Use pattern assets for ordinary literal gameplay references."))
	FName Name;

	bool IsValid() const { return !Name.IsNone(); }
};

USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.MakeHapticLibraryIdentifier", HasNativeBreak = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.BreakHapticLibraryIdentifier"))
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLibraryIdentifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Identifiers", meta = (ToolTip = "Typed configured Haptics library. It cannot connect to pattern, channel, category, or effect identifier pins."))
	FName Name;

	bool IsValid() const { return !Name.IsNone(); }
};

USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.MakeHapticChannelIdentifier", HasNativeBreak = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.BreakHapticChannelIdentifier"))
struct OPENMOBILEHAPTICS_API FOpenMobileHapticChannelIdentifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Identifiers", meta = (ToolTip = "Typed project Haptics channel. It cannot connect to category or effect identifier pins."))
	FName Name;

	bool IsValid() const { return !Name.IsNone(); }
};

USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.MakeHapticCategoryIdentifier", HasNativeBreak = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.BreakHapticCategoryIdentifier"))
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCategoryIdentifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Identifiers", meta = (ToolTip = "Typed player-policy category. It cannot connect to channel or effect identifier pins."))
	FName Name;

	bool IsValid() const { return !Name.IsNone(); }
};

USTRUCT(BlueprintType, meta = (HasNativeMake = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.MakeHapticEffectIdentifier", HasNativeBreak = "/Script/OpenMobileHaptics.OpenMobileHapticsBlueprintLibrary.BreakHapticEffectIdentifier"))
struct OPENMOBILEHAPTICS_API FOpenMobileHapticEffectIdentifier
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Identifiers", meta = (ToolTip = "Typed effect-scale key. It cannot connect to channel or category identifier pins."))
	FName Name;

	bool IsValid() const { return !Name.IsNone(); }
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Opaque raw request identifier. Prefer a Haptic Playback object in common graphs."))
	FGuid Id;

	bool IsValid() const
	{
		return Id.IsValid();
	}

	bool operator==(const FOpenMobileHapticPlaybackHandle& Other) const
	{
		return Id == Other.Id;
	}

	bool operator!=(const FOpenMobileHapticPlaybackHandle& Other) const
	{
		return !(*this == Other);
	}
};

FORCEINLINE uint32 GetTypeHash(const FOpenMobileHapticPlaybackHandle& Handle)
{
	return GetTypeHash(Handle.Id);
}

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticDynamicParameterUpdate
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "When true, the update includes the normalized Intensity value."))
	bool bUpdateIntensity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized runtime intensity applied when Update Intensity is true."))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "When true, the update includes the normalized Sharpness value."))
	bool bUpdateSharpness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized sharpness control. 0.5 is neutral, 0 is softer, and 1 is sharper."))
	float Sharpness = 0.5f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Stable Haptics-specific error code used for graph decisions."))
	EOpenMobileHapticErrorCode Code = EOpenMobileHapticErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Provider-neutral OpenMobile error code for cross-plugin handling."))
	EOpenMobileErrorCode CommonCode = EOpenMobileErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Runtime stage where the request failed or was rejected."))
	EOpenMobileHapticFailureStage Stage =
		EOpenMobileHapticFailureStage::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Short safe developer-facing description of the failure."))
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Platform-native error domain for advanced device diagnostics."))
	FString NativeDomain;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Platform-native error code for advanced device diagnostics."))
	FString NativeCode;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Configured pattern, asset, or operation item that failed, when known."))
	FName FailedItem;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Resolved playback channel associated with the failure, when known."))
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Raw playback handle associated with an accepted request that later failed."))
	FOpenMobileHapticPlaybackHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Bounded ordered representations attempted before the failure."))
	TArray<FName> FallbackAttempts;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Actionable correction or recovery hint for development UI."))
	FString Correction;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when native submission was never attempted."))
	bool bRejectedBeforeSubmission = true;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when accepted playback was interrupted after native submission."))
	bool bInterruptedAfterAcceptance = false;

	bool IsSet() const
	{
		return Code != EOpenMobileHapticErrorCode::None;
	}

	static FOpenMobileHapticError Make(
		EOpenMobileHapticErrorCode HapticCode,
		EOpenMobileErrorCode InCommonCode,
		EOpenMobileHapticFailureStage InStage,
		FString InMessage
	)
	{
		FOpenMobileHapticError Error;
		Error.Code = HapticCode;
		Error.CommonCode = InCommonCode;
		Error.Stage = InStage;
		Error.Message = MoveTemp(InMessage);
		return Error;
	}

	static FOpenMobileHapticError FromCommon(
		EOpenMobileErrorCode InCommonCode,
		FString InMessage,
		EOpenMobileHapticFailureStage InStage =
			EOpenMobileHapticFailureStage::None
	)
	{
		EOpenMobileHapticErrorCode HapticCode =
			EOpenMobileHapticErrorCode::Internal;
		switch (InCommonCode)
		{
		case EOpenMobileErrorCode::None:
			HapticCode = EOpenMobileHapticErrorCode::None;
			break;
		case EOpenMobileErrorCode::NotSupported:
			HapticCode = EOpenMobileHapticErrorCode::UnsupportedFeature;
			break;
		case EOpenMobileErrorCode::NotConfigured:
			HapticCode = EOpenMobileHapticErrorCode::NotConfigured;
			break;
		case EOpenMobileErrorCode::Unavailable:
			HapticCode = EOpenMobileHapticErrorCode::BackendUnavailable;
			break;
		case EOpenMobileErrorCode::Busy:
			HapticCode = EOpenMobileHapticErrorCode::ChannelBusy;
			break;
		case EOpenMobileErrorCode::Cancelled:
			HapticCode = EOpenMobileHapticErrorCode::Cancelled;
			break;
		case EOpenMobileErrorCode::InvalidArgument:
			HapticCode = EOpenMobileHapticErrorCode::InvalidRequest;
			break;
		case EOpenMobileErrorCode::NativeFailure:
			HapticCode = EOpenMobileHapticErrorCode::NativeEngineFailure;
			break;
		case EOpenMobileErrorCode::Internal:
			HapticCode = EOpenMobileHapticErrorCode::Internal;
			break;
		}
		return Make(HapticCode, InCommonCode, InStage, MoveTemp(InMessage));
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticNamedSupport
{
	GENERATED_BODY()

	FOpenMobileHapticNamedSupport() = default;
	FOpenMobileHapticNamedSupport(
		FName InName,
		EOpenMobileHapticSupportState InSupport
	)
		: Name(InName)
		, Support(InSupport)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Native primitive or preset name reported by the backend."))
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Tri-state support reported for the named primitive or preset."))
	EOpenMobileHapticSupportState Support =
		EOpenMobileHapticSupportState::Unknown;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticIntegerLimit
{
	GENERATED_BODY()

	FOpenMobileHapticIntegerLimit() = default;
	FOpenMobileHapticIntegerLimit(bool bInKnown, int32 InValue)
		: bKnown(bInKnown)
		, Value(InValue)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when the device reported this integer limit."))
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Reported integer limit when Known is true."))
	int32 Value = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticDurationLimit
{
	GENERATED_BODY()

	FOpenMobileHapticDurationLimit() = default;
	FOpenMobileHapticDurationLimit(bool bInKnown, double InSeconds)
		: bKnown(bInKnown)
		, Seconds(InSeconds)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when the device reported this duration limit."))
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Reported duration in seconds when Known is true."))
	double Seconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticFrequencyRange
{
	GENERATED_BODY()

	FOpenMobileHapticFrequencyRange() = default;
	FOpenMobileHapticFrequencyRange(
		bool bInKnown,
		float InMinimumHertz,
		float InMaximumHertz
	)
		: bKnown(bInKnown)
		, MinimumHertz(InMinimumHertz)
		, MaximumHertz(InMaximumHertz)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when the device reported a frequency range."))
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "Hz", ToolTip = "Minimum supported frequency in hertz when Known is true."))
	float MinimumHertz = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "Hz", ToolTip = "Maximum supported frequency in hertz when Known is true."))
	float MaximumHertz = 0.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCapabilities
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Current categorical availability. Do not compare its numeric ordinal."))
	EOpenMobileHapticAvailability Availability =
		EOpenMobileHapticAvailability::UnsupportedPlatform;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for basic bounded phone vibration."))
	EOpenMobileHapticSupportState BasicVibration =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for portable semantic selection, impact, and notification feedback."))
	EOpenMobileHapticSupportState SemanticFeedback =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for prepared rich pattern playback."))
	EOpenMobileHapticSupportState RichHaptics =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for request or waveform amplitude control."))
	EOpenMobileHapticSupportState AmplitudeControl =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for the backend semantic-effect family."))
	EOpenMobileHapticSupportState SemanticEffects =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for native predefined effect identifiers."))
	EOpenMobileHapticSupportState PredefinedEffects =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for authored waveform timing steps."))
	EOpenMobileHapticSupportState WaveformTiming =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for bounded native or emulated looping."))
	EOpenMobileHapticSupportState Looping =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for native primitive composition."))
	EOpenMobileHapticSupportState Primitives =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for amplitude or waveform envelope playback."))
	EOpenMobileHapticSupportState Envelopes =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for frequency control or frequency intent."))
	EOpenMobileHapticSupportState FrequencyControl =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for short transient pattern events."))
	EOpenMobileHapticSupportState TransientEvents =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for duration-bearing continuous pattern events."))
	EOpenMobileHapticSupportState ContinuousEvents =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for runtime intensity or sharpness updates."))
	EOpenMobileHapticSupportState DynamicParameters =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for audio events embedded in rich Haptic content."))
	EOpenMobileHapticSupportState AudioEvents =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for Apple Haptic and Audio Pattern content."))
	EOpenMobileHapticSupportState AHAP =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for delayed or calibrated absolute scheduling."))
	EOpenMobileHapticSupportState Scheduling =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for simultaneous mixed playback."))
	EOpenMobileHapticSupportState Mixing =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for permitted Critical background alerts."))
	EOpenMobileHapticSupportState BackgroundAlerts =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for pausing active playback."))
	EOpenMobileHapticSupportState Pause =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for resuming paused playback."))
	EOpenMobileHapticSupportState Resume =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Support for seeking within active playback."))
	EOpenMobileHapticSupportState Seek =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Per-name support states for native primitive identifiers."))
	TArray<FOpenMobileHapticNamedSupport> PrimitiveSupport;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Per-name support states for native preset identifiers."))
	TArray<FOpenMobileHapticNamedSupport> PresetSupport;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Maximum native or portable event count when the device reports it."))
	FOpenMobileHapticIntegerLimit MaximumEventCount;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Maximum parameter control-point count when the device reports it."))
	FOpenMobileHapticIntegerLimit MaximumControlPointCount;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Maximum supported pattern duration when the device reports it."))
	FOpenMobileHapticDurationLimit MaximumDurationSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Maximum native request queue depth when the device reports it."))
	FOpenMobileHapticIntegerLimit MaximumQueueDepth;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Minimum native timing interval when the device reports it."))
	FOpenMobileHapticDurationLimit MinimumTimingGranularitySeconds;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Maximum control-point timeline duration when the device reports it."))
	FOpenMobileHapticDurationLimit MaximumControlPointDurationSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Supported frequency range when the device reports it."))
	FOpenMobileHapticFrequencyRange FrequencyRange;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Selected provider-neutral backend name."))
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Bounded backend capability detail intended for development diagnostics."))
	FString Detail;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPatternEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ToolTip = "Transient or continuous portable event type."))
	EOpenMobileHapticPatternEventType Type =
		EOpenMobileHapticPatternEventType::Transient;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Event start time in seconds from pattern start."))
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Continuous event duration in seconds. Transient events use zero."))
	double DurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized event strength from zero through one."))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized tactile character from soft through sharp."))
	float Sharpness = 0.5f;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized low-to-high frequency intent used by capable backends."))
	float FrequencyIntent = 0.5f;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCurvePoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Point time in seconds relative to the parameter curve start."))
	double RelativeTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized control value. Intensity 1.0 and sharpness 0.5 are neutral."))
	float Value = 1.0f;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticParameterCurve
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ToolTip = "Pattern parameter controlled by this curve."))
	EOpenMobileHapticCurveParameter Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Curve start time in seconds from pattern start."))
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ToolTip = "Ordered normalized control points. Intensity 1.0 and sharpness 0.5 are neutral."))
	TArray<FOpenMobileHapticCurvePoint> ControlPoints;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ToolTip = "Portable Haptic event sequence authored inside a Pattern asset."))
	TArray<FOpenMobileHapticPatternEvent> Events;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ToolTip = "Pattern-wide intensity and sharpness control curves."))
	TArray<FOpenMobileHapticParameterCurve> ParameterCurves;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticSchedule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Immediate, relative, game-clock, or audio-clock scheduling mode."))
	EOpenMobileHapticScheduleMode Mode =
		EOpenMobileHapticScheduleMode::Immediate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Delay for Relative mode or absolute clock time for Game and Audio modes. Immediate mode uses zero."))
	double TimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Signed latency compensation in seconds, bounded by the runtime timing policy."))
	double LatencyOffsetSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticTimingAnchor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "External clock represented by this calibration anchor."))
	EOpenMobileHapticTimingClock Clock = EOpenMobileHapticTimingClock::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Sampled external clock time in seconds."))
	double ClockTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Platform monotonic time captured with the external clock sample."))
	double PlatformMonotonicTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Estimated clock-sample accuracy in seconds."))
	double EstimatedPrecisionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Monotonic calibration revision used to reject stale schedules."))
	int64 CalibrationRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Application lifecycle generation in which this anchor was captured."))
	int64 LifecycleGeneration = 0;

	bool IsValid() const
	{
		return Clock != EOpenMobileHapticTimingClock::None
			&& CalibrationRevision > 0
			&& LifecycleGeneration > 0;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticTimingCalibrationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when the timing sample was accepted for absolute scheduling."))
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Typed calibration acceptance or discontinuity status."))
	EOpenMobileHapticTimingCalibrationStatus Status =
		EOpenMobileHapticTimingCalibrationStatus::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Accepted timing anchor. Invalid when calibration was rejected."))
	FOpenMobileHapticTimingAnchor Anchor;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Plain-language calibration failure or clock-discontinuity reason."))
	FString Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticSynchronizationDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Synchronization quality requested from the active backend."))
	EOpenMobileHapticSynchronizationMode Mode =
		EOpenMobileHapticSynchronizationMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Source clock used to resolve this request."))
	EOpenMobileHapticTimingClock Clock = EOpenMobileHapticTimingClock::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Original relative delay or absolute clock target in seconds."))
	double RequestedTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Resolved platform monotonic target in seconds."))
	double ResolvedPlatformTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Estimated scheduling accuracy in seconds."))
	double EstimatedPrecisionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Seconds late at resolution time. Zero means the target was not late."))
	double LatenessSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Calibration revision used to resolve the schedule."))
	int64 CalibrationRevision = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLoopOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Enables bounded repeat behavior for this request."))
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ClampMin = "0", ToolTip = "Repeats after the first play. Zero means repeat until stopped within Maximum Duration."))
	int32 RepeatCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Timeline position in seconds used when each repeat begins."))
	double RepeatStartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Safety duration in seconds for all loop modes."))
	double MaximumDurationSeconds = 30.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Resolved project channel. Use purpose-built option constructors in common graphs."))
	FName Channel = TEXT("Gameplay");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Player-policy category. Empty defers through effect, asset, and project defaults."))
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Request priority combined with channel policy."))
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Behavior when this request overlaps active work on the same channel."))
	EOpenMobileHapticOverlapPolicy OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Allowed fallback behavior when the requested representation is unsupported."))
	EOpenMobileHapticFallbackPolicy FallbackPolicy =
		EOpenMobileHapticFallbackPolicy::Automatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Immediate, relative, or calibrated absolute playback schedule."))
	FOpenMobileHapticSchedule Schedule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Bounded loop configuration for this request."))
	FOpenMobileHapticLoopOptions Loop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Playback behavior after an application or audio interruption."))
	EOpenMobileHapticInterruptionPolicy InterruptionPolicy =
		EOpenMobileHapticInterruptionPolicy::Stop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Additional normalized request scale multiplied with project and player policy."))
	float IntensityScale = 1.0f;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticSemanticRequest
{
	GENERATED_BODY()

	UPROPERTY(meta = (ToolTip = "Portable semantic effect selected by the native C++ request."))
	EOpenMobileHapticSemanticEffect Effect =
		EOpenMobileHapticSemanticEffect::Selection;

	UPROPERTY(meta = (ToolTip = "Normalized C++ request intensity from zero through one."))
	float Intensity = 1.0f;

	UPROPERTY(meta = (ToolTip = "Advanced playback policy for the native C++ request."))
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticOneShotRequest
{
	GENERATED_BODY()

	UPROPERTY(meta = (ToolTip = "Requested one-shot duration in seconds."))
	float DurationSeconds = 0.05f;

	UPROPERTY(meta = (ToolTip = "Normalized C++ request intensity from zero through one."))
	float Intensity = 1.0f;

	UPROPERTY(meta = (ToolTip = "Advanced playback policy for the native C++ request."))
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticNamedPatternRequest
{
	GENERATED_BODY()

	UPROPERTY(meta = (ToolTip = "Prepared configured pattern alias used by the native C++ request."))
	FName PatternName;

	UPROPERTY(meta = (ToolTip = "Resolved portable pattern asset path after configured lookup."))
	FSoftObjectPath PatternAsset;

	UPROPERTY(meta = (ToolTip = "Resolved platform override asset path when one is selected."))
	FSoftObjectPath PlatformOverrideAsset;

	UPROPERTY(meta = (ToolTip = "Normalized C++ request intensity from zero through one."))
	float Intensity = 1.0f;

	UPROPERTY(meta = (ToolTip = "Advanced playback policy for the native C++ request."))
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLibraryPreloadHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Opaque raw preparation identifier. Prefer an owned preparation task in common graphs."))
	FGuid Id;

	bool IsValid() const { return Id.IsValid(); }

	friend bool operator==(
		const FOpenMobileHapticLibraryPreloadHandle& Left,
		const FOpenMobileHapticLibraryPreloadHandle& Right
	)
	{
		return Left.Id == Right.Id;
	}
};

UENUM(BlueprintType, meta = (ToolTip = "Terminal result of the advanced raw-handle library preload contract."))
enum class EOpenMobileHapticLibraryPreloadOutcome : uint8
{
	Prepared UMETA(DisplayName = "Prepared", ToolTip = "Configured libraries and required native resources are ready."),
	Cancelled UMETA(DisplayName = "Cancelled", ToolTip = "The matching advanced preload was cancelled."),
	Failed UMETA(DisplayName = "Failed", ToolTip = "Library loading, pattern validation, or native preparation failed.")
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLibraryPreloadResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Raw preload handle that produced this advanced compatibility result."))
	FOpenMobileHapticLibraryPreloadHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Prepared, cancelled, or failed terminal preload outcome."))
	EOpenMobileHapticLibraryPreloadOutcome Outcome =
		EOpenMobileHapticLibraryPreloadOutcome::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Number of configured named patterns prepared by this preload."))
	int32 PreparedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Legacy per-item error strings. Prefer typed preparation task errors."))
	TArray<FString> Errors;
};

UENUM(BlueprintType, meta = (ToolTip = "Terminal outcome for an owned Haptics preparation task."))
enum class EOpenMobileHapticPreparationOutcome : uint8
{
	Ready UMETA(
		DisplayName = "Ready",
		ToolTip = "Configured Haptics content and native resources are prepared."
	),
	Cancelled UMETA(
		DisplayName = "Cancelled",
		ToolTip = "This caller stopped waiting. Other preparation owners remain valid."
	),
	Failed UMETA(
		DisplayName = "Failed",
		ToolTip = "Preparation could not start or did not complete."
	)
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPreparationResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Prepare", meta = (ToolTip = "Terminal preparation outcome."))
	EOpenMobileHapticPreparationOutcome Outcome =
		EOpenMobileHapticPreparationOutcome::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Prepare", meta = (ToolTip = "Owned preparation claim returned only for Ready. Store it for as long as this feature needs prepared content."))
	TObjectPtr<UOpenMobileHapticPreparationLease> Lease = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Prepare", meta = (ToolTip = "Number of configured named patterns that are ready."))
	int32 PreparedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Prepare", meta = (ToolTip = "Primary typed failure. None for successful preparation."))
	FOpenMobileHapticError Error;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Prepare", meta = (ToolTip = "Typed per-item failures when more than one configured resource failed."))
	TArray<FOpenMobileHapticError> ItemErrors;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticDurationDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Duration requested before platform and project constraints."))
	double RequestedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Portable duration after project validation and clamping."))
	double ResolvedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when the active backend reported a native duration."))
	bool bNativeDurationKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Native duration in seconds when Known is true."))
	double NativeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when the backend clamped the resolved duration."))
	bool bNativeClamped = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticIntensityDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Normalized intensity requested before policy and backend scaling."))
	float Requested = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Normalized intensity after project, asset, and player policy."))
	float Resolved = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when the active backend reported its applied intensity."))
	bool bNativeIntensityKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Normalized native intensity when Known is true."))
	float Native = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when the backend clamped the resolved intensity."))
	bool bNativeClamped = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Immediate accepted, fallback, suppressed, or rejected outcome."))
	EOpenMobileHapticPlaybackOutcome Outcome =
		EOpenMobileHapticPlaybackOutcome::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Playback state at the moment this result was returned."))
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Reason for intentional silence when Outcome is Suppressed."))
	EOpenMobileHapticSuppressionReason SuppressionReason =
		EOpenMobileHapticSuppressionReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Raw playback handle for accepted controllable requests."))
	FOpenMobileHapticPlaybackHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Typed error for rejected or failed requests."))
	FOpenMobileHapticError Error;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Resolved channel used for policy, overlap, and diagnostics."))
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Representation selected for playback, such as native semantic, portable rich, or basic fallback."))
	FName ResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Bounded ordered representation attempts made during fallback resolution."))
	TArray<FName> FallbackAttempts;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Requested, resolved, and native duration diagnostics."))
	FOpenMobileHapticDurationDiagnostics Duration;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Requested, policy-resolved, and native intensity diagnostics."))
	FOpenMobileHapticIntensityDiagnostics Intensity;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Timing resolution, calibration, precision, and lateness diagnostics."))
	FOpenMobileHapticSynchronizationDiagnostics Synchronization;

	bool IsAccepted() const
	{
		return Outcome == EOpenMobileHapticPlaybackOutcome::Accepted
			|| Outcome == EOpenMobileHapticPlaybackOutcome::Fallback;
	}

	static FOpenMobileHapticPlaybackResult MakeRejected(
		EOpenMobileErrorCode ErrorCode,
		FString Message
	)
	{
		FOpenMobileHapticPlaybackResult Result;
		Result.State = EOpenMobileHapticPlaybackState::Failed;
		Result.Error = FOpenMobileHapticError::FromCommon(
			ErrorCode,
			MoveTemp(Message)
		);
		return Result;
	}

	static FOpenMobileHapticPlaybackResult MakeRejected(
		FOpenMobileHapticError Error
	)
	{
		FOpenMobileHapticPlaybackResult Result;
		Result.State = EOpenMobileHapticPlaybackState::Failed;
		Result.Error = MoveTemp(Error);
		return Result;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticControlResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Accepted, no-op, or rejected control outcome."))
	EOpenMobileHapticControlOutcome Outcome =
		EOpenMobileHapticControlOutcome::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Native, emulated, or unavailable implementation used for this control."))
	EOpenMobileHapticControlImplementation Implementation =
		EOpenMobileHapticControlImplementation::None;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Playback state after the control operation."))
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Seek position requested in seconds. Zero for non-seek controls."))
	double RequestedPositionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Seek position applied after validation or quantization."))
	double ResolvedPositionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Native or emulated seek granularity in seconds."))
	double PositionGranularitySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Completed repeat count reported with repeat-aware controls."))
	int32 CompletedRepeatCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Monotonic revision assigned to successful control changes."))
	int64 ControlRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when a seek or parameter value was quantized."))
	bool bQuantized = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Typed validation, stale-handle, or backend control error."))
	FOpenMobileHapticError Error;

	static FOpenMobileHapticControlResult MakeRejected(
		EOpenMobileErrorCode ErrorCode,
		FString Message
	)
	{
		FOpenMobileHapticControlResult Result;
		Result.Error = FOpenMobileHapticError::FromCommon(
			ErrorCode,
			MoveTemp(Message)
		);
		return Result;
	}

	static FOpenMobileHapticControlResult MakeRejected(
		FOpenMobileHapticError Error
	)
	{
		FOpenMobileHapticControlResult Result;
		Result.Error = MoveTemp(Error);
		return Result;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Raw handle whose lifecycle changed."))
	FOpenMobileHapticPlaybackHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Original handle when this event belongs to recovered playback."))
	FOpenMobileHapticPlaybackHandle RecoverySourceHandle;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "New request-scoped playback state."))
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Whether the state is backend-confirmed or runtime-estimated."))
	EOpenMobileHapticEventEvidence Evidence =
		EOpenMobileHapticEventEvidence::Estimated;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "s", ToolTip = "Platform monotonic event time in seconds."))
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Stable pattern, preset, or semantic effect identity."))
	FName PatternOrEffect;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Resolved playback channel."))
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Representation selected for this playback."))
	FName ResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Typed terminal or lifecycle error when present."))
	FOpenMobileHapticError Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticUserPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Per-player Haptics switch for the current Game Instance. Persistence is game-owned."))
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Allows Critical-priority Alerts or Accessibility feedback while the global Haptics switch is off."))
	bool bAllowCriticalFeedbackWhenDisabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized per-player master intensity from zero through one."))
	float MasterIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Typed category keys mapped to normalized per-player scales. Prefer granular policy nodes."))
	TMap<FName, float> CategoryScales;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Typed effect keys mapped to normalized per-player scales. Prefer granular policy nodes."))
	TMap<FName, float> EffectScales;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticsPerformanceDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Requests rejected, suppressed, expired, or dropped before native playback."))
	int64 DroppedRequestCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Highest observed queued playback count."))
	int32 PeakQueuedPlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Resolved timeline cache hits since subsystem initialization."))
	int64 TimelineCacheHitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Resolved timeline cache misses since subsystem initialization."))
	int64 TimelineCacheMissCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Resolved timeline entries evicted by count or memory limits."))
	int64 TimelineCacheEvictionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Current resolved timeline cache entry count."))
	int32 TimelineCacheEntryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "B", ToolTip = "Approximate bytes used by resolved timeline cache entries."))
	int64 TimelineCacheMemoryBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Configured maximum resolved timeline entry count."))
	int32 TimelineCacheMaximumEntryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "B", ToolTip = "Configured approximate memory limit for resolved timelines."))
	int64 TimelineCacheMaximumMemoryBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Completed preparation attempts since subsystem initialization."))
	int64 PreparationCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "ms", ToolTip = "Most recent preparation latency in milliseconds."))
	double LastPreparationLatencyMilliseconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "ms", ToolTip = "Highest observed preparation latency in milliseconds."))
	double MaximumPreparationLatencyMilliseconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Requests submitted to a native backend."))
	int64 NativeSubmissionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "ms", ToolTip = "Most recent native submission latency in milliseconds."))
	double LastNativeSubmissionLatencyMilliseconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (Units = "ms", ToolTip = "Highest observed native submission latency in milliseconds."))
	double MaximumNativeSubmissionLatencyMilliseconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticChannelDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Resolved channel represented by this diagnostic row."))
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Active playback count for this channel."))
	int32 ActivePlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Queued playback count for this channel."))
	int32 QueuedPlaybackCount = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticHandleDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Snapshot-local stable ordinal that does not expose the raw GUID."))
	int32 Ordinal = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Current state for this active or queued request."))
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Resolved channel for this request."))
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Stable pattern, preset, or semantic effect identity."))
	FName PatternOrEffect;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Representation selected for this request."))
	FName ResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when the request is waiting rather than actively playing."))
	bool bQueued = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticsDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Full capability snapshot captured with these diagnostics."))
	FOpenMobileHapticCapabilities Capabilities;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Current active playback count."))
	int32 ActivePlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Current queued playback count."))
	int32 QueuedPlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Sanitized application lifecycle state."))
	FName ApplicationState = TEXT("Active");

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True while the backend is attempting recovery."))
	bool bBackendRecovering = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True while backend shutdown blocks new submissions."))
	bool bBackendShuttingDown = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Accepted requests that used a lower-quality fallback path."))
	int64 FallbackPlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Bounded runtime performance counters and latency observations."))
	FOpenMobileHapticsPerformanceDiagnostics Performance;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Most recent typed runtime error."))
	FOpenMobileHapticError LastError;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Most recent duration resolution diagnostics."))
	FOpenMobileHapticDurationDiagnostics LastDuration;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Most recent intensity resolution diagnostics."))
	FOpenMobileHapticIntensityDiagnostics LastIntensity;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Last configured pattern alias looked up by the runtime."))
	FName LastNamedPattern;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Readiness status from the most recent configured pattern lookup."))
	EOpenMobileHapticNamedPatternStatus LastNamedPatternStatus =
		EOpenMobileHapticNamedPatternStatus::Unprepared;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Current number of prepared configured pattern aliases."))
	int32 PreparedNamedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Current aggregate configured-content and backend preparation state."))
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Representation selected by the most recent playback request."))
	FName LastResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Bounded fallback attempts from the most recent request."))
	TArray<FName> LastFallbackAttempts;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Bounded recent playback lifecycle events."))
	TArray<FOpenMobileHapticPlaybackEvent> RecentPlaybackEvents;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Per-channel active and queued counts."))
	TArray<FOpenMobileHapticChannelDiagnostics> Channels;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "Sanitized active-handle rows using snapshot-local ordinals."))
	TArray<FOpenMobileHapticHandleDiagnostics> ActiveHandles;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Haptics|Advanced", meta = (ToolTip = "True when bounded arrays omitted older diagnostic entries."))
	bool bTruncated = false;
};
