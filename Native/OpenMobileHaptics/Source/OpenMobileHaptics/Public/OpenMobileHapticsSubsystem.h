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

class UOpenMobileHapticLibrary;
class UOpenMobileHapticPatternAsset;
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
UCLASS()
class OPENMOBILEHAPTICS_API UOpenMobileHapticsSubsystem final :
	public UGameInstanceSubsystem,
	public IOpenMobileHaptics
{
	GENERATED_BODY()

public:
	virtual ~UOpenMobileHapticsSubsystem() override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Haptic Capabilities", ToolTip = "Returns a side-effect-free snapshot of current Haptics support."))
	FOpenMobileHapticCapabilities GetHapticCapabilities() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Play Selection Feedback", ToolTip = "Plays low-latency feedback for selection changes, picker steps, and slider detents."))
	FOpenMobileHapticPlaybackResult PlaySelectionFeedback(
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Play Impact Feedback", ToolTip = "Plays a portable light, medium, heavy, soft, or rigid impact with normalized intensity."))
	FOpenMobileHapticPlaybackResult PlayImpactFeedback(
		EOpenMobileHapticImpactStyle Style,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Play Notification Feedback", ToolTip = "Plays portable success, warning, or error feedback without delivering an operating-system notification."))
	FOpenMobileHapticPlaybackResult PlayNotificationFeedback(
		EOpenMobileHapticNotificationType Type,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Play Game Feedback", ToolTip = "Plays a stable confirm, reject, tick, click, bump, damage, pickup, or achievement preset."))
	FOpenMobileHapticPlaybackResult PlayGameFeedback(
		EOpenMobileHapticGamePreset Preset,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (AdvancedDisplay = "Options", DisplayName = "Play Game Feedback (Advanced)", ToolTip = "Plays a game preset with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult PlayGameFeedbackAdvanced(
		EOpenMobileHapticGamePreset Preset,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Play Semantic Feedback", ToolTip = "Requests portable semantic feedback with useful UI defaults."))
	FOpenMobileHapticPlaybackResult PlaySemanticFeedback(
		EOpenMobileHapticSemanticEffect Effect,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (AdvancedDisplay = "Options", DisplayName = "Play Semantic Feedback (Advanced)", ToolTip = "Requests semantic feedback with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult PlaySemanticFeedbackAdvanced(
		EOpenMobileHapticSemanticEffect Effect,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Vibrate", ToolTip = "Requests a short one-shot phone vibration with bounded portable defaults."))
	FOpenMobileHapticPlaybackResult Vibrate(
		float DurationSeconds = 0.05f,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (AdvancedDisplay = "Options", DisplayName = "Vibrate (Advanced)", ToolTip = "Requests one-shot phone vibration with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult VibrateAdvanced(
		float DurationSeconds,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Play Named Haptic Pattern", ToolTip = "Requests a prepared named Haptics pattern with gameplay defaults."))
	FOpenMobileHapticPlaybackResult PlayNamedPattern(
		FName PatternName,
		float Intensity = 1.0f,
		FName Channel = NAME_None
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (AdvancedDisplay = "Options", DisplayName = "Play Named Haptic Pattern (Advanced)", ToolTip = "Requests a named Haptics pattern with explicit scheduling, channel, overlap, loop, priority, and fallback options."))
	FOpenMobileHapticPlaybackResult PlayNamedPatternAdvanced(
		FName PatternName,
		float Intensity,
		const FOpenMobileHapticPlaybackOptions& Options
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Calibrate Haptic Timing Clock", ToolTip = "Captures a game or audio clock sample against platform monotonic time for absolute Haptics scheduling."))
	FOpenMobileHapticTimingCalibrationResult CalibrateTimingClock(
		EOpenMobileHapticTimingClock Clock,
		double ClockTimeSeconds,
		double EstimatedPrecisionSeconds = 0.005
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Preload Named Haptic Libraries", ToolTip = "Asynchronously loads configured Haptics libraries and their portable patterns."))
	FOpenMobileHapticLibraryPreloadHandle PreloadNamedLibraries();

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Cancel Named Haptic Library Preload", ToolTip = "Cancels the matching active library preload and ignores its late callbacks."))
	FOpenMobileHapticControlResult CancelNamedLibraryPreload(
		FOpenMobileHapticLibraryPreloadHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Release Named Haptic Libraries", ToolTip = "Releases prepared named libraries and their loaded portable patterns."))
	void ReleaseNamedLibraries();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Named Haptic Pattern Status", ToolTip = "Reports whether a named portable pattern is unprepared, loading, loaded, missing, or invalid."))
	EOpenMobileHapticNamedPatternStatus GetNamedPatternStatus(
		FName PatternName
	) const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Haptic Preparation State", ToolTip = "Reports aggregate named-asset and native prewarm readiness."))
	EOpenMobileHapticPreparationState GetPreparationState() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Stop Haptic Playback", ToolTip = "Stops plugin-owned work for one playback handle."))
	FOpenMobileHapticControlResult StopPlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Cancel Haptic Playback", ToolTip = "Cancels pending or scheduled plugin-owned work for one playback handle."))
	FOpenMobileHapticControlResult CancelPlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Update Haptic Playback Parameters", ToolTip = "Queues normalized intensity or sharpness changes for one active handle. Updates use latest-value coalescing and are submitted to native playback as soon as the configured rate limit allows."))
	FOpenMobileHapticControlResult UpdatePlaybackParameters(
		FOpenMobileHapticPlaybackHandle Handle,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Pause Haptic Playback", ToolTip = "Pauses a playback handle only when its resolved native or portable path supports pause."))
	FOpenMobileHapticControlResult PausePlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Resume Haptic Playback", ToolTip = "Resumes a paused playback handle using its resolved native or portable control path."))
	FOpenMobileHapticControlResult ResumePlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Seek Haptic Playback", ToolTip = "Moves a controllable playback handle to a validated timeline position and reports any platform quantization."))
	FOpenMobileHapticControlResult SeekPlayback(
		FOpenMobileHapticPlaybackHandle Handle,
		double PositionSeconds
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Stop Haptic Channel", ToolTip = "Stops plugin-owned work on one named Haptics channel."))
	FOpenMobileHapticControlResult StopChannel(FName Channel);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Stop All Haptics", ToolTip = "Stops all phone haptics owned by this plugin."))
	FOpenMobileHapticControlResult StopAll();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Haptic Playback State", ToolTip = "Returns the latest state for a playback handle without waiting."))
	EOpenMobileHapticPlaybackState GetPlaybackState(
		FOpenMobileHapticPlaybackHandle Handle
	) const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Is Haptics Enabled", ToolTip = "Returns the current per-player global Haptics switch held by this Game Instance."))
	bool IsHapticsEnabled() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Set Haptics Enabled", ToolTip = "Updates the current per-player global Haptics switch without saving it. Games remain responsible for persistence."))
	FOpenMobileHapticControlResult SetHapticsEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Haptics Master Intensity", ToolTip = "Returns the finite normalized per-player master intensity held by this Game Instance."))
	float GetMasterIntensity() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Set Haptics Master Intensity", ToolTip = "Updates the finite normalized per-player master intensity without saving it. Games remain responsible for persistence."))
	FOpenMobileHapticControlResult SetMasterIntensity(
		UPARAM(meta = (ClampMin = "0.0", ClampMax = "1.0"))
		float MasterIntensity
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Haptics User Policy", ToolTip = "Returns the current per-player Haptics policy held by this Game Instance."))
	FOpenMobileHapticUserPolicy GetUserPolicy() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Set Haptics User Policy", ToolTip = "Updates the current per-player Haptics policy without persisting it."))
	FOpenMobileHapticControlResult SetUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Haptics Diagnostics", ToolTip = "Returns a bounded snapshot of Haptics state and the latest sanitized error."))
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

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Haptics", meta = (DisplayName = "On Haptic Playback Event", ToolTip = "Broadcasts ordered playback state changes on the game thread."))
	FOpenMobileHapticPlaybackEventDynamic OnPlaybackEvent;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Haptics", meta = (DisplayName = "On Named Haptic Libraries Prepared", ToolTip = "Broadcasts the terminal result of an explicit named-library preload."))
	FOpenMobileHapticLibraryPreloadEventDynamic OnNamedLibrariesPrepared;

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
	friend class UOpenMobileHapticPlaybackAsyncAction;
	friend class FOpenMobileHapticsAsyncContractTest;
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
	FOpenMobileHapticNativePlaybackEvent NativePlaybackEvent;
	TSet<TWeakObjectPtr<UOpenMobileHapticPlaybackAsyncAction>> ActiveAsyncActions;
	mutable TUniquePtr<
		FOpenMobileHapticsSubsystemState,
		FOpenMobileHapticsSubsystemStateDeleter
	> State;
	FDelegateHandle InterruptionDelegateHandle;
	FDelegateHandle RecoveryDelegateHandle;
	FDelegateHandle ApplicationLifecycleDelegateHandle;
	bool bDeinitialized = false;
};
