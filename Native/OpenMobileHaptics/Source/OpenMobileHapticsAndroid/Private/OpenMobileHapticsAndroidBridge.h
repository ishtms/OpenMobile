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
	FName PatternOrEffect;
	FName Channel;
	FName ResolvedPath;
	FOpenMobileHapticsBackendEventCallback Callback;
};

class FOpenMobileHapticsAndroidBridge final
{
public:
	FOpenMobileHapticsAndroidBridge();
	~FOpenMobileHapticsAndroidBridge();

	FOpenMobileHapticsAndroidHardwareProbe QueryHardware();
	FOpenMobileHapticsAndroidBridgeSubmission PlaySemantic(
		const FOpenMobileHapticsBackendRequestToken& Token,
		EOpenMobileHapticsSemanticBehavior Behavior,
		float Intensity,
		EOpenMobileHapticsSemanticPath Path,
		int32 Purpose,
		int64 StartDelayMilliseconds,
		FName PatternOrEffect,
		FName Channel,
		FName ResolvedPath,
		FOpenMobileHapticsBackendEventCallback Callback
	);
	int32 PlayOneShot(
		const FOpenMobileHapticsBackendRequestToken& Token,
		int64 DurationMillis,
		float Intensity,
		EOpenMobileHapticsOneShotPath Path,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	int32 PlayWaveform(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const TArray<int64>& TimingsMilliseconds,
		const TArray<int32>& Amplitudes,
		int32 RepeatIndex,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	int32 PlayPredefined(
		const FOpenMobileHapticsBackendRequestToken& Token,
		int32 Effect,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	int32 PlayPrimitives(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const TArray<EOpenMobileHapticAndroidPrimitive>& Primitives,
		const TArray<float>& Scales,
		const TArray<int32>& DelaysMilliseconds,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	int32 PlayEnvelope(
		const FOpenMobileHapticsBackendRequestToken& Token,
		EOpenMobileHapticAndroidPatternFormat Format,
		const TArray<float>& Amplitudes,
		const TArray<float>& ControlValues,
		const TArray<int64>& DurationsMilliseconds,
		int32 Purpose,
		FOpenMobileHapticsAndroidScheduledPlayback Scheduled = {}
	);
	bool StopAll();
	void Shutdown();
	void HandleBridgeResult(uint64 RequestId, int32 Result);

private:
	struct FPendingCallback
	{
		FOpenMobileHapticsBackendRequestToken Token;
		FName PatternOrEffect;
		FName Channel;
		FName ResolvedPath;
		FOpenMobileHapticsBackendEventCallback Callback;
	};

	bool EnsureInitialized(JNIEnv* Env);
	void ClearException(JNIEnv* Env);
	void RegisterScheduledCallback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		int32 Result,
		FOpenMobileHapticsAndroidScheduledPlayback&& Scheduled
	);

	FCriticalSection Mutex;
	jclass BridgeClass = nullptr;
	jmethodID QueryCapabilitiesMethod = nullptr;
	jmethodID PlaySemanticMethod = nullptr;
	jmethodID PlayOneShotMethod = nullptr;
	jmethodID PlayWaveformMethod = nullptr;
	jmethodID PlayPredefinedMethod = nullptr;
	jmethodID PlayPrimitivesMethod = nullptr;
	jmethodID PlayEnvelopeMethod = nullptr;
	jmethodID StopAllMethod = nullptr;
	TMap<uint64, FPendingCallback> PendingCallbacks;
};
