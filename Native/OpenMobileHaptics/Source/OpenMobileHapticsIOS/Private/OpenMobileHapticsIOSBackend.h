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

	TUniquePtr<FOpenMobileHapticsAppleBridgeService> BridgeService;
};
