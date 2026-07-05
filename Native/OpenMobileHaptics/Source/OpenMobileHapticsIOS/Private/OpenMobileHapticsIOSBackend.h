#pragma once

#include "OpenMobileHapticsPlatformBackend.h"

class FOpenMobileHapticsAppleBridgeService;

class FOpenMobileHapticsIOSBackend final
	: public FOpenMobileHapticsPlatformBackend
{
public:
	FOpenMobileHapticsIOSBackend();
	virtual ~FOpenMobileHapticsIOSBackend() override;

	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	virtual FOpenMobileHapticCapabilities GetCapabilities() const override;
	virtual EOpenMobileHapticPreparationState
	GetPreparationState() const override;
	virtual FOpenMobileHapticsBackendPreparationResult PrepareResources(
		const FOpenMobileHapticsBackendPreparationRequest& Request
	) override;
	virtual void ReleasePreparedResources() override;
	virtual FOpenMobileHapticsBackendControlSupport
	GetControlSupport() const override;
	virtual void HandleLifecycleChange() override;
	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsSemanticResolution& Resolution,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	virtual FOpenMobileHapticsBackendSubmission SubmitOneShot(
		const FOpenMobileHapticOneShotRequest& Request,
		const FOpenMobileHapticsOneShotResolution& Resolution,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	virtual FOpenMobileHapticsBackendSubmission SubmitNamedPattern(
		const FOpenMobileHapticNamedPatternRequest& Request,
		const FOpenMobileHapticsBackendPlaybackParameters& Parameters,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;
	virtual FOpenMobileHapticControlResult StopPlayback(
		const FOpenMobileHapticsBackendRequestToken& Token
	) override;
	virtual FOpenMobileHapticControlResult UpdatePlaybackParameters(
		const FOpenMobileHapticsBackendRequestToken& Token,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) override;
	virtual void BeginShutdown() override;

private:
	FOpenMobileHapticCapabilities ProbeHardwareCapabilities() const;
	void RememberAHAPIntensityScale(uint64 RequestId, float Scale);
	void ForgetAHAPIntensityScale(uint64 RequestId);
	void ForgetAllAHAPIntensityScales();
	FOpenMobileHapticDynamicParameterUpdate ComposeAHAPDynamicUpdate(
		uint64 RequestId,
		const FOpenMobileHapticDynamicParameterUpdate& Update
	) const;

	TUniquePtr<FOpenMobileHapticsAppleBridgeService> BridgeService;
	mutable FCriticalSection PreparationMutex;
	EOpenMobileHapticPreparationState PreparationState =
		EOpenMobileHapticPreparationState::Unprepared;
	mutable FCriticalSection AHAPIntensityMutex;
	TMap<uint64, float> AHAPStaticIntensityScales;
};
