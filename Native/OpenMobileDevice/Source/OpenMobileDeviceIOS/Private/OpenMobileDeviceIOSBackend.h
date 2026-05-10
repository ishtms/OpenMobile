#pragma once

#include "IOpenMobileDeviceBackend.h"

class FOpenMobileDeviceIOSBackend final : public IOpenMobileDeviceBackend
{
public:
	virtual FName GetBackendName() const override { return TEXT("IOS"); }
	virtual FOpenMobileCapability GetDomainCapability(
		EOpenMobileDeviceBackendDomain Domain
	) const override;
	virtual FOpenMobileDeviceCapability GetCapability(
		FName CapabilityName
	) const override;
	virtual FOpenMobileDeviceInformationSnapshot
	GetDeviceInformationSnapshot() const override;
	virtual EOpenMobileDeviceFormFactor GetDeviceFormFactor() const override;
	virtual FOpenMobilePowerSnapshot GetPowerSnapshot() const override;
	virtual FOpenMobileMediaVolumeSnapshot GetMediaVolumeSnapshot() const override;
};
