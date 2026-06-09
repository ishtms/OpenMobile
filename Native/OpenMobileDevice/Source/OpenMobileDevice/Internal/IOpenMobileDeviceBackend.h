#pragma once

#include "CoreMinimal.h"
#include "Features/IModularFeature.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceBrightnessControl.h"
#include "OpenMobileDeviceCapabilities.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceFlashlightTypes.h"
#include "OpenMobileDeviceIdentityTypes.h"
#include "OpenMobileDeviceKeepScreenAwakeControl.h"
#include "OpenMobileDeviceLocaleTypes.h"
#include "OpenMobileDeviceMonitoringCallback.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceOrientationControl.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "OpenMobileDeviceSystemUiControl.h"
#include "OpenMobileDeviceRefreshRateControl.h"

enum class EOpenMobileDeviceBackendDomain : uint8
{
	Identity,
	Environment,
	Power,
	Connectivity,
	Display,
	Accessibility,
	Utility
};

class IOpenMobileDeviceBackend : public IModularFeature
{
public:
	virtual ~IOpenMobileDeviceBackend() = default;

	static FName GetModularFeatureName()
	{
		static const FName FeatureName(TEXT("OpenMobile.Device.Backend"));
		return FeatureName;
	}

	static FName GetDomainCapabilityName(EOpenMobileDeviceBackendDomain Domain)
	{
		switch (Domain)
		{
		case EOpenMobileDeviceBackendDomain::Identity:
			return TEXT("OpenMobile.Device.Backend.Identity");
		case EOpenMobileDeviceBackendDomain::Environment:
			return TEXT("OpenMobile.Device.Backend.Environment");
		case EOpenMobileDeviceBackendDomain::Power:
			return TEXT("OpenMobile.Device.Backend.Power");
		case EOpenMobileDeviceBackendDomain::Connectivity:
			return TEXT("OpenMobile.Device.Backend.Connectivity");
		case EOpenMobileDeviceBackendDomain::Display:
			return TEXT("OpenMobile.Device.Backend.Display");
		case EOpenMobileDeviceBackendDomain::Accessibility:
			return TEXT("OpenMobile.Device.Backend.Accessibility");
		case EOpenMobileDeviceBackendDomain::Utility:
			return TEXT("OpenMobile.Device.Backend.Utility");
		}
		return NAME_None;
	}

	virtual FName GetBackendName() const = 0;
	virtual int32 GetPriority() const { return 0; }
	virtual bool IsAvailable() const { return true; }

	virtual FOpenMobileCapability GetDomainCapability(
		EOpenMobileDeviceBackendDomain Domain
	) const
	{
		FOpenMobileCapability Capability;
		Capability.Name = GetDomainCapabilityName(Domain);
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		return Capability;
	}

	virtual FOpenMobileDeviceCapability GetCapability(FName CapabilityName) const
	{
		FOpenMobileDeviceCapability Capability;
		Capability.Name = CapabilityName;
		Capability.State = EOpenMobileCapabilityState::NotSupported;
		Capability.BackendName = GetBackendName();
		return Capability;
	}

	virtual FOpenMobileDeviceInformationSnapshot GetDeviceInformationSnapshot() const
	{
		return {};
	}

	virtual EOpenMobileDeviceFormFactor GetDeviceFormFactor() const
	{
		return EOpenMobileDeviceFormFactor::Unknown;
	}

	virtual FOpenMobileApplicationMetadataSnapshot GetApplicationMetadataSnapshot() const
	{
		return {};
	}

	virtual FOpenMobileLocaleSnapshot GetLocaleSnapshot() const
	{
		return {};
	}

	virtual FOpenMobileLocaleSnapshot GetLocaleSnapshotAtUtc(
		const FDateTime& UtcInstant
	) const
	{
		static_cast<void>(UtcInstant);
		return GetLocaleSnapshot();
	}

	virtual FOpenMobilePowerSnapshot GetPowerSnapshot() const
	{
		return {};
	}

	virtual FOpenMobileMediaVolumeSnapshot GetMediaVolumeSnapshot() const
	{
		return {};
	}

	virtual FOpenMobileMemorySnapshot GetMemorySnapshot() const
	{
		return {};
	}

	virtual FOpenMobileStorageSnapshot GetStorageSnapshot() const
	{
		return {};
	}

	virtual bool QueryStorageSnapshot(
		FOpenMobileStorageSnapshot& OutSnapshot,
		FOpenMobileError& OutError
	) const
	{
		OutSnapshot = {};
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The active Device backend does not support storage queries.")
		);
		return false;
	}

	virtual int64 GetPlatformLowStorageThresholdBytes(
		const FOpenMobileStorageSnapshot& Snapshot
	) const
	{
		static_cast<void>(Snapshot);
		return 512ll * 1024 * 1024;
	}

	virtual FOpenMobileNetworkPathSnapshot GetNetworkPathSnapshot() const
	{
		return {};
	}

	virtual FOpenMobileWindowDisplaySnapshot GetWindowDisplaySnapshot() const
	{
		return {};
	}

	virtual FOpenMobileBrightnessSnapshot GetBrightnessSnapshot() const
	{
		return {};
	}

	virtual FOpenMobileFlashlightSnapshot GetFlashlightSnapshot() const
	{
		return {};
	}

	virtual FOpenMobileBrightnessResult ApplyBrightness(
		const FOpenMobileBrightnessRequest& Request
	)
	{
		FOpenMobileBrightnessResult Result;
		Result.Request = Request;
		Result.State = EOpenMobileBrightnessApplyState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The active Device backend does not support brightness control.")
		);
		return Result;
	}

	virtual void ClearBrightness() {}

	virtual FOpenMobileKeepScreenAwakeResult ApplyKeepScreenAwake()
	{
		FOpenMobileKeepScreenAwakeResult Result;
		Result.State = EOpenMobileKeepScreenAwakeApplyState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The active Device backend does not support keep-awake control.")
		);
		return Result;
	}

	virtual void ClearKeepScreenAwake() {}

	virtual FOpenMobileSystemUiResult ApplySystemUiMode(
		const FOpenMobileSystemUiRequest& Request
	)
	{
		FOpenMobileSystemUiResult Result;
		Result.Request = Request;
		Result.State = EOpenMobileSystemUiApplyState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The active Device backend does not support system UI control.")
		);
		return Result;
	}

	virtual void ClearSystemUiMode() {}

	virtual FOpenMobilePreferredRefreshRateResult ApplyPreferredRefreshRate(
		const FOpenMobilePreferredRefreshRateRequest& Request
	)
	{
		FOpenMobilePreferredRefreshRateResult Result;
		Result.Request = Request;
		Result.State = EOpenMobilePreferredRefreshRateApplyState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The active Device backend does not support preferred refresh rates.")
		);
		return Result;
	}

	virtual void ClearPreferredRefreshRate() {}

	virtual FOpenMobileOrientationPolicyResult ApplyOrientationPolicy(
		const FOpenMobileOrientationPolicyRequest& Request
	)
	{
		FOpenMobileOrientationPolicyResult Result;
		Result.Request = Request;
		Result.State = EOpenMobileOrientationPolicyApplyState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("The active Device backend does not support orientation control.")
		);
		return Result;
	}

	virtual void ClearOrientationPolicy() {}

	virtual FOpenMobileAppearanceSnapshot GetAppearanceSnapshot() const
	{
		return {};
	}

	virtual FOpenMobileAccessibilitySnapshot GetAccessibilitySnapshot() const
	{
		return {};
	}

	virtual bool StartMonitoring(
		EOpenMobileDeviceMonitoringGroup Group,
		const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
	)
	{
		static_cast<void>(Group);
		static_cast<void>(CallbackToken);
		return false;
	}

	virtual void StopMonitoring(EOpenMobileDeviceMonitoringGroup Group)
	{
		static_cast<void>(Group);
	}

	virtual bool RequiresFallbackPolling(
		EOpenMobileDeviceMonitoringGroup Group
	) const
	{
		static_cast<void>(Group);
		return false;
	}

	virtual void BeginShutdown() {}
};
