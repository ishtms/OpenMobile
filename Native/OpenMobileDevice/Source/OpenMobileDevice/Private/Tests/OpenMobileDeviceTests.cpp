#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "IOpenMobileDeviceBackend.h"
#include "Misc/AutomationTest.h"
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
#include "OpenMobileDeviceNetworkTypes.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "OpenMobileDeviceSnapshotService.h"
#include "OpenMobileDeviceSubsystem.h"
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
		bool bInBackground = false;
		FOpenMobileDeviceInformationSnapshot DeviceInformation;
		FOpenMobilePowerSnapshot Power;

	private:
		FName Name;
		int32 Priority;
		bool bAvailable;
		EOpenMobileDeviceBackendDomain SupportedDomain;
		TMap<FName, FOpenMobileDeviceCapability> Capabilities;
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
