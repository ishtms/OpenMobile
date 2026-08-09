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
	void operator()(FOpenMobileHapticsSubsystemState* State) const;
};

/**
 * Game-facing Haptics facade owned by one Game Instance.
 * Submission and control calls run on the game thread. Accepted controllable
 * work receives a stable handle, and all public events return on the game thread.
 */
UCLASS(meta = (DisplayName = "Open Mobile Haptics"))
class OPENMOBILEHAPTICS_API UOpenMobileHapticsSubsystem final :
	public UGameInstanceSubsystem,
	public IOpenMobileHaptics
{
	GENERATED_BODY()

public:
	virtual ~UOpenMobileHapticsSubsystem() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptic Capabilities (Advanced)", ToolTip = "Returns the full side-effect-free capability snapshot. Prefer small capability nodes in common graphs."))
	FOpenMobileHapticCapabilities GetHapticCapabilities() const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Selection Haptic for direct outcome branches and a playback object.", DisplayName = "Play Selection Feedback (Legacy)", ToolTip = "Legacy broad-result API. Plays low-latency selection feedback."))
	FOpenMobileHapticPlaybackResult PlaySelectionFeedback(
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Impact Haptic for direct outcome branches and a playback object.", DisplayName = "Play Impact Feedback (Legacy)", ToolTip = "Legacy broad-result API for portable impact feedback."))
	FOpenMobileHapticPlaybackResult PlayImpactFeedback(
		EOpenMobileHapticImpactStyle Style,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Notification Haptic for direct outcome branches and a playback object.", DisplayName = "Play Notification Feedback (Legacy)", ToolTip = "Legacy broad-result API for portable in-app notification feedback."))
	FOpenMobileHapticPlaybackResult PlayNotificationFeedback(
		EOpenMobileHapticNotificationType Type,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Game Haptic for direct outcome branches and a playback object.", DisplayName = "Play Game Feedback (Legacy)", ToolTip = "Legacy broad-result API for stable game presets."))
	FOpenMobileHapticPlaybackResult PlayGameFeedback(
		EOpenMobileHapticGamePreset Preset,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (AdvancedDisplay = "Options", DisplayName = "Play Game Feedback (Advanced)", ToolTip = "Plays a game preset with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult PlayGameFeedbackAdvanced(
		EOpenMobileHapticGamePreset Preset,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Play Semantic Feedback (Advanced)", ToolTip = "Requests the broad semantic enum directly. Prefer selection, impact, notification, or game Haptic nodes for their distinct channel and override behavior."))
	FOpenMobileHapticPlaybackResult PlaySemanticFeedback(
		EOpenMobileHapticSemanticEffect Effect,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (AdvancedDisplay = "Options", DisplayName = "Play Semantic Feedback With Options (Advanced)", ToolTip = "Requests semantic feedback with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult PlaySemanticFeedbackAdvanced(
		EOpenMobileHapticSemanticEffect Effect,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Vibrate Phone for direct outcome branches and a playback object.", DisplayName = "Vibrate (Legacy)", ToolTip = "Legacy broad-result API for one bounded phone vibration."))
	FOpenMobileHapticPlaybackResult Vibrate(
		float DurationSeconds = 0.05f,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (AdvancedDisplay = "Options", DisplayName = "Vibrate (Advanced)", ToolTip = "Requests one-shot phone vibration with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult VibrateAdvanced(
		float DurationSeconds,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DeprecatedFunction, DeprecationMessage = "Use Play Named Haptic with a typed identifier, or Play Haptic Pattern Asset.", DisplayName = "Play Named Haptic Pattern (Legacy)", ToolTip = "Legacy raw-name broad-result API. The named pattern must already be prepared."))
	FOpenMobileHapticPlaybackResult PlayNamedPattern(
		FName PatternName,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (AdvancedDisplay = "Options", DisplayName = "Play Named Haptic Pattern (Advanced)", ToolTip = "Requests a prepared raw-name pattern with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult PlayNamedPatternAdvanced(
		FName PatternName,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	FOpenMobileHapticPlaybackResult SubmitPatternAsset(
		UOpenMobileHapticPatternAsset* PatternAsset,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Calibrate Haptic Timing Clock (Advanced)", ToolTip = "Returns the full calibration result and internal anchor. Prefer Calibrate Haptic Timing in common graphs."))
	FOpenMobileHapticTimingCalibrationResult CalibrateTimingClock(
		EOpenMobileHapticTimingClock Clock,
		double ClockTimeSeconds,
		double EstimatedPrecisionSeconds = 0.005
	);

	bool GetTimingCalibrationPrecision(
		EOpenMobileHapticTimingClock Clock,
		double& OutEstimatedPrecisionSeconds
	) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Preload Named Haptic Libraries (Advanced)", ToolTip = "Starts the raw-handle compatibility preload. Prefer Prepare Haptics Async and an owned lease."))
	FOpenMobileHapticLibraryPreloadHandle PreloadNamedLibraries();

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Cancel Named Haptic Library Preload (Advanced)", ToolTip = "Cancels the matching raw-handle compatibility preload and ignores late callbacks."))
	FOpenMobileHapticControlResult CancelNamedLibraryPreload(
		FOpenMobileHapticLibraryPreloadHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Release Named Haptic Libraries (Advanced)", ToolTip = "Releases only the legacy preparation claim. Active owned leases remain valid."))
	void ReleaseNamedLibraries();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Named Haptic Pattern Status (Advanced)", ToolTip = "Reports readiness for one raw configured alias without loading it."))
	EOpenMobileHapticNamedPatternStatus GetNamedPatternStatus(
		FName PatternName
	) const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptic Preparation State (Advanced)", ToolTip = "Reports aggregate named-asset and native prewarm readiness."))
	EOpenMobileHapticPreparationState GetPreparationState() const;

	void GetPreparedPatternNames(TArray<FName>& OutPatternNames) const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Stop Haptic Playback By Handle (Advanced)", ToolTip = "Stops plugin-owned work for one raw playback handle. Prefer the playback object's Stop node."))
	FOpenMobileHapticControlResult StopPlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Cancel Haptic Playback By Handle (Advanced)", ToolTip = "Cancels work for one raw playback handle. Prefer the playback object's Cancel node."))
	FOpenMobileHapticControlResult CancelPlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Update Haptic Playback Parameters (Advanced)", ToolTip = "Queues a masked intensity or sharpness update for one raw handle. Prefer focused playback-object controls."))
	FOpenMobileHapticControlResult UpdatePlaybackParameters(
		FOpenMobileHapticPlaybackHandle Handle,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Pause Haptic Playback By Handle (Advanced)", ToolTip = "Pauses one raw handle when its resolved path supports pause."))
	FOpenMobileHapticControlResult PausePlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Resume Haptic Playback By Handle (Advanced)", ToolTip = "Resumes one paused raw handle when its resolved path supports resume."))
	FOpenMobileHapticControlResult ResumePlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Seek Haptic Playback By Handle (Advanced)", ToolTip = "Moves one raw playback handle and returns the broad control result."))
	FOpenMobileHapticControlResult SeekPlayback(
		FOpenMobileHapticPlaybackHandle Handle,
		double PositionSeconds
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Stop Haptic Channel (Advanced)", ToolTip = "Stops plugin-owned work on one raw named Haptics channel."))
	FOpenMobileHapticControlResult StopChannel(FName Channel);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Stop All Haptics (Advanced)", ToolTip = "Stops all phone Haptics owned by this Game Instance subsystem."))
	FOpenMobileHapticControlResult StopAll();

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptic Playback State By Handle (Advanced)", ToolTip = "Returns the latest state for one raw playback handle without waiting."))
	EOpenMobileHapticPlaybackState GetPlaybackState(
		FOpenMobileHapticPlaybackHandle Handle
	) const;

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Is Haptics Enabled (Advanced)", ToolTip = "Returns the current Game Instance player switch. Prefer Get Haptics Enabled without a subsystem target."))
	bool IsHapticsEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Set Haptics Enabled (Advanced)", ToolTip = "Returns the broad control result for a player-switch update. Games remain responsible for persistence."))
	FOpenMobileHapticControlResult SetHapticsEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptics Master Intensity (Advanced)", ToolTip = "Returns the normalized Game Instance player intensity."))
	float GetMasterIntensity() const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Set Haptics Master Intensity (Advanced)", ToolTip = "Returns the broad control result for a normalized player-intensity update."))
	FOpenMobileHapticControlResult SetMasterIntensity(
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float MasterIntensity
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptics User Policy (Advanced)", ToolTip = "Returns the full atomic Game Instance player policy for save-game restoration or advanced tooling."))
	FOpenMobileHapticUserPolicy GetUserPolicy() const;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Set Haptics User Policy (Advanced)", ToolTip = "Atomically replaces the full Game Instance player policy without saving it."))
	FOpenMobileHapticControlResult SetUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy
	);

	UFUNCTION(BlueprintCallable, BlueprintPure = false, Category = "OpenMobile|Haptics|Advanced", meta = (DisplayName = "Get Haptics Diagnostics (Advanced)", ToolTip = "Captures the full bounded diagnostics snapshot once per execution. Prefer small pure queries in common graphs."))
	FOpenMobileHapticsDiagnostics GetDiagnostics() const;

#if !UE_BUILD_SHIPPING
	FOpenMobileHapticPlaybackResult SubmitCookedPreview(
		UOpenMobileHapticPatternAsset* PatternAsset,
		const FOpenMobileHapticPlaybackOptions& Options
	);
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

	virtual FOpenMobileHapticCapabilities GetCapabilitiesNative() const override;
	virtual FOpenMobileHapticPlaybackResult SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request
	) override;
	virtual FOpenMobileHapticPlaybackResult SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request
	) override;
	virtual FOpenMobileHapticPlaybackResult SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request
	) override;
	virtual FOpenMobileHapticControlResult StopPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) override;
	virtual FOpenMobileHapticControlResult CancelPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) override;
	virtual FOpenMobileHapticControlResult UpdatePlaybackParametersNative(
		FOpenMobileHapticPlaybackHandle Handle,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) override;
	virtual FOpenMobileHapticControlResult PausePlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) override;
	virtual FOpenMobileHapticControlResult ResumePlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle
	) override;
	virtual FOpenMobileHapticControlResult SeekPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle,
		double PositionSeconds
	) override;
	virtual FOpenMobileHapticControlResult StopChannelNative(
		FName Channel
	) override;
	virtual FOpenMobileHapticControlResult StopAllNative() override;
	virtual EOpenMobileHapticPlaybackState GetPlaybackStateNative(
		FOpenMobileHapticPlaybackHandle Handle
	) const override;
	virtual bool IsHapticsEnabledNative() const override;
	virtual FOpenMobileHapticControlResult SetHapticsEnabledNative(
		bool bEnabled
	) override;
	virtual float GetMasterIntensityNative() const override;
	virtual FOpenMobileHapticControlResult SetMasterIntensityNative(
		float MasterIntensity
	) override;
	virtual FOpenMobileHapticUserPolicy GetUserPolicyNative() const override;
	virtual FOpenMobileHapticControlResult UpdateUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy
	) override;
	virtual FOpenMobileHapticsDiagnostics GetDiagnosticsNative() const override;
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

	void RegisterAsyncAction(UOpenMobileHapticPlaybackAsyncAction* Action);
	void UnregisterAsyncAction(UOpenMobileHapticPlaybackAsyncAction* Action);
	void RegisterPlaybackObject(UOpenMobileHapticPlayback* Playback);
	void UnregisterPlaybackObject(UOpenMobileHapticPlayback* Playback);
	FOpenMobileHapticLibraryPreloadHandle PreloadNamedLibrariesInternal(
		bool bAddLegacyClaim
	);
	UOpenMobileHapticPreparationLease* AcquirePreparationLease();
	void ReleasePreparationLease(UOpenMobileHapticPreparationLease* Lease);
	void RegisterPreparationAction(
		UOpenMobileHapticPreparationAsyncAction* Action
	);
	void UnregisterPreparationAction(
		UOpenMobileHapticPreparationAsyncAction* Action
	);
	void RegisterNamedPlaybackAction(
		UOpenMobileHapticNamedPlaybackAsyncAction* Action
	);
	void UnregisterNamedPlaybackAction(
		UOpenMobileHapticNamedPlaybackAsyncAction* Action
	);
	void BroadcastPreparationStateChange(
		EOpenMobileHapticPreparationState PreviousState,
		EOpenMobileHapticPreparationState NewState,
		FString Reason,
		bool bPreparedAssetsRemainLoaded
	);
	void BroadcastAvailabilityIfChanged();
	FOpenMobileHapticsSubsystemState& GetOrCreateState() const;
	FOpenMobileHapticControlResult ApplyUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy,
		bool bPreserveAllowedScheduledStarts
	);
	TFunction<void(const FOpenMobileHapticsBackendCallback&)>
	MakeBackendCallback();
	void EnsureNativeEventDispatcher(
		FOpenMobileHapticsSubsystemState& LocalState
	) const;
	void HandleBackendCallback(
		const FOpenMobileHapticsBackendCallback& Callback
	);
	void BindRecoveryEvents();
	void UnbindRecoveryEvents();
	void HandleInterruption(
		const FOpenMobileHapticsInterruption& Interruption
	);
	void HandleRecovery();
	void HandleApplicationLifecycle(
		const FOpenMobileHapticsLifecycleTransition& Transition
	);
	void PublishSubmissionEvents(
		const FOpenMobileHapticPlaybackResult& Result,
		const FOpenMobileHapticsTimingResolution& Timing
	);
	void PublishDeferredSubmissionEvents(uint64 RequestId);
	void PublishPlaybackEvent(
		uint64 RequestId,
		FOpenMobileHapticPlaybackEvent Event
	);
	void ScheduleEstimatedStart(uint64 RequestId, double DelaySeconds);
	void PublishEstimatedStart(uint64 RequestId);
	void ScheduleTerminalWatchdog(uint64 RequestId, double DelaySeconds);
	void PublishTerminalTimeout(uint64 RequestId);
	FOpenMobileHapticControlResult EndPlaybackNative(
		FOpenMobileHapticPlaybackHandle Handle,
		EOpenMobileHapticPlaybackState TerminalState
	);
	void CompleteControlledRequest(
		uint64 RequestId,
		EOpenMobileHapticPlaybackState TerminalState
	);
	FOpenMobileHapticControlResult QueueDynamicParameterUpdate(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& EffectiveUpdate,
		const FOpenMobileHapticDynamicParameterUpdate* RequestedUpdate
	);
	FOpenMobileHapticControlResult SubmitDynamicParameterUpdate(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update,
		double SubmissionTimeSeconds
	);
	FOpenMobileHapticControlResult ApplyPlaybackCursorControl(
		FOpenMobileHapticPlaybackHandle Handle,
		EPlaybackCursorControl Control,
		double PositionSeconds
	);
	void ScheduleDynamicParameterFlush();
	void FlushDynamicParameterUpdates(double NowSeconds);
	void FlushDynamicParameterUpdatesForTests(double NowSeconds)
	{
		FlushDynamicParameterUpdates(NowSeconds);
	}
	bool TickDynamicParameterUpdates(float DeltaTime);
	FOpenMobileHapticPlaybackResult TrackInitialSubmissionResult(
		FOpenMobileHapticPlaybackResult Result
	);
	FOpenMobileHapticPlaybackResult SubmitSemanticOrOverride(
		const FOpenMobileHapticSemanticRequest& Request,
		FName PatternOverride,
		const FOpenMobileHapticsBackendRequestToken* ExistingToken = nullptr
	);
	FOpenMobileHapticPlaybackResult SubmitOneShotInternal(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsBackendRequestToken* ExistingToken
	);
	FOpenMobileHapticPlaybackResult SubmitNamedPatternInternal(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendRequestToken* ExistingToken,
		bool bBypassNamedLibraries = false
	);
	bool ResolveAndApplyOverlap(
		const FOpenMobileHapticPlaybackOptions& Options,
		FName Effect,
		uint64 ExcludedRequestId,
		FOpenMobileHapticPlaybackResult& OutResult,
		bool& bOutShouldQueue,
		bool& bOutUsedMixFallback
	);
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
	void ScheduleOverlapQueueExpiry(uint64 RequestId, double DelaySeconds);
	void ExpireOverlapQueue(uint64 RequestId);
	void ScheduleOverlapQueueDrain();
	void DrainOverlapQueues(double NowSeconds);
	void FinishPromotedOverlapRequest(
		uint64 RequestId,
		const FOpenMobileHapticPlaybackResult& Result
	);
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
	bool PrepareLoadedNamedLibraries(
		const TArray<UOpenMobileHapticLibrary*>& Libraries,
		TArray<FString>& Errors
	);
	bool PrepareResolvedResources(
		TArray<FString>& Errors,
		bool bPreserveResolvedLibrariesOnNativeFailure = false
	);
	void HandleNamedLibrariesLoaded(
		uint64 Generation,
		FOpenMobileHapticLibraryPreloadHandle Handle
	);
	void HandleNamedPatternsLoaded(
		uint64 Generation,
		FOpenMobileHapticLibraryPreloadHandle Handle
	);
	void HandleNamedOverridesLoaded(
		uint64 Generation,
		FOpenMobileHapticLibraryPreloadHandle Handle
	);
	void FinishNamedLibraryPreload(
		FOpenMobileHapticLibraryPreloadHandle Handle,
		EOpenMobileHapticLibraryPreloadOutcome Outcome,
		TArray<FString> Errors
	);
	void ReleaseNamedLibrariesInternal(bool bNotifyCancellation);

	FOpenMobileHapticUserPolicy UserPolicy;
	TAtomic<bool> bUserPolicyEnabled = true;
	EOpenMobileHapticAvailability LastBroadcastAvailability =
		EOpenMobileHapticAvailability::UnsupportedPlatform;
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
	bool bLegacyPreparationClaim = false;
	bool bDeinitialized = false;
};
