#if WITH_DEV_AUTOMATION_TESTS

#include "IOpenMobileDeviceBackend.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBrightnessControlService.h"
#include "OpenMobileDeviceDiagnostics.h"
#include "OpenMobileDeviceDiagnosticsSource.h"
#include "OpenMobileDeviceKeepScreenAwakeControlService.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceOrientationControlService.h"
#include "OpenMobileDeviceRefreshRateControlService.h"
#include "OpenMobileDeviceSettings.h"
#include "OpenMobileDeviceSystemUiControlService.h"

namespace OpenMobileDeviceDiagnosticsTests
{
	class FBackend final : public IOpenMobileDeviceBackend
	{
	public:
		virtual FName GetBackendName() const override
		{
			return TEXT("DiagnosticsTest");
		}

		virtual FOpenMobileCapability GetDomainCapability(
			EOpenMobileDeviceBackendDomain Domain
		) const override
		{
			FOpenMobileCapability Capability;
			Capability.Name = GetDomainCapabilityName(Domain);
			Capability.State = EOpenMobileCapabilityState::Available;
			return Capability;
		}

		virtual FOpenMobileDeviceCapability GetCapability(
			FName CapabilityName
		) const override
		{
			FOpenMobileDeviceCapability Capability;
			Capability.Name = CapabilityName;
			Capability.State = CapabilityName
				== FOpenMobileDeviceCapabilityNames::ChargingSource
				? EOpenMobileCapabilityState::NotSupported
				: EOpenMobileCapabilityState::Available;
			Capability.Limit = CapabilityName
				== FOpenMobileDeviceCapabilityNames::ChargingSource
				? EOpenMobileDeviceCapabilityLimit::UnsupportedPlatform
				: EOpenMobileDeviceCapabilityLimit::None;
			Capability.BackendName = GetBackendName();
			return Capability;
		}

		virtual FOpenMobileDeviceInformationSnapshot
		GetDeviceInformationSnapshot() const override
		{
			FOpenMobileDeviceInformationSnapshot Snapshot;
			Snapshot.Platform = EOpenMobileDevicePlatform::Android;
			Snapshot.ReadableOsVersion =
				FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("15"));
			return Snapshot;
		}

		virtual EOpenMobileDeviceFormFactor GetDeviceFormFactor() const override
		{
			return EOpenMobileDeviceFormFactor::Phone;
		}

		virtual FOpenMobileApplicationMetadataSnapshot
		GetApplicationMetadataSnapshot() const override
		{
			FOpenMobileApplicationMetadataSnapshot Snapshot;
			Snapshot.VersionName =
				FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("2.1.0"));
			Snapshot.BuildNumber =
				FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("42"));
			return Snapshot;
		}

		virtual FOpenMobilePowerSnapshot GetPowerSnapshot() const override
		{
			FOpenMobilePowerSnapshot Snapshot;
			Snapshot.BatteryPercent =
				FOpenMobileDeviceOptionalFloat::MakeAvailable(17.0f);
			Snapshot.ChargingState = EOpenMobileBatteryChargingState::Discharging;
			return Snapshot;
		}

		virtual bool StartMonitoring(
			EOpenMobileDeviceMonitoringGroup Group,
			const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
		) override
		{
			static_cast<void>(Group);
			return CallbackToken.IsValid();
		}

		virtual FOpenMobileBrightnessResult ApplyBrightness(
			const FOpenMobileBrightnessRequest& Request
		) override
		{
			FOpenMobileBrightnessResult Result;
			Result.Request = Request;
			Result.State = EOpenMobileBrightnessApplyState::Applied;
			return Result;
		}

		virtual FOpenMobileKeepScreenAwakeResult ApplyKeepScreenAwake() override
		{
			FOpenMobileKeepScreenAwakeResult Result;
			Result.State = EOpenMobileKeepScreenAwakeApplyState::Applied;
			return Result;
		}

		virtual FOpenMobileSystemUiResult ApplySystemUiMode(
			const FOpenMobileSystemUiRequest& Request
		) override
		{
			FOpenMobileSystemUiResult Result;
			Result.Request = Request;
			Result.State = EOpenMobileSystemUiApplyState::Applied;
			return Result;
		}

		virtual FOpenMobilePreferredRefreshRateResult ApplyPreferredRefreshRate(
			const FOpenMobilePreferredRefreshRateRequest& Request
		) override
		{
			FOpenMobilePreferredRefreshRateResult Result;
			Result.Request = Request;
			Result.State = EOpenMobilePreferredRefreshRateApplyState::Applied;
			return Result;
		}

		virtual FOpenMobileOrientationPolicyResult ApplyOrientationPolicy(
			const FOpenMobileOrientationPolicyRequest& Request
		) override
		{
			FOpenMobileOrientationPolicyResult Result;
			Result.Request = Request;
			Result.State = EOpenMobileOrientationPolicyApplyState::Applied;
			return Result;
		}
	};

	const FOpenMobileDeviceDiagnosticControlLease* FindLease(
		const FOpenMobileDeviceDiagnosticsSnapshot& Snapshot,
		FName Name
	)
	{
		return Snapshot.ControlLeases.FindByPredicate(
			[Name](const FOpenMobileDeviceDiagnosticControlLease& Lease)
			{
				return Lease.Name == Name;
			}
		);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceDiagnosticsSourceTest,
	"OpenMobile.Device.Diagnostics.Source",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceDiagnosticsSourceTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceDiagnosticsTests;
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceDiagnosticsSource::ResetForTests();
	FOpenMobileDeviceBrightnessControlService::ResetForTests();
	FOpenMobileDeviceKeepScreenAwakeControlService::ResetForTests();
	FOpenMobileDeviceSystemUiControlService::ResetForTests();
	FOpenMobileDeviceRefreshRateControlService::ResetForTests();
	FOpenMobileDeviceOrientationControlService::ResetForTests();
	FBackend Backend;
	TestTrue(TEXT("Backend registers"), FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend));

	const FGuid MonitoringRequest =
		FOpenMobileDeviceMonitoringService::AddSubscription(
			{EOpenMobileDeviceMonitoringGroup::Power,
				EOpenMobileDeviceMonitoringGroup::Network},
			1.0f
		);
	FOpenMobileBrightnessResult BrightnessResult;
	const FGuid BrightnessRequest =
		FOpenMobileDeviceBrightnessControlService::AddRequest(
			FOpenMobileBrightnessRequest(),
			BrightnessResult
		);
	FOpenMobileKeepScreenAwakeResult KeepAwakeResult;
	const FGuid KeepAwakeRequest =
		FOpenMobileDeviceKeepScreenAwakeControlService::AddRequest(
			KeepAwakeResult
		);
	FOpenMobileSystemUiResult SystemUiResult;
	const FGuid SystemUiRequest =
		FOpenMobileDeviceSystemUiControlService::AddRequest(
			FOpenMobileSystemUiRequest(),
			SystemUiResult
		);
	FOpenMobilePreferredRefreshRateRequest RefreshRequestValue;
	RefreshRequestValue.bUsePreferredTargetHz = true;
	FOpenMobilePreferredRefreshRateResult RefreshResult;
	const FGuid RefreshRequest =
		FOpenMobileDeviceRefreshRateControlService::AddRequest(
			RefreshRequestValue,
			RefreshResult
		);
	FOpenMobileOrientationPolicyResult OrientationResult;
	const FGuid OrientationRequest =
		FOpenMobileDeviceOrientationControlService::AddRequest(
			FOpenMobileOrientationPolicyRequest(),
			OrientationResult
		);

	for (int32 Index = 0;
		Index < FOpenMobileDeviceDiagnosticsSource::GetMaximumRecentErrorCount() + 4;
		++Index)
	{
		FOpenMobileDeviceDiagnosticsSource::RecordError(
			*FString::Printf(TEXT("Operation%d"), Index),
			FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("https://private.example/path?token=secret")
			)
		);
	}

	UOpenMobileDeviceSettings* Settings = GetMutableDefault<UOpenMobileDeviceSettings>();
	const float PreviousPolling = Settings->FallbackPollingIntervalSeconds;
	const bool bPreviousPlatformThreshold =
		Settings->bUsePlatformDefaultLowStorageThreshold;
	const int64 PreviousThreshold = Settings->LowStorageThresholdBytes;
	const TArray<FString> PreviousSchemes = Settings->DeclaredUrlSchemes;
	Settings->FallbackPollingIntervalSeconds = NAN;
	Settings->bUsePlatformDefaultLowStorageThreshold = false;
	Settings->LowStorageThresholdBytes = -1;
	Settings->DeclaredUrlSchemes = {TEXT("valid-scheme"), TEXT("bad value")};

	const FOpenMobileDeviceDiagnosticsSnapshot Snapshot =
		FOpenMobileDeviceDiagnosticsSource::Capture();
	TestEqual(TEXT("Backend identity is captured"), Snapshot.BackendName, FName(TEXT("DiagnosticsTest")));
	TestEqual(TEXT("Platform is captured"), Snapshot.DeviceInformation.Platform, EOpenMobileDevicePlatform::Android);
	TestEqual(TEXT("Application version is captured"), Snapshot.ApplicationMetadata.VersionName.Value, FString(TEXT("2.1.0")));
	TestEqual(TEXT("Current power state is captured"), Snapshot.Power.BatteryPercent.Value, 17.0f);
	TestTrue(TEXT("Capabilities are bounded"), Snapshot.Capabilities.Num() <= FOpenMobileDeviceDiagnosticsSource::GetMaximumCapabilityCount());
	TestNotNull(
		TEXT("Unsupported reasons are retained"),
		Snapshot.Capabilities.FindByPredicate([](const FOpenMobileDeviceCapability& Capability)
		{
			return Capability.Name == FOpenMobileDeviceCapabilityNames::ChargingSource
				&& Capability.Limit == EOpenMobileDeviceCapabilityLimit::UnsupportedPlatform;
		})
	);
	TestEqual(TEXT("Active monitoring groups are captured"), Snapshot.ActiveMonitoringGroups.Num(), 2);
	for (const FName LeaseName : {
		FName(TEXT("Brightness")),
		FName(TEXT("KeepScreenAwake")),
		FName(TEXT("SystemUi")),
		FName(TEXT("PreferredRefreshRate")),
		FName(TEXT("Orientation"))
	})
	{
		const FOpenMobileDeviceDiagnosticControlLease* Lease =
			FindLease(Snapshot, LeaseName);
		TestTrue(*FString::Printf(TEXT("%s lease is present"), *LeaseName.ToString()), Lease && Lease->Count == 1);
	}
	TestEqual(
		TEXT("Recent error history is bounded"),
		Snapshot.RecentErrors.Num(),
		FOpenMobileDeviceDiagnosticsSource::GetMaximumRecentErrorCount()
	);
	TestEqual(TEXT("Old errors are evicted"), Snapshot.RecentErrors[0].Operation, FName(TEXT("Operation4")));
	TestTrue(TEXT("Invalid configuration is summarized"), Snapshot.ConfigurationIssues.Num() >= 3);
	TestFalse(TEXT("No native objects are retained"), Snapshot.CapturedAtUtc == FDateTime());

	Settings->FallbackPollingIntervalSeconds = PreviousPolling;
	Settings->bUsePlatformDefaultLowStorageThreshold = bPreviousPlatformThreshold;
	Settings->LowStorageThresholdBytes = PreviousThreshold;
	Settings->DeclaredUrlSchemes = PreviousSchemes;
	FOpenMobileDeviceOrientationControlService::RemoveRequest(OrientationRequest);
	FOpenMobileDeviceRefreshRateControlService::RemoveRequest(RefreshRequest);
	FOpenMobileDeviceSystemUiControlService::RemoveRequest(SystemUiRequest);
	FOpenMobileDeviceKeepScreenAwakeControlService::RemoveRequest(KeepAwakeRequest);
	FOpenMobileDeviceBrightnessControlService::RemoveRequest(BrightnessRequest);
	FOpenMobileDeviceMonitoringService::RemoveSubscription(MonitoringRequest);
	TestTrue(TEXT("Backend unregisters"), FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend));
	FOpenMobileDeviceDiagnosticsSource::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	return true;
}

#endif
