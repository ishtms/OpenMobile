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
	virtual FOpenMobileNetworkPathSnapshot GetNetworkPathSnapshot() const override;
	virtual FOpenMobileWindowDisplaySnapshot
	GetWindowDisplaySnapshot() const override;
	virtual FOpenMobileBrightnessSnapshot GetBrightnessSnapshot() const override;
	virtual FOpenMobileFlashlightSnapshot GetFlashlightSnapshot() const override;
	virtual FOpenMobileFlashlightOperationResult ApplyFlashlight(
		const FOpenMobileFlashlightRequest& Request
	) override;
	virtual void ClearFlashlight() override;
	virtual FOpenMobileClipboardOperationResult
	CheckClipboardContentTypes() const override;
	virtual FOpenMobileClipboardOperationResult WriteClipboard(
		const FOpenMobileClipboardWriteRequest& Request
	) override;
	virtual FOpenMobileClipboardOperationResult ReadClipboard(
		EOpenMobileClipboardContentType ContentType
	) override;
	virtual FOpenMobileClipboardOperationResult ClearClipboard() override;
	virtual bool BeginUserInitiatedPaste(
		const FOpenMobileUserInitiatedPasteRequest& Request,
		const FGuid& OperationId,
		FOpenMobileDeviceUserInitiatedPasteCompletion&& Completion,
		FOpenMobileError& OutError
	) override;
	virtual void CancelUserInitiatedPaste(
		const FGuid& OperationId
	) override;
	virtual FOpenMobileBrightnessResult ApplyBrightness(
		const FOpenMobileBrightnessRequest& Request
	) override;
	virtual void ClearBrightness() override;
	virtual FOpenMobileKeepScreenAwakeResult ApplyKeepScreenAwake() override;
	virtual void ClearKeepScreenAwake() override;
	virtual FOpenMobileSystemUiResult ApplySystemUiMode(
		const FOpenMobileSystemUiRequest& Request
	) override;
	virtual void ClearSystemUiMode() override;
	virtual FOpenMobileOrientationPolicyResult ApplyOrientationPolicy(
		const FOpenMobileOrientationPolicyRequest& Request
	) override;
	virtual void ClearOrientationPolicy() override;
	virtual bool QueryStorageSnapshot(
		FOpenMobileStorageSnapshot& OutSnapshot,
		FOpenMobileError& OutError
	) const override;
	virtual int64 GetPlatformLowStorageThresholdBytes(
		const FOpenMobileStorageSnapshot& Snapshot
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
