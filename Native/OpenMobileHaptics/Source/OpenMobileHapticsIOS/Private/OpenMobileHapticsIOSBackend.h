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
	virtual void BeginShutdown() override;

private:
	FOpenMobileHapticCapabilities ProbeHardwareCapabilities() const;

	TUniquePtr<FOpenMobileHapticsAppleBridgeService> BridgeService;
};
