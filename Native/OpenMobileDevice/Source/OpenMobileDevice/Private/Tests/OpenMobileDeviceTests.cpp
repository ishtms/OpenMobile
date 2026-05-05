#if WITH_DEV_AUTOMATION_TESTS

#include "IOpenMobileDeviceBackend.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileDeviceAccessibilityTypes.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceBlueprintLibrary.h"
#include "OpenMobileDeviceClipboardTypes.h"
#include "OpenMobileDeviceCommonTypes.h"
#include "OpenMobileDeviceDisplayTypes.h"
#include "OpenMobileDeviceIdentityTypes.h"
#include "OpenMobileDeviceLocaleTypes.h"
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

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

		virtual void BeginShutdown() override
		{
			++ShutdownCount;
		}

		int32 ShutdownCount = 0;

	private:
		FName Name;
		int32 Priority;
		bool bAvailable;
		EOpenMobileDeviceBackendDomain SupportedDomain;
	};
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
