#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsNative.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "OpenMobileHapticsSubsystem.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticPlaybackEventDynamic,
	const FOpenMobileHapticPlaybackEvent&,
	Event
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileHapticLibraryPreloadEventDynamic,
	const FOpenMobileHapticLibraryPreloadResult&,
	Result
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOpenMobileHapticPreparationStateChangedDynamic,
	EOpenMobileHapticPreparationState,
	PreviousState,
	EOpenMobileHapticPreparationState,
	NewState,
	const FString&,
	Reason,
	bool,
	bPreparedAssetsRemainLoaded
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileHapticPolicyChangedDynamic,
	const FOpenMobileHapticUserPolicy&,
	PreviousPolicy,
	const FOpenMobileHapticUserPolicy&,
	NewPolicy
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileHapticAvailabilityChangedDynamic,
	EOpenMobileHapticAvailability,
	PreviousAvailability,
	EOpenMobileHapticAvailability,
	NewAvailability
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileHapticCapabilitiesChangedDynamic,
	const FOpenMobileHapticCapabilities&,
	PreviousCapabilities,
	const FOpenMobileHapticCapabilities&,
	NewCapabilities
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileHapticMasterIntensityChangedDynamic,
	float,
	PreviousIntensity,
	float,
	NewIntensity
);

class UOpenMobileHapticLibrary;
class UOpenMobileHapticNamedPlaybackAsyncAction;
class UOpenMobileHapticPatternAsset;
class UOpenMobileHapticPreparationAsyncAction;
class UOpenMobileHapticPreparationLease;
class UOpenMobileHapticPlayback;
class UOpenMobileHapticPatternPlaybackAsyncAction;
class UOpenMobileHapticPlaybackAsyncAction;
struct FOpenMobileHapticsBackendCallback;
struct FOpenMobileHapticsBackendRequestToken;
struct FOpenMobileHapticsSubsystemState;
struct FOpenMobileHapticsInterruption;
struct FOpenMobileHapticsLifecycleTransition;
struct FOpenMobileHapticsTimingResolution;
struct FOpenMobileHapticsResolvedChannel;

struct FOpenMobileHapticsSubsystemStateDeleter
{
	/** Keeps the large private state opaque in this header while still deleting it in the module that knows its full type. */
	void operator()(FOpenMobileHapticsSubsystemState* State) const;
};

/**
 * You get one of these for each Game Instance, and it owns that instance's policy, prepared content, and playback handles. Call it on the game thread only, events come back there also.
 */
UCLASS(meta = (DisplayName = "Open Mobile Haptics"))
class OPENMOBILEHAPTICS_API UOpenMobileHapticsSubsystem final :
	public UGameInstanceSubsystem,
	public IOpenMobileHaptics
{
	GENERATED_BODY()

public:
	/** Owns an out-of-line destructor because the private state is intentionally incomplete in this public header. */
	virtual ~UOpenMobileHapticsSubsystem() override;
	/** Registers the Game Instance scoped backend, policy, and lifecycle bindings before requests can arrive. */
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	/** Stops owned work and unbinds engine events before the Game Instance releases the subsystem. */
	virtual void Deinitialize() override;

	/** Returns the latest capability snapshot with runtime policy and availability already applied. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptic Capabilities (Advanced)", ToolTip = "Returns the full side-effect-free capability snapshot. Prefer small capability nodes in common graphs."))
	FOpenMobileHapticCapabilities GetHapticCapabilities() const;

	/** Uses the portable selection effect and UI defaults, so callers don't have to choose platform feedback names. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Selection Haptic for direct outcome branches and a playback object.", DisplayName = "Play Selection Feedback (Legacy)", ToolTip = "Legacy broad-result API. Plays low-latency selection feedback."))
	FOpenMobileHapticPlaybackResult PlaySelectionFeedback(
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	/** Maps a stable impact style to portable semantics while preserving the caller's intensity and options. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Impact Haptic for direct outcome branches and a playback object.", DisplayName = "Play Impact Feedback (Legacy)", ToolTip = "Legacy broad-result API for portable impact feedback."))
	FOpenMobileHapticPlaybackResult PlayImpactFeedback(
		EOpenMobileHapticImpactStyle Style,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	/** Sends success, warning, or error feedback on the expected Alerts path without creating an OS notification. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Notification Haptic for direct outcome branches and a playback object.", DisplayName = "Play Notification Feedback (Legacy)", ToolTip = "Legacy broad-result API for portable in-app notification feedback."))
	FOpenMobileHapticPlaybackResult PlayNotificationFeedback(
		EOpenMobileHapticNotificationType Type,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	/** Prefers a prepared project override for the preset and falls back to its portable semantic meaning. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Game Haptic for direct outcome branches and a playback object.", DisplayName = "Play Game Feedback (Legacy)", ToolTip = "Legacy broad-result API for stable game presets."))
	FOpenMobileHapticPlaybackResult PlayGameFeedback(
		EOpenMobileHapticGamePreset Preset,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	/** Lets C++ callers override channel and policy while keeping the preset's prepared-content resolution. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (AdvancedDisplay = "Options", DisplayName = "Play Game Feedback (Advanced)", ToolTip = "Plays a game preset with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult PlayGameFeedbackAdvanced(
		EOpenMobileHapticGamePreset Preset,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	/** Submits a portable semantic effect with common defaults and returns suppression instead of inventing success for silence. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Play Semantic Feedback (Advanced)", ToolTip = "Requests the broad semantic enum directly. Prefer selection, impact, notification, or game Haptic nodes for their distinct channel and override behavior."))
	FOpenMobileHapticPlaybackResult PlaySemanticFeedback(
		EOpenMobileHapticSemanticEffect Effect,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	/** Accepts full options for native callers that need scheduling, looping, or explicit overlap policy. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (AdvancedDisplay = "Options", DisplayName = "Play Semantic Feedback With Options (Advanced)", ToolTip = "Requests semantic feedback with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult PlaySemanticFeedbackAdvanced(
		EOpenMobileHapticSemanticEffect Effect,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	/** Requests one bounded pulse with common Gameplay defaults when semantic meaning isn't needed. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Vibrate Phone for direct outcome branches and a playback object.", DisplayName = "Vibrate (Legacy)", ToolTip = "Legacy broad-result API for one bounded phone vibration."))
	FOpenMobileHapticPlaybackResult Vibrate(
		float DurationSeconds = 0.05f,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	/** Applies full request policy to a one-shot pulse and reports every duration or intensity clamp. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (AdvancedDisplay = "Options", DisplayName = "Vibrate (Advanced)", ToolTip = "Requests one-shot phone vibration with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult VibrateAdvanced(
		float DurationSeconds,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	/** Plays an already prepared alias with common options, it won't synchronously load a library. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Named Haptic with a typed identifier, or Play Haptic Pattern Asset.", DisplayName = "Play Named Haptic Pattern (Legacy)", ToolTip = "Legacy raw-name broad-result API. The named pattern must already be prepared."))
	FOpenMobileHapticPlaybackResult PlayNamedPattern(
		FName PatternName,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	/** Submits prepared named content with explicit options while keeping lookup scoped to this Game Instance. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (AdvancedDisplay = "Options", DisplayName = "Play Named Haptic Pattern (Advanced)", ToolTip = "Requests a prepared raw-name pattern with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult PlayNamedPatternAdvanced(
		FName PatternName,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	/** Submits a direct asset and its loaded platform override, unresolved soft references are rejected instead of sync-loaded. */
	FOpenMobileHapticPlaybackResult SubmitPatternAsset(
		UOpenMobileHapticPatternAsset* PatternAsset,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	/** Anchors game or audio time to the platform clock for schedules that need measured timing diagnostics. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Calibrate Haptic Timing Clock (Advanced)", ToolTip = "Returns the full calibration result and internal anchor. Prefer Calibrate Haptic Timing in common graphs."))
	FOpenMobileHapticTimingCalibrationResult CalibrateTimingClock(
		EOpenMobileHapticTimingClock Clock,
		double ClockTimeSeconds,
		double EstimatedPrecisionSeconds = 0.005
	);

	/** Returns precision only for a calibration valid in the current lifecycle generation. */
	bool GetTimingCalibrationPrecision(
		EOpenMobileHapticTimingClock Clock,
		double& OutEstimatedPrecisionSeconds
	) const;

	/** Starts or joins shared configured-library preparation for legacy native callers. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Preload Named Haptic Libraries (Advanced)", ToolTip = "Starts the raw-handle compatibility preload. Prefer Prepare Haptics Async and an owned lease."))
	FOpenMobileHapticLibraryPreloadHandle PreloadNamedLibraries();

	/** Cancels the matching preload claim without releasing preparation owned by another handle. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Cancel Named Haptic Library Preload (Advanced)", ToolTip = "Cancels the matching raw-handle compatibility preload and ignores late callbacks."))
	FOpenMobileHapticControlResult CancelNamedLibraryPreload(
		FOpenMobileHapticLibraryPreloadHandle Handle
	);

	/** Releases the legacy global claim, owned preparation leases stay valid till their callers release them. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Release Named Haptic Libraries (Advanced)", ToolTip = "Releases only the legacy preparation claim. Active owned leases remain valid."))
	void ReleaseNamedLibraries();

	/** Reports whether an alias is configured and ready without loading its library as a side effect. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Named Haptic Pattern Status (Advanced)", ToolTip = "Reports readiness for one raw configured alias without loading it."))
	EOpenMobileHapticNamedPatternStatus GetNamedPatternStatus(
		FName PatternName
	) const;

	/** Combines library and backend resource preparation into the state visible to this Game Instance. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptic Preparation State (Advanced)", ToolTip = "Reports aggregate named-asset and native prewarm readiness."))
	EOpenMobileHapticPreparationState GetPreparationState() const;

	/** Copies sorted ready aliases for tooling without exposing the mutable preparation cache. */
	void GetPreparedPatternNames(TArray<FName>& OutPatternNames) const;

	/** Requests an ordinary early finish for one handle and preserves Stopped as the terminal reason. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Stop Haptic Playback By Handle (Advanced)", ToolTip = "Stops plugin-owned work for one raw playback handle. Prefer the playback object's Stop node."))
	FOpenMobileHapticControlResult StopPlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	/** Cancels pending or active ownership and rejects stale handles before they reach the backend. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Cancel Haptic Playback By Handle (Advanced)", ToolTip = "Cancels work for one raw playback handle. Prefer the playback object's Cancel node."))
	FOpenMobileHapticControlResult CancelPlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	/** Coalesces selected dynamic values according to the configured per-handle update limit. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Update Haptic Playback Parameters (Advanced)", ToolTip = "Queues a masked intensity or sharpness update for one raw handle. Prefer focused playback-object controls."))
	FOpenMobileHapticControlResult UpdatePlaybackParameters(
		FOpenMobileHapticPlaybackHandle Handle,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	);

	/** Pauses through native support or an allowed emulation path, otherwise returns a typed unsupported result. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Pause Haptic Playback By Handle (Advanced)", ToolTip = "Pauses one raw handle when its resolved path supports pause."))
	FOpenMobileHapticControlResult PausePlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	/** Continues only a successfully paused request, terminal and stale handles remain rejected. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Resume Haptic Playback By Handle (Advanced)", ToolTip = "Resumes one paused raw handle when its resolved path supports resume."))
	FOpenMobileHapticControlResult ResumePlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	/** Resolves and reports backend timing quantization instead of claiming the exact requested position. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Seek Haptic Playback By Handle (Advanced)", ToolTip = "Moves one raw playback handle and returns the broad control result."))
	FOpenMobileHapticControlResult SeekPlayback(
		FOpenMobileHapticPlaybackHandle Handle,
		double PositionSeconds
	);

	/** Stops work on one configured channel and leaves independent feedback running. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Stop Haptic Channel (Advanced)", ToolTip = "Stops plugin-owned work on one raw named Haptics channel."))
	FOpenMobileHapticControlResult StopChannel(FName Channel);

	/** Stops every request owned by this subsystem, unrelated platform vibration isn't claimed here. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Stop All Haptics (Advanced)", ToolTip = "Stops all phone Haptics owned by this Game Instance subsystem."))
	FOpenMobileHapticControlResult StopAll();

	/** Reads plugin-tracked lifecycle state only, hardware APIs can't confirm physical actuator motion. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptic Playback State By Handle (Advanced)", ToolTip = "Returns the latest state for one raw playback handle without waiting."))
	EOpenMobileHapticPlaybackState GetPlaybackState(
		FOpenMobileHapticPlaybackHandle Handle
	) const;

	/** Returns the current runtime player switch and doesn't imply hardware support. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Is Haptics Enabled (Advanced)", ToolTip = "Returns the current Game Instance player switch. Prefer Get Haptics Enabled without a subsystem target."))
	bool IsHapticsEnabled() const;

	/** Updates this Game Instance's player switch without persisting it outside runtime. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Set Haptics Enabled (Advanced)", ToolTip = "Returns the broad control result for a player-switch update. Games remain responsible for persistence."))
	FOpenMobileHapticControlResult SetHapticsEnabled(bool bEnabled);

	/** Returns the normalized runtime master scale used by policy resolution. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptics Master Intensity (Advanced)", ToolTip = "Returns the normalized Game Instance player intensity."))
	float GetMasterIntensity() const;

	/** Clamps and broadcasts a runtime master scale change, saving the preference still belongs to the game. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Set Haptics Master Intensity (Advanced)", ToolTip = "Returns the broad control result for a normalized player-intensity update."))
	FOpenMobileHapticControlResult SetMasterIntensity(
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float MasterIntensity
	);

	/** Copies the complete player policy so callers can edit one field and submit a coherent replacement. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptics User Policy (Advanced)", ToolTip = "Returns the full atomic Game Instance player policy for save-game restoration or advanced tooling."))
	FOpenMobileHapticUserPolicy GetUserPolicy() const;

	/** Validates and replaces runtime player policy in one operation, avoiding half-applied accessibility values. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Set Haptics User Policy (Advanced)", ToolTip = "Atomically replaces the full Game Instance player policy without saving it."))
	FOpenMobileHapticControlResult SetUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy
	);

	/** Returns a bounded sanitized snapshot for development UI and support logs. */
	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptics Diagnostics (Advanced)", ToolTip = "Captures the full bounded diagnostics snapshot once per execution. Prefer small pure queries in common graphs."))
	FOpenMobileHapticsDiagnostics GetDiagnostics() const;

#if !UE_BUILD_SHIPPING
	/** Lets editor preview send already cooked data through runtime policy without exposing authored source structs. */
	FOpenMobileHapticPlaybackResult SubmitCookedPreview(
		UOpenMobileHapticPatternAsset* PatternAsset,
		const FOpenMobileHapticPlaybackOptions& Options
	);
	/** Bypasses ordinary content lookup for explicit capability testing while still honouring subsystem lifetime. */
	FOpenMobileHapticPlaybackResult SubmitCapabilityTestPattern(
		UOpenMobileHapticPatternAsset* PatternAsset,
		const FOpenMobileHapticPlaybackOptions& Options
	);
#endif

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "On Haptic Playback Event (Advanced)", ToolTip = "Broadcasts every ordered playback state change. Prefer request-scoped playback object events."))
	FOpenMobileHapticPlaybackEventDynamic OnPlaybackEvent;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "On Named Haptic Libraries Prepared (Advanced)", ToolTip = "Broadcasts the terminal result of the raw-handle compatibility preload."))
	FOpenMobileHapticLibraryPreloadEventDynamic OnNamedLibrariesPrepared;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Prepare", meta = (DisplayName = "On Haptic Preparation State Changed", ToolTip = "Reports preparation transitions with a reason and whether already loaded pattern assets remain available."))
	FOpenMobileHapticPreparationStateChangedDynamic OnPreparationStateChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Policy", meta = (DisplayName = "On Haptics Policy Changed", ToolTip = "Reports the previous and new Game Instance player policy after a successful runtime update. Games remain responsible for persistence."))
	FOpenMobileHapticPolicyChangedDynamic OnPolicyChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "On Haptics Availability Changed", ToolTip = "Reports categorical availability changes caused by player policy, application lifecycle, or backend recovery."))
	FOpenMobileHapticAvailabilityChangedDynamic OnAvailabilityChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Capabilities", meta = (DisplayName = "On Haptics Capabilities Changed", ToolTip = "Reports old and new full capability snapshots after backend, lifecycle, recovery, or player-policy changes. Use the availability event when only output readiness matters."))
	FOpenMobileHapticCapabilitiesChangedDynamic OnCapabilitiesChanged;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Haptics|Policy", meta = (DisplayName = "On Haptics Master Intensity Changed", ToolTip = "Reports old and new normalized master intensity after a successful dedicated or whole-policy update."))
	FOpenMobileHapticMasterIntensityChangedDynamic OnMasterIntensityChanged;

	/** Implements the native facade with the same capability snapshot used by Blueprint callers. */
	virtual FOpenMobileHapticCapabilities GetCapabilitiesNative() const override;
	/** Routes the low-level semantic contract through policy, fallback, overlap, and tracking. */
	virtual FOpenMobileHapticPlaybackResult SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request
	) override;
	/** Routes bounded vibration through the same admission and lifecycle rules as higher-level calls. */
	virtual FOpenMobileHapticPlaybackResult SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request
	) override;
	/** Resolves a prepared alias for the native interface without introducing synchronous loading. */
	virtual FOpenMobileHapticPlaybackResult SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request
	) override;
	/** Shares the public stop implementation so native and Blueprint callers see identical terminal behavior. */
	virtual FOpenMobileHapticControlResult StopPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) override;
	/** Shares cancellation and stale-handle rules with the Blueprint-facing control path. */
	virtual FOpenMobileHapticControlResult CancelPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) override;
	/** Accepts the native partial-update struct and keeps the same rate limiting used by playback objects. */
	virtual FOpenMobileHapticControlResult UpdatePlaybackParametersNative(
		FOpenMobileHapticPlaybackHandle Handle,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) override;
	/** Exposes pause through the native facade without adding another state machine. */
	virtual FOpenMobileHapticControlResult PausePlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) override;
	/** Exposes resume through the same tracked playback cursor state as public controls. */
	virtual FOpenMobileHapticControlResult ResumePlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) override;
	/** Exposes seek and its resolved position diagnostics to native callers. */
	virtual FOpenMobileHapticControlResult SeekPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle,
		double PositionSeconds
	) override;
	/** Keeps native channel stopping on the same configured-name validation path. */
	virtual FOpenMobileHapticControlResult StopChannelNative(
		FName Channel
	) override;
	/** Delegates native stop-all to the subsystem-owned request registry. */
	virtual FOpenMobileHapticControlResult StopAllNative() override;
	/** Returns the tracked state for the native interface without copying internal request records. */
	virtual EOpenMobileHapticPlaybackState GetPlaybackStateNative(
		FOpenMobileHapticPlaybackHandle Handle
	) const override;
	/** Mirrors the runtime player switch through the native facade. */
	virtual bool IsHapticsEnabledNative() const override;
	/** Applies the native enable change through the validated policy update path. */
	virtual FOpenMobileHapticControlResult SetHapticsEnabledNative(
		bool bEnabled
	) override;
	/** Mirrors the current normalized master scale through the native facade. */
	virtual float GetMasterIntensityNative() const override;
	/** Applies native intensity changes with the same clamping and event broadcast as Blueprint. */
	virtual FOpenMobileHapticControlResult SetMasterIntensityNative(
		float MasterIntensity
	) override;
	/** Returns a copy so native callers can't mutate policy without validation. */
	virtual FOpenMobileHapticUserPolicy GetUserPolicyNative() const override;
	/** Implements the native full-policy replacement and publishes only actual changes. */
	virtual FOpenMobileHapticControlResult UpdateUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy
	) override;
	/** Mirrors the bounded diagnostics snapshot for native development tools. */
	virtual FOpenMobileHapticsDiagnostics GetDiagnosticsNative() const override;
	/** Returns the native event object owned by this subsystem, bindings mustn't outlive it. */
	virtual FOpenMobileHapticNativePlaybackEvent& OnPlaybackEventNative() override;

private:
	friend class UOpenMobileHapticPreparationAsyncAction;
	friend class UOpenMobileHapticNamedPlaybackAsyncAction;
	friend class UOpenMobileHapticPreparationLease;
	friend class UOpenMobileHapticPlayback;
	friend class UOpenMobileHapticPlaybackAsyncAction;
	friend class FOpenMobileHapticsAsyncContractTest;
	friend class FOpenMobileHapticsPreparationAsyncContractTest;
	friend class FOpenMobileHapticNamedLibrarySubsystemTest;
	friend class FOpenMobileHapticsDynamicParameterSubsystemTest;
	friend class FOpenMobileHapticsMasterIntensityTest;
	friend class FOpenMobileHapticsCategoryEffectScaleTest;
	friend class FOpenMobileHapticsPlaybackLifecycleMissingCallbackTest;
	friend class FOpenMobileHapticsRecoveryPreparedAssetsTest;
	friend class FOpenMobileHapticsLifecyclePreparedAssetsTest;
	friend class FOpenMobileHapticsOverlapSubsystemTest;

	enum class EPlaybackCursorControl : uint8
	{
		Pause,
		Resume,
		Seek
	};

	/** Retains legacy async tasks till they broadcast one final result, Blueprint may not keep another strong reference. */
	void RegisterAsyncAction(UOpenMobileHapticPlaybackAsyncAction* Action);
	/** Drops a finished legacy task so the subsystem doesn't extend its lifetime past cleanup. */
	void UnregisterAsyncAction(UOpenMobileHapticPlaybackAsyncAction* Action);
	/** Keeps controllable playback objects alive while subsystem events can still update them. */
	void RegisterPlaybackObject(UOpenMobileHapticPlayback* Playback);
	/** Releases terminal playback objects after their delegates and preparation claims are cleaned up. */
	void UnregisterPlaybackObject(UOpenMobileHapticPlayback* Playback);
	/** Starts shared named preparation and records whether the old global ownership API asked for it. */
	FOpenMobileHapticLibraryPreloadHandle PreloadNamedLibrariesInternal(
		bool bAddLegacyClaim
	);
	/** Creates one ownership claim over prepared libraries so unrelated callers can release independently. */
	UOpenMobileHapticPreparationLease* AcquirePreparationLease();
	/** Removes one claim and releases shared resources only after the last owner has gone. */
	void ReleasePreparationLease(UOpenMobileHapticPreparationLease* Lease);
	/** Retains a preparation task across async loading even when Blueprint doesn't store the proxy. */
	void RegisterPreparationAction(
		UOpenMobileHapticPreparationAsyncAction* Action
	);
	/** Releases a completed preparation task after its final delegate has been sent. */
	void UnregisterPreparationAction(
		UOpenMobileHapticPreparationAsyncAction* Action
	);
	/** Keeps a prepare-if-needed named request alive till it reaches submission or a final failure. */
	void RegisterNamedPlaybackAction(
		UOpenMobileHapticNamedPlaybackAsyncAction* Action
	);
	/** Drops the subsystem reference once the named task has sealed its result. */
	void UnregisterNamedPlaybackAction(
		UOpenMobileHapticNamedPlaybackAsyncAction* Action
	);
	/** Publishes state transitions with a reason and asset-retention flag so listeners don't infer either from enum order. */
	void BroadcastPreparationStateChange(
		EOpenMobileHapticPreparationState PreviousState,
		EOpenMobileHapticPreparationState NewState,
		FString Reason,
		bool bPreparedAssetsRemainLoaded
	);
	/** Avoids noisy capability events by comparing the whole sanitized snapshot first. */
	void BroadcastCapabilitiesIfChanged();
	/** Allocates opaque runtime state lazily, side-effect-free capability queries can still work before the first request. */
	FOpenMobileHapticsSubsystemState& GetOrCreateState() const;
	/** Validates one full policy replacement and optionally keeps scheduled starts that are still allowed. */
	FOpenMobileHapticControlResult ApplyUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy,
		bool bPreserveAllowedScheduledStarts
	);
	/** Captures a weak subsystem owner so native callbacks arriving after teardown are harmless. */
	TFunction<void(const FOpenMobileHapticsBackendCallback&)>
	MakeBackendCallback();
	/** Creates the game-thread dispatcher once because backends may call from their native worker threads. */
	void EnsureNativeEventDispatcher(
		FOpenMobileHapticsSubsystemState& LocalState
	) const;
	/** Validates generation and sequence before a backend event can change tracked playback state. */
	void HandleBackendCallback(
		const FOpenMobileHapticsBackendCallback& Callback
	);
	/** Hooks interruption, recovery, lifecycle, and capability changes while this Game Instance is active. */
	void BindRecoveryEvents();
	/** Removes every recovery hook before teardown so engine singletons can't call a dead subsystem. */
	void UnbindRecoveryEvents();
	/** Applies the chosen interruption policy to active requests and records what may resume later. */
	void HandleInterruption(
		const FOpenMobileHapticsInterruption& Interruption
	);
	/** Rebuilds backend state after interruption and resumes only requests that still satisfy policy. */
	void HandleRecovery();
	/** Updates availability and prepared state across foreground, background, and shutdown transitions. */
	void HandleApplicationLifecycle(
		const FOpenMobileHapticsLifecycleTransition& Transition
	);
	/** Re-evaluates pending and active work when the backend reports a real feature change. */
	void HandleCapabilitiesChanged(
		const FOpenMobileHapticCapabilities& PreviousCapabilities,
		const FOpenMobileHapticCapabilities& NewCapabilities
	);
	/** Emits accepted and start evidence in the correct order, including delayed schedules. */
	void PublishSubmissionEvents(
		const FOpenMobileHapticPlaybackResult& Result,
		const FOpenMobileHapticsTimingResolution& Timing
	);
	/** Publishes events after the caller has had a chance to bind its request-scoped playback object. */
	void PublishDeferredSubmissionEvents(uint64 RequestId);
	/** Centralizes sequence assignment and state tracking before an event reaches public delegates. */
	void PublishPlaybackEvent(
		uint64 RequestId,
		FOpenMobileHapticPlaybackEvent Event
	);
	/** Adds estimated start evidence only when the backend can't promise a native confirmation callback. */
	void ScheduleEstimatedStart(uint64 RequestId, double DelaySeconds);
	/** Checks the request is still live before publishing a timer-based start estimate. */
	void PublishEstimatedStart(uint64 RequestId);
	/** Prevents callback-capable backends from leaving accepted handles active forever after a missing final event. */
	void ScheduleTerminalWatchdog(uint64 RequestId, double DelaySeconds);
	/** Fails only the still-active request associated with the watchdog, late timers can't hit recycled work. */
	void PublishTerminalTimeout(uint64 RequestId);
	/** Shares stop and cancel completion while keeping their requested terminal states separate. */
	FOpenMobileHapticControlResult EndPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle,
		EOpenMobileHapticPlaybackState TerminalState
	);
	/** Finishes locally controlled work when the backend won't send its own terminal callback. */
	void CompleteControlledRequest(
		uint64 RequestId,
		EOpenMobileHapticPlaybackState TerminalState
	);
	/** Coalesces rapid values per handle so the newest request survives the configured submission interval. */
	FOpenMobileHapticControlResult QueueDynamicParameterUpdate(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& EffectiveUpdate,
		const FOpenMobileHapticDynamicParameterUpdate* RequestedUpdate
	);
	/** Sends one resolved update and records its time only after the backend accepts it. */
	FOpenMobileHapticControlResult SubmitDynamicParameterUpdate(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update,
		double SubmissionTimeSeconds
	);
	/** Keeps pause, resume, and seek on one revision-checked cursor state machine. */
	FOpenMobileHapticControlResult ApplyPlaybackCursorControl(
		FOpenMobileHapticPlaybackHandle Handle,
		EPlaybackCursorControl Control,
		double PositionSeconds
	);
	/** Starts one core ticker only when at least one handle has a deferred dynamic update. */
	void ScheduleDynamicParameterFlush();
	/** Submits eligible latest values and removes stale or terminal handle entries. */
	void FlushDynamicParameterUpdates(double NowSeconds);
	/** Gives contract tests a deterministic clock without changing production ticker behavior. */
	void FlushDynamicParameterUpdatesForTests(double NowSeconds)
	{
		FlushDynamicParameterUpdates(NowSeconds);
	}
	/** Drives deferred updates on the game thread and stops ticking when the queue becomes empty. */
	bool TickDynamicParameterUpdates(float DeltaTime);
	/** Registers accepted handles and diagnostics before any deferred callback can refer to the request. */
	FOpenMobileHapticPlaybackResult TrackInitialSubmissionResult(
		FOpenMobileHapticPlaybackResult Result
	);
	/** Tries a prepared preset override first when supplied, then keeps semantic fallback on the same request token. */
	FOpenMobileHapticPlaybackResult SubmitSemanticOrOverride(
		const FOpenMobileHapticSemanticRequest& Request,
		FName PatternOverride,
		const FOpenMobileHapticsBackendRequestToken* ExistingToken = nullptr
	);
	/** Runs one-shot policy and backend submission with an optional existing token used by overlap promotion. */
	FOpenMobileHapticPlaybackResult SubmitOneShotInternal(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsBackendRequestToken* ExistingToken
	);
	/** Resolves prepared content and can reuse a queued token without allocating a second public handle. */
	FOpenMobileHapticPlaybackResult SubmitNamedPatternInternal(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendRequestToken* ExistingToken,
		bool bBypassNamedLibraries = false
	);
	/** Decides replace, ignore, queue, or mix before backend admission and reports when mix had to fall back. */
	bool ResolveAndApplyOverlap(
		const FOpenMobileHapticPlaybackOptions& Options,
		FName Effect,
		uint64 ExcludedRequestId,
		FOpenMobileHapticPlaybackResult& OutResult,
		bool& bOutShouldQueue,
		bool& bOutUsedMixFallback
	);
	/** Stores the original typed request so later promotion repeats full validation against current policy. */
	FOpenMobileHapticPlaybackResult QueueOverlapRequest(
		const FOpenMobileHapticPlaybackOptions& Options,
		const FOpenMobileHapticsResolvedChannel& ResolvedChannel,
		FName Effect,
		bool bRepeating,
		bool bUsedMixFallback,
		const FOpenMobileHapticsBackendRequestToken* ExistingToken,
		const FOpenMobileHapticSemanticRequest* SemanticRequest,
		const FOpenMobileHapticOneShotRequest* OneShotRequest,
		const FOpenMobileHapticNamedPatternRequest* NamedRequest,
		FName SemanticPatternOverride = NAME_None
	);
	/** Arms a per-request expiry because queued feedback becomes misleading after its gameplay moment has passed. */
	void ScheduleOverlapQueueExpiry(uint64 RequestId, double DelaySeconds);
	/** Suppresses only a request still waiting in the overlap queue when its age limit is reached. */
	void ExpireOverlapQueue(uint64 RequestId);
	/** Defers draining till current terminal events finish mutating channel ownership. */
	void ScheduleOverlapQueueDrain();
	/** Promotes eligible requests by priority and age while respecting current channel capacity. */
	void DrainOverlapQueues(double NowSeconds);
	/** Connects a promoted backend result to the queued public handle that callers already received. */
	void FinishPromotedOverlapRequest(
		uint64 RequestId,
		const FOpenMobileHapticPlaybackResult& Result
	);
	/** Enforces global and per-channel active or queued limits before the request enters tracked state. */
	bool AdmitChannelRequest(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticPlaybackOptions& Options,
		int32 MaximumActiveHandles,
		int32 MaximumQueueDepth,
		bool bQueued,
		bool bWaitingForOverlap,
		bool bRepeating,
		FName Effect,
		FOpenMobileHapticPlaybackResult& OutRejection
	);
	/** Merges loaded libraries into one validated alias table and rejects duplicates before resource preparation. */
	bool PrepareLoadedNamedLibraries(
		const TArray<UOpenMobileHapticLibrary*>& Libraries,
		TArray<FString>& Errors
	);
	/** Prepares pattern overrides and backend resources after aliases have resolved, with explicit retention on recoverable native failure. */
	bool PrepareResolvedResources(
		TArray<FString>& Errors,
		bool bPreserveResolvedLibrariesOnNativeFailure = false
	);
	/** Ignores stale loading generations before reading the configured library objects. */
	void HandleNamedLibrariesLoaded(
		uint64 Generation,
		FOpenMobileHapticLibraryPreloadHandle Handle
	);
	/** Continues only after every soft pattern referenced by the current library generation is loaded. */
	void HandleNamedPatternsLoaded(
		uint64 Generation,
		FOpenMobileHapticLibraryPreloadHandle Handle
	);
	/** Finishes platform override streaming for the same generation before native preparation starts. */
	void HandleNamedOverridesLoaded(
		uint64 Generation,
		FOpenMobileHapticLibraryPreloadHandle Handle
	);
	/** Broadcasts one preload result and updates shared ownership without mixing older generation errors into it. */
	void FinishNamedLibraryPreload(
		FOpenMobileHapticLibraryPreloadHandle Handle,
		EOpenMobileHapticLibraryPreloadOutcome Outcome,
		TArray<FString> Errors
	);
	/** Clears shared prepared state only when no claim remains, optionally notifying an unfinished legacy preload. */
	void ReleaseNamedLibrariesInternal(bool bNotifyCancellation);

	FOpenMobileHapticUserPolicy UserPolicy;
	TAtomic<bool> bUserPolicyEnabled = true;
	FOpenMobileHapticCapabilities LastBroadcastCapabilities;
	FOpenMobileHapticNativePlaybackEvent NativePlaybackEvent;
	TSet<TWeakObjectPtr<UOpenMobileHapticPlaybackAsyncAction>> ActiveAsyncActions;
	TSet<TWeakObjectPtr<UOpenMobileHapticPreparationAsyncAction>>
		ActivePreparationActions;
	TSet<TWeakObjectPtr<UOpenMobileHapticNamedPlaybackAsyncAction>>
		ActiveNamedPlaybackActions;
	TSet<TWeakObjectPtr<UOpenMobileHapticPreparationLease>>
		ActivePreparationLeases;

	UPROPERTY(Transient)
	TSet<TObjectPtr<UOpenMobileHapticPlayback>> ActivePlaybackObjects;

	mutable TUniquePtr<
		FOpenMobileHapticsSubsystemState,
		FOpenMobileHapticsSubsystemStateDeleter
	> State;
	FDelegateHandle InterruptionDelegateHandle;
	FDelegateHandle RecoveryDelegateHandle;
	FDelegateHandle ApplicationLifecycleDelegateHandle;
	FDelegateHandle CapabilitiesChangedDelegateHandle;
	bool bLegacyPreparationClaim = false;
	bool bDeinitialized = false;
};
