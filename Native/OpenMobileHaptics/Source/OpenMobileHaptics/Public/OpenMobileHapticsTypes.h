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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	bool bUpdateIntensity = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	bool bUpdateSharpness = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized sharpness control. 0.5 is neutral, 0 is softer, and 1 is sharper."))
	float Sharpness = 0.5f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticErrorCode Code = EOpenMobileHapticErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileErrorCode CommonCode = EOpenMobileErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticFailureStage Stage =
		EOpenMobileHapticFailureStage::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString Message;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString NativeDomain;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString NativeCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName FailedItem;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FName> FallbackAttempts;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString Correction;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bRejectedBeforeSubmission = true;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (Units = "Hz"))
	float MinimumHertz = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics", meta = (Units = "Hz"))
	float MaximumHertz = 0.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCapabilities
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticAvailability Availability =
		EOpenMobileHapticAvailability::UnsupportedPlatform;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState BasicVibration =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState SemanticFeedback =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState RichHaptics =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState AmplitudeControl =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState SemanticEffects =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState PredefinedEffects =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState WaveformTiming =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Looping =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Primitives =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Envelopes =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState FrequencyControl =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState TransientEvents =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState ContinuousEvents =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState DynamicParameters =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState AudioEvents =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState AHAP =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Scheduling =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Mixing =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState BackgroundAlerts =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Pause =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Resume =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSupportState Seek =
		EOpenMobileHapticSupportState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticNamedSupport> PrimitiveSupport;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticNamedSupport> PresetSupport;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntegerLimit MaximumEventCount;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntegerLimit MaximumControlPointCount;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationLimit MaximumDurationSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntegerLimit MaximumQueueDepth;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationLimit MinimumTimingGranularitySeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationLimit MaximumControlPointDurationSeconds;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticFrequencyRange FrequencyRange;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName BackendName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString Detail;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPatternEvent
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPatternEventType Type =
		EOpenMobileHapticPatternEventType::Transient;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	double DurationSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Sharpness = 0.5f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float FrequencyIntent = 0.5f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCurvePoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", Units = "s"))
	double RelativeTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Value = 1.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticParameterCurve
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticCurveParameter Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", Units = "s"))
	double StartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ToolTip = "Pattern-wide normalized control points. Intensity 1.0 and sharpness 0.5 are neutral."))
	TArray<FOpenMobileHapticCurvePoint> ControlPoints;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPattern
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticPatternEvent> Events;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticParameterCurve> ParameterCurves;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticSchedule
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticScheduleMode Mode =
		EOpenMobileHapticScheduleMode::Immediate;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	double TimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	double LatencyOffsetSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticTimingAnchor
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticTimingClock Clock = EOpenMobileHapticTimingClock::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double ClockTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double PlatformMonotonicTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double EstimatedPrecisionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 CalibrationRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bAccepted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticTimingCalibrationStatus Status =
		EOpenMobileHapticTimingCalibrationStatus::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticTimingAnchor Anchor;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FString Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticSynchronizationDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSynchronizationMode Mode =
		EOpenMobileHapticSynchronizationMode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticTimingClock Clock = EOpenMobileHapticTimingClock::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double RequestedTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double ResolvedPlatformTimeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double EstimatedPrecisionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double LatenessSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 CalibrationRevision = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLoopOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	bool bLoop = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0"))
	int32 RepeatCount = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	double RepeatStartTimeSeconds = 0.0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	double MaximumDurationSeconds = 30.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FName Channel = TEXT("Gameplay");

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FName Category;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticOverlapPolicy OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticFallbackPolicy FallbackPolicy =
		EOpenMobileHapticFallbackPolicy::Automatic;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticSchedule Schedule;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticLoopOptions Loop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticInterruptionPolicy InterruptionPolicy =
		EOpenMobileHapticInterruptionPolicy::Stop;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float IntensityScale = 1.0f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticSemanticRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSemanticEffect Effect =
		EOpenMobileHapticSemanticEffect::Selection;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticOneShotRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0"))
	float DurationSeconds = 0.05f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticNamedPatternRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FName PatternName;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FSoftObjectPath PatternAsset;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FSoftObjectPath PlatformOverrideAsset;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Intensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackOptions Options;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLibraryPreloadHandle
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticLibraryPreloadHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticLibraryPreloadOutcome Outcome =
		EOpenMobileHapticLibraryPreloadOutcome::Failed;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 PreparedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double RequestedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double ResolvedSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bNativeDurationKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double NativeSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bNativeClamped = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticIntensityDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	float Requested = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	float Resolved = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bNativeIntensityKnown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	float Native = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bNativeClamped = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPlaybackResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPlaybackOutcome Outcome =
		EOpenMobileHapticPlaybackOutcome::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticSuppressionReason SuppressionReason =
		EOpenMobileHapticSuppressionReason::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticError Error;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName ResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FName> FallbackAttempts;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationDiagnostics Duration;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntensityDiagnostics Intensity;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticControlOutcome Outcome =
		EOpenMobileHapticControlOutcome::Rejected;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticControlImplementation Implementation =
		EOpenMobileHapticControlImplementation::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double RequestedPositionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double ResolvedPositionSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double PositionGranularitySeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 CompletedRepeatCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 ControlRevision = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bQuantized = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackHandle Handle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticPlaybackHandle RecoverySourceHandle;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticEventEvidence Evidence =
		EOpenMobileHapticEventEvidence::Estimated;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double TimestampSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName PatternOrEffect;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName ResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticError Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticUserPolicy
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	bool bEnabled = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ToolTip = "Allows Critical-priority Alerts or Accessibility feedback while the global Haptics switch is off."))
	bool bAllowCriticalFeedbackWhenDisabled = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MasterIntensity = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	TMap<FName, float> CategoryScales;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Haptics")
	TMap<FName, float> EffectScales;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticsPerformanceDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 DroppedRequestCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 PeakQueuedPlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 TimelineCacheHitCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 TimelineCacheMissCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 TimelineCacheEvictionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 TimelineCacheEntryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 TimelineCacheMemoryBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 TimelineCacheMaximumEntryCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 TimelineCacheMaximumMemoryBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 PreparationCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double LastPreparationLatencyMilliseconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double MaximumPreparationLatencyMilliseconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 NativeSubmissionCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double LastNativeSubmissionLatencyMilliseconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	double MaximumNativeSubmissionLatencyMilliseconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticChannelDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 ActivePlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 QueuedPlaybackCount = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticHandleDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 Ordinal = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPlaybackState State =
		EOpenMobileHapticPlaybackState::Invalid;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName Channel;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName PatternOrEffect;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName ResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bQueued = false;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticsDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticCapabilities Capabilities;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 ActivePlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 QueuedPlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName ApplicationState = TEXT("Active");

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bBackendRecovering = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bBackendShuttingDown = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int64 FallbackPlaybackCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticsPerformanceDiagnostics Performance;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticError LastError;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticDurationDiagnostics LastDuration;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FOpenMobileHapticIntensityDiagnostics LastIntensity;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName LastNamedPattern;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticNamedPatternStatus LastNamedPatternStatus =
		EOpenMobileHapticNamedPatternStatus::Unprepared;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	int32 PreparedNamedPatternCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	FName LastResolvedPath;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FName> LastFallbackAttempts;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticPlaybackEvent> RecentPlaybackEvents;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticChannelDiagnostics> Channels;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticHandleDiagnostics> ActiveHandles;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Haptics")
	bool bTruncated = false;
};
