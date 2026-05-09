#pragma once

#include "IOpenMobileDeviceBackend.h"

class FOpenMobileDeviceAndroidBackend final : public IOpenMobileDeviceBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("Android"); }
	virtual FOpenMobileCapability GetDomainCapability(
		EOpenMobileDeviceBackendDomain Domain
	) const override;
	virtual FOpenMobileDeviceCapability GetCapability(
		FName CapabilityName
	) const override;
	virtual FOpenMobileDeviceInformationSnapshot
	GetDeviceInformationSnapshot() const override;
	virtual FOpenMobilePowerSnapshot GetPowerSnapshot() const override;
	virtual FOpenMobileMediaVolumeSnapshot GetMediaVolumeSnapshot() const override;
};
