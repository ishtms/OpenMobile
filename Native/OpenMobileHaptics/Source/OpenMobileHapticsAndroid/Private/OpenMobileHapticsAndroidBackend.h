#pragma once

#include "HAL/CriticalSection.h"
#include "OpenMobileHapticsAndroidBridge.h"
#include "OpenMobileHapticsPlatformBackend.h"

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
	mutable FOpenMobileHapticsAndroidBridge Bridge;
};
