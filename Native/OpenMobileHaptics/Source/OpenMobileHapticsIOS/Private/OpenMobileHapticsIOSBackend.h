#pragma once

#include "HAL/CriticalSection.h"
#include "OpenMobileHapticsPlatformBackend.h"

class FOpenMobileHapticsIOSBackend final
	: public FOpenMobileHapticsPlatformBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	virtual FOpenMobileHapticCapabilities GetCapabilities() const override;
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

	mutable FCriticalSection CacheMutex;
	mutable TOptional<FOpenMobileHapticCapabilities> StableCapabilities;
	void* SemanticGeneratorCache = nullptr;
};
