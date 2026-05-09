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
#include "OpenMobileDeviceAsyncActionBase.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBlueprintLibrary.h"
#include "OpenMobileDeviceCapabilities.h"
#include "OpenMobileDeviceClipboardTypes.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceIdentityTypes.h"
#include "OpenMobileDeviceLocaleTypes.h"
#include "OpenMobileDeviceMonitoring.h"
#include "OpenMobileDeviceMonitoringService.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "OpenMobileDeviceSettings.h"
#include "OpenMobileDeviceSnapshotService.h"
#include "OpenMobileDeviceSubsystem.h"
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
		mutable int32 PowerQueries = 0;
		mutable int32 MediaVolumeQueries = 0;
		bool bInBackground = false;
		FOpenMobileDeviceInformationSnapshot DeviceInformation;
		FOpenMobilePowerSnapshot Power;
		FOpenMobileMediaVolumeSnapshot MediaVolume;
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
