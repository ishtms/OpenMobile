#pragma once

#include "HAL/CriticalSection.h"
#include "OpenMobileHapticsAndroidBridge.h"
#include "OpenMobileHapticsPlatformBackend.h"

struct FOpenMobileHapticsPortableTimeline;

struct FOpenMobileHapticsAndroidControlledPlayback
{
	TSharedPtr<
		const FOpenMobileHapticsPortableTimeline,
		ESPMode::ThreadSafe
	> Timeline;
	int32 Purpose = 0;
};

class FOpenMobileHapticsAndroidPlaybackControlStore final
{
public:
	/** Stores the accepted portable timeline by request id so emulated controls can rebuild the remaining waveform. */
	void Add(
		uint64 RequestId,
		FOpenMobileHapticsAndroidControlledPlayback Playback
	);
	/** Copies control state under the store lock, JNI work can continue after the lock is released. */
	bool Find(
		uint64 RequestId,
		FOpenMobileHapticsAndroidControlledPlayback& OutPlayback
	) const;
	/** Removes control data as soon as playback becomes terminal, request ids may be reused much later. */
	void Remove(uint64 RequestId);
	/** Clears every controlled playback when Android service lifecycle is replaced. */
	void Reset();

private:
	mutable FCriticalSection Mutex;
	TMap<uint64, FOpenMobileHapticsAndroidControlledPlayback> Playbacks;
};

class FOpenMobileHapticsAndroidBackend final
	: public FOpenMobileHapticsPlatformBackend
{
public:
	/** Keeps request tokens and diagnostics tied to the Android backend name. */
	virtual FName GetBackendName() const override { return TEXT("Android"); }
	/** Returns a cached stable hardware probe with current project configuration applied. */
	virtual FOpenMobileHapticCapabilities GetCapabilities() const override;
	/** Reports waveform cache preparation separately from ordinary vibrator availability. */
	virtual EOpenMobileHapticPreparationState
	GetPreparationState() const override;
	/** Prepares Android waveforms within native cache limits and fails the batch when any timeline can't be represented. */
	virtual FOpenMobileHapticsBackendPreparationResult PrepareResources(
		const FOpenMobileHapticsBackendPreparationRequest& Request
	) override;
	/** Releases Java waveform cache and resets preparation state together. */
	virtual void ReleasePreparedResources() override;
	/** Applies the Android project switch after device support has been discovered. */
	virtual bool IsCustomPlaybackConfigured() const override;
	/** Advertises controls supported directly or through waveform reconstruction on this Android version. */
	virtual FOpenMobileHapticsBackendControlSupport
	GetControlSupport() const override;
	/** Invalidates cached hardware when the older generic lifecycle hook is used. */
	virtual void HandleLifecycleChange() override;
	/** Cancels or refreshes Android work according to the resolved application transition. */
	virtual void HandleApplicationLifecycle(
		const FOpenMobileHapticsLifecycleTransition& Transition
	) override;
	/** Drops cached service state after Android vibrator interruption or activity replacement. */
	virtual void HandleInterruption(
		EOpenMobileHapticsInterruptionReason Reason
	) override;
	/** Re-probes JNI service state and tells registry whether another retry could help. */
	virtual EOpenMobileHapticsRecoveryResult
	RecoverFromInterruption() override;
	/** Resolves semantic playback into the matching Java route and wires scheduled callbacks when needed. */
	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsSemanticResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	/** Stops scheduled or active work for the exact request token and clears its control timeline. */
	virtual FOpenMobileHapticControlResult StopPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token
	) override;
	/** Pauses a controlled waveform at a revisioned playhead, ordinary vibrator requests stay unsupported. */
	virtual FOpenMobileHapticControlResult PausePlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	) override;
	/** Rebuilds the remaining waveform from policy state before asking Java to resume. */
	virtual FOpenMobileHapticControlResult ResumePlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	) override;
	/** Rebuilds waveform segments from the resolved seek position and forwards the new revision. */
	virtual FOpenMobileHapticControlResult SeekPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	) override;
	/** Cancels Java vibrator work and clears every locally tracked controlled playback. */
	virtual FOpenMobileHapticControlResult StopAll() override;
	/** Seals JNI callbacks and cache state before the Android module unregisters. */
	virtual void BeginShutdown() override;
	/** Submits one-shot vibration through the already selected semantic, predefined, or basic route. */
	virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsOneShotResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	/** Submits the prepared Android timeline or its accepted fallback while retaining data needed for controls. */
	virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;

private:
	/** Converts raw JNI capability flags and limits into the stable cross-platform capability contract. */
	FOpenMobileHapticCapabilities ProbeHardwareCapabilities() const;

	mutable FCriticalSection CacheMutex;
	mutable FCriticalSection PreparationMutex;
	mutable TOptional<FOpenMobileHapticCapabilities> StableCapabilities;
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;
	FOpenMobileHapticsAndroidPlaybackControlStore PlaybackControlStore;
	mutable FOpenMobileHapticsAndroidBridge Bridge;
};
