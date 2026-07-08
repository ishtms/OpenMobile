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
	void Add(
		uint64 RequestId,
		FOpenMobileHapticsAndroidControlledPlayback Playback
	);
	bool Find(
		uint64 RequestId,
		FOpenMobileHapticsAndroidControlledPlayback& OutPlayback
	) const;
	void Remove(uint64 RequestId);
	void Reset();

private:
	mutable FCriticalSection Mutex;
	TMap<uint64, FOpenMobileHapticsAndroidControlledPlayback> Playbacks;
};

class FOpenMobileHapticsAndroidBackend final
	: public FOpenMobileHapticsPlatformBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("Android"); }
	virtual FOpenMobileHapticCapabilities GetCapabilities() const override;
	virtual EOpenMobileHapticPreparationState
	GetPreparationState() const override;
	virtual FOpenMobileHapticsBackendPreparationResult PrepareResources(
		const FOpenMobileHapticsBackendPreparationRequest& Request
	) override;
	virtual void ReleasePreparedResources() override;
	virtual bool IsCustomPlaybackConfigured() const override;
	virtual FOpenMobileHapticsBackendControlSupport
	GetControlSupport() const override;
	virtual void HandleLifecycleChange() override;
	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsSemanticResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	virtual FOpenMobileHapticControlResult StopPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token
	) override;
	virtual FOpenMobileHapticControlResult PausePlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	) override;
	virtual FOpenMobileHapticControlResult ResumePlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	) override;
	virtual FOpenMobileHapticControlResult SeekPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticsBackendControlCommand& Command
	) override;
	virtual FOpenMobileHapticControlResult StopAll() override;
	virtual void BeginShutdown() override;
	virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsOneShotResolution& Resolution,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;

private:
	FOpenMobileHapticCapabilities ProbeHardwareCapabilities() const;

	mutable FCriticalSection CacheMutex;
	mutable FCriticalSection PreparationMutex;
	mutable TOptional<FOpenMobileHapticCapabilities> StableCapabilities;
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;
	FOpenMobileHapticsAndroidPlaybackControlStore PlaybackControlStore;
	mutable FOpenMobileHapticsAndroidBridge Bridge;
};
