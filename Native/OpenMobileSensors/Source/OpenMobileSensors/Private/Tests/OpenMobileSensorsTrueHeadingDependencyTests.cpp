#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "HAL/PlatformTime.h"
#include "IOpenMobilePermissionProvider.h"
#include "Misc/AutomationTest.h"
#include "Misc/DateTime.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsPermissionPolicy.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"
#include "OpenMobileSensorsTrueHeadingService.h"

namespace OpenMobileSensorsTrueHeadingDependencyTestsPrivate
{
	class FLocationPermissionProvider final
		: public IOpenMobilePermissionProvider
	{
	public:
		virtual FName GetProviderName() const override
		{
			return TEXT("LocationDependencyTestProvider");
		}

		virtual bool SupportsPermission(FName Permission) const override
		{
			return Permission == LocationPermission;
		}

		virtual FOpenMobilePermissionResult GetStatus(
			FName Permission
		) const override
		{
			++StatusQueryCount;
			FOpenMobilePermissionResult Result;
			Result.Permission = Permission;
			Result.Status = Status;
			return Result;
		}

		virtual bool RequestPermission(
			FName Permission,
			const FGuid& RequestIdentifier,
			FOpenMobileNativePermissionCompletion&& Completion,
			FOpenMobileError& OutError
		) override
		{
			static_cast<void>(Permission);
			static_cast<void>(RequestIdentifier);
			static_cast<void>(Completion);
			OutError = FOpenMobileError::Make(
				EOpenMobileErrorCode::NotSupported,
				TEXT("The owning location provider controls its own prompt.")
			);
			return false;
		}

		virtual void CancelRequest(const FGuid& RequestIdentifier) override
		{
			static_cast<void>(RequestIdentifier);
		}

		virtual void BeginShutdown() override
		{
		}

		FName LocationPermission =
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::TrueHeadingLocation
			);
		EOpenMobilePermissionStatus Status =
			EOpenMobilePermissionStatus::NotDetermined;
		mutable int32 StatusQueryCount = 0;
	};

	FOpenMobileSensorCapability MakeMagneticCapability()
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::MagneticHeading;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(
				EOpenMobileSensorType::MagneticHeading
			);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 100.0;
		return Capability;
	}

	const FOpenMobileSensorCapability* FindCapability(
		const FOpenMobileSensorCapabilitySnapshot& Snapshot,
		EOpenMobileSensorType Type
	)
	{
		return Snapshot.Sensors.FindByPredicate(
			[Type](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type == Type;
			}
		);
	}

	const FOpenMobileSensorPrerequisiteCapability* FindLocationPrerequisite(
		const FOpenMobileSensorCapability& Capability
	)
	{
		return Capability.Prerequisites.FindByPredicate(
			[](const FOpenMobileSensorPrerequisiteCapability& Prerequisite)
			{
				return Prerequisite.Name ==
					FOpenMobileSensorsPermissionPolicy::TrueHeadingLocationInput();
			}
		);
	}

	void ResetServices()
	{
		FOpenMobilePermissionProviderRegistry::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsTrueHeadingService::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTrueHeadingLocationRetentionTest,
	"OpenMobile.Sensors.Heading.True.LocationRetention",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTrueHeadingLocationRetentionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsTrueHeadingDependencyTestsPrivate;
	ResetServices();
	const FGuid Owner = FGuid::NewGuid();
	constexpr double CurrentUnixSeconds = 1735689600.0;
	constexpr double CurrentMonotonicSeconds = 100.0;
	TestEqual(
		TEXT("An owner starts without location input"),
		FOpenMobileSensorsTrueHeadingService::GetLocationInputState(
			Owner,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::MissingLocationInput
	);

	FOpenMobileSensorLocationInput Location;
	Location.LatitudeDegrees = 51.5;
	Location.LongitudeDegrees = -0.1;
	Location.AltitudeMeters = 20.0;
	Location.HorizontalAccuracyMeters = 101.0;
	Location.TimestampSeconds = CurrentUnixSeconds;
	TestEqual(
		TEXT("Poor location accuracy is explicit"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			Location,
			CurrentUnixSeconds,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::PoorLocationAccuracy
	);
	TestFalse(
		TEXT("Rejected coordinates are not retained"),
		FOpenMobileSensorsTrueHeadingService::
			HasRetainedLocationInputForTests(Owner)
	);

	Location.HorizontalAccuracyMeters = 10.0;
	TestEqual(
		TEXT("Accurate location input is accepted"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			Location,
			CurrentUnixSeconds,
			CurrentMonotonicSeconds
		),
		EOpenMobileSensorFailureReason::None
	);
	TestTrue(
		TEXT("Usable coordinates are retained while fresh"),
		FOpenMobileSensorsTrueHeadingService::
			HasRetainedLocationInputForTests(Owner)
	);
	TestEqual(
		TEXT("Expired location input is reported as stale"),
		FOpenMobileSensorsTrueHeadingService::GetLocationInputState(
			Owner,
			CurrentMonotonicSeconds + 61.0
		),
		EOpenMobileSensorFailureReason::StaleLocationInput
	);
	TestFalse(
		TEXT("Expiry scrubs retained coordinates"),
		FOpenMobileSensorsTrueHeadingService::
			HasRetainedLocationInputForTests(Owner)
	);

	Location.TimestampSeconds += 61.0;
	TestEqual(
		TEXT("Fresh input recovers after expiry"),
		FOpenMobileSensorsTrueHeadingService::SetLocationInput(
			Owner,
			Location,
			CurrentUnixSeconds + 61.0,
			CurrentMonotonicSeconds + 61.0
		),
		EOpenMobileSensorFailureReason::None
	);
	TestTrue(
		TEXT("Explicit clearing removes retained location"),
		FOpenMobileSensorsTrueHeadingService::ClearLocationInput(Owner)
	);
	TestEqual(
		TEXT("Cleared input returns to missing"),
		FOpenMobileSensorsTrueHeadingService::GetLocationInputState(
			Owner,
			CurrentMonotonicSeconds + 61.0
		),
		EOpenMobileSensorFailureReason::MissingLocationInput
	);
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsTrueHeadingPrerequisiteCapabilityTest,
	"OpenMobile.Sensors.Heading.True.PrerequisiteTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsTrueHeadingPrerequisiteCapabilityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsTrueHeadingDependencyTestsPrivate;
	ResetServices();
	FLocationPermissionProvider PermissionProvider;
	TestTrue(
		TEXT("The location permission provider registers"),
		FOpenMobilePermissionProviderRegistry::RegisterProvider(
			PermissionProvider
		)
	);
	FOpenMobileSensorsMockBackend Backend(TEXT("TrueHeadingDependencies"));
	Backend.SetSensorCapabilities({MakeMagneticCapability()});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);

	FOpenMobileSensorCapabilitySnapshot Snapshot =
		Subsystem->GetCapabilitySnapshotNative();
	const FOpenMobileSensorCapability* TrueHeading = FindCapability(
		Snapshot,
		EOpenMobileSensorType::TrueHeading
	);
	const FOpenMobileSensorCapability* Magnetic = FindCapability(
		Snapshot,
		EOpenMobileSensorType::MagneticHeading
	);
	TestNotNull(TEXT("True heading capability is present"), TrueHeading);
	TestNotNull(TEXT("Magnetic heading capability is present"), Magnetic);
	if (TrueHeading && Magnetic)
	{
		const FOpenMobileSensorPrerequisiteCapability* Prerequisite =
			FindLocationPrerequisite(*TrueHeading);
		TestNotNull(TEXT("Location prerequisite is separate"), Prerequisite);
		if (Prerequisite)
		{
			TestEqual(
				TEXT("The prerequisite names its owning permission"),
				Prerequisite->RequiredPermission,
				PermissionProvider.LocationPermission
			);
			TestTrue(
				TEXT("External authorization status is known"),
				Prerequisite->bPermissionStatusKnown
			);
			TestEqual(
				TEXT("The normalized status is exposed"),
				Prerequisite->PermissionStatus,
				EOpenMobilePermissionStatus::NotDetermined
			);
			TestEqual(
				TEXT("The maximum location age is exposed"),
				Prerequisite->MaximumAgeSeconds,
				60.0
			);
			TestEqual(
				TEXT("The maximum location accuracy is exposed"),
				Prerequisite->MaximumHorizontalAccuracyMeters,
				100.0
			);
			TestEqual(
				TEXT("Missing input is explicit"),
				Prerequisite->InputFailureReason,
				EOpenMobileSensorFailureReason::MissingLocationInput
			);
			TestEqual(
				TEXT("Undecided authorization is explicit"),
				Prerequisite->PermissionFailureReason,
				EOpenMobileSensorFailureReason::PermissionRequired
			);
			TestFalse(TEXT("Blocked prerequisite is not ready"), Prerequisite->bReady);
		}
		TestTrue(
			TEXT("Magnetic fallback hardware remains separately available"),
			TrueHeading->Fallback.bRequiredInputsAvailable
		);
		TestFalse(
			TEXT("The complete true-heading fallback is not ready"),
			TrueHeading->Fallback.bAvailable
		);
		TestEqual(
			TEXT("Magnetic heading remains independent"),
			Magnetic->Availability.State,
			EOpenMobileCapabilityState::Available
		);
	}
	TestTrue(
		TEXT("Sensors queries authorization without requesting it"),
		PermissionProvider.StatusQueryCount > 0
	);

	PermissionProvider.Status = EOpenMobilePermissionStatus::Denied;
	Snapshot = Subsystem->GetCapabilitySnapshotNative();
	TrueHeading = FindCapability(Snapshot, EOpenMobileSensorType::TrueHeading);
	if (TrueHeading)
	{
		const FOpenMobileSensorPrerequisiteCapability* Prerequisite =
			FindLocationPrerequisite(*TrueHeading);
		TestEqual(
			TEXT("Denied authorization blocks true heading"),
			TrueHeading->Availability.State,
			EOpenMobileCapabilityState::Denied
		);
		if (Prerequisite)
		{
			TestEqual(
				TEXT("Denied authorization is typed"),
				Prerequisite->PermissionFailureReason,
				EOpenMobileSensorFailureReason::PermissionDenied
			);
		}
	}

	PermissionProvider.Status = EOpenMobilePermissionStatus::Granted;
	FOpenMobileSensorLocationInput PoorLocation;
	PoorLocation.HorizontalAccuracyMeters = 101.0;
	PoorLocation.TimestampSeconds = static_cast<double>(
		FDateTime::UtcNow().ToUnixTimestamp()
	);
	TestEqual(
		TEXT("The subsystem reports poor location accuracy"),
		Subsystem->SetTrueHeadingLocationInputNative(PoorLocation).Failure.Reason,
		EOpenMobileSensorFailureReason::PoorLocationAccuracy
	);
	Snapshot = Subsystem->GetCapabilitySnapshotNative();
	TrueHeading = FindCapability(Snapshot, EOpenMobileSensorType::TrueHeading);
	if (TrueHeading)
	{
		const FOpenMobileSensorPrerequisiteCapability* Prerequisite =
			FindLocationPrerequisite(*TrueHeading);
		if (Prerequisite)
		{
			TestEqual(
				TEXT("Poor accuracy remains visible in capabilities"),
				Prerequisite->InputFailureReason,
				EOpenMobileSensorFailureReason::PoorLocationAccuracy
			);
		}
	}

	FOpenMobileSensorLocationInput Location = PoorLocation;
	Location.HorizontalAccuracyMeters = 10.0;
	TestTrue(
		TEXT("A fresh accurate location is accepted"),
		Subsystem->SetTrueHeadingLocationInputNative(Location).IsSuccess()
	);
	Snapshot = Subsystem->GetCapabilitySnapshotNative();
	TrueHeading = FindCapability(Snapshot, EOpenMobileSensorType::TrueHeading);
	if (TrueHeading)
	{
		const FOpenMobileSensorPrerequisiteCapability* Prerequisite =
			FindLocationPrerequisite(*TrueHeading);
		TestEqual(
			TEXT("True heading becomes available"),
			TrueHeading->Availability.State,
			EOpenMobileCapabilityState::Available
		);
		if (Prerequisite)
		{
			TestEqual(
				TEXT("Granted authorization is normalized"),
				Prerequisite->PermissionStatus,
				EOpenMobilePermissionStatus::Granted
			);
			TestEqual(
				TEXT("Location input has no remaining failure"),
				Prerequisite->InputFailureReason,
				EOpenMobileSensorFailureReason::None
			);
			TestEqual(
				TEXT("Authorization has no remaining failure"),
				Prerequisite->PermissionFailureReason,
				EOpenMobileSensorFailureReason::None
			);
			TestTrue(TEXT("The location prerequisite is ready"), Prerequisite->bReady);
		}
		TestTrue(
			TEXT("Complete true-heading fallback becomes ready"),
			TrueHeading->Fallback.bAvailable
		);
	}

	PermissionProvider.Status = EOpenMobilePermissionStatus::Denied;
	Snapshot = Subsystem->GetCapabilitySnapshotNative();
	TestFalse(
		TEXT("Authorization loss scrubs retained coordinates"),
		FOpenMobileSensorsTrueHeadingService::HasAnyLocationInput(
			FPlatformTime::Seconds()
		)
	);
	Magnetic = FindCapability(Snapshot, EOpenMobileSensorType::MagneticHeading);
	TestEqual(
		TEXT("Location denial does not block magnetic heading"),
		Magnetic ? Magnetic->Availability.State :
			EOpenMobileCapabilityState::NotSupported,
		EOpenMobileCapabilityState::Available
	);
	FOpenMobileSensorSubscriptionRequest MagneticRequest;
	MagneticRequest.Sensor.Type = EOpenMobileSensorType::MagneticHeading;
	MagneticRequest.Sensor.InstanceId = TEXT("Default");
	const FOpenMobileSensorSubscriptionResult MagneticSubscription =
		Subsystem->StartSubscriptionNative(MagneticRequest);
	TestEqual(
		TEXT("Magnetic heading still accepts a subscription"),
		MagneticSubscription.Operation.Code,
		EOpenMobileSensorResultCode::Accepted
	);
	Subsystem->StopSubscriptionNative(MagneticSubscription.Handle);

	TestTrue(
		TEXT("The subsystem clears caller-owned location"),
		Subsystem->ClearTrueHeadingLocationInputNative().IsSuccess()
	);
	Subsystem->Deinitialize();
	FOpenMobilePermissionProviderRegistry::UnregisterProvider(
		PermissionProvider
	);
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobilePermissionProviderRegistry::ResetForTests();
	FOpenMobileSensorsTrueHeadingService::ResetForTests();
	return true;
}

#endif
