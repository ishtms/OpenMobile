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
	virtual FOpenMobileApplicationMetadataSnapshot
	GetApplicationMetadataSnapshot() const override;
	virtual FOpenMobileLocaleSnapshot GetLocaleSnapshot() const override;
	virtual FOpenMobileLocaleSnapshot GetLocaleSnapshotAtUtc(
		const FDateTime& UtcInstant
	) const override;
	virtual FOpenMobileMemorySnapshot GetMemorySnapshot() const override;
	virtual bool QueryStorageSnapshot(
		FOpenMobileStorageSnapshot& OutSnapshot,
		FOpenMobileError& OutError
	) const override;
	virtual FOpenMobilePowerSnapshot GetPowerSnapshot() const override;
	virtual FOpenMobileMediaVolumeSnapshot GetMediaVolumeSnapshot() const override;
	virtual bool StartMonitoring(
		EOpenMobileDeviceMonitoringGroup Group,
		const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
	) override;
	virtual void StopMonitoring(
		EOpenMobileDeviceMonitoringGroup Group
	) override;
	virtual void BeginShutdown() override;
};
