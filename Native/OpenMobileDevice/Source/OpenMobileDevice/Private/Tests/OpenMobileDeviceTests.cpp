#if WITH_DEV_AUTOMATION_TESTS

#include <limits>

#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "IOpenMobileDeviceBackend.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceApplicationInfo.h"
#include "OpenMobileDeviceArchitecture.h"
#include "OpenMobileDeviceAsyncActionBase.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBatteryInfo.h"
#include "OpenMobileDeviceBlueprintLibrary.h"
#include "OpenMobileDeviceCapabilities.h"
#include "OpenMobileDeviceClipboardTypes.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceEmulatorDetection.h"
#include "OpenMobileDeviceFormFactor.h"
#include "OpenMobileDeviceIdentityTypes.h"
#include "OpenMobileDeviceLocaleTypes.h"
#include "OpenMobileDeviceLocaleInfo.h"
#include "OpenMobileDeviceMemoryInfo.h"
#include "OpenMobileDeviceMonitoring.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDevicePlatformInfo.h"
#include "OpenMobileDeviceProcessorInfo.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "OpenMobileDeviceSettings.h"
#include "OpenMobileDeviceSnapshotService.h"
#include "OpenMobileDeviceSubsystem.h"
#include "OpenMobileDeviceTimeZoneInfo.h"
#include "OpenMobileDeviceThermalHeadroom.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"

namespace OpenMobileDeviceTests
{
	class FMockBackend final : public IOpenMobileDeviceBackend
	{
	public:
		FMockBackend(
			FName InName,
			int32 InPriority = 0,
			bool bInAvailable = true,
			EOpenMobileDeviceBackendDomain InSupportedDomain =
				EOpenMobileDeviceBackendDomain::Power
		)
			: Name(InName)
			, Priority(InPriority)
			, bAvailable(bInAvailable)
			, SupportedDomain(InSupportedDomain)
		{
		}

		virtual FName GetBackendName() const override { return Name; }
		virtual int32 GetPriority() const override { return Priority; }
		virtual bool IsAvailable() const override { return bAvailable; }

		virtual FOpenMobileCapability GetDomainCapability(
			EOpenMobileDeviceBackendDomain Domain
		) const override
		{
			FOpenMobileCapability Capability;
			Capability.Name = IOpenMobileDeviceBackend::GetDomainCapabilityName(Domain);
			Capability.State = Domain == SupportedDomain
				? EOpenMobileCapabilityState::Available
				: EOpenMobileCapabilityState::NotSupported;
			return Capability;
		}

		virtual FOpenMobileDeviceCapability GetCapability(
			FName CapabilityName
		) const override
		{
			if (const FOpenMobileDeviceCapability* Capability =
				Capabilities.Find(CapabilityName))
			{
				return *Capability;
			}
			return IOpenMobileDeviceBackend::GetCapability(CapabilityName);
		}

		virtual FOpenMobileDeviceInformationSnapshot
		GetDeviceInformationSnapshot() const override
		{
			++DeviceInformationQueries;
			return DeviceInformation;
		}

		virtual EOpenMobileDeviceFormFactor GetDeviceFormFactor() const override
		{
			++FormFactorQueries;
			return DeviceFormFactor;
		}

		virtual FOpenMobilePowerSnapshot GetPowerSnapshot() const override
		{
			++PowerQueries;
			FOpenMobilePowerSnapshot Snapshot = Power;
			if (bInBackground)
			{
				Snapshot.BatteryPercent =
					FOpenMobileDeviceOptionalFloat::MakeAvailable(20.0f);
			}
			return Snapshot;
		}

		virtual FOpenMobileMediaVolumeSnapshot GetMediaVolumeSnapshot() const override
		{
			++MediaVolumeQueries;
			return MediaVolume;
		}

		virtual FOpenMobileLocaleSnapshot GetLocaleSnapshot() const override
		{
			++LocaleQueries;
			return Locale;
		}

		virtual FOpenMobileLocaleSnapshot GetLocaleSnapshotAtUtc(
			const FDateTime& UtcInstant
		) const override
		{
			LastLocaleInstant = UtcInstant;
			return GetLocaleSnapshot();
		}

		virtual bool StartMonitoring(
			EOpenMobileDeviceMonitoringGroup Group,
			const FOpenMobileDeviceMonitoringCallbackToken& CallbackToken
		) override
		{
			++MonitoringStarts.FindOrAdd(Group);
			MonitoringTokens.Add(Group, CallbackToken);
			return NativeMonitoringGroups.Contains(Group);
		}

		virtual void StopMonitoring(
			EOpenMobileDeviceMonitoringGroup Group
		) override
		{
			++MonitoringStops.FindOrAdd(Group);
		}

		void SetCapability(
			FName CapabilityName,
			EOpenMobileCapabilityState State,
			EOpenMobileDeviceCapabilityLimit Limit =
				EOpenMobileDeviceCapabilityLimit::None
		)
		{
			FOpenMobileDeviceCapability Capability;
			Capability.Name = CapabilityName;
			Capability.State = State;
			Capability.Limit = Limit;
			Capabilities.Add(CapabilityName, MoveTemp(Capability));
		}

		virtual void BeginShutdown() override
		{
			++ShutdownCount;
		}

		int32 ShutdownCount = 0;
		mutable int32 DeviceInformationQueries = 0;
		mutable int32 FormFactorQueries = 0;
		mutable int32 PowerQueries = 0;
		mutable int32 MediaVolumeQueries = 0;
		mutable int32 LocaleQueries = 0;
		mutable FDateTime LastLocaleInstant;
		bool bInBackground = false;
		FOpenMobileDeviceInformationSnapshot DeviceInformation;
		EOpenMobileDeviceFormFactor DeviceFormFactor =
			EOpenMobileDeviceFormFactor::Unknown;
		FOpenMobilePowerSnapshot Power;
		FOpenMobileMediaVolumeSnapshot MediaVolume;
		FOpenMobileLocaleSnapshot Locale;
		TSet<EOpenMobileDeviceMonitoringGroup> NativeMonitoringGroups;
		TMap<EOpenMobileDeviceMonitoringGroup, int32> MonitoringStarts;
		TMap<EOpenMobileDeviceMonitoringGroup, int32> MonitoringStops;
		TMap<
			EOpenMobileDeviceMonitoringGroup,
			FOpenMobileDeviceMonitoringCallbackToken
		> MonitoringTokens;

	private:
		FName Name;
		int32 Priority;
		bool bAvailable;
		EOpenMobileDeviceBackendDomain SupportedDomain;
		TMap<FName, FOpenMobileDeviceCapability> Capabilities;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceFormFactorTest,
	"OpenMobile.Device.Identity.FormFactor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceFormFactorTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	FOpenMobileDeviceFormFactorTraits Traits;
	Traits.bPhoneIdiom = true;
	TestEqual(TEXT("Phone idiom classifies as phone"), FOpenMobileDeviceFormFactor::Classify(Traits), EOpenMobileDeviceFormFactor::Phone);

	Traits = {};
	Traits.bTabletIdiom = true;
	TestEqual(TEXT("Tablet idiom classifies as tablet"), FOpenMobileDeviceFormFactor::Classify(Traits), EOpenMobileDeviceFormFactor::Tablet);

	Traits = {};
	Traits.bPhoneIdiom = true;
	Traits.bSimulator = true;
	TestEqual(TEXT("Simulator preserves selected phone idiom"), FOpenMobileDeviceFormFactor::Classify(Traits), EOpenMobileDeviceFormFactor::Phone);

	Traits = {};
	Traits.bHasFoldableHardware = true;
	TestEqual(TEXT("Foldable hardware takes priority"), FOpenMobileDeviceFormFactor::Classify(Traits), EOpenMobileDeviceFormFactor::Foldable);

	Traits = {};
	Traits.bSeparatingPosture = true;
	TestEqual(TEXT("Separating posture classifies as foldable"), FOpenMobileDeviceFormFactor::Classify(Traits), EOpenMobileDeviceFormFactor::Foldable);

	Traits = {};
	Traits.SmallestWindowWidthDp = 800;
	Traits.WindowSizeClass = EOpenMobileDeviceWindowSizeClass::Large;
	TestEqual(TEXT("Consistent large window traits classify as tablet"), FOpenMobileDeviceFormFactor::Classify(Traits), EOpenMobileDeviceFormFactor::Tablet);

	Traits = {};
	Traits.SmallestWindowWidthDp = 411;
	Traits.WindowSizeClass = EOpenMobileDeviceWindowSizeClass::Compact;
	TestEqual(TEXT("Consistent compact window traits classify as phone"), FOpenMobileDeviceFormFactor::Classify(Traits), EOpenMobileDeviceFormFactor::Phone);

	Traits = {};
	Traits.SmallestWindowWidthDp = 900;
	Traits.WindowSizeClass = EOpenMobileDeviceWindowSizeClass::Compact;
	TestEqual(TEXT("Conflicting resizable-window traits stay unknown"), FOpenMobileDeviceFormFactor::Classify(Traits), EOpenMobileDeviceFormFactor::Unknown);
	TestEqual(TEXT("Desktop editor traits stay unknown"), FOpenMobileDeviceFormFactor::Classify({}), EOpenMobileDeviceFormFactor::Unknown);

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("DynamicFormFactor"));
	Backend.DeviceInformation.Platform = EOpenMobileDevicePlatform::Android;
	Backend.DeviceFormFactor = EOpenMobileDeviceFormFactor::Phone;
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileDeviceInformationSnapshot Folded =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	Backend.DeviceFormFactor = EOpenMobileDeviceFormFactor::Foldable;
	const FOpenMobileDeviceInformationSnapshot Unfolded =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	TestEqual(TEXT("Initial posture reports phone"), Folded.FormFactor, EOpenMobileDeviceFormFactor::Phone);
	TestEqual(TEXT("Posture change is reevaluated"), Unfolded.FormFactor, EOpenMobileDeviceFormFactor::Foldable);
	TestEqual(TEXT("Immutable identity remains cached"), Backend.DeviceInformationQueries, 1);
	TestEqual(TEXT("Form factor is queried on each read"), Backend.FormFactorQueries, 2);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceArchitectureTest,
	"OpenMobile.Device.Identity.CpuArchitecture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceArchitectureTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	FOpenMobileDeviceInformationSnapshot Snapshot;
	FOpenMobileDeviceArchitecture::Apply(
		Snapshot,
		TEXT(" ARM64 "),
		{
			TEXT(" ARM64-V8A "),
			TEXT("armeabi-v7a"),
			TEXT("arm64-v8a"),
			TEXT(" X86_64 ")
		},
		true
	);
	TestEqual(TEXT("Process architecture is normalized"), Snapshot.ProcessArchitecture.Value, FString(TEXT("arm64")));
	TestTrue(TEXT("Android supported ABI list is available"), Snapshot.bSupportedAbisAvailable);
	TestEqual(TEXT("Supported ABI order is retained"), Snapshot.SupportedAbis, TArray<FString>({TEXT("arm64-v8a"), TEXT("armeabi-v7a"), TEXT("x86_64")}));

	FOpenMobileDeviceInformationSnapshot Mismatch;
	FOpenMobileDeviceArchitecture::Apply(
		Mismatch,
		TEXT("arm64"),
		{TEXT("x86_64")},
		true
	);
	TestEqual(TEXT("Consumer process architecture stays independent"), Mismatch.ProcessArchitecture.Value, FString(TEXT("arm64")));
	TestEqual(TEXT("Artifact mismatch does not invent device support"), Mismatch.SupportedAbis, TArray<FString>({TEXT("x86_64")}));

	FOpenMobileDeviceInformationSnapshot Future;
	FOpenMobileDeviceArchitecture::Apply(
		Future,
		TEXT(" RISC-V64 "),
		{TEXT(" RISC-V64 "), TEXT("risc-v64")},
		true
	);
	TestEqual(TEXT("Future process architecture is preserved"), Future.ProcessArchitecture.Value, FString(TEXT("risc-v64")));
	TestEqual(TEXT("Future ABI is deduplicated"), Future.SupportedAbis.Num(), 1);

	FOpenMobileDeviceInformationSnapshot Unknown;
	FOpenMobileDeviceArchitecture::Apply(
		Unknown,
		FString(),
		{TEXT("arm64-v8a")},
		false
	);
	TestFalse(TEXT("Missing process architecture stays unavailable"), Unknown.ProcessArchitecture.bIsAvailable);
	TestFalse(TEXT("Unavailable ABI source remains explicit"), Unknown.bSupportedAbisAvailable);
	TestTrue(TEXT("Unavailable ABI source returns no values"), Unknown.SupportedAbis.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceLogicalProcessorCountTest,
	"OpenMobile.Device.Identity.LogicalProcessorCount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceLogicalProcessorCountTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	FOpenMobileDeviceInformationSnapshot Snapshot;
	FOpenMobileDeviceProcessorInfo::ApplyLogicalProcessorCount(Snapshot, 0);
	TestFalse(TEXT("Zero logical processors is unavailable"), Snapshot.LogicalProcessorCount.bIsAvailable);

	FOpenMobileDeviceProcessorInfo::ApplyLogicalProcessorCount(Snapshot, -8);
	TestFalse(TEXT("Negative logical processors is unavailable"), Snapshot.LogicalProcessorCount.bIsAvailable);

	FOpenMobileDeviceProcessorInfo::ApplyLogicalProcessorCount(Snapshot, 1);
	TestEqual(TEXT("Single logical processor is retained"), Snapshot.LogicalProcessorCount.Value, 1);

	FOpenMobileDeviceProcessorInfo::ApplyLogicalProcessorCount(Snapshot, 512);
	TestEqual(TEXT("Large valid logical processor count is retained"), Snapshot.LogicalProcessorCount.Value, 512);

	FOpenMobileDeviceProcessorInfo::ApplyLogicalProcessorCount(Snapshot, 2);
	TestEqual(TEXT("Restricted process count is not expanded"), Snapshot.LogicalProcessorCount.Value, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDevicePhysicalMemoryTest,
	"OpenMobile.Device.Memory.PhysicalSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDevicePhysicalMemoryTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const FOpenMobileMemorySnapshot Unknown = FOpenMobileDeviceMemoryInfo::Build(
		0,
		0,
		false,
		EOpenMobileMemoryPressureState::Unknown
	);
	TestFalse(TEXT("Unknown total is unavailable"), Unknown.TotalPhysicalBytes.bIsAvailable);
	TestFalse(TEXT("Unknown available memory is unavailable"), Unknown.AvailablePhysicalBytes.bIsAvailable);

	const FOpenMobileMemorySnapshot Android = FOpenMobileDeviceMemoryInfo::Build(
		8ull * 1024 * 1024 * 1024,
		3ull * 1024 * 1024 * 1024,
		false,
		EOpenMobileMemoryPressureState::Nominal
	);
	TestEqual(TEXT("Android total bytes stay exact"), Android.TotalPhysicalBytes.Value, int64(8ull * 1024 * 1024 * 1024));
	TestEqual(TEXT("Android available bytes stay exact"), Android.AvailablePhysicalBytes.Value, int64(3ull * 1024 * 1024 * 1024));
	TestFalse(TEXT("Android estimate is not relabeled approximate"), Android.bAvailableBytesAreApproximate);

	const FOpenMobileMemorySnapshot IOS = FOpenMobileDeviceMemoryInfo::Build(
		16ull * 1024 * 1024 * 1024,
		6ull * 1024 * 1024 * 1024,
		true,
		EOpenMobileMemoryPressureState::Warning
	);
	TestTrue(TEXT("iOS available bytes are approximate"), IOS.bAvailableBytesAreApproximate);
	TestEqual(TEXT("Memory pressure is prioritized"), IOS.PressureState, EOpenMobileMemoryPressureState::Warning);

	const FOpenMobileMemorySnapshot IOSSimulator =
		FOpenMobileDeviceMemoryInfo::Build(
			0,
			0,
			true,
			EOpenMobileMemoryPressureState::Unknown
		);
	TestFalse(TEXT("Simulator total stays unavailable"), IOSSimulator.TotalPhysicalBytes.bIsAvailable);
	TestFalse(TEXT("Simulator estimate stays unavailable"), IOSSimulator.AvailablePhysicalBytes.bIsAvailable);
	TestFalse(TEXT("Missing simulator estimate is not approximate"), IOSSimulator.bAvailableBytesAreApproximate);

	const FOpenMobileMemorySnapshot Critical = FOpenMobileDeviceMemoryInfo::Build(
		16ull * 1024 * 1024 * 1024,
		512ull * 1024 * 1024,
		true,
		EOpenMobileMemoryPressureState::Critical
	);
	TestEqual(TEXT("Pressure changes are retained"), Critical.PressureState, EOpenMobileMemoryPressureState::Critical);

	const FOpenMobileMemorySnapshot UnknownTotal = FOpenMobileDeviceMemoryInfo::Build(
		0,
		1024,
		true,
		EOpenMobileMemoryPressureState::Unknown
	);
	TestFalse(TEXT("Unknown total remains unavailable"), UnknownTotal.TotalPhysicalBytes.bIsAvailable);
	TestEqual(TEXT("Independent safe estimate remains available"), UnknownTotal.AvailablePhysicalBytes.Value, int64(1024));

	const FOpenMobileMemorySnapshot Overflow = FOpenMobileDeviceMemoryInfo::Build(
		static_cast<uint64>(MAX_int64) + 1,
		static_cast<uint64>(MAX_int64) + 1,
		false,
		EOpenMobileMemoryPressureState::Nominal
	);
	TestFalse(TEXT("Overflowing total is unavailable"), Overflow.TotalPhysicalBytes.bIsAvailable);
	TestFalse(TEXT("Overflowing available bytes are unavailable"), Overflow.AvailablePhysicalBytes.bIsAvailable);

	const FOpenMobileMemorySnapshot InvalidOrder = FOpenMobileDeviceMemoryInfo::Build(
		1024,
		2048,
		true,
		EOpenMobileMemoryPressureState::Unknown
	);
	TestFalse(TEXT("Available bytes above total are unavailable"), InvalidOrder.AvailablePhysicalBytes.bIsAvailable);
	TestFalse(TEXT("Missing estimate is not approximate"), InvalidOrder.bAvailableBytesAreApproximate);

	TestTrue(TEXT("Negative byte formatting is empty"), UOpenMobileDeviceBlueprintLibrary::FormatByteCount(-1).IsEmpty());
	TestTrue(TEXT("IEC formatting labels kibibytes"), UOpenMobileDeviceBlueprintLibrary::FormatByteCount(1024).ToString().Contains(TEXT("KiB")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceApplicationMetadataTest,
	"OpenMobile.Device.Environment.ApplicationMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceApplicationMetadataTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	const FOpenMobileApplicationMetadataSnapshot Android =
		FOpenMobileDeviceApplicationInfo::Build(
			TEXT(" Open Mobile Test "),
			TEXT("com.example.openmobile"),
			TEXT("2.4.0"),
			TEXT("104"),
			EBuildConfiguration::Development
		);
	TestEqual(TEXT("Android display name comes from manifest"), Android.DisplayName.Value, FString(TEXT("Open Mobile Test")));
	TestEqual(TEXT("Android package identifier is retained"), Android.PackageIdentifier.Value, FString(TEXT("com.example.openmobile")));
	TestEqual(TEXT("Android version name is retained"), Android.VersionName.Value, FString(TEXT("2.4.0")));
	TestEqual(TEXT("Android build number is retained"), Android.BuildNumber.Value, FString(TEXT("104")));
	TestEqual(TEXT("Development build maps explicitly"), Android.BuildConfiguration, EOpenMobileBuildConfiguration::Development);

	const FOpenMobileApplicationMetadataSnapshot IOS =
		FOpenMobileDeviceApplicationInfo::Build(
			TEXT("Open Mobile iOS"),
			TEXT("com.example.openmobile.ios"),
			TEXT("3.0-beta"),
			TEXT("104-beta.2"),
			EBuildConfiguration::Shipping
		);
	TestEqual(TEXT("iOS plist version is not forced numeric"), IOS.VersionName.Value, FString(TEXT("3.0-beta")));
	TestEqual(TEXT("iOS plist build is not forced numeric"), IOS.BuildNumber.Value, FString(TEXT("104-beta.2")));
	TestEqual(TEXT("Shipping build maps explicitly"), IOS.BuildConfiguration, EOpenMobileBuildConfiguration::Shipping);

	const FOpenMobileApplicationMetadataSnapshot MissingDisplay =
		FOpenMobileDeviceApplicationInfo::Build(
			FString(),
			TEXT("com.example.nolabel"),
			TEXT("1.0"),
			TEXT("1"),
			EBuildConfiguration::Test
		);
	TestFalse(TEXT("Missing display name stays unavailable"), MissingDisplay.DisplayName.bIsAvailable);
	TestEqual(TEXT("Test build maps explicitly"), MissingDisplay.BuildConfiguration, EOpenMobileBuildConfiguration::Test);

	const FOpenMobileApplicationMetadataSnapshot Editor =
		FOpenMobileDeviceApplicationInfo::Build(
			FString(),
			FString(),
			FString(),
			FString(),
			EBuildConfiguration::Unknown
		);
	TestFalse(TEXT("Unsupported editor package stays unavailable"), Editor.PackageIdentifier.bIsAvailable);
	TestEqual(TEXT("Unknown build configuration stays unknown"), Editor.BuildConfiguration, EOpenMobileBuildConfiguration::Unknown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceEmulatorDetectionTest,
	"OpenMobile.Device.Environment.EmulatorDetection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceEmulatorDetectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);

	FOpenMobileDeviceInformationSnapshot AndroidEmulator;
	FOpenMobileDeviceAndroidEmulatorEvidence EmulatorEvidence;
	EmulatorEvidence.Manufacturer = TEXT("Google");
	EmulatorEvidence.Brand = TEXT("google");
	EmulatorEvidence.Model = TEXT("sdk_gphone64_arm64");
	EmulatorEvidence.Device = TEXT("emu64a");
	EmulatorEvidence.Hardware = TEXT("ranchu");
	EmulatorEvidence.Product = TEXT("sdk_gphone64_arm64");
	EmulatorEvidence.Fingerprint = TEXT("google/sdk_gphone64/generic:15/test-keys");
	FOpenMobileDeviceEmulatorDetection::ApplyAndroid(
		AndroidEmulator,
		EmulatorEvidence
	);
	TestTrue(TEXT("Known Android emulator is probable"), AndroidEmulator.bProbablyEmulator.Value);
	TestEqual(TEXT("Android evidence is likely, not confirmed"), AndroidEmulator.EmulatorConfidence, EOpenMobileDeviceEmulatorConfidence::Likely);
	TestTrue(TEXT("Android explanation is available"), AndroidEmulator.EmulatorReason.bIsAvailable);
	TestFalse(TEXT("Android explanation omits raw fingerprint"), AndroidEmulator.EmulatorReason.Value.Contains(EmulatorEvidence.Fingerprint));

	FOpenMobileDeviceInformationSnapshot PhysicalLooking;
	FOpenMobileDeviceAndroidEmulatorEvidence PhysicalEvidence;
	PhysicalEvidence.Manufacturer = TEXT("Google");
	PhysicalEvidence.Brand = TEXT("google");
	PhysicalEvidence.Model = TEXT("Pixel 9 Pro");
	PhysicalEvidence.Device = TEXT("caiman");
	PhysicalEvidence.Hardware = TEXT("caiman");
	PhysicalEvidence.Product = TEXT("caiman");
	PhysicalEvidence.Fingerprint = TEXT("google/caiman/caiman:15/release-keys");
	FOpenMobileDeviceEmulatorDetection::ApplyAndroid(
		PhysicalLooking,
		PhysicalEvidence
	);
	TestFalse(TEXT("Physical-looking device is not probable"), PhysicalLooking.bProbablyEmulator.Value);
	TestEqual(TEXT("No evidence stays explicit"), PhysicalLooking.EmulatorConfidence, EOpenMobileDeviceEmulatorConfidence::NoEvidence);

	FOpenMobileDeviceInformationSnapshot WeakEvidence;
	FOpenMobileDeviceAndroidEmulatorEvidence Weak;
	Weak.Fingerprint = TEXT("generic/device/build");
	FOpenMobileDeviceEmulatorDetection::ApplyAndroid(WeakEvidence, Weak);
	TestFalse(TEXT("One weak trait does not become probable"), WeakEvidence.bProbablyEmulator.Value);
	TestEqual(TEXT("One weak trait is possible"), WeakEvidence.EmulatorConfidence, EOpenMobileDeviceEmulatorConfidence::Possible);

	FOpenMobileDeviceInformationSnapshot IOSSimulator;
	FOpenMobileDeviceEmulatorDetection::ApplyIOS(IOSSimulator, true);
	TestTrue(TEXT("iOS Simulator is detected directly"), IOSSimulator.bProbablyEmulator.Value);
	TestEqual(TEXT("iOS Simulator detection is confirmed"), IOSSimulator.EmulatorConfidence, EOpenMobileDeviceEmulatorConfidence::Confirmed);

	FOpenMobileDeviceInformationSnapshot IOSDevice;
	FOpenMobileDeviceEmulatorDetection::ApplyIOS(IOSDevice, false);
	TestFalse(TEXT("iOS hardware reports no simulator evidence"), IOSDevice.bProbablyEmulator.Value);
	TestEqual(TEXT("iOS hardware has no evidence"), IOSDevice.EmulatorConfidence, EOpenMobileDeviceEmulatorConfidence::NoEvidence);

	const FOpenMobileDeviceInformationSnapshot UnsupportedEditor;
	TestFalse(TEXT("Unsupported editor has no detection result"), UnsupportedEditor.bProbablyEmulator.bIsAvailable);
	TestEqual(TEXT("Unsupported editor confidence stays unknown"), UnsupportedEditor.EmulatorConfidence, EOpenMobileDeviceEmulatorConfidence::Unknown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDevicePreferredLanguagesTest,
	"OpenMobile.Device.Environment.PreferredLanguages",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDevicePreferredLanguagesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	const FOpenMobileLocaleSnapshot Multiple =
		FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
			{
				TEXT("zh_hant_tw"),
				TEXT("en-us"),
				TEXT("ZH-Hant-TW"),
				TEXT("sr_Latn_RS")
			},
			true,
			TEXT("fr-FR")
		);
	TestTrue(TEXT("OS preference source is available"), Multiple.bPreferredLanguagesAvailable);
	TestEqual(TEXT("Language order and scripts normalize"), Multiple.PreferredLanguages, TArray<FString>({TEXT("zh-Hant-TW"), TEXT("en-US"), TEXT("sr-Latn-RS")}));
	TestEqual(TEXT("Active Unreal culture remains separate"), Multiple.ActiveUnrealCulture.Value, FString(TEXT("fr-FR")));

	const FOpenMobileLocaleSnapshot Malformed =
		FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
			{
				TEXT(""),
				TEXT("bad tag"),
				TEXT("en--US"),
				TEXT("de-DE-u-hc-h23"),
				TEXT("ja")
			},
			true,
			TEXT("en")
		);
	TestEqual(TEXT("Malformed tags are skipped without fallback"), Malformed.PreferredLanguages, TArray<FString>({TEXT("de-DE-u-hc-h23"), TEXT("ja")}));

	const FOpenMobileLocaleSnapshot Single =
		FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
			{TEXT("pt_BR")},
			true,
			TEXT("pt-BR")
		);
	TestEqual(TEXT("Single language separator normalizes"), Single.PreferredLanguages, TArray<FString>({TEXT("pt-BR")}));

	const FOpenMobileLocaleSnapshot Unsupported =
		FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
			{},
			false,
			FString()
		);
	TestFalse(TEXT("Unsupported editor list stays unavailable"), Unsupported.bPreferredLanguagesAvailable);
	TestTrue(TEXT("Unsupported editor list stays empty"), Unsupported.PreferredLanguages.IsEmpty());

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("ChangingLanguages"));
	Backend.Locale = Single;
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileLocaleSnapshot First =
		FOpenMobileDeviceSnapshotService::GetLocaleSnapshot();
	Backend.Locale = Multiple;
	const FOpenMobileLocaleSnapshot Second =
		FOpenMobileDeviceSnapshotService::GetLocaleSnapshot();
	TestEqual(TEXT("Initial OS preference is returned"), First.PreferredLanguages[0], FString(TEXT("pt-BR")));
	TestEqual(TEXT("Runtime OS preference changes are visible"), Second.PreferredLanguages[0], FString(TEXT("zh-Hant-TW")));
	TestEqual(TEXT("Locale snapshots are queried each time"), Backend.LocaleQueries, 2);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceLocaleAndRegionTest,
	"OpenMobile.Device.Environment.LocaleAndRegion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceLocaleAndRegionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	FOpenMobileLocaleSnapshot ScriptSpecific;
	FOpenMobileDeviceLocaleInfo::ApplyLocale(
		ScriptSpecific,
		TEXT("zh-Hant-TW"),
		TEXT("zh"),
		TEXT("Hant"),
		TEXT("TW"),
		TEXT("TWD")
	);
	TestEqual(TEXT("Script-specific locale is retained"), ScriptSpecific.LocaleIdentifier.Value, FString(TEXT("zh-Hant-TW")));
	TestEqual(TEXT("Script is independently available"), ScriptSpecific.ScriptCode.Value, FString(TEXT("Hant")));
	TestEqual(TEXT("Region is independently available"), ScriptSpecific.RegionCode.Value, FString(TEXT("TW")));
	TestEqual(TEXT("Currency is independently available"), ScriptSpecific.CurrencyCode.Value, FString(TEXT("TWD")));

	FOpenMobileLocaleSnapshot MissingRegion;
	FOpenMobileDeviceLocaleInfo::ApplyLocale(
		MissingRegion,
		TEXT("eo"),
		TEXT("eo"),
		FString(),
		FString(),
		FString()
	);
	TestFalse(TEXT("Missing script stays unavailable"), MissingRegion.ScriptCode.bIsAvailable);
	TestFalse(TEXT("Missing region stays unavailable"), MissingRegion.RegionCode.bIsAvailable);
	TestFalse(TEXT("Currency is not guessed without region"), MissingRegion.CurrencyCode.bIsAvailable);

	FOpenMobileLocaleSnapshot UnicodeExtension;
	FOpenMobileDeviceLocaleInfo::ApplyLocale(
		UnicodeExtension,
		TEXT("en-US-u-ca-buddhist"),
		TEXT("en"),
		FString(),
		TEXT("US"),
		TEXT("USD")
	);
	TestEqual(TEXT("Unicode locale extension is preserved"), UnicodeExtension.LocaleIdentifier.Value, FString(TEXT("en-US-u-ca-buddhist")));

	FOpenMobileLocaleSnapshot EditorOverride;
	FOpenMobileDeviceLocaleInfo::ApplyLocale(
		EditorOverride,
		TEXT("fr_CA"),
		TEXT("fr"),
		FString(),
		TEXT("CA"),
		TEXT("CAD")
	);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("EditorLocaleOverride"));
	Backend.Locale = EditorOverride;
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileLocaleSnapshot Overridden =
		FOpenMobileDeviceSnapshotService::GetLocaleSnapshot();
	TestEqual(TEXT("Editor backend override is retained"), Overridden.LocaleIdentifier.Value, FString(TEXT("fr_CA")));
	TestEqual(TEXT("Editor region override is retained"), Overridden.RegionCode.Value, FString(TEXT("CA")));
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceTimeZoneTest,
	"OpenMobile.Device.Environment.TimeZone",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceTimeZoneTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	FOpenMobileLocaleSnapshot NoDaylightSaving;
	FOpenMobileDeviceTimeZoneInfo::Apply(
		NoDaylightSaving,
		TEXT("Asia/Kolkata"),
		19800,
		true,
		false,
		true
	);
	TestEqual(TEXT("Half-hour zone identifier is retained"), NoDaylightSaving.TimeZoneIdentifier.Value, FString(TEXT("Asia/Kolkata")));
	TestEqual(TEXT("Half-hour offset is retained"), NoDaylightSaving.UtcOffsetSeconds.Value, 19800);
	TestFalse(TEXT("Zone without DST reports false"), NoDaylightSaving.bIsDaylightSavingTime.Value);

	FOpenMobileLocaleSnapshot Winter;
	FOpenMobileDeviceTimeZoneInfo::Apply(
		Winter,
		TEXT("America/New_York"),
		-18000,
		true,
		false,
		true
	);
	FOpenMobileLocaleSnapshot Summer;
	FOpenMobileDeviceTimeZoneInfo::Apply(
		Summer,
		TEXT("America/New_York"),
		-14400,
		true,
		true,
		true
	);
	TestEqual(TEXT("Winter negative offset is retained"), Winter.UtcOffsetSeconds.Value, -18000);
	TestEqual(TEXT("Summer clock change is retained"), Summer.UtcOffsetSeconds.Value, -14400);
	TestTrue(TEXT("Summer daylight saving is retained"), Summer.bIsDaylightSavingTime.Value);

	FOpenMobileLocaleSnapshot LargePositive;
	FOpenMobileDeviceTimeZoneInfo::Apply(
		LargePositive,
		TEXT("Pacific/Chatham"),
		45900,
		true,
		true,
		true
	);
	TestEqual(TEXT("Large positive offset is retained"), LargePositive.UtcOffsetSeconds.Value, 45900);

	FOpenMobileLocaleSnapshot Unavailable;
	FOpenMobileDeviceTimeZoneInfo::Apply(
		Unavailable,
		FString(),
		0,
		false,
		false,
		false
	);
	TestFalse(TEXT("Missing zone identifier stays unavailable"), Unavailable.TimeZoneIdentifier.bIsAvailable);
	TestFalse(TEXT("Missing offset stays unavailable"), Unavailable.UtcOffsetSeconds.bIsAvailable);
	TestFalse(TEXT("Missing DST state stays unavailable"), Unavailable.bIsDaylightSavingTime.bIsAvailable);

	FOpenMobileLocaleSnapshot InvalidOffset;
	FOpenMobileDeviceTimeZoneInfo::Apply(
		InvalidOffset,
		TEXT("Invalid/Offset"),
		100000,
		true,
		false,
		true
	);
	TestFalse(TEXT("Out-of-range offset stays unavailable"), InvalidOffset.UtcOffsetSeconds.bIsAvailable);

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("TimeZoneAtInstant"));
	Backend.Locale = Winter;
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	const FDateTime WinterInstant(2026, 1, 15, 12, 0, 0);
	FOpenMobileDeviceSnapshotService::GetLocaleSnapshotAtUtc(WinterInstant);
	TestEqual(TEXT("Supplied UTC instant reaches backend"), Backend.LastLocaleInstant, WinterInstant);
	Backend.Locale = LargePositive;
	const FOpenMobileLocaleSnapshot ChangedZone =
		FOpenMobileDeviceSnapshotService::GetLocaleSnapshotAtUtc(
			FDateTime(2026, 8, 15, 12, 0, 0)
		);
	TestEqual(TEXT("Manual time-zone change is visible"), ChangedZone.TimeZoneIdentifier.Value, FString(TEXT("Pacific/Chatham")));
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceRegionalPreferencesTest,
	"OpenMobile.Device.Environment.RegionalPreferences",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceRegionalPreferencesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	FOpenMobileLocaleSnapshot UnitedStates;
	FOpenMobileDeviceLocaleInfo::ApplyRegionalPreferences(
		UnitedStates,
		TEXT("12"),
		TEXT("imperial")
	);
	TestEqual(TEXT("US clock preference is retained"), UnitedStates.TimeFormat, EOpenMobileTimeFormatPreference::TwelveHour);
	TestEqual(TEXT("US measurement preference is retained"), UnitedStates.MeasurementSystem, EOpenMobileMeasurementSystem::Imperial);

	FOpenMobileLocaleSnapshot Metric;
	FOpenMobileDeviceLocaleInfo::ApplyRegionalPreferences(
		Metric,
		TEXT("24"),
		TEXT("metric")
	);
	TestEqual(TEXT("24-hour preference is retained"), Metric.TimeFormat, EOpenMobileTimeFormatPreference::TwentyFourHour);
	TestEqual(TEXT("Metric preference is retained"), Metric.MeasurementSystem, EOpenMobileMeasurementSystem::Metric);

	FOpenMobileLocaleSnapshot Unavailable =
		FOpenMobileDeviceLocaleInfo::BuildPreferredLanguages(
			{TEXT("en-US")},
			true,
			TEXT("en-US")
		);
	FOpenMobileDeviceLocaleInfo::ApplyRegionalPreferences(
		Unavailable,
		FString(),
		FString()
	);
	TestEqual(TEXT("Language does not imply time preference"), Unavailable.TimeFormat, EOpenMobileTimeFormatPreference::Unknown);
	TestEqual(TEXT("Language does not imply measurement preference"), Unavailable.MeasurementSystem, EOpenMobileMeasurementSystem::Unknown);
	FOpenMobileDeviceLocaleInfo::ApplyRegionalPreferences(
		Unavailable,
		TEXT("locale-default"),
		TEXT("mixed")
	);
	TestEqual(TEXT("Unrecognized time preference stays unknown"), Unavailable.TimeFormat, EOpenMobileTimeFormatPreference::Unknown);
	TestEqual(TEXT("Mixed measurement preference stays unknown"), Unavailable.MeasurementSystem, EOpenMobileMeasurementSystem::Unknown);

	FOpenMobileLocaleSnapshot RegionOverride;
	FOpenMobileDeviceLocaleInfo::ApplyLocale(
		RegionOverride,
		TEXT("en-US"),
		TEXT("en"),
		FString(),
		TEXT("US"),
		TEXT("USD")
	);
	FOpenMobileDeviceLocaleInfo::ApplyRegionalPreferences(
		RegionOverride,
		TEXT("24"),
		TEXT("metric")
	);
	TestEqual(TEXT("Platform time override wins over locale"), RegionOverride.TimeFormat, EOpenMobileTimeFormatPreference::TwentyFourHour);
	TestEqual(TEXT("Platform measurement override wins over region"), RegionOverride.MeasurementSystem, EOpenMobileMeasurementSystem::Metric);

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("RegionalChange"));
	Backend.Locale = UnitedStates;
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileLocaleSnapshot BeforeChange =
		FOpenMobileDeviceSnapshotService::GetLocaleSnapshot();
	Backend.Locale = Metric;
	const FOpenMobileLocaleSnapshot AfterChange =
		FOpenMobileDeviceSnapshotService::GetLocaleSnapshot();
	TestEqual(TEXT("Initial regional preference is visible"), BeforeChange.TimeFormat, EOpenMobileTimeFormatPreference::TwelveHour);
	TestEqual(TEXT("Changed regional preference is visible"), AfterChange.TimeFormat, EOpenMobileTimeFormatPreference::TwentyFourHour);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceBackendRegistry::ResetForTests();

	const FDateTime Instant(2026, 8, 22, 14, 30, 0);
	TestFalse(TEXT("Localized date helper returns text"), UOpenMobileDeviceBlueprintLibrary::FormatLocalizedDate(Instant).IsEmpty());
	TestFalse(TEXT("Localized time helper returns text"), UOpenMobileDeviceBlueprintLibrary::FormatLocalizedTime(Instant).IsEmpty());
	TestFalse(TEXT("Localized date-time helper returns text"), UOpenMobileDeviceBlueprintLibrary::FormatLocalizedDateTime(Instant).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceLocaleChangeEventsTest,
	"OpenMobile.Device.Environment.LocaleChangeEvents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceLocaleChangeEventsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;
	using Group = EOpenMobileDeviceMonitoringGroup;

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FMockBackend Backend(TEXT("LocaleEvents"));
	Backend.NativeMonitoringGroups = {Group::Locale};
	FOpenMobileDeviceLocaleInfo::ApplyLocale(
		Backend.Locale,
		TEXT("en-US"),
		TEXT("en"),
		FString(),
		TEXT("US"),
		TEXT("USD")
	);
	FOpenMobileDeviceTimeZoneInfo::Apply(
		Backend.Locale,
		TEXT("America/New_York"),
		-18000,
		true,
		false,
		true
	);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* Subsystem =
		NewObject<UOpenMobileDeviceSubsystem>(GameInstance);
	TArray<FOpenMobileLocaleSnapshotChange> Changes;
	bool bRefreshedBeforeBroadcast = true;
	Subsystem->OnNativeLocaleSnapshotChanged().AddLambda(
		[&Changes, &Backend, &bRefreshedBeforeBroadcast](
			const FOpenMobileLocaleSnapshotChange& Change
		)
		{
			bRefreshedBeforeBroadcast &= Change.CurrentSnapshot.LocaleIdentifier
				== Backend.Locale.LocaleIdentifier;
			Changes.Add(Change);
		}
	);
	UOpenMobileDeviceMonitoringSubscription* Subscription =
		Subsystem->StartMonitoring(GameInstance, {Group::Locale}, 1.0f);
	TestNotNull(TEXT("Locale monitoring starts"), Subscription);
	TestEqual(TEXT("Locale observer starts once"), Backend.MonitoringStarts.FindRef(Group::Locale), 1);
	TestFalse(TEXT("Native locale observer avoids fallback polling"), FOpenMobileDeviceMonitoringService::UsesFallbackForTests(Group::Locale));

	const FOpenMobileLocaleSnapshot Initial = Backend.Locale;
	FOpenMobileDeviceLocaleInfo::ApplyLocale(
		Backend.Locale,
		TEXT("fr-CA"),
		TEXT("fr"),
		FString(),
		TEXT("CA"),
		TEXT("CAD")
	);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Locale);
	TestEqual(TEXT("Locale change emits once"), Changes.Num(), 1);
	TestTrue(TEXT("Snapshot refresh precedes the event"), bRefreshedBeforeBroadcast);
	TestEqual(TEXT("Event retains the old locale"), Changes[0].PreviousSnapshot.LocaleIdentifier, Initial.LocaleIdentifier);
	TestEqual(TEXT("Event contains the new locale"), Changes[0].CurrentSnapshot.LocaleIdentifier, Backend.Locale.LocaleIdentifier);
	TestTrue(TEXT("New snapshot generation follows old generation"), Changes[0].CurrentSnapshot.Metadata.Generation > Changes[0].PreviousSnapshot.Metadata.Generation);

	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Locale);
	TestEqual(TEXT("Duplicate native notifications coalesce"), Changes.Num(), 1);

	FOpenMobileDeviceTimeZoneInfo::Apply(
		Backend.Locale,
		TEXT("America/New_York"),
		-14400,
		true,
		true,
		true
	);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Locale);
	TestEqual(TEXT("Daylight-saving transition emits once"), Changes.Num(), 2);
	TestEqual(TEXT("Transition retains old offset"), Changes[1].PreviousSnapshot.UtcOffsetSeconds.Value, -18000);
	TestEqual(TEXT("Transition contains new offset"), Changes[1].CurrentSnapshot.UtcOffsetSeconds.Value, -14400);

	FOpenMobileDeviceMonitoringService::SetApplicationActiveForTests(false);
	FOpenMobileDeviceLocaleInfo::ApplyLocale(
		Backend.Locale,
		TEXT("de-DE"),
		TEXT("de"),
		FString(),
		TEXT("DE"),
		TEXT("EUR")
	);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Locale);
	TestEqual(TEXT("Background notification does not emit"), Changes.Num(), 2);
	FOpenMobileDeviceMonitoringService::SetApplicationActiveForTests(true);
	TestEqual(TEXT("Foreground refresh emits the background change"), Changes.Num(), 3);
	TestEqual(TEXT("Foreground event contains latest locale"), Changes[2].CurrentSnapshot.LocaleIdentifier.Value, FString(TEXT("de-DE")));

	const FOpenMobileDeviceMonitoringCallbackToken StoppedToken =
		Backend.MonitoringTokens.FindRef(Group::Locale);
	Subscription->Stop();
	TestEqual(TEXT("Locale observer stops with final listener"), Backend.MonitoringStops.FindRef(Group::Locale), 1);
	FOpenMobileDeviceMonitoringService::NotifyNativeChange(StoppedToken, 100);
	TestEqual(TEXT("Stopped locale observer cannot emit"), Changes.Num(), 3);

	Subsystem->Deinitialize();
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceBatteryLevelTest,
	"OpenMobile.Device.Power.BatteryLevel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceBatteryLevelTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;
	using Group = EOpenMobileDeviceMonitoringGroup;

	FOpenMobilePowerSnapshot Unavailable;
	FOpenMobileDeviceBatteryInfo::ApplyFraction(Unavailable, 0.0, false);
	TestFalse(TEXT("Unavailable battery has no percent"), Unavailable.BatteryPercent.bIsAvailable);
	TestFalse(TEXT("Unavailable battery has no native level"), Unavailable.NativeBatteryLevel.bIsAvailable);

	FOpenMobilePowerSnapshot Empty;
	FOpenMobileDeviceBatteryInfo::ApplyRatio(Empty, 0, 100, true);
	TestEqual(TEXT("Empty battery normalizes to zero percent"), Empty.BatteryPercent.Value, 0.0f);
	TestEqual(TEXT("Empty battery retains native zero"), Empty.NativeBatteryLevel.Value, 0.0f);

	FOpenMobilePowerSnapshot Full;
	FOpenMobileDeviceBatteryInfo::ApplyRatio(Full, 100, 100, true);
	TestEqual(TEXT("Full battery normalizes to 100 percent"), Full.BatteryPercent.Value, 100.0f);
	TestEqual(TEXT("Full battery retains native one"), Full.NativeBatteryLevel.Value, 1.0f);

	FOpenMobilePowerSnapshot Precise;
	FOpenMobileDeviceBatteryInfo::ApplyRatio(Precise, 37, 64, true);
	TestEqual(TEXT("Android scale precision is preserved"), Precise.NativeBatteryLevel.Value, 0.578125f);
	TestEqual(TEXT("Percent derives from native precision"), Precise.BatteryPercent.Value, 57.8125f);

	for (const double InvalidLevel : {
		-0.01,
		1.01,
		std::numeric_limits<double>::quiet_NaN()
	})
	{
		FOpenMobilePowerSnapshot Invalid;
		FOpenMobileDeviceBatteryInfo::ApplyFraction(Invalid, InvalidLevel, true);
		TestFalse(TEXT("Invalid native fraction stays unavailable"), Invalid.BatteryPercent.bIsAvailable);
	}
	FOpenMobilePowerSnapshot InvalidScale;
	FOpenMobileDeviceBatteryInfo::ApplyRatio(InvalidScale, 50, 0, true);
	TestFalse(TEXT("Invalid Android scale stays unavailable"), InvalidScale.BatteryPercent.bIsAvailable);
	FOpenMobilePowerSnapshot OutOfRangeRatio;
	FOpenMobileDeviceBatteryInfo::ApplyRatio(OutOfRangeRatio, 101, 100, true);
	TestFalse(TEXT("Out-of-range Android level stays unavailable"), OutOfRangeRatio.BatteryPercent.bIsAvailable);

	FOpenMobilePowerSnapshot AndroidEmulator;
	FOpenMobileDeviceBatteryInfo::ApplyRatio(AndroidEmulator, 50, 100, true);
	TestTrue(TEXT("Android emulator may expose synthetic battery data"), AndroidEmulator.BatteryPercent.bIsAvailable);
	FOpenMobilePowerSnapshot IOSSimulator;
	FOpenMobileDeviceBatteryInfo::ApplyFraction(IOSSimulator, -1.0, false);
	TestFalse(TEXT("iOS Simulator battery stays unavailable"), IOSSimulator.BatteryPercent.bIsAvailable);

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FMockBackend Backend(TEXT("BatteryLifecycle"));
	Backend.Power = Precise;
	Backend.NativeMonitoringGroups = {Group::Power};
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* Subsystem =
		NewObject<UOpenMobileDeviceSubsystem>(GameInstance);
	UOpenMobileDeviceMonitoringSubscription* Subscription =
		Subsystem->StartMonitoring(GameInstance, {Group::Power}, 1.0f);
	TestEqual(TEXT("Battery monitoring starts on first subscription"), Backend.MonitoringStarts.FindRef(Group::Power), 1);
	Backend.bInBackground = true;
	const FOpenMobilePowerSnapshot Background = Subsystem->GetPowerSnapshot();
	TestTrue(TEXT("Background battery remains queryable"), Background.BatteryPercent.bIsAvailable);
	Subscription->Stop();
	TestEqual(TEXT("Battery monitoring stops with final subscription"), Backend.MonitoringStops.FindRef(Group::Power), 1);
	Subsystem->Deinitialize();
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceChargingStateTest,
	"OpenMobile.Device.Power.ChargingState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceChargingStateTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	FOpenMobilePowerSnapshot AndroidUnknown;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(
		AndroidUnknown,
		1,
		true
	);
	TestEqual(TEXT("Android unknown state stays unknown"), AndroidUnknown.ChargingState, EOpenMobileBatteryChargingState::Unknown);
	TestEqual(TEXT("Android unknown state keeps raw detail"), AndroidUnknown.NativeChargingState.Value, FString(TEXT("Android:1")));

	FOpenMobilePowerSnapshot AndroidCharging;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(
		AndroidCharging,
		2,
		true
	);
	TestEqual(TEXT("Android charging state maps"), AndroidCharging.ChargingState, EOpenMobileBatteryChargingState::Charging);

	FOpenMobilePowerSnapshot AndroidDischarging;
	FOpenMobileDeviceBatteryInfo::ApplyFraction(
		AndroidDischarging,
		1.0,
		true
	);
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(
		AndroidDischarging,
		3,
		true
	);
	TestEqual(TEXT("Full percentage does not imply full state"), AndroidDischarging.ChargingState, EOpenMobileBatteryChargingState::Discharging);

	FOpenMobilePowerSnapshot AndroidNotCharging;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(
		AndroidNotCharging,
		4,
		true
	);
	TestEqual(TEXT("Ambiguous Android not-charging stays unknown"), AndroidNotCharging.ChargingState, EOpenMobileBatteryChargingState::Unknown);
	TestEqual(TEXT("Ambiguous Android state keeps raw detail"), AndroidNotCharging.NativeChargingState.Value, FString(TEXT("Android:4")));

	FOpenMobilePowerSnapshot AndroidFull;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(
		AndroidFull,
		5,
		true
	);
	TestEqual(TEXT("Android full state maps"), AndroidFull.ChargingState, EOpenMobileBatteryChargingState::Full);

	FOpenMobilePowerSnapshot WirelessCharging;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(
		WirelessCharging,
		2,
		true
	);
	TestEqual(TEXT("Wireless charging still maps from native status"), WirelessCharging.ChargingState, EOpenMobileBatteryChargingState::Charging);

	FOpenMobilePowerSnapshot FutureAndroid;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(
		FutureAndroid,
		99,
		true
	);
	TestEqual(TEXT("Future Android state maps safely"), FutureAndroid.ChargingState, EOpenMobileBatteryChargingState::Unknown);
	TestEqual(TEXT("Future Android state remains diagnostic"), FutureAndroid.NativeChargingState.Value, FString(TEXT("Android:99")));

	for (const TPair<int64, EOpenMobileBatteryChargingState>& Fixture : {
		TPair<int64, EOpenMobileBatteryChargingState>(0, EOpenMobileBatteryChargingState::Unknown),
		TPair<int64, EOpenMobileBatteryChargingState>(1, EOpenMobileBatteryChargingState::Discharging),
		TPair<int64, EOpenMobileBatteryChargingState>(2, EOpenMobileBatteryChargingState::Charging),
		TPair<int64, EOpenMobileBatteryChargingState>(3, EOpenMobileBatteryChargingState::Full)
	})
	{
		FOpenMobilePowerSnapshot IOS;
		FOpenMobileDeviceBatteryInfo::ApplyIOSChargingState(
			IOS,
			Fixture.Key,
			true
		);
		TestEqual(TEXT("iOS charging state maps"), IOS.ChargingState, Fixture.Value);
		TestTrue(TEXT("iOS native state stays diagnostic"), IOS.NativeChargingState.bIsAvailable);
	}

	FOpenMobilePowerSnapshot Missing;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingState(Missing, 0, false);
	TestEqual(TEXT("Missing state stays unknown"), Missing.ChargingState, EOpenMobileBatteryChargingState::Unknown);
	TestFalse(TEXT("Missing state has no raw detail"), Missing.NativeChargingState.bIsAvailable);

	FOpenMobilePowerSnapshot IOSSimulator;
	FOpenMobileDeviceBatteryInfo::ApplyIOSChargingState(IOSSimulator, 0, false);
	TestEqual(TEXT("iOS Simulator state stays unknown"), IOSSimulator.ChargingState, EOpenMobileBatteryChargingState::Unknown);
	TestFalse(TEXT("iOS Simulator has no raw state"), IOSSimulator.NativeChargingState.bIsAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceChargingSourceTest,
	"OpenMobile.Device.Power.ChargingSource",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceChargingSourceTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	for (const TPair<int64, EOpenMobileChargingSource>& Fixture : {
		TPair<int64, EOpenMobileChargingSource>(0, EOpenMobileChargingSource::Unknown),
		TPair<int64, EOpenMobileChargingSource>(1, EOpenMobileChargingSource::AC),
		TPair<int64, EOpenMobileChargingSource>(2, EOpenMobileChargingSource::USB),
		TPair<int64, EOpenMobileChargingSource>(4, EOpenMobileChargingSource::Wireless),
		TPair<int64, EOpenMobileChargingSource>(8, EOpenMobileChargingSource::Other),
		TPair<int64, EOpenMobileChargingSource>(16, EOpenMobileChargingSource::Other)
	})
	{
		FOpenMobilePowerSnapshot Snapshot;
		FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingSource(
			Snapshot,
			Fixture.Key,
			true
		);
		TestEqual(TEXT("Android charging source maps"), Snapshot.ChargingSource, Fixture.Value);
	}

	for (const int64 Contradictory : {3, 5, 6, 7, 9})
	{
		FOpenMobilePowerSnapshot Snapshot;
		FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingSource(
			Snapshot,
			Contradictory,
			true
		);
		TestEqual(TEXT("Contradictory Android source stays unknown"), Snapshot.ChargingSource, EOpenMobileChargingSource::Unknown);
	}

	FOpenMobilePowerSnapshot Missing;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingSource(
		Missing,
		0,
		false
	);
	TestEqual(TEXT("Missing Android source stays unknown"), Missing.ChargingSource, EOpenMobileChargingSource::Unknown);

	FOpenMobilePowerSnapshot Invalid;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidChargingSource(
		Invalid,
		-1,
		true
	);
	TestEqual(TEXT("Invalid Android source stays unknown"), Invalid.ChargingSource, EOpenMobileChargingSource::Unknown);

	FOpenMobilePowerSnapshot IOS;
	FOpenMobileDeviceBatteryInfo::ApplyIOSChargingSource(IOS);
	TestEqual(TEXT("iOS source is explicitly unsupported"), IOS.ChargingSource, EOpenMobileChargingSource::Unsupported);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDevicePowerSavingModeTest,
	"OpenMobile.Device.Power.PowerSavingMode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDevicePowerSavingModeTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;
	using Group = EOpenMobileDeviceMonitoringGroup;

	FOpenMobilePowerSnapshot AndroidEnabled;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidPowerSavingState(
		AndroidEnabled,
		true,
		true
	);
	TestTrue(TEXT("Android enabled state is available"), AndroidEnabled.bPowerSavingEnabled.bIsAvailable);
	TestTrue(TEXT("Android enabled state maps true"), AndroidEnabled.bPowerSavingEnabled.Value);
	TestEqual(TEXT("Android raw state is retained"), AndroidEnabled.NativePowerSavingState.Value, FString(TEXT("Android:true")));

	FOpenMobilePowerSnapshot IOSDisabled;
	FOpenMobileDeviceBatteryInfo::ApplyIOSPowerSavingState(
		IOSDisabled,
		false,
		true
	);
	TestTrue(TEXT("iOS disabled state is available"), IOSDisabled.bPowerSavingEnabled.bIsAvailable);
	TestFalse(TEXT("iOS disabled state maps false"), IOSDisabled.bPowerSavingEnabled.Value);
	TestEqual(TEXT("iOS raw state is retained"), IOSDisabled.NativePowerSavingState.Value, FString(TEXT("IOS:false")));

	for (const TCHAR* Label : {TEXT("Unsupported"), TEXT("Restricted")})
	{
		FOpenMobilePowerSnapshot Unavailable;
		FOpenMobileDeviceBatteryInfo::ApplyAndroidPowerSavingState(
			Unavailable,
			false,
			false
		);
		TestFalse(Label, Unavailable.bPowerSavingEnabled.bIsAvailable);
		TestFalse(TEXT("Unavailable state has no raw detail"), Unavailable.NativePowerSavingState.bIsAvailable);
	}

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FMockBackend Backend(TEXT("PowerSavingEvents"));
	Backend.NativeMonitoringGroups = {Group::Power};
	FOpenMobileDeviceBatteryInfo::ApplyAndroidPowerSavingState(
		Backend.Power,
		false,
		true
	);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* Subsystem =
		NewObject<UOpenMobileDeviceSubsystem>(GameInstance);
	TArray<bool> Events;
	Subsystem->OnNativePowerSnapshotChanged().AddLambda(
		[&Events](const FOpenMobilePowerSnapshot& Snapshot)
		{
			Events.Add(Snapshot.bPowerSavingEnabled.Value);
		}
	);
	UOpenMobileDeviceMonitoringSubscription* Subscription =
		Subsystem->StartMonitoring(GameInstance, {Group::Power}, 1.0f);

	FOpenMobileDeviceBatteryInfo::ApplyAndroidPowerSavingState(
		Backend.Power,
		true,
		true
	);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Power);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Power);
	TestEqual(TEXT("Duplicate enabled notifications coalesce"), Events.Num(), 1);
	FOpenMobileDeviceBatteryInfo::ApplyAndroidPowerSavingState(
		Backend.Power,
		false,
		true
	);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Power);
	TestEqual(TEXT("Rapid disable emits the new state"), Events.Num(), 2);
	TestFalse(TEXT("Rapid disable event carries false"), Events.Last());

	FOpenMobileDeviceMonitoringService::SetApplicationActiveForTests(false);
	FOpenMobileDeviceBatteryInfo::ApplyAndroidPowerSavingState(
		Backend.Power,
		true,
		true
	);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Power);
	TestEqual(TEXT("Background power-mode change does not emit"), Events.Num(), 2);
	FOpenMobileDeviceMonitoringService::SetApplicationActiveForTests(true);
	TestEqual(TEXT("Foreground refresh emits latest power mode"), Events.Num(), 3);
	TestTrue(TEXT("Foreground event carries latest state"), Events.Last());

	Subscription->Stop();
	Subsystem->Deinitialize();
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	const FOpenMobilePowerSnapshot UnsupportedEditor =
		FOpenMobileDeviceSnapshotService::GetPowerSnapshot();
	TestFalse(TEXT("Unsupported editor power mode is unavailable"), UnsupportedEditor.bPowerSavingEnabled.bIsAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceThermalStateTest,
	"OpenMobile.Device.Power.ThermalState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceThermalStateTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;
	using Group = EOpenMobileDeviceMonitoringGroup;

	for (const TPair<int32, EOpenMobileThermalState>& Fixture : {
		TPair<int32, EOpenMobileThermalState>(0, EOpenMobileThermalState::Nominal),
		TPair<int32, EOpenMobileThermalState>(1, EOpenMobileThermalState::Fair),
		TPair<int32, EOpenMobileThermalState>(2, EOpenMobileThermalState::Serious),
		TPair<int32, EOpenMobileThermalState>(3, EOpenMobileThermalState::Critical),
		TPair<int32, EOpenMobileThermalState>(4, EOpenMobileThermalState::Critical),
		TPair<int32, EOpenMobileThermalState>(5, EOpenMobileThermalState::Critical),
		TPair<int32, EOpenMobileThermalState>(6, EOpenMobileThermalState::Critical)
	})
	{
		FOpenMobilePowerSnapshot Snapshot;
		FOpenMobileDeviceBatteryInfo::ApplyAndroidThermalState(
			Snapshot,
			Fixture.Key,
			true
		);
		TestEqual(TEXT("Android thermal severity maps conservatively"), Snapshot.ThermalState, Fixture.Value);
		TestEqual(TEXT("Android raw thermal state is retained"), Snapshot.NativeThermalState.Value, Fixture.Key);
	}

	for (const TPair<int32, EOpenMobileThermalState>& Fixture : {
		TPair<int32, EOpenMobileThermalState>(0, EOpenMobileThermalState::Nominal),
		TPair<int32, EOpenMobileThermalState>(1, EOpenMobileThermalState::Fair),
		TPair<int32, EOpenMobileThermalState>(2, EOpenMobileThermalState::Serious),
		TPair<int32, EOpenMobileThermalState>(3, EOpenMobileThermalState::Critical)
	})
	{
		FOpenMobilePowerSnapshot Snapshot;
		FOpenMobileDeviceBatteryInfo::ApplyIOSThermalState(
			Snapshot,
			Fixture.Key,
			true
		);
		TestEqual(TEXT("iOS thermal severity maps"), Snapshot.ThermalState, Fixture.Value);
		TestEqual(TEXT("iOS raw thermal state is retained"), Snapshot.NativeThermalState.Value, Fixture.Key);
	}

	FOpenMobilePowerSnapshot Future;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidThermalState(Future, 99, true);
	TestEqual(TEXT("Future thermal severity maps safely"), Future.ThermalState, EOpenMobileThermalState::Unknown);
	TestEqual(TEXT("Future thermal severity remains diagnostic"), Future.NativeThermalState.Value, 99);

	FOpenMobilePowerSnapshot Missing;
	FOpenMobileDeviceBatteryInfo::ApplyIOSThermalState(Missing, 0, false);
	TestEqual(TEXT("Missing thermal state stays unknown"), Missing.ThermalState, EOpenMobileThermalState::Unknown);
	TestFalse(TEXT("Missing thermal state has no raw detail"), Missing.NativeThermalState.bIsAvailable);

	FOpenMobilePowerSnapshot AndroidEmulator;
	FOpenMobileDeviceBatteryInfo::ApplyAndroidThermalState(
		AndroidEmulator,
		0,
		true
	);
	TestEqual(TEXT("Android emulator may expose synthetic nominal state"), AndroidEmulator.ThermalState, EOpenMobileThermalState::Nominal);
	FOpenMobilePowerSnapshot IOSSimulator;
	FOpenMobileDeviceBatteryInfo::ApplyIOSThermalState(IOSSimulator, 0, false);
	TestEqual(TEXT("iOS Simulator thermal state stays unknown"), IOSSimulator.ThermalState, EOpenMobileThermalState::Unknown);

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FMockBackend Backend(TEXT("ThermalEvents"));
	Backend.NativeMonitoringGroups = {Group::Power};
	FOpenMobileDeviceBatteryInfo::ApplyAndroidThermalState(
		Backend.Power,
		0,
		true
	);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* Subsystem =
		NewObject<UOpenMobileDeviceSubsystem>(GameInstance);
	TArray<EOpenMobileThermalState> Events;
	Subsystem->OnNativePowerSnapshotChanged().AddLambda(
		[&Events](const FOpenMobilePowerSnapshot& Snapshot)
		{
			Events.Add(Snapshot.ThermalState);
		}
	);
	UOpenMobileDeviceMonitoringSubscription* Subscription =
		Subsystem->StartMonitoring(GameInstance, {Group::Power}, 1.0f);
	FOpenMobileDeviceBatteryInfo::ApplyAndroidThermalState(
		Backend.Power,
		2,
		true
	);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Power);
	FOpenMobileDeviceBatteryInfo::ApplyAndroidThermalState(
		Backend.Power,
		4,
		true
	);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Power);
	TestEqual(TEXT("Thermal transitions emit in order"), Events.Num(), 2);
	TestEqual(TEXT("Moderate transition becomes serious"), Events[0], EOpenMobileThermalState::Serious);
	TestEqual(TEXT("Critical transition remains critical"), Events[1], EOpenMobileThermalState::Critical);
	Subscription->Stop();
	Subsystem->Deinitialize();
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceThermalHeadroomTest,
	"OpenMobile.Device.Power.ThermalHeadroom",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceThermalHeadroomTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileDeviceThermalHeadroomTracker Tracker;
	const FDateTime FirstTime(2026, 8, 22, 10, 0, 0);

	TestTrue(TEXT("First thermal headroom query is allowed"), Tracker.ShouldSample(100.0));
	FOpenMobilePowerSnapshot First;
	Tracker.ApplyNativeSample(First, 0.4f, 10, 100.0, FirstTime, true);
	TestTrue(TEXT("Valid thermal headroom is available"), First.ThermalHeadroom.bIsAvailable);
	TestEqual(TEXT("Native normalized headroom is preserved"), First.ThermalHeadroom.Value, 0.4f);
	TestEqual(TEXT("Forecast window is exposed"), First.ThermalForecastSeconds.Value, 10.0f);
	TestEqual(TEXT("Thermal sample time is exposed"), First.ThermalHeadroomSampleTimeUtc, FirstTime);
	TestEqual(TEXT("One thermal sample has no trend"), First.ThermalTrend, EOpenMobileThermalTrend::Unknown);
	TestFalse(TEXT("Thermal query is rate limited"), Tracker.ShouldSample(109.99));

	FOpenMobilePowerSnapshot Cached;
	Tracker.ApplyLatest(Cached);
	TestEqual(TEXT("Rate-limited reads use the latest sample"), Cached.ThermalHeadroom.Value, 0.4f);
	TestEqual(TEXT("Cached sample time stays unchanged"), Cached.ThermalHeadroomSampleTimeUtc, FirstTime);

	TestTrue(TEXT("Thermal query resumes after ten seconds"), Tracker.ShouldSample(110.0));
	FOpenMobilePowerSnapshot Heating;
	Tracker.ApplyNativeSample(Heating, 0.45f, 10, 110.0, FirstTime + FTimespan::FromSeconds(10), true);
	TestEqual(TEXT("Rising headroom use is heating"), Heating.ThermalTrend, EOpenMobileThermalTrend::Heating);
	FOpenMobilePowerSnapshot Stable;
	Tracker.ApplyNativeSample(Stable, 0.46f, 10, 120.0, FirstTime + FTimespan::FromSeconds(20), true);
	TestEqual(TEXT("Small headroom movement is stable"), Stable.ThermalTrend, EOpenMobileThermalTrend::Stable);
	FOpenMobilePowerSnapshot Cooling;
	Tracker.ApplyNativeSample(Cooling, 0.3f, 10, 130.0, FirstTime + FTimespan::FromSeconds(30), true);
	TestEqual(TEXT("Falling headroom use is cooling"), Cooling.ThermalTrend, EOpenMobileThermalTrend::Cooling);

	FOpenMobilePowerSnapshot Invalid;
	Tracker.ApplyNativeSample(Invalid, NAN, 10, 140.0, FirstTime + FTimespan::FromSeconds(40), true);
	TestFalse(TEXT("NaN thermal headroom is unavailable"), Invalid.ThermalHeadroom.bIsAvailable);
	TestEqual(TEXT("Invalid thermal samples clear their time"), Invalid.ThermalHeadroomSampleTimeUtc, FDateTime());
	TestFalse(TEXT("Invalid attempts remain rate limited"), Tracker.ShouldSample(149.0));

	FOpenMobilePowerSnapshot InvalidForecast;
	Tracker.ApplyNativeSample(InvalidForecast, 0.5f, 61, 150.0, FirstTime, true);
	TestFalse(TEXT("Forecasts above sixty seconds are rejected"), InvalidForecast.ThermalHeadroom.bIsAvailable);
	FOpenMobilePowerSnapshot AfterInvalid;
	Tracker.ApplyNativeSample(AfterInvalid, 0.5f, 10, 160.0, FirstTime, true);
	TestEqual(TEXT("History resets after an invalid sample"), AfterInvalid.ThermalTrend, EOpenMobileThermalTrend::Unknown);
	FOpenMobilePowerSnapshot AfterGap;
	Tracker.ApplyNativeSample(AfterGap, 0.7f, 10, 200.0, FirstTime, true);
	TestEqual(TEXT("History resets after a sampling gap"), AfterGap.ThermalTrend, EOpenMobileThermalTrend::Unknown);

	Tracker.ResetTrend();
	FOpenMobilePowerSnapshot AfterLifecycle;
	Tracker.ApplyLatest(AfterLifecycle);
	TestEqual(TEXT("Lifecycle reset clears the cached trend"), AfterLifecycle.ThermalTrend, EOpenMobileThermalTrend::Unknown);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDevicePlatformInformationTest,
	"OpenMobile.Device.Identity.PlatformAndOS",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDevicePlatformInformationTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	const FOpenMobileDeviceInformationSnapshot Android =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::Android,
			TEXT("14.0.1"),
			34,
			TEXT("  Samsung  "),
			TEXT(" samsung "),
			TEXT(" Galaxy Tab S9 ")
		);
	TestEqual(TEXT("Android platform normalizes"), Android.Platform, EOpenMobileDevicePlatform::Android);
	TestEqual(TEXT("Android readable version includes platform"), Android.ReadableOsVersion.Value, FString(TEXT("Android 14.0.1")));
	TestEqual(TEXT("Android raw version is retained"), Android.RawOsVersion.Value, FString(TEXT("14.0.1")));
	TestEqual(TEXT("Android major version parses"), Android.OsVersionMajor.Value, 14);
	TestEqual(TEXT("Android minor version parses"), Android.OsVersionMinor.Value, 0);
	TestEqual(TEXT("Android patch version parses"), Android.OsVersionPatch.Value, 1);
	TestEqual(TEXT("Android API level is available"), Android.AndroidApiLevel.Value, 34);
	TestEqual(TEXT("Manufacturer whitespace is trimmed"), Android.Manufacturer.Value, FString(TEXT("Samsung")));
	TestEqual(TEXT("Brand remains distinct"), Android.Brand.Value, FString(TEXT("samsung")));
	TestEqual(TEXT("Tablet model is readable"), Android.Model.Value, FString(TEXT("Galaxy Tab S9")));

	const FOpenMobileDeviceInformationSnapshot IOS =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::IOS,
			TEXT("17.5"),
			0
		);
	TestEqual(TEXT("iOS platform normalizes"), IOS.Platform, EOpenMobileDevicePlatform::IOS);
	TestEqual(TEXT("iOS readable version includes platform"), IOS.ReadableOsVersion.Value, FString(TEXT("iOS 17.5")));
	TestFalse(TEXT("Missing iOS patch stays unavailable"), IOS.OsVersionPatch.bIsAvailable);
	TestFalse(TEXT("iOS has no Android API level"), IOS.AndroidApiLevel.bIsAvailable);

	const FOpenMobileDeviceInformationSnapshot Unicode =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::Android,
			TEXT("15"),
			35,
			TEXT("小米"),
			TEXT("Redmi"),
			TEXT("红米 K80")
		);
	TestEqual(TEXT("Unicode manufacturer is preserved"), Unicode.Manufacturer.Value, FString(TEXT("小米")));
	TestEqual(TEXT("Unicode model is preserved"), Unicode.Model.Value, FString(TEXT("红米 K80")));

	const FOpenMobileDeviceInformationSnapshot Unfamiliar =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::Android,
			TEXT("16"),
			36,
			TEXT("Aether Labs"),
			FString(),
			TEXT("Aurora One")
		);
	TestEqual(TEXT("Unfamiliar manufacturer is not guessed"), Unfamiliar.Manufacturer.Value, FString(TEXT("Aether Labs")));
	TestFalse(TEXT("Missing brand stays unavailable"), Unfamiliar.Brand.bIsAvailable);

	const FOpenMobileDeviceInformationSnapshot IOSSimulator =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::IOS,
			TEXT("18.0"),
			0,
			FString(),
			FString(),
			TEXT(" iPhone Simulator ")
		);
	TestEqual(TEXT("Simulator model is preserved"), IOSSimulator.Model.Value, FString(TEXT("iPhone Simulator")));
	TestFalse(TEXT("iOS manufacturer is not guessed"), IOSSimulator.Manufacturer.bIsAvailable);

	const FOpenMobileDeviceInformationSnapshot Simulator =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::IOS,
			TEXT("18.0.0"),
			0
		);
	TestEqual(TEXT("Simulator fixture keeps iOS platform"), Simulator.Platform, EOpenMobileDevicePlatform::IOS);
	TestEqual(TEXT("Simulator fixture parses OS version"), Simulator.OsVersionMajor.Value, 18);

	const FOpenMobileDeviceInformationSnapshot Malformed =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::Android,
			TEXT("Future Preview"),
			99
		);
	TestTrue(TEXT("Malformed raw version remains diagnostic"), Malformed.RawOsVersion.bIsAvailable);
	TestFalse(TEXT("Malformed major version stays unavailable"), Malformed.OsVersionMajor.bIsAvailable);
	TestEqual(TEXT("Independent API level remains available"), Malformed.AndroidApiLevel.Value, 99);

	const FOpenMobileDeviceInformationSnapshot Future =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::Android,
			TEXT("123.45.678.9-preview"),
			150
		);
	TestEqual(TEXT("Future major version parses"), Future.OsVersionMajor.Value, 123);
	TestEqual(TEXT("Future minor version parses"), Future.OsVersionMinor.Value, 45);
	TestEqual(TEXT("Future patch version parses"), Future.OsVersionPatch.Value, 678);

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	const FOpenMobileDeviceInformationSnapshot Editor =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	TestEqual(TEXT("Unsupported editor platform stays unknown"), Editor.Platform, EOpenMobileDevicePlatform::Unknown);
	TestFalse(TEXT("Unsupported editor version stays unavailable"), Editor.ReadableOsVersion.bIsAvailable);

	FMockBackend First(TEXT("CachedPlatform"));
	First.DeviceInformation = Android;
	FOpenMobileDeviceBackendRegistry::RegisterBackend(First);
	const FOpenMobileDeviceInformationSnapshot CachedFirst =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	First.DeviceInformation.RawOsVersion =
		FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("mutated"));
	const FOpenMobileDeviceInformationSnapshot CachedSecond =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	TestEqual(TEXT("Immutable platform values query once"), First.DeviceInformationQueries, 1);
	TestEqual(TEXT("Same backend keeps cached raw version"), CachedSecond.RawOsVersion.Value, CachedFirst.RawOsVersion.Value);
	TestTrue(TEXT("Cached reads still receive fresh generations"), CachedSecond.Metadata.Generation > CachedFirst.Metadata.Generation);

	FOpenMobileDeviceBackendRegistry::UnregisterBackend(First);
	FMockBackend Replacement(TEXT("ReplacementPlatform"));
	Replacement.DeviceInformation = IOS;
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Replacement);
	const FOpenMobileDeviceInformationSnapshot Replaced =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	TestEqual(TEXT("Backend replacement invalidates platform cache"), Replaced.Platform, EOpenMobileDevicePlatform::IOS);
	TestEqual(TEXT("Replacement backend queries once"), Replacement.DeviceInformationQueries, 1);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Replacement);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceHardwareModelIdentifierTest,
	"OpenMobile.Device.Identity.HardwareModelIdentifier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceHardwareModelIdentifierTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	const FOpenMobileDeviceInformationSnapshot Android =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::Android,
			TEXT("15"),
			35,
			TEXT("Google"),
			TEXT("google"),
			TEXT("Pixel 9 Pro"),
			TEXT(" caiman ")
		);
	TestEqual(TEXT("Android model stays readable"), Android.Model.Value, FString(TEXT("Pixel 9 Pro")));
	TestEqual(TEXT("Android hardware model is trimmed"), Android.HardwareModelIdentifier.Value, FString(TEXT("caiman")));

	const FOpenMobileDeviceInformationSnapshot IOS =
		FOpenMobileDevicePlatformInfo::BuildSnapshot(
			EOpenMobileDevicePlatform::IOS,
			TEXT("18.0"),
			0,
			FString(),
			FString(),
			TEXT("iPhone"),
			TEXT("iPhone17,1")
		);
	TestEqual(TEXT("iOS machine identifier is preserved"), IOS.HardwareModelIdentifier.Value, FString(TEXT("iPhone17,1")));

	for (const FName ForbiddenName : {
		FName(TEXT("IMEI")),
		FName(TEXT("SerialNumber")),
		FName(TEXT("RadioIdentifier")),
		FName(TEXT("MacAddress")),
		FName(TEXT("AdvertisingId")),
		FName(TEXT("IDFV"))
	})
	{
		TestNull(
			*FString::Printf(
				TEXT("Device information excludes %s"),
				*ForbiddenName.ToString()
			),
			FOpenMobileDeviceInformationSnapshot::StaticStruct()
				->FindPropertyByName(ForbiddenName)
		);
	}

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FMockBackend Backend(TEXT("HardwareModel"));
	Backend.DeviceInformation = IOS;
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileDeviceInformationSnapshot First =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	const FOpenMobileDeviceInformationSnapshot Second =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	TestEqual(TEXT("Repeated reads query immutable identity once"), Backend.DeviceInformationQueries, 1);
	TestEqual(TEXT("Repeated reads preserve only the model class"), Second.HardwareModelIdentifier.Value, First.HardwareModelIdentifier.Value);
	TestTrue(TEXT("Repeated reads still receive new metadata"), Second.Metadata.Generation > First.Metadata.Generation);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceSettingsContractTest,
	"OpenMobile.Device.Settings.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceSettingsContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UOpenMobileDeviceSettings* Settings = NewObject<UOpenMobileDeviceSettings>();
	TestEqual(
		TEXT("Device settings use the OpenMobile category"),
		Settings->GetCategoryName(),
		FName(TEXT("OpenMobile"))
	);
	TestEqual(
		TEXT("Device settings keep their own section"),
		Settings->GetSectionName(),
		FName(TEXT("OpenMobile Device"))
	);
#if WITH_METADATA
	TestEqual(
		TEXT("Device settings display their section name"),
		Settings->GetClass()->GetMetaData(TEXT("DisplayName")),
		FString(TEXT("OpenMobile Device"))
	);
#endif
	TestTrue(
		TEXT("Device settings use default config"),
		Settings->GetClass()->HasAnyClassFlags(CLASS_DefaultConfig)
	);
	const FFloatProperty* PollingProperty = FindFProperty<FFloatProperty>(
		UOpenMobileDeviceSettings::StaticClass(),
		GET_MEMBER_NAME_CHECKED(
			UOpenMobileDeviceSettings,
			FallbackPollingIntervalSeconds
		)
	);
	TestTrue(
		TEXT("Fallback polling is serialized to config"),
		PollingProperty && PollingProperty->HasAnyPropertyFlags(CPF_Config)
	);
	TestEqual(
		TEXT("Default fallback polling interval is one second"),
		Settings->GetValidatedFallbackPollingIntervalSeconds(),
		1.0f
	);

	Settings->FallbackPollingIntervalSeconds =
		std::numeric_limits<float>::quiet_NaN();
	TestEqual(
		TEXT("Non-finite fallback interval uses the safe default"),
		Settings->GetValidatedFallbackPollingIntervalSeconds(),
		1.0f
	);
	Settings->FallbackPollingIntervalSeconds = -5.0f;
	TestEqual(
		TEXT("Negative fallback interval clamps to the safe minimum"),
		Settings->GetValidatedFallbackPollingIntervalSeconds(),
		0.1f
	);
	Settings->FallbackPollingIntervalSeconds = 500.0f;
	TestEqual(
		TEXT("Large fallback interval clamps to the safe maximum"),
		Settings->GetValidatedFallbackPollingIntervalSeconds(),
		60.0f
	);

	const FString ConfigPath = FPaths::CreateTempFilename(
		*FPaths::ProjectIntermediateDir(),
		TEXT("OpenMobileDeviceSettings"),
		TEXT(".ini")
	);
	Settings->FallbackPollingIntervalSeconds = 2.5f;
	Settings->SaveConfig(CPF_Config, *ConfigPath, GConfig, false);
	UOpenMobileDeviceSettings* Loaded = NewObject<UOpenMobileDeviceSettings>();
	Loaded->LoadConfig(UOpenMobileDeviceSettings::StaticClass(), *ConfigPath);
	IFileManager::Get().Delete(*ConfigPath, false, true, true);
	TestEqual(
		TEXT("Fallback polling interval survives config serialization"),
		Loaded->FallbackPollingIntervalSeconds,
		2.5f
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceStatusRangeTest,
	"OpenMobile.Device.StatusRange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceStatusRangeTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	const FOpenMobileDeviceStatus Status = UOpenMobileDeviceBlueprintLibrary::GetDeviceStatus();
	TestTrue(
		TEXT("Battery is unavailable or normalized"),
		Status.BatteryPercent == -1 || FMath::IsWithinInclusive(Status.BatteryPercent, 0, 100)
	);
	TestTrue(
		TEXT("Volume is unavailable or normalized"),
		Status.VolumePercent == -1 || FMath::IsWithinInclusive(Status.VolumePercent, 0, 100)
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceLegacyMigrationTest,
	"OpenMobile.Device.LegacyMigration.BatteryAndVolume",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceLegacyMigrationTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;
	using Group = EOpenMobileDeviceMonitoringGroup;

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FMockBackend Backend(TEXT("LegacyMigration"));
	Backend.NativeMonitoringGroups = {Group::Power, Group::MediaVolume};
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(45.4f);
	Backend.MediaVolume.VolumePercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(62.6f);
	Backend.SetCapability(
		FOpenMobileDeviceCapabilityNames::BatteryLevel,
		EOpenMobileCapabilityState::Available
	);
	Backend.SetCapability(
		FOpenMobileDeviceCapabilityNames::MediaVolume,
		EOpenMobileCapabilityState::Available
	);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);

	TestEqual(
		TEXT("Legacy battery reads the typed backend snapshot"),
		UOpenMobileDeviceBlueprintLibrary::GetBatteryPercent(),
		45
	);
	TestEqual(
		TEXT("Legacy volume reads the typed backend snapshot"),
		UOpenMobileDeviceBlueprintLibrary::GetVolumePercent(),
		63
	);
	const FOpenMobileDeviceStatus LegacyStatus =
		UOpenMobileDeviceBlueprintLibrary::GetDeviceStatus();
	TestTrue(TEXT("Legacy battery availability follows typed data"), LegacyStatus.bBatteryAvailable);
	TestTrue(TEXT("Legacy volume availability follows typed data"), LegacyStatus.bVolumeAvailable);
	TestEqual(
		TEXT("Battery capability remains typed"),
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
			FOpenMobileDeviceCapabilityNames::BatteryLevel
		).State,
		EOpenMobileCapabilityState::Available
	);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* Subsystem =
		NewObject<UOpenMobileDeviceSubsystem>(GameInstance);
	const FOpenMobileMediaVolumeSnapshot TypedVolume =
		Subsystem->GetMediaVolumeSnapshot();
	TestTrue(TEXT("Typed media volume is available"), TypedVolume.VolumePercent.bIsAvailable);
	TestEqual(TEXT("Typed media volume preserves precision"), TypedVolume.VolumePercent.Value, 62.6f);

	int32 PowerEvents = 0;
	int32 VolumeEvents = 0;
	int32 CombinedEvents = 0;
	Subsystem->OnNativePowerSnapshotChanged().AddLambda(
		[&PowerEvents](const FOpenMobilePowerSnapshot&)
		{
			++PowerEvents;
		}
	);
	Subsystem->OnNativeMediaVolumeSnapshotChanged().AddLambda(
		[&VolumeEvents](const FOpenMobileMediaVolumeSnapshot&)
		{
			++VolumeEvents;
		}
	);
	Subsystem->OnNativeDeviceStatusChanged().AddLambda(
		[&CombinedEvents](const FOpenMobileDeviceStatus&)
		{
			++CombinedEvents;
		}
	);
	UOpenMobileDeviceMonitoringSubscription* Subscription =
		Subsystem->StartMonitoring(
			GameInstance,
			{Group::Power, Group::MediaVolume},
			1.0f
		);
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(50.0f);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Power);
	TestEqual(TEXT("Battery changes emit only the power event"), PowerEvents, 1);
	TestEqual(TEXT("Battery changes do not emit volume events"), VolumeEvents, 0);
	TestEqual(TEXT("Battery changes do not rebroadcast combined status"), CombinedEvents, 0);

	Backend.MediaVolume.VolumePercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(70.0f);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(
		Group::MediaVolume
	);
	TestEqual(TEXT("Volume changes do not emit power events"), PowerEvents, 1);
	TestEqual(TEXT("Volume changes emit their own event"), VolumeEvents, 1);
	TestEqual(TEXT("Volume changes do not rebroadcast combined status"), CombinedEvents, 0);

#if WITH_METADATA
	for (const FName FunctionName : {
		GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceBlueprintLibrary, GetBatteryPercent),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceBlueprintLibrary, GetVolumePercent),
		GET_FUNCTION_NAME_CHECKED(UOpenMobileDeviceBlueprintLibrary, GetDeviceStatus)
	})
	{
		const UFunction* Function =
			UOpenMobileDeviceBlueprintLibrary::StaticClass()->FindFunctionByName(
				FunctionName
			);
		TestTrue(
			*FString::Printf(TEXT("%s is marked deprecated"), *FunctionName.ToString()),
			Function && Function->HasMetaData(TEXT("DeprecatedFunction"))
		);
	}
#endif

	Subscription->Stop();
	Subsystem->Deinitialize();
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	TestEqual(
		TEXT("Legacy battery keeps its unavailable sentinel without a backend"),
		UOpenMobileDeviceBlueprintLibrary::GetBatteryPercent(),
		-1
	);
	TestEqual(
		TEXT("Legacy volume keeps its unavailable sentinel without a backend"),
		UOpenMobileDeviceBlueprintLibrary::GetVolumePercent(),
		-1
	);
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDevicePublicTypeModelTest,
	"OpenMobile.Device.Types.PublicModel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDevicePublicTypeModelTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);

	const TArray<UScriptStruct*> SnapshotTypes = {
		FOpenMobileDeviceInformationSnapshot::StaticStruct(),
		FOpenMobileApplicationMetadataSnapshot::StaticStruct(),
		FOpenMobileLocaleSnapshot::StaticStruct(),
		FOpenMobilePowerSnapshot::StaticStruct(),
		FOpenMobileMediaVolumeSnapshot::StaticStruct(),
		FOpenMobileMemorySnapshot::StaticStruct(),
		FOpenMobileStorageSnapshot::StaticStruct(),
		FOpenMobileNetworkPathSnapshot::StaticStruct(),
		FOpenMobileWindowDisplaySnapshot::StaticStruct(),
		FOpenMobileAppearanceSnapshot::StaticStruct(),
		FOpenMobileAccessibilitySnapshot::StaticStruct(),
		FOpenMobileClipboardContent::StaticStruct()
	};
	for (const UScriptStruct* Struct : SnapshotTypes)
	{
#if WITH_METADATA
		TestTrue(
			*FString::Printf(TEXT("%s is available to Blueprint"), *Struct->GetName()),
			Struct->HasMetaData(TEXT("BlueprintType"))
		);
#endif
		TestNotNull(
			*FString::Printf(TEXT("%s has snapshot metadata"), *Struct->GetName()),
			Struct->FindPropertyByName(TEXT("Metadata"))
		);
		for (const FName ForbiddenName : {
			FName(TEXT("IMEI")),
			FName(TEXT("SerialNumber")),
			FName(TEXT("AndroidId")),
			FName(TEXT("IDFV")),
			FName(TEXT("MacAddress")),
			FName(TEXT("AdvertisingId")),
			FName(TEXT("InstalledApps"))
		})
		{
			TestNull(
				*FString::Printf(
					TEXT("%s excludes personal field %s"),
					*Struct->GetName(),
					*ForbiddenName.ToString()
				),
				Struct->FindPropertyByName(ForbiddenName)
			);
		}
	}
	TestNotNull(
		TEXT("Locale change exposes its previous snapshot"),
		FOpenMobileLocaleSnapshotChange::StaticStruct()->FindPropertyByName(
			TEXT("PreviousSnapshot")
		)
	);
	TestNotNull(
		TEXT("Locale change exposes its current snapshot"),
		FOpenMobileLocaleSnapshotChange::StaticStruct()->FindPropertyByName(
			TEXT("CurrentSnapshot")
		)
	);

	const FOpenMobileDeviceInformationSnapshot DeviceDefaults;
	TestEqual(
		TEXT("Device platform defaults to unknown"),
		DeviceDefaults.Platform,
		EOpenMobileDevicePlatform::Unknown
	);
	TestFalse(
		TEXT("Android API level defaults to unavailable"),
		DeviceDefaults.AndroidApiLevel.bIsAvailable
	);
	TestEqual(
		TEXT("Snapshot generation defaults to zero"),
		DeviceDefaults.Metadata.Generation,
		int64(0)
	);
	TestEqual(
		TEXT("Snapshot capture time defaults to unset"),
		DeviceDefaults.Metadata.CapturedAtUtc,
		FDateTime()
	);

	FOpenMobilePowerSnapshot Source;
	Source.Metadata.CapturedAtUtc = FDateTime(2026, 8, 22, 1, 2, 3);
	Source.Metadata.Generation = 42;
	Source.BatteryPercent = FOpenMobileDeviceOptionalFloat::MakeAvailable(57.5f);
	Source.ChargingState = EOpenMobileBatteryChargingState::Charging;
	Source.ThermalState = EOpenMobileThermalState::Serious;
	FOpenMobilePowerSnapshot EqualCopy = Source;
	TestTrue(TEXT("Equal snapshots compare equal"), EqualCopy == Source);
	EqualCopy.Metadata.Generation++;
	TestTrue(TEXT("Changed snapshots compare unequal"), EqualCopy != Source);

	TArray<uint8> Bytes;
	FMemoryWriter Writer(Bytes, true);
	FOpenMobilePowerSnapshot::StaticStruct()->SerializeItem(Writer, &Source, nullptr);
	Writer.Close();
	FOpenMobilePowerSnapshot RoundTrip;
	FMemoryReader Reader(Bytes, true);
	FOpenMobilePowerSnapshot::StaticStruct()->SerializeItem(Reader, &RoundTrip, nullptr);
	Reader.Close();
	TestTrue(TEXT("Snapshot serialization preserves values"), RoundTrip == Source);
	TestFalse(
		TEXT("Future enum values are not mistaken for known values"),
		StaticEnum<EOpenMobileThermalState>()->IsValidEnumValue(255)
	);
	TestEqual(
		TEXT("Thermal enum has a stable unknown fallback"),
		static_cast<uint8>(EOpenMobileThermalState::Unknown),
		uint8(0)
	);

	const FOpenMobileNetworkPathSnapshot NetworkDefaults;
	TestEqual(
		TEXT("Network path defaults to unknown"),
		NetworkDefaults.PathState,
		EOpenMobileNetworkPathState::Unknown
	);
	TestFalse(
		TEXT("Metered state defaults to unavailable"),
		NetworkDefaults.bIsMetered.bIsAvailable
	);
	TestTrue(
		TEXT("Default network transports are empty"),
		NetworkDefaults.Transports.IsEmpty()
	);
#if WITH_METADATA
	TestTrue(
		TEXT("Control result is reflected"),
		FOpenMobileDeviceControlResult::StaticStruct()->HasMetaData(TEXT("BlueprintType"))
	);
#endif
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceCapabilityReportTest,
	"OpenMobile.Device.Capabilities.Report",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceCapabilityReportTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	const TArray<FName>& Names = FOpenMobileDeviceCapabilityNames::GetAll();
	TSet<FName> UniqueNames;
	for (const FName Name : Names)
	{
		UniqueNames.Add(Name);
	}
	TestTrue(TEXT("Capability catalog covers the Device surface"), Names.Num() >= 50);
	TestEqual(TEXT("Capability names are unique"), UniqueNames.Num(), Names.Num());
	TestTrue(
		TEXT("Catalog includes endpoint reachability"),
		UniqueNames.Contains(FOpenMobileDeviceCapabilityNames::EndpointReachability)
	);
	TestTrue(
		TEXT("Catalog includes Android foldable posture"),
		UniqueNames.Contains(FOpenMobileDeviceCapabilityNames::FoldablePosture)
	);
	TestTrue(
		TEXT("Catalog includes user-initiated clipboard read"),
		UniqueNames.Contains(FOpenMobileDeviceCapabilityNames::ClipboardRead)
	);

	const FOpenMobileDeviceCapability Unsupported =
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
			FOpenMobileDeviceCapabilityNames::BatteryLevel
		);
	TestEqual(
		TEXT("Editor without backend reports unsupported"),
		Unsupported.State,
		EOpenMobileCapabilityState::NotSupported
	);
	TestEqual(
		TEXT("Unsupported editor limit is explicit"),
		Unsupported.Limit,
		EOpenMobileDeviceCapabilityLimit::UnsupportedPlatform
	);
	TestEqual(
		TEXT("Unknown capability names remain distinguishable"),
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
			TEXT("OpenMobile.Device.Future.Query")
		).State,
		EOpenMobileCapabilityState::NotConfigured
	);

	FOpenMobileDeviceCapability Available;
	Available.Name = FOpenMobileDeviceCapabilityNames::FlashlightControl;
	Available.State = EOpenMobileCapabilityState::Available;
	FOpenMobileDeviceCapability PermissionRequired = Available;
	PermissionRequired.State = EOpenMobileCapabilityState::PermissionRequired;
	FOpenMobileDeviceCapability Denied = Available;
	Denied.State = EOpenMobileCapabilityState::Denied;
	const TArray<FOpenMobileDeviceCapability> Candidates = {
		Available,
		PermissionRequired,
		Denied
	};
	TestEqual(
		TEXT("Denied wins capability-state precedence"),
		FOpenMobileDeviceCapability::Resolve(
			FOpenMobileDeviceCapabilityNames::FlashlightControl,
			Candidates
		).State,
		EOpenMobileCapabilityState::Denied
	);
	FOpenMobileDeviceCapability Restricted = Available;
	Restricted.State = EOpenMobileCapabilityState::Restricted;
	FOpenMobileDeviceCapability NotSupported = Available;
	NotSupported.State = EOpenMobileCapabilityState::NotSupported;
	const TArray<FOpenMobileDeviceCapability> PermanentLimits = {
		Available,
		Denied,
		Restricted,
		NotSupported
	};
	TestEqual(
		TEXT("Permanent platform limit wins capability-state precedence"),
		FOpenMobileDeviceCapability::Resolve(
			FOpenMobileDeviceCapabilityNames::FlashlightControl,
			PermanentLimits
		).State,
		EOpenMobileCapabilityState::NotSupported
	);

	FMockBackend Platform(TEXT("Platform"), 0);
	Platform.SetCapability(
		FOpenMobileDeviceCapabilityNames::BatteryLevel,
		EOpenMobileCapabilityState::NotSupported,
		EOpenMobileDeviceCapabilityLimit::MissingHardware
	);
	FMockBackend Mock(TEXT("Mock"), 100);
	Mock.SetCapability(
		FOpenMobileDeviceCapabilityNames::BatteryLevel,
		EOpenMobileCapabilityState::Available
	);
	Mock.SetCapability(
		FOpenMobileDeviceCapabilityNames::FlashlightControl,
		EOpenMobileCapabilityState::PermissionRequired
	);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Platform);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Mock);
	TestEqual(
		TEXT("Higher-priority mock overrides platform capability"),
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
			FOpenMobileDeviceCapabilityNames::BatteryLevel
		).State,
		EOpenMobileCapabilityState::Available
	);
	TestEqual(
		TEXT("Permission-required state is returned without prompting"),
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
			FOpenMobileDeviceCapabilityNames::FlashlightControl
		).State,
		EOpenMobileCapabilityState::PermissionRequired
	);
	Mock.SetCapability(
		FOpenMobileDeviceCapabilityNames::FlashlightControl,
		EOpenMobileCapabilityState::Denied
	);
	TestEqual(
		TEXT("Permission changes are visible on the next query"),
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
			FOpenMobileDeviceCapabilityNames::FlashlightControl
		).State,
		EOpenMobileCapabilityState::Denied
	);
	Mock.SetCapability(
		FOpenMobileDeviceCapabilityNames::ThermalState,
		static_cast<EOpenMobileCapabilityState>(255)
	);
	TestEqual(
		TEXT("Unknown future states normalize to unavailable"),
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapability(
			FOpenMobileDeviceCapabilityNames::ThermalState
		).State,
		EOpenMobileCapabilityState::Unavailable
	);

	const FOpenMobileDeviceCapabilityReport Report =
		UOpenMobileDeviceBlueprintLibrary::GetDeviceCapabilityReport();
	TestEqual(TEXT("Complete report contains every name"), Report.Capabilities.Num(), Names.Num());
	TestNotNull(
		TEXT("Complete report contains battery capability"),
		Report.Find(FOpenMobileDeviceCapabilityNames::BatteryLevel)
	);
	const FOpenMobileDeviceCapability* PartialCapability =
		Report.Find(FOpenMobileDeviceCapabilityNames::ClipboardClear);
	TestNotNull(TEXT("Complete report contains partial support"), PartialCapability);
	if (PartialCapability)
	{
		TestEqual(
			TEXT("Unimplemented mock capability reports unsupported"),
			PartialCapability->State,
			EOpenMobileCapabilityState::NotSupported
		);
	}
	TestTrue(TEXT("Capability report is timestamped"), Report.Metadata.CapturedAtUtc > FDateTime());
	TestTrue(TEXT("Capability report has a generation"), Report.Metadata.Generation > 0);

	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Mock);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Platform);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceSynchronousSnapshotTest,
	"OpenMobile.Device.Snapshots.SynchronousLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceSynchronousSnapshotTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	const FOpenMobileDeviceInformationSnapshot Startup =
		FOpenMobileDeviceSnapshotService::GetDeviceInformationSnapshot();
	TestTrue(TEXT("Startup fallback has capture time"), Startup.Metadata.CapturedAtUtc > FDateTime());
	TestTrue(TEXT("Startup fallback has generation"), Startup.Metadata.Generation > 0);
	TestFalse(TEXT("Startup fallback keeps model unavailable"), Startup.Model.bIsAvailable);

	FMockBackend First(TEXT("First"));
	First.DeviceInformation.Model =
		FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("First Model"));
	First.Power.BatteryPercent = FOpenMobileDeviceOptionalFloat::MakeAvailable(80.0f);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(First);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* Subsystem =
		NewObject<UOpenMobileDeviceSubsystem>(GameInstance);
	const FOpenMobileDeviceInformationSnapshot FirstSnapshot =
		Subsystem->GetDeviceInformationSnapshot();
	TestEqual(TEXT("Subsystem returns selected backend model"), FirstSnapshot.Model.Value, FString(TEXT("First Model")));
	TestEqual(TEXT("Focused identity query runs once"), First.DeviceInformationQueries, 1);
	TestEqual(TEXT("Focused identity query does not read power"), First.PowerQueries, 0);

	First.bInBackground = true;
	const FOpenMobilePowerSnapshot Background = Subsystem->GetPowerSnapshot();
	TestEqual(TEXT("Background snapshot remains queryable"), Background.BatteryPercent.Value, 20.0f);
	TestEqual(TEXT("Background power query runs once"), First.PowerQueries, 1);

	FOpenMobileDeviceBackendRegistry::UnregisterBackend(First);
	FMockBackend Second(TEXT("Second"));
	Second.DeviceInformation.Model =
		FOpenMobileDeviceOptionalString::MakeAvailable(TEXT("Second Model"));
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Second);
	const FOpenMobileDeviceInformationSnapshot Replacement =
		Subsystem->GetDeviceInformationSnapshot();
	TestEqual(TEXT("Replacement backend is visible immediately"), Replacement.Model.Value, FString(TEXT("Second Model")));
	TestTrue(
		TEXT("Replacement snapshot supersedes earlier generation"),
		Replacement.Metadata.Generation > FirstSnapshot.Metadata.Generation
	);

	Subsystem->Deinitialize();
	const int32 QueriesBeforeTeardownRead = Second.DeviceInformationQueries;
	const FOpenMobileDeviceInformationSnapshot AfterTeardown =
		Subsystem->GetDeviceInformationSnapshot();
	TestEqual(TEXT("Teardown rejects snapshot work"), AfterTeardown.Metadata.Generation, int64(0));
	TestEqual(
		TEXT("Teardown does not touch backend"),
		Second.DeviceInformationQueries,
		QueriesBeforeTeardownRead
	);

	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Second);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceAsyncContractTest,
	"OpenMobile.Device.Async.ExactlyOnceAndTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceAsyncContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	const UClass* ActionClass = UOpenMobileDeviceAsyncActionBase::StaticClass();
	TestNotNull(TEXT("Async action exposes Success branch"), ActionClass->FindPropertyByName(TEXT("Success")));
	TestNotNull(TEXT("Async action exposes Cancelled branch"), ActionClass->FindPropertyByName(TEXT("Cancelled")));
	TestNotNull(TEXT("Async action exposes Failed branch"), ActionClass->FindPropertyByName(TEXT("Failed")));
	int32 TerminalCount = 0;
	EOpenMobileDeviceAsyncTerminalState LastState =
		EOpenMobileDeviceAsyncTerminalState::Pending;
	UOpenMobileDeviceAsyncActionBase* Action =
		NewObject<UOpenMobileDeviceAsyncActionBase>();
	Action->OnNativeTerminal().AddLambda(
		[&TerminalCount, &LastState](
			EOpenMobileDeviceAsyncTerminalState State,
			const FOpenMobileError&)
		{
			++TerminalCount;
			LastState = State;
		}
	);
	Action->FinishSucceeded();
	Action->FinishFailed(FOpenMobileError::Make(
		EOpenMobileErrorCode::NativeFailure,
		TEXT("late failure")
	));
	Action->Cancel();
	TestEqual(TEXT("Only first terminal path broadcasts"), TerminalCount, 1);
	TestEqual(TEXT("First terminal state wins"), LastState, EOpenMobileDeviceAsyncTerminalState::Succeeded);

	int32 WorldCancellationCount = 0;
	UWorld* TargetWorld = NewObject<UWorld>();
	UOpenMobileDeviceAsyncActionBase* WorldAction =
		NewObject<UOpenMobileDeviceAsyncActionBase>();
	WorldAction->TargetWorld = TargetWorld;
	WorldAction->OnNativeTerminal().AddLambda(
		[&WorldCancellationCount](
			EOpenMobileDeviceAsyncTerminalState State,
			const FOpenMobileError& Error)
		{
			if (State == EOpenMobileDeviceAsyncTerminalState::Cancelled
				&& Error.Code == EOpenMobileErrorCode::Cancelled)
			{
				++WorldCancellationCount;
			}
		}
	);
	WorldAction->HandleWorldCleanup(NewObject<UWorld>(), true, true);
	WorldAction->HandleWorldCleanup(TargetWorld, true, true);
	WorldAction->HandleWorldCleanup(TargetWorld, true, true);
	TestEqual(TEXT("Matching world cleanup cancels exactly once"), WorldCancellationCount, 1);

	int32 GameInstanceCancellationCount = 0;
	UGameInstance* AsyncGameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* AsyncSubsystem =
		NewObject<UOpenMobileDeviceSubsystem>(AsyncGameInstance);
	UOpenMobileDeviceAsyncActionBase* GameInstanceAction =
		NewObject<UOpenMobileDeviceAsyncActionBase>();
	GameInstanceAction->OnNativeTerminal().AddLambda(
		[&GameInstanceCancellationCount](
			EOpenMobileDeviceAsyncTerminalState State,
			const FOpenMobileError&)
		{
			if (State == EOpenMobileDeviceAsyncTerminalState::Cancelled)
			{
				++GameInstanceCancellationCount;
			}
		}
	);
	AsyncSubsystem->RegisterAsyncAction(GameInstanceAction);
	GameInstanceAction->Subsystem = AsyncSubsystem;
	AsyncSubsystem->Deinitialize();
	AsyncSubsystem->Deinitialize();
	TestEqual(TEXT("Game Instance teardown cancels exactly once"), GameInstanceCancellationCount, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceDemandDrivenMonitoringTest,
	"OpenMobile.Device.Monitoring.DemandDriven",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceDemandDrivenMonitoringTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;
	using Group = EOpenMobileDeviceMonitoringGroup;

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FMockBackend Backend(TEXT("Monitoring"));
	Backend.NativeMonitoringGroups = {Group::Power, Group::Network};
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(80.0f);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);
	TestFalse(
		TEXT("Zero listeners use no ticker"),
		FOpenMobileDeviceMonitoringService::IsTickerActiveForTests()
	);
	TestEqual(
		TEXT("Zero listeners start no native observer"),
		Backend.MonitoringStarts.Num(),
		0
	);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* Subsystem =
		NewObject<UOpenMobileDeviceSubsystem>(GameInstance);
	int32 PowerEvents = 0;
	Subsystem->OnNativePowerSnapshotChanged().AddLambda(
		[&PowerEvents](const FOpenMobilePowerSnapshot&)
		{
			++PowerEvents;
		}
	);
	UGameInstance* OwnerA = NewObject<UGameInstance>();
	UGameInstance* OwnerB = NewObject<UGameInstance>();
	UOpenMobileDeviceMonitoringSubscription* First = Subsystem->StartMonitoring(
		OwnerA,
		{Group::Power},
		1.0f
	);
	UOpenMobileDeviceMonitoringSubscription* Duplicate = Subsystem->StartMonitoring(
		OwnerA,
		{Group::Power},
		1.0f
	);
	TestNotNull(TEXT("First subscription starts"), First);
	TestNotNull(TEXT("Duplicate subscription starts independently"), Duplicate);
	TestEqual(
		TEXT("Duplicate starts increase reference count"),
		FOpenMobileDeviceMonitoringService::GetReferenceCountForTests(Group::Power),
		2
	);
	TestEqual(
		TEXT("Multiple consumers share one native observer"),
		Backend.MonitoringStarts.FindRef(Group::Power),
		1
	);
	TestTrue(
		TEXT("Active subscriptions keep maintenance ticker"),
		FOpenMobileDeviceMonitoringService::IsTickerActiveForTests()
	);
	const int32 NativeQueriesBeforeTick = Backend.PowerQueries;
	FOpenMobileDeviceMonitoringService::TickForTests(5.0f);
	TestEqual(
		TEXT("Native notification source is not fallback-polled"),
		Backend.PowerQueries,
		NativeQueriesBeforeTick
	);
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(70.0f);
	FOpenMobileDeviceMonitoringService::NotifyNativeChangeForTests(Group::Power);
	TestEqual(TEXT("Native change emits one coalesced event"), PowerEvents, 1);

	First->Stop();
	TestEqual(
		TEXT("Partial stop keeps shared observer active"),
		FOpenMobileDeviceMonitoringService::GetReferenceCountForTests(Group::Power),
		1
	);
	TestEqual(
		TEXT("Partial stop does not stop native observer"),
		Backend.MonitoringStops.FindRef(Group::Power),
		0
	);
	Duplicate->Stop();
	TestEqual(
		TEXT("Final stop releases shared observer"),
		Backend.MonitoringStops.FindRef(Group::Power),
		1
	);
	TestFalse(
		TEXT("Final stop removes ticker"),
		FOpenMobileDeviceMonitoringService::IsTickerActiveForTests()
	);

	UOpenMobileDeviceMonitoringSubscription* MultiGroup = Subsystem->StartMonitoring(
		OwnerA,
		{Group::Power, Group::Network},
		1.0f
	);
	UOpenMobileDeviceMonitoringSubscription* PowerOnly = Subsystem->StartMonitoring(
		OwnerB,
		{Group::Power},
		1.0f
	);
	MultiGroup->Stop();
	TestEqual(
		TEXT("Stopping multi-group subscription releases its sole network observer"),
		Backend.MonitoringStops.FindRef(Group::Network),
		1
	);
	TestEqual(
		TEXT("Stopping multi-group subscription preserves other power consumer"),
		FOpenMobileDeviceMonitoringService::GetReferenceCountForTests(Group::Power),
		1
	);
	PowerOnly->Stop();

	Backend.NativeMonitoringGroups.Remove(Group::Power);
	Backend.PowerQueries = 0;
	PowerEvents = 0;
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(50.0f);
	UOpenMobileDeviceMonitoringSubscription* Fallback = Subsystem->StartMonitoring(
		OwnerB,
		{Group::Power},
		0.001f
	);
	TestTrue(
		TEXT("Unsupported native source uses fallback"),
		FOpenMobileDeviceMonitoringService::UsesFallbackForTests(Group::Power)
	);
	TestEqual(
		TEXT("Fallback interval clamps to minimum"),
		FOpenMobileDeviceMonitoringService::GetEffectiveIntervalForTests(Group::Power),
		0.1f
	);
	TestEqual(TEXT("Subscription primes one baseline snapshot"), Backend.PowerQueries, 1);
	FOpenMobileDeviceMonitoringService::TickForTests(0.1f);
	TestEqual(TEXT("Equal fallback sample is coalesced"), PowerEvents, 0);
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(50.2f);
	FOpenMobileDeviceMonitoringService::TickForTests(0.1f);
	TestEqual(TEXT("Battery noise inside tolerance is coalesced"), PowerEvents, 0);
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(50.8f);
	FOpenMobileDeviceMonitoringService::TickForTests(0.1f);
	TestEqual(TEXT("Meaningful battery change broadcasts"), PowerEvents, 1);

	const int32 QueriesBeforeBackground = Backend.PowerQueries;
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(60.0f);
	FOpenMobileDeviceMonitoringService::SetApplicationActiveForTests(false);
	FOpenMobileDeviceMonitoringService::TickForTests(10.0f);
	TestEqual(
		TEXT("Background suspension performs no fallback query"),
		Backend.PowerQueries,
		QueriesBeforeBackground
	);
	FOpenMobileDeviceMonitoringService::SetApplicationActiveForTests(true);
	TestEqual(TEXT("Foreground refresh broadcasts latest value"), PowerEvents, 2);

	Fallback->Owner.Reset();
	FOpenMobileDeviceMonitoringService::TickForTests(0.1f);
	TestFalse(TEXT("Invalid owner releases subscription"), Fallback->IsActive());
	TestEqual(
		TEXT("Invalid owner releases final reference"),
		FOpenMobileDeviceMonitoringService::GetReferenceCountForTests(Group::Power),
		0
	);
	TestFalse(
		TEXT("No listeners restore zero idle ticker cost"),
		FOpenMobileDeviceMonitoringService::IsTickerActiveForTests()
	);
	UOpenMobileDeviceMonitoringSubscription* MaximumInterval =
		Subsystem->StartMonitoring(OwnerA, {Group::Power}, 120.0f);
	TestEqual(
		TEXT("Fallback interval clamps to maximum"),
		FOpenMobileDeviceMonitoringService::GetEffectiveIntervalForTests(Group::Power),
		60.0f
	);
	MaximumInterval->Stop();
	TestFalse(
		TEXT("Stopping maximum interval request restores zero idle cost"),
		FOpenMobileDeviceMonitoringService::IsTickerActiveForTests()
	);
	{
		FOpenMobileDeviceMonitoringHandle NativeHandle =
			Subsystem->StartMonitoringNative({Group::Power}, 1.0f);
		TestTrue(TEXT("Native monitoring handle starts active"), NativeHandle.IsActive());
		FOpenMobileDeviceMonitoringHandle MovedHandle = MoveTemp(NativeHandle);
		TestFalse(TEXT("Moved-from native handle becomes inactive"), NativeHandle.IsActive());
		TestTrue(TEXT("Moved native handle preserves subscription"), MovedHandle.IsActive());
	}
	TestEqual(
		TEXT("Native monitoring handle releases on scope exit"),
		FOpenMobileDeviceMonitoringService::GetReferenceCountForTests(Group::Power),
		0
	);
	UOpenMobileDeviceSettings* DeviceSettings =
		GetMutableDefault<UOpenMobileDeviceSettings>();
	const float SavedFallbackInterval =
		DeviceSettings->FallbackPollingIntervalSeconds;
	DeviceSettings->FallbackPollingIntervalSeconds = 2.5f;
	UOpenMobileDeviceMonitoringSubscription* ConfiguredInterval =
		Subsystem->StartMonitoring(OwnerA, {Group::Power}, 0.0f);
	TestEqual(
		TEXT("Zero fallback interval uses Project Settings"),
		FOpenMobileDeviceMonitoringService::GetEffectiveIntervalForTests(Group::Power),
		2.5f
	);
	ConfiguredInterval->Stop();
	DeviceSettings->FallbackPollingIntervalSeconds = SavedFallbackInterval;

	Subsystem->Deinitialize();
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceCallbackDispatchTest,
	"OpenMobile.Device.Callbacks.GameThreadOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceCallbackDispatchTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;
	using Group = EOpenMobileDeviceMonitoringGroup;

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FMockBackend Backend(TEXT("Callbacks"));
	Backend.NativeMonitoringGroups = {Group::Power};
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(10.0f);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Backend);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileDeviceSubsystem* Subsystem =
		NewObject<UOpenMobileDeviceSubsystem>(GameInstance);
	TArray<int64> Generations;
	bool bEveryCallbackWasOnGameThread = true;
	Subsystem->OnNativePowerSnapshotChanged().AddLambda(
		[&Generations, &bEveryCallbackWasOnGameThread](
			const FOpenMobilePowerSnapshot& Snapshot
		)
		{
			bEveryCallbackWasOnGameThread &= IsInGameThread();
			Generations.Add(Snapshot.Metadata.Generation);
		}
	);
	UOpenMobileDeviceMonitoringSubscription* Subscription =
		Subsystem->StartMonitoring(GameInstance, {Group::Power}, 1.0f);
	const FOpenMobileDeviceMonitoringCallbackToken FirstToken =
		Backend.MonitoringTokens.FindRef(Group::Power);

	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(20.0f);
	TFuture<void> SequenceTwo = Async(EAsyncExecution::ThreadPool, [FirstToken]()
	{
		FOpenMobileDeviceMonitoringService::NotifyNativeChange(FirstToken, 2);
	});
	TFuture<void> SequenceOne = Async(EAsyncExecution::ThreadPool, [FirstToken]()
	{
		FOpenMobileDeviceMonitoringService::NotifyNativeChange(FirstToken, 1);
	});
	SequenceTwo.Wait();
	SequenceOne.Wait();
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	TestTrue(
		TEXT("Worker callbacks reach public delegates on the game thread"),
		bEveryCallbackWasOnGameThread
	);
	TestEqual(
		TEXT("Concurrent callbacks retain the latest source sequence"),
		FOpenMobileDeviceMonitoringService::GetLastNativeSequenceForTests(
			Group::Power
		),
		static_cast<uint64>(2)
	);
	TestEqual(TEXT("Equivalent concurrent samples coalesce"), Generations.Num(), 1);

	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(30.0f);
	FOpenMobileDeviceMonitoringService::NotifyNativeChange(FirstToken, 1);
	FOpenMobileDeviceMonitoringService::NotifyNativeChange(FirstToken, 2);
	TestEqual(TEXT("Out-of-order and duplicate callbacks are dropped"), Generations.Num(), 1);

	Subscription->Stop();
	FOpenMobileDeviceMonitoringService::NotifyNativeChange(FirstToken, 3);
	TestEqual(TEXT("Callbacks after cancellation are dropped"), Generations.Num(), 1);

	UOpenMobileDeviceMonitoringSubscription* Restarted =
		Subsystem->StartMonitoring(GameInstance, {Group::Power}, 1.0f);
	const FOpenMobileDeviceMonitoringCallbackToken RestartedToken =
		Backend.MonitoringTokens.FindRef(Group::Power);
	FOpenMobileDeviceMonitoringService::NotifyNativeChange(FirstToken, 4);
	TestEqual(TEXT("Callbacks from a stopped observer stay stale"), Generations.Num(), 1);
	Backend.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(35.0f);
	FOpenMobileDeviceMonitoringService::NotifyNativeChange(RestartedToken, 1);
	TestEqual(TEXT("A restarted observer accepts its first callback"), Generations.Num(), 2);

	FMockBackend Replacement(TEXT("Replacement"), 1);
	Replacement.NativeMonitoringGroups = {Group::Power};
	Replacement.Power.BatteryPercent =
		FOpenMobileDeviceOptionalFloat::MakeAvailable(40.0f);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Replacement);
	FOpenMobileDeviceMonitoringService::TickForTests(0.0f);
	TestEqual(
		TEXT("Backend replacement stops the previous native observer"),
		Backend.MonitoringStops.FindRef(Group::Power),
		2
	);
	const FOpenMobileDeviceMonitoringCallbackToken ReplacementToken =
		Replacement.MonitoringTokens.FindRef(Group::Power);
	FOpenMobileDeviceMonitoringService::NotifyNativeChange(RestartedToken, 2);
	TestEqual(TEXT("Callbacks from a replaced backend are dropped"), Generations.Num(), 2);
	FOpenMobileDeviceMonitoringService::NotifyNativeChange(ReplacementToken, 1);
	TestEqual(TEXT("Replacement backend callbacks are accepted"), Generations.Num(), 3);
	TestTrue(
		TEXT("Accepted snapshot generations increase"),
		Generations.Num() == 3
			&& Generations[0] < Generations[1]
			&& Generations[1] < Generations[2]
	);

	FOpenMobileDeviceMonitoringService::Shutdown();
	FOpenMobileDeviceMonitoringService::NotifyNativeChange(ReplacementToken, 2);
	TestEqual(TEXT("Callbacks after shutdown are dropped"), Generations.Num(), 3);
	Restarted->Stop();
	Subsystem->Deinitialize();
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Replacement);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileDeviceMonitoringService::Start();
	FOpenMobileDeviceMonitoringService::ResetForTests();
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileDeviceBackendRegistryTest,
	"OpenMobile.Device.Backend.Registry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileDeviceBackendRegistryTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileDeviceTests;

	FOpenMobileDeviceBackendRegistry::ResetForTests();
	TestNull(
		TEXT("No backend remains unavailable"),
		FOpenMobileDeviceBackendRegistry::FindBackend()
	);

	FMockBackend Single(TEXT("Single"));
	TestTrue(
		TEXT("One backend registers"),
		FOpenMobileDeviceBackendRegistry::RegisterBackend(Single)
	);
	TestTrue(
		TEXT("One backend is selected"),
		FOpenMobileDeviceBackendRegistry::FindBackend() == &Single
	);
	TestEqual(
		TEXT("Supported domain reports available"),
		Single.GetDomainCapability(EOpenMobileDeviceBackendDomain::Power).State,
		EOpenMobileCapabilityState::Available
	);
	TestEqual(
		TEXT("Partial backend reports unsupported domains"),
		Single.GetDomainCapability(EOpenMobileDeviceBackendDomain::Display).State,
		EOpenMobileCapabilityState::NotSupported
	);
	TestFalse(
		TEXT("Duplicate pointer registration is rejected"),
		FOpenMobileDeviceBackendRegistry::RegisterBackend(Single)
	);
	FMockBackend DuplicateName(TEXT("Single"), 100);
	TestFalse(
		TEXT("Duplicate name registration is rejected"),
		FOpenMobileDeviceBackendRegistry::RegisterBackend(DuplicateName)
	);
	TestTrue(
		TEXT("Registered backend unregisters"),
		FOpenMobileDeviceBackendRegistry::UnregisterBackend(Single)
	);
	TestEqual(TEXT("Unregistration shuts backend down once"), Single.ShutdownCount, 1);

	FMockBackend Beta(TEXT("Beta"), 10);
	FMockBackend Alpha(TEXT("Alpha"), 10);
	FMockBackend Low(TEXT("Low"), 5);
	FMockBackend Unavailable(TEXT("Unavailable"), 100, false);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Beta);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Alpha);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Low);
	FOpenMobileDeviceBackendRegistry::RegisterBackend(Unavailable);
	TestTrue(
		TEXT("Equal priorities use backend name as stable tie break"),
		FOpenMobileDeviceBackendRegistry::FindBackend() == &Alpha
	);
	const FOpenMobileDeviceCallbackToken OldToken =
		FOpenMobileDeviceBackendRegistry::CaptureCallbackToken();
	TestTrue(
		TEXT("Current callback token is valid"),
		FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(OldToken)
	);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Alpha);
	TestFalse(
		TEXT("Backend change invalidates stale callbacks"),
		FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(OldToken)
	);
	TestTrue(
		TEXT("Unregistration selects deterministic fallback"),
		FOpenMobileDeviceBackendRegistry::FindBackend() == &Beta
	);

	const FOpenMobileDeviceCallbackToken ShutdownToken =
		FOpenMobileDeviceBackendRegistry::CaptureCallbackToken();
	FOpenMobileDeviceBackendRegistry::BeginShutdown();
	TestTrue(
		TEXT("Registry enters shutdown"),
		FOpenMobileDeviceBackendRegistry::IsShuttingDown()
	);
	TestNull(
		TEXT("Shutdown rejects backend lookup"),
		FOpenMobileDeviceBackendRegistry::FindBackend()
	);
	TestFalse(
		TEXT("Shutdown invalidates callbacks"),
		FOpenMobileDeviceBackendRegistry::IsCallbackCurrent(ShutdownToken)
	);
	FMockBackend Late(TEXT("Late"), 200);
	TestFalse(
		TEXT("Shutdown rejects new work"),
		FOpenMobileDeviceBackendRegistry::RegisterBackend(Late)
	);

	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Beta);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Low);
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(Unavailable);
	TestEqual(TEXT("Shutdown is delivered once"), Beta.ShutdownCount, 1);
	TestEqual(TEXT("Shutdown is delivered once to fallback"), Low.ShutdownCount, 1);
	TestEqual(
		TEXT("Shutdown is delivered once to unavailable backend"),
		Unavailable.ShutdownCount,
		1
	);
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	return true;
}

#endif
