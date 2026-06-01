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

class UOpenMobileHapticPlaybackAsyncAction;
struct FOpenMobileHapticsBackendCallback;
struct FOpenMobileHapticsSubsystemState;

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

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Stop Haptic Playback", ToolTip = "Stops plugin-owned work for one playback handle."))
	FOpenMobileHapticControlResult StopPlayback(
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Stop Haptic Channel", ToolTip = "Stops plugin-owned work on one named Haptics channel."))
	FOpenMobileHapticControlResult StopChannel(FName Channel);

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Stop All Haptics", ToolTip = "Stops all phone haptics owned by this plugin."))
	FOpenMobileHapticControlResult StopAll();

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Haptic Playback State", ToolTip = "Returns the latest state for a playback handle without waiting."))
	EOpenMobileHapticPlaybackState GetPlaybackState(
		FOpenMobileHapticPlaybackHandle Handle
	) const;

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Haptics User Policy", ToolTip = "Returns the current per-player Haptics policy held by this Game Instance."))
	FOpenMobileHapticUserPolicy GetUserPolicy() const;

	UFUNCTION(BlueprintCallable, Category = "Open Mobile|Haptics", meta = (DisplayName = "Set Haptics User Policy", ToolTip = "Updates the current per-player Haptics policy without persisting it."))
	FOpenMobileHapticControlResult SetUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Haptics", meta = (DisplayName = "Get Haptics Diagnostics", ToolTip = "Returns a bounded snapshot of Haptics state and the latest sanitized error."))
	FOpenMobileHapticsDiagnostics GetDiagnostics() const;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Haptics", meta = (DisplayName = "On Haptic Playback Event", ToolTip = "Broadcasts ordered playback state changes on the game thread."))
	FOpenMobileHapticPlaybackEventDynamic OnPlaybackEvent;

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
	virtual FOpenMobileHapticControlResult StopChannelNative(
		FName Channel
	) override;
	virtual FOpenMobileHapticControlResult StopAllNative() override;
	virtual EOpenMobileHapticPlaybackState GetPlaybackStateNative(
		FOpenMobileHapticPlaybackHandle Handle
	) const override;
	virtual FOpenMobileHapticUserPolicy GetUserPolicyNative() const override;
	virtual FOpenMobileHapticControlResult UpdateUserPolicy(
		const FOpenMobileHapticUserPolicy& Policy
	) override;
	virtual FOpenMobileHapticsDiagnostics GetDiagnosticsNative() const override;
	virtual FOpenMobileHapticNativePlaybackEvent& OnPlaybackEventNative() override;

private:
	friend class UOpenMobileHapticPlaybackAsyncAction;
	friend class FOpenMobileHapticsAsyncContractTest;

	void RegisterAsyncAction(UOpenMobileHapticPlaybackAsyncAction* Action);
	void UnregisterAsyncAction(UOpenMobileHapticPlaybackAsyncAction* Action);
	FOpenMobileHapticsSubsystemState& GetOrCreateState() const;
	TFunction<void(const FOpenMobileHapticsBackendCallback&)>
	MakeBackendCallback();
	void HandleBackendCallback(
		const FOpenMobileHapticsBackendCallback& Callback
	);

	FOpenMobileHapticUserPolicy UserPolicy;
	TAtomic<bool> bUserPolicyEnabled = true;
	FOpenMobileHapticNativePlaybackEvent NativePlaybackEvent;
	TSet<TWeakObjectPtr<UOpenMobileHapticPlaybackAsyncAction>> ActiveAsyncActions;
	mutable TUniquePtr<
		FOpenMobileHapticsSubsystemState,
		FOpenMobileHapticsSubsystemStateDeleter
	> State;
	bool bDeinitialized = false;
};
