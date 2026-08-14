#pragma once

#include "Android/AndroidJNI.h"
#include "HAL/CriticalSection.h"
#include "IOpenMobileHapticsBackend.h"
#include "OpenMobileHapticPlatformAssets.h"

struct FOpenMobileHapticsAndroidHardwareProbe
{
	int64 Flags = -1;
	uint64 PresetSupport = 0;
	uint64 PrimitiveSupport = 0;
	int64 MaximumControlPointCount = -1;
	int64 MaximumDurationMillis = -1;
	int64 MinimumTimingMillis = -1;
	int64 MaximumControlPointDurationMillis = -1;
	int64 MinimumFrequencyMilliHertz = -1;
	int64 MaximumFrequencyMilliHertz = -1;
};

struct FOpenMobileHapticsAndroidBridgeSubmission
{
	int32 Result = 0;
	bool bExpectsCallback = false;
};

struct FOpenMobileHapticsAndroidScheduledPlayback
{
	int64 StartDelayMilliseconds = 0;
	TSharedPtr<
		FOpenMobileHapticsScheduledStartGuard,
		ESPMode::ThreadSafe
	> ScheduledStartGuard;
	FName PatternOrEffect;
	FName Channel;
	FName ResolvedPath;
	FOpenMobileHapticsBackendEventCallback Callback;
};

class FOpenMobileHapticsAndroidBridge final
{
public:
	/** Leaves JNI lookup lazy because module startup can run before the Android activity and class loader are ready. */
	FOpenMobileHapticsAndroidBridge();
	/** Releases the global Java class reference and seals pending callbacks before native unload. */
	~FOpenMobileHapticsAndroidBridge();

	/** Fetches vibrator features and limits in one JNI call so the backend caches a coherent hardware snapshot. */
	FOpenMobileHapticsAndroidHardwareProbe QueryHardware();
	/** Sends semantic playback through Java and registers completion only when the route is scheduled. */
	FOpenMobileHapticsAndroidBridgeSubmission PlaySemantic(
		const FOpenMobileHapticsBackendRequestToken& Token,
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity,
		EOpenMobileHapticsSemanticPath Path,
		int32 Purpose,
		int64 StartDelayMilliseconds,
		TSharedPtr<
			FOpenMobileHapticsScheduledStartGuard,
			ESPMode::ThreadSafe
		> ScheduledStartGuard,
		FName PatternOrEffect,
		FName Channel,
		FName ResolvedPath,
		FOpenMobileHapticsBackendEventCallback Callback
	);
	/** Sends a bounded one-shot request and optionally defers start through the scheduled callback record. */
	int32 PlayOneShot(
		const FOpenMobileHapticsBackendRequestToken& Token,
		int64 DurationMillis,
		float Intensity,
		EOpenMobileHapticsOneShotPath Path,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	/** Sends waveform arrays or a prepared resource id, Java owns the actual vibrator call. */
	int32 PlayWaveform(
		const FOpenMobileHapticsBackendRequestToken& Token,
		uint64 PreparedResourceId,
		const TArray<int64>& TimingsMilliseconds,
		const TArray<int32>& Amplitudes,
		int32 RepeatIndex,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	/** Starts a waveform whose terminal and revision events must return to native control state. */
	int32 PlayControlledWaveform(
		const FOpenMobileHapticsBackendRequestToken& Token,
		uint64 PreparedResourceId,
		const TArray<int64>& TimingsMilliseconds,
		const TArray<int32>& Amplitudes,
		int32 RepeatIndex,
		int32 Purpose,
		int64 CompletionDurationMilliseconds,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled
	);
	/** Freezes the Java control record only when the supplied revision is newer than prior commands. */
	int32 PauseControlledWaveform(uint64 RequestId, uint64 Revision);
	/** Replaces the remaining Java waveform for resume while retaining ordered control events. */
	int32 ResumeControlledWaveform(
		uint64 RequestId,
		uint64 Revision,
		const TArray<int64>& TimingsMilliseconds,
		const TArray<int32>& Amplitudes,
		int32 RepeatIndex,
		int32 Purpose,
		int64 CompletionDurationMilliseconds
	);
	/** Replaces the remaining Java waveform for seek and associates callbacks with the new revision. */
	int32 SeekControlledWaveform(
		uint64 RequestId,
		uint64 Revision,
		const TArray<int64>& TimingsMilliseconds,
		const TArray<int32>& Amplitudes,
		int32 RepeatIndex,
		int32 Purpose,
		int64 CompletionDurationMilliseconds
	);
	/** Cancels one controlled waveform and removes native callback ownership together. */
	bool StopControlledWaveform(uint64 RequestId);
	/** Copies a validated waveform into Java's bounded prepared cache using the same byte estimate as native policy. */
	int32 PrepareWaveform(
		uint64 ResourceId,
		const TArray<int64>& TimingsMilliseconds,
		const TArray<int32>& Amplitudes,
		int32 RepeatIndex,
		int64 EstimatedBytes,
		const FOpenMobileHapticsPreparedResourceLimits& Limits
	);
	/** Clears Java prepared resources when library or lifecycle generation changes. */
	void ReleasePreparedResources();
	/** Cancels a delayed Java start before its runnable reaches the vibrator service. */
	bool CancelScheduled(uint64 RequestId);
	/** Plays one Android predefined effect after policy has checked API and device support. */
	int32 PlayPredefined(
		const FOpenMobileHapticsBackendRequestToken& Token,
		int32 Effect,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	/** Sends validated primitive ids, scales, and delays as paired JNI arrays. */
	int32 PlayPrimitives(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const TArray<EOpenMobileHapticAndroidPrimitive>& Primitives,
		const TArray<float>& Scales,
		const TArray<int32>& DelaysMilliseconds,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	/** Sends a validated basic or frequency envelope using the format accepted by the current API. */
	int32 PlayEnvelope(
		const FOpenMobileHapticsBackendRequestToken& Token,
		EOpenMobileHapticAndroidPatternFormat Format,
		const TArray<float>& Amplitudes,
		const TArray<float>& ControlValues,
		const TArray<int64>& DurationsMilliseconds,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	/** Cancels all Java vibrator activity and pending scheduled callbacks. */
	bool StopAll();
	/** Seals callback lookup and releases JNI references, it can be called more than once during teardown. */
	void Shutdown();
	/** Lets Java check the native lifecycle guard immediately before a delayed start. */
	bool HandleCanStart(uint64 RequestId) const;
	/** Converts scheduled Java completion into one ordered backend event and removes terminal callback state. */
	void HandleBridgeResult(uint64 RequestId, int32 Result);
	/** Filters stale control revisions and native event sequences before forwarding a controlled waveform event. */
	void HandleControlledWaveformEvent(
		uint64 RequestId,
		uint64 ControlRevision,
		uint64 EventSequence,
		int32 Event
	);

private:
	struct FPendingCallback
	{
		FOpenMobileHapticsBackendRequestToken Token;
		TSharedPtr<
			FOpenMobileHapticsScheduledStartGuard,
			ESPMode::ThreadSafe
		> ScheduledStartGuard;
		FName PatternOrEffect;
		FName Channel;
		FName ResolvedPath;
		FOpenMobileHapticsBackendEventCallback Callback;
		uint64 LastControlRevision = 0;
		uint64 LastNativeEventSequence = 0;
		uint64 LastForwardedEventSequence = 0;
		bool bControlledWaveform = false;
	};

	/** Resolves the Java bridge class and every method id once, partial lookup is cleared and retried later. */
	bool EnsureInitialized(JNIEnv* Env);
	/** Clears a pending JNI exception after a failed bridge call so later JNI work isn't poisoned. */
	void ClearException(JNIEnv* Env);
	/** Stores callback state only when Java accepted a scheduled request that will report later. */
	void RegisterScheduledCallback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		int32 Result,
		FOpenMobileHapticsAndroidScheduledPlayback&& Scheduled
	);
	/** Shares JNI array conversion and revision handling between resume and seek commands. */
	int32 UpdateControlledWaveform(
		jmethodID Method,
		uint64 RequestId,
		uint64 Revision,
		const TArray<int64>& TimingsMilliseconds,
		const TArray<int32>& Amplitudes,
		int32 RepeatIndex,
		int32 Purpose,
		int64 CompletionDurationMilliseconds
	);

	mutable FCriticalSection Mutex;
	jclass BridgeClass = nullptr;
	jmethodID QueryCapabilitiesMethod = nullptr;
	jmethodID PlaySemanticMethod = nullptr;
	jmethodID PlayOneShotMethod = nullptr;
	jmethodID PrepareWaveformMethod = nullptr;
	jmethodID PlayWaveformMethod = nullptr;
	jmethodID PlayControlledWaveformMethod = nullptr;
	jmethodID PauseControlledWaveformMethod = nullptr;
	jmethodID ResumeControlledWaveformMethod = nullptr;
	jmethodID SeekControlledWaveformMethod = nullptr;
	jmethodID StopControlledWaveformMethod = nullptr;
	jmethodID PlayPredefinedMethod = nullptr;
	jmethodID PlayPrimitivesMethod = nullptr;
	jmethodID PlayEnvelopeMethod = nullptr;
	jmethodID StopAllMethod = nullptr;
	jmethodID CancelScheduledMethod = nullptr;
	jmethodID ReleasePreparedResourcesMethod = nullptr;
	TMap<uint64, FPendingCallback> PendingCallbacks;
};
