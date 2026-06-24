#if WITH_DEV_AUTOMATION_TESTS

#include "Framework/Docking/TabManager.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileDeviceDiagnostics.h"
#include "OpenMobileDeviceDiagnosticsOutput.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceDiagnosticsOutputTest,
	"OpenMobile.Device.Diagnostics.Output",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceDiagnosticsOutputTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	TestTrue(
		TEXT("Diagnostics tab is registered"),
		FGlobalTabmanager::Get()->HasTabSpawner(
			TEXT("OpenMobileDeviceDiagnostics")
		)
	);
	FOpenMobileDeviceDiagnosticsSnapshot Snapshot;
	Snapshot.CapturedAtUtc = FDateTime::UtcNow();
	Snapshot.OpenMobileVersion = TEXT("0.1.0");
	Snapshot.BackendName = TEXT("AndroidBackend");
	Snapshot.DeviceInformation.Platform = EOpenMobileDevicePlatform::Android;
	Snapshot.DeviceInformation.ReadableOsVersion =
		FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("15"));
	Snapshot.DeviceInformation.Manufacturer =
		FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("private-manufacturer"));
	Snapshot.ApplicationMetadata.VersionName =
		FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("2.1.0"));
	Snapshot.ApplicationMetadata.BuildNumber =
		FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("42"));
	Snapshot.ApplicationMetadata.PackageIdentifier =
		FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("com.private.secret"));
	Snapshot.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(25.0f);
	Snapshot.Network.PathState = EOpenMobileNetworkPathState::InternetCapable;
	Snapshot.Window.CurrentScreenIdentifier =
		FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("private-screen-id"));
	Snapshot.Accessibility.ReducedAnimationPlatformDetail =
		FOpenMobileDeviceOptionalString::MakeAvailable(
			TEXT("https://private.example/path?token=secret")
		);
	FOpenMobileDeviceCapability Capability;
	Capability.Name = FOpenMobileDeviceCapabilityNames::ChargingSource;
	Capability.State = EOpenMobileCapabilityState::NotSupported;
	Capability.Limit = EOpenMobileDeviceCapabilityLimit::UnsupportedPlatform;
	Capability.Detail = TEXT("carrier secret query token");
	Snapshot.Capabilities.Add(Capability);
	Snapshot.ActiveMonitoringGroups.Add(EOpenMobileDeviceMonitoringGroup::Power);
	Snapshot.ControlLeases.Add({TEXT("Brightness"), 1});
	Snapshot.RecentErrors.Add({
		FDateTime::UtcNow(),
		TEXT("EndpointReachability"),
		EOpenMobileErrorCode::NativeFailure
	});
	Snapshot.ConfigurationIssues.Add({
		TEXT("Device.Configuration.InvalidPollingInterval"),
		EOpenMobileDeviceDiagnosticIssueSeverity::Warning
	});

	FString AndroidJson;
	FOpenMobileDeviceDiagnosticsOutputResult Result =
		FOpenMobileDeviceDiagnosticsOutput::Serialize(Snapshot, AndroidJson);
	TestEqual(TEXT("Android report serializes"), Result.Code, EOpenMobileDeviceDiagnosticsOutputCode::Succeeded);
	for (const FString& Required : {
		FString(TEXT("Android")),
		FString(TEXT("15")),
		FString(TEXT("2.1.0")),
		FString(TEXT("UnsupportedPlatform")),
		FString(TEXT("EndpointReachability")),
		FString(TEXT("InvalidPollingInterval"))
	})
	{
		TestTrue(*FString::Printf(TEXT("Report contains %s"), *Required), AndroidJson.Contains(Required));
	}
	for (const FString& Forbidden : {
		FString(TEXT("private-manufacturer")),
		FString(TEXT("com.private.secret")),
		FString(TEXT("private-screen-id")),
		FString(TEXT("private.example")),
		FString(TEXT("token=secret")),
		FString(TEXT("carrier secret")),
		FString(TEXT("clipboard"))
	})
	{
		TestFalse(*FString::Printf(TEXT("Report excludes %s"), *Forbidden), AndroidJson.Contains(Forbidden));
	}

	Snapshot.DeviceInformation.Platform = EOpenMobileDevicePlatform::IOS;
	Snapshot.DeviceInformation.ReadableOsVersion.Value = TEXT("18.0");
	Snapshot.Power.BatteryPercent = {};
	Snapshot.CapturedAtUtc = FDateTime::UtcNow() - FTimespan::FromHours(1.0);
	FString IOSJson;
	Result = FOpenMobileDeviceDiagnosticsOutput::Serialize(Snapshot, IOSJson);
	TestEqual(TEXT("iOS report serializes"), Result.Code, EOpenMobileDeviceDiagnosticsOutputCode::Succeeded);
	TestTrue(TEXT("iOS platform is reported"), IOSJson.Contains(TEXT("IOS")));
	TestTrue(TEXT("Unavailable values are null"), IOSJson.Contains(TEXT("\"batteryPercent\": null")));
	TestTrue(TEXT("Old captures are marked stale"), IOSJson.Contains(TEXT("\"stale\": true")));
	TestTrue(TEXT("Stale helper agrees"), FOpenMobileDeviceDiagnosticsOutput::IsStale(Snapshot));

	Capability.Name = *FString::ChrN(400, TEXT('A'));
	for (int32 Index = 0; Index < 300; ++Index)
	{
		Snapshot.Capabilities.Add(Capability);
	}
	Snapshot.OpenMobileVersion = FString::ChrN(10000, TEXT('V'));
	FString BoundedJson;
	Result = FOpenMobileDeviceDiagnosticsOutput::Serialize(Snapshot, BoundedJson);
	TestEqual(TEXT("Oversized fields are bounded"), Result.Code, EOpenMobileDeviceDiagnosticsOutputCode::Succeeded);
	TestTrue(
		TEXT("Export stays within the byte limit"),
		FTCHARToUTF8(*BoundedJson).Length()
			<= FOpenMobileDeviceDiagnosticsOutput::GetMaximumExportBytes()
	);

	FOpenMobileDeviceDiagnosticsOutput::SetClipboardWriterForTests(
		[](const FString& Text)
		{
			static_cast<void>(Text);
			return false;
		}
	);
	Result = FOpenMobileDeviceDiagnosticsOutput::CopyToClipboard(Snapshot);
	TestEqual(TEXT("Clipboard failure is typed"), Result.Code, EOpenMobileDeviceDiagnosticsOutputCode::ClipboardWriteFailed);
	FString CopiedText;
	FOpenMobileDeviceDiagnosticsOutput::SetClipboardWriterForTests(
		[&CopiedText](const FString& Text)
		{
			CopiedText = Text;
			return true;
		}
	);
	Result = FOpenMobileDeviceDiagnosticsOutput::CopyToClipboard(Snapshot);
	TestEqual(TEXT("Clipboard success is typed"), Result.Code, EOpenMobileDeviceDiagnosticsOutputCode::Succeeded);
	TestFalse(TEXT("Clipboard receives JSON"), CopiedText.IsEmpty());

	FString WrittenPath;
	FString WrittenText;
	FOpenMobileDeviceDiagnosticsOutput::SetFileWriterForTests(
		[&WrittenPath, &WrittenText](const FString& Path, const FString& Text)
		{
			WrittenPath = Path;
			WrittenText = Text;
			return false;
		}
	);
	Result = FOpenMobileDeviceDiagnosticsOutput::ExportToFile(
		Snapshot,
		TEXT("/tmp/openmobile-device-diagnostics.json")
	);
	TestEqual(TEXT("File failure is typed"), Result.Code, EOpenMobileDeviceDiagnosticsOutputCode::FileWriteFailed);
	TestEqual(TEXT("Requested path reaches the writer"), WrittenPath, FString(TEXT("/tmp/openmobile-device-diagnostics.json")));
	TestFalse(TEXT("Writer receives bounded JSON"), WrittenText.IsEmpty());
	FOpenMobileDeviceDiagnosticsOutput::SetFileWriterForTests(
		[](const FString& Path, const FString& Text)
		{
			return !Path.IsEmpty() && !Text.IsEmpty();
		}
	);
	Result = FOpenMobileDeviceDiagnosticsOutput::ExportToFile(
		Snapshot,
		TEXT("/tmp/openmobile-device-diagnostics.json")
	);
	TestEqual(TEXT("File success is typed"), Result.Code, EOpenMobileDeviceDiagnosticsOutputCode::Succeeded);
	FOpenMobileDeviceDiagnosticsOutput::ResetWritersForTests();
	return true;
}

#endif
