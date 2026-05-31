#pragma once

#include "HAL/CriticalSection.h"
#include "OpenMobileHapticsPlatformBackend.h"

class FOpenMobileHapticsAndroidBackend final
	: public FOpenMobileHapticsPlatformBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("Android"); }
	virtual FOpenMobileHapticCapabilities GetCapabilities() const override;
	virtual FOpenMobileHapticsBackendSubmission SubmitSemantic(
		const FOpenMobileHapticSemanticRequest& Request,
		const FOpenMobileHapticsSemanticResolution& Resolution,
		const FOpenMobileHapticsBackendRequestToken& Token,
		FOpenMobileHapticsBackendEventCallback Callback
	) override;

private:
	FOpenMobileHapticCapabilities ProbeHardwareCapabilities() const;

	mutable FCriticalSection CacheMutex;
	mutable TOptional<FOpenMobileHapticCapabilities> StableCapabilities;
};
