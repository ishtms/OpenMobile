#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorCoordinates.h"
#include "OpenMobileSensorUnits.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSampleService.h"
#include "OpenMobileSensorScreenRotationService.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsAttitudeTestsPrivate
{
	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 15.0;
		Capability.MaximumFrequencyHz = 200.0;
		return Capability;
	}

	FOpenMobileAttitudeReferenceFrameCapability MakeReferenceCapability(
		EOpenMobileAttitudeReferenceFrame ReferenceFrame,
		EOpenMobileCapabilityState State,
		bool bHeadingDependent = false,
		bool bLocationDependent = false,
		bool bCalibrationRequired = false,
		bool bExpectedToDrift = false
	)
	{
		FOpenMobileAttitudeReferenceFrameCapability Capability;
		Capability.ReferenceFrame = ReferenceFrame;
		Capability.Availability.Name = TEXT("AttitudeReference");
		Capability.Availability.State = State;
		Capability.bHeadingDependent = bHeadingDependent;
		Capability.bLocationDependent = bLocationDependent;
		Capability.bCalibrationRequired = bCalibrationRequired;
		Capability.bExpectedToDrift = bExpectedToDrift;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileAttitudeReferenceFrame ReferenceFrame =
			EOpenMobileAttitudeReferenceFrame::GameRelative
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::Attitude;
		Request.Sensor.InstanceId = TEXT("Default");
		Request.Options.RatePreset = EOpenMobileSensorRatePreset::Custom;
		Request.Options.CustomFrequencyHz = 60.0;
		Request.Options.AttitudeReferenceFrame = ReferenceFrame;
		return Request;
	}

	FOpenMobileAttitudeSensorSample MakeSample(
		double TimestampSeconds,
		const FQuat& Quaternion,
		EOpenMobileSensorAccuracy Accuracy,
		EOpenMobileAttitudeReferenceFrame ReferenceFrame =
			EOpenMobileAttitudeReferenceFrame::GameRelative,
		bool bCalibrationRequired = false
	)
	{
		FOpenMobileAttitudeSensorSample Sample;
		Sample.Header.Sensor = MakeRequest().Sensor;
		Sample.Header.TimestampSeconds = TimestampSeconds;
		Sample.Header.bValid = true;
		Sample.Header.bUnitsNormalized = true;
		Sample.Header.bCoordinatesNormalized = true;
		Sample.Header.Accuracy = Accuracy;
		Sample.Header.bCalibrationRequired = bCalibrationRequired;
		Sample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::NativeFused
		);
		Sample.Quaternion = Quaternion;
		Sample.ReferenceFrame = ReferenceFrame;
		return Sample;
	}

	bool SameRotation(const FQuat& Left, const FQuat& Right)
	{
		return FMath::Abs(Left | Right) >= 1.0 - 1.e-9;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsSampleService::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeRepresentationDeliveryTest,
	"OpenMobile.Sensors.Attitude.Representations.Delivery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeRepresentationDeliveryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsScreenRotationService::ResetForTests();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeRepresentations"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Attitude)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileSensorsScreenRotationService::CaptureApplicationWindowRotation(
		Owner,
		EOpenMobileSensorScreenRotation::Rotation90,
		0.5,
		false
	);
	FOpenMobileSensorSubscriptionRequest AllRepresentations = MakeRequest();
	AllRepresentations.Options.CoordinateSpace =
		EOpenMobileSensorCoordinateSpace::CurrentScreen;
	AllRepresentations.Options.AttitudeRepresentations =
		static_cast<int32>(EOpenMobileAttitudeRepresentation::Quaternion)
		| static_cast<int32>(EOpenMobileAttitudeRepresentation::EulerAngles)
		| static_cast<int32>(
			EOpenMobileAttitudeRepresentation::RotationMatrix
		);
	FOpenMobileSensorSubscriptionRequest QuaternionOnly = MakeRequest();
	QuaternionOnly.Options.AttitudeRepresentations =
		static_cast<int32>(EOpenMobileAttitudeRepresentation::Quaternion);
	const FOpenMobileSensorSubscriptionResult AllSubscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			AllRepresentations
		);
	const FOpenMobileSensorSubscriptionResult QuaternionSubscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			QuaternionOnly
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Representation choices share a physical stream"),
		Backend.GetStartSensorStreamCount(), 1);
	FOpenMobileAttitudeSensorBatch Batch;
	Batch.Samples.Add(MakeSample(
		1.0,
		FQuat(FVector::RightVector, UE_DOUBLE_PI * 0.25),
		EOpenMobileSensorAccuracy::High
	));
	FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		Batch
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileAttitudeSensorSample AllOutput;
	FOpenMobileAttitudeSensorSample QuaternionOutput;
	FOpenMobileSensorsSampleService::ReadLatestAttitude(
		Owner,
		AllSubscription.Handle,
		0,
		1.1,
		Read,
		AllOutput
	);
	FOpenMobileSensorsSampleService::ReadLatestAttitude(
		Owner,
		QuaternionSubscription.Handle,
		0,
		1.1,
		Read,
		QuaternionOutput
	);
	TestTrue(TEXT("Requested Euler output is present"),
		AllOutput.bHasEulerDegrees);
	TestTrue(TEXT("Requested matrix output is present"),
		AllOutput.bHasRotationMatrix);
	TestTrue(TEXT("Euler output matches the delivered quaternion"),
		SameRotation(
			AllOutput.EulerDegrees.Quaternion(),
			AllOutput.Quaternion
		));
	TestEqual(TEXT("Matrix X basis matches the delivered quaternion"),
		AllOutput.RotationMatrix.XAxis,
		AllOutput.Quaternion.RotateVector(FVector::ForwardVector));
	TestEqual(TEXT("Matrix Y basis matches the delivered quaternion"),
		AllOutput.RotationMatrix.YAxis,
		AllOutput.Quaternion.RotateVector(FVector::RightVector));
	TestEqual(TEXT("Matrix Z basis matches the delivered quaternion"),
		AllOutput.RotationMatrix.ZAxis,
		AllOutput.Quaternion.RotateVector(FVector::UpVector));
	TestFalse(TEXT("Unused Euler output is not calculated"),
		QuaternionOutput.bHasEulerDegrees);
	TestFalse(TEXT("Unused matrix output is not calculated"),
		QuaternionOutput.bHasRotationMatrix);
	FinishBackend(Backend);
	FOpenMobileSensorsScreenRotationService::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeReferenceCapabilityTest,
	"OpenMobile.Sensors.Attitude.ReferenceCapabilities",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeReferenceCapabilityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeReferences"));
	FOpenMobileSensorCapability Attitude =
		MakeCapability(EOpenMobileSensorType::Attitude);
	Attitude.AttitudeReferenceFrames = {
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::GameRelative,
			EOpenMobileCapabilityState::Available,
			false,
			false,
			false,
			true
		),
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::ArbitraryVertical,
			EOpenMobileCapabilityState::Available,
			false,
			false,
			false,
			true
		),
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::MagneticNorth,
			EOpenMobileCapabilityState::Available,
			true,
			false,
			true
		),
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::TrueNorth,
			EOpenMobileCapabilityState::NotSupported,
			true,
			true
		)
	};
	Backend.SetSensorCapabilities({Attitude});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorCapabilitySnapshot Snapshot =
		FOpenMobileSensorsCapabilityService::GetSnapshot();
	const FOpenMobileSensorCapability* Reported =
		Snapshot.Sensors.FindByPredicate(
			[](const FOpenMobileSensorCapability& Capability)
			{
				return Capability.Sensor.Type ==
					EOpenMobileSensorType::Attitude;
			}
		);
	TestNotNull(TEXT("Attitude capability is reported"), Reported);
	if (Reported)
	{
		TestEqual(TEXT("Every reference frame has a capability result"),
			Reported->AttitudeReferenceFrames.Num(), 4);
		TestTrue(TEXT("True north declares its location dependency"),
			Reported->AttitudeReferenceFrames[3].bLocationDependent);
	}
	const FOpenMobileSensorSubscriptionResult Unsupported =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest(EOpenMobileAttitudeReferenceFrame::TrueNorth)
		);
	TestEqual(TEXT("An unsupported reference is rejected immediately"),
		Unsupported.Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	TestEqual(TEXT("An unsupported reference starts no native stream"),
		Backend.GetStartSensorStreamCount(), 0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeReferenceActiveStateTest,
	"OpenMobile.Sensors.Attitude.ReferenceActiveState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeReferenceActiveStateTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeReferenceState"));
	FOpenMobileSensorCapability Attitude =
		MakeCapability(EOpenMobileSensorType::Attitude);
	Attitude.AttitudeReferenceFrames = {
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::GameRelative,
			EOpenMobileCapabilityState::Available
		),
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::ArbitraryVertical,
			EOpenMobileCapabilityState::Available,
			false,
			false,
			false,
			true
		)
	};
	Backend.SetSensorCapabilities({Attitude});
	FOpenMobileAttitudeReferenceState AppliedReference;
	AppliedReference.RequestedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;
	AppliedReference.AppliedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::ArbitraryVertical;
	AppliedReference.bFallbackApplied = true;
	AppliedReference.bExpectedToDrift = true;
	Backend.SetAppliedAttitudeReferenceForTests(AppliedReference);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorSubscriptionStateSnapshot State;
	TestTrue(TEXT("Active attitude state is queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Subscription.Handle,
			State
		));
	TestEqual(TEXT("The requested reference remains visible"),
		State.AttitudeReference.RequestedReferenceFrame,
		EOpenMobileAttitudeReferenceFrame::GameRelative);
	TestEqual(TEXT("The closest native reference is visible"),
		State.AttitudeReference.AppliedReferenceFrame,
		EOpenMobileAttitudeReferenceFrame::ArbitraryVertical);
	TestTrue(TEXT("Native fallback is explicit"),
		State.AttitudeReference.bFallbackApplied);
	TestFalse(TEXT("Arbitrary vertical has no heading dependency"),
		State.AttitudeReference.bHeadingDependent);
	TestFalse(TEXT("Arbitrary vertical has no location dependency"),
		State.AttitudeReference.bLocationDependent);
	TestFalse(TEXT("Arbitrary vertical does not require calibration"),
		State.AttitudeReference.bCalibrationRequired);
	TestTrue(TEXT("Arbitrary yaw drift is explicit"),
		State.AttitudeReference.bExpectedToDrift);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeReferenceChangeTest,
	"OpenMobile.Sensors.Attitude.ReferenceChange",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeReferenceChangeTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeReferenceChange"));
	FOpenMobileSensorCapability Attitude =
		MakeCapability(EOpenMobileSensorType::Attitude);
	Attitude.AttitudeReferenceFrames = {
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::GameRelative,
			EOpenMobileCapabilityState::Available
		),
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::MagneticNorth,
			EOpenMobileCapabilityState::Available,
			true,
			false,
			true
		)
	};
	Backend.SetSensorCapabilities({Attitude});
	FOpenMobileAttitudeReferenceState GameReference;
	GameReference.RequestedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;
	GameReference.AppliedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;
	GameReference.bExpectedToDrift = true;
	Backend.SetAppliedAttitudeReferenceForTests(GameReference);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorBackendStreamHandle PreviousPhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();

	FOpenMobileAttitudeReferenceState MagneticReference;
	MagneticReference.RequestedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::MagneticNorth;
	MagneticReference.AppliedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::MagneticNorth;
	MagneticReference.bHeadingDependent = true;
	MagneticReference.bCalibrationRequired = true;
	Backend.SetAppliedAttitudeReferenceForTests(MagneticReference);
	const FOpenMobileSensorOperationResult Updated =
		FOpenMobileSensorsSubscriptionService::UpdateSubscription(
			Owner,
			Subscription.Handle,
			MakeRequest(
				EOpenMobileAttitudeReferenceFrame::MagneticNorth
			).Options
		);
	TestTrue(TEXT("An active subscription can change reference frame"),
		Updated.IsSuccess());
	TestEqual(TEXT("Changing frame starts one replacement stream"),
		Backend.GetStartSensorStreamCount(), 2);
	TestEqual(TEXT("The old physical stream is released"),
		Backend.GetStopSensorStreamCount(), 1);
	const FOpenMobileSensorBackendStreamHandle NewPhysicalHandle =
		Backend.GetLastStartedPhysicalHandle();
	TestNotEqual(TEXT("Changing frame replaces the physical stream"),
		NewPhysicalHandle.Identifier,
		PreviousPhysicalHandle.Identifier);

	FOpenMobileSensorSubscriptionStateSnapshot State;
	TestTrue(TEXT("The changed subscription remains queryable"),
		FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
			Owner,
			Subscription.Handle,
			State
		));
	TestEqual(TEXT("The changed subscription remains active"),
		State.State,
		EOpenMobileSensorSubscriptionState::Active);
	TestEqual(TEXT("Active state reports the new reference"),
		State.AttitudeReference.AppliedReferenceFrame,
		EOpenMobileAttitudeReferenceFrame::MagneticNorth);

	FOpenMobileAttitudeSensorBatch Batch;
	Batch.Samples.Add(MakeSample(
		2.0,
		FQuat::Identity,
		EOpenMobileSensorAccuracy::High,
		EOpenMobileAttitudeReferenceFrame::MagneticNorth
	));
	FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		NewPhysicalHandle,
		Batch
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileAttitudeSensorSample Output;
	TestTrue(TEXT("The new reference produces samples"),
		FOpenMobileSensorsSampleService::ReadLatestAttitude(
			Owner,
			Subscription.Handle,
			0,
			2.1,
			Read,
			Output
		));
	TestTrue(TEXT("The first sample after a frame change resets state"),
		Output.Header.bStatefulProcessingReset);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeReferenceRestartTest,
	"OpenMobile.Sensors.Attitude.ReferenceRestart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeReferenceRestartTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeReferenceRestart"));
	FOpenMobileSensorCapability Attitude =
		MakeCapability(EOpenMobileSensorType::Attitude);
	Attitude.AttitudeReferenceFrames = {
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::GameRelative,
			EOpenMobileCapabilityState::Available
		),
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::MagneticNorth,
			EOpenMobileCapabilityState::Available,
			true,
			false,
			true
		)
	};
	Backend.SetSensorCapabilities({Attitude});
	FOpenMobileAttitudeReferenceState GameReference;
	GameReference.RequestedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;
	GameReference.AppliedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::GameRelative;
	Backend.SetAppliedAttitudeReferenceForTests(GameReference);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();

	Backend.SetStartSensorStreamResult(
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::UnsupportedOperation
		)
	);
	FOpenMobileAttitudeReferenceState MagneticReference;
	MagneticReference.RequestedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::MagneticNorth;
	MagneticReference.AppliedReferenceFrame =
		EOpenMobileAttitudeReferenceFrame::MagneticNorth;
	MagneticReference.bHeadingDependent = true;
	MagneticReference.bCalibrationRequired = true;
	Backend.SetAppliedAttitudeReferenceForTests(MagneticReference);
	const FOpenMobileSensorOperationResult Updated =
		FOpenMobileSensorsSubscriptionService::UpdateSubscription(
			Owner,
			Subscription.Handle,
			MakeRequest(
				EOpenMobileAttitudeReferenceFrame::MagneticNorth
			).Options
		);
	TestTrue(TEXT("A single-stream backend can restart in place"),
		Updated.IsSuccess());
	TestEqual(TEXT("The replacement start is attempted first"),
		Backend.GetStartSensorStreamCount(), 2);
	TestEqual(TEXT("The existing physical stream is reconfigured once"),
		Backend.GetReconfigureSensorStreamCount(), 1);
	TestEqual(TEXT("An in-place restart does not stop its handle"),
		Backend.GetStopSensorStreamCount(), 0);
	TestEqual(TEXT("The in-place restart keeps one physical stream"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		1);
	FOpenMobileSensorSubscriptionStateSnapshot State;
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		Owner,
		Subscription.Handle,
		State
	);
	TestEqual(TEXT("The restarted stream reports its new frame"),
		State.AttitudeReference.AppliedReferenceFrame,
		EOpenMobileAttitudeReferenceFrame::MagneticNorth);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeReferenceFailureTest,
	"OpenMobile.Sensors.Attitude.ReferenceFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeReferenceFailureTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeReferenceFailure"));
	FOpenMobileSensorCapability Attitude =
		MakeCapability(EOpenMobileSensorType::Attitude);
	Attitude.AttitudeReferenceFrames = {
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::GameRelative,
			EOpenMobileCapabilityState::Available
		),
		MakeReferenceCapability(
			EOpenMobileAttitudeReferenceFrame::MagneticNorth,
			EOpenMobileCapabilityState::Available,
			true,
			false,
			true
		)
	};
	Backend.SetSensorCapabilities({Attitude});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	const FOpenMobileSensorOperationResult Failure =
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::OperationalFailure
		);
	Backend.SetStartSensorStreamResult(Failure);
	Backend.SetReconfigureSensorStreamResult(Failure);
	const FOpenMobileSensorOperationResult Updated =
		FOpenMobileSensorsSubscriptionService::UpdateSubscription(
			Owner,
			Subscription.Handle,
			MakeRequest(
				EOpenMobileAttitudeReferenceFrame::MagneticNorth
			).Options
		);
	TestFalse(TEXT("A failed frame change is reported"),
		Updated.IsSuccess());
	FOpenMobileSensorSubscriptionStateSnapshot State;
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		Owner,
		Subscription.Handle,
		State
	);
	TestEqual(TEXT("A failed change preserves the requested frame"),
		State.RequestedOptions.AttitudeReferenceFrame,
		EOpenMobileAttitudeReferenceFrame::GameRelative);
	TestEqual(TEXT("A failed change preserves the applied frame"),
		State.AttitudeReference.AppliedReferenceFrame,
		EOpenMobileAttitudeReferenceFrame::GameRelative);
	TestEqual(TEXT("A failed change preserves the physical stream"),
		FOpenMobileSensorsSubscriptionService::
			GetPhysicalStreamCountForTests(),
		1);
	TestEqual(TEXT("A failed change does not stop the old stream"),
		Backend.GetStopSensorStreamCount(), 0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeReferencePermissionLossTest,
	"OpenMobile.Sensors.Attitude.ReferencePermissionLoss",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeReferencePermissionLossTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeReferencePermission"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Attitude)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(EOpenMobileAttitudeReferenceFrame::MagneticNorth)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestTrue(TEXT("Permission loss fails the active physical stream"),
		FOpenMobileSensorsSubscriptionService::FailPhysicalStreamFromBackend(
			FOpenMobileSensorsBackendRegistry::CaptureToken(),
			Backend.GetLastStartedPhysicalHandle(),
			FOpenMobileSensorsErrorMapper::Map(
				EOpenMobileSensorFailureReason::PermissionDenied
			)
		));
	FOpenMobileSensorSubscriptionStateSnapshot FailedState;
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		Owner,
		Subscription.Handle,
		FailedState
	);
	TestEqual(TEXT("Permission loss is visible on the subscription"),
		FailedState.State,
		EOpenMobileSensorSubscriptionState::Failed);
	const FOpenMobileSensorSubscriptionResult Restarted =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(EOpenMobileAttitudeReferenceFrame::MagneticNorth)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileSensorSubscriptionStateSnapshot RestartedState;
	FOpenMobileSensorsSubscriptionService::GetSubscriptionState(
		Owner,
		Restarted.Handle,
		RestartedState
	);
	TestEqual(TEXT("A fresh subscription restarts after permission recovery"),
		RestartedState.State,
		EOpenMobileSensorSubscriptionState::Active);
	TestEqual(TEXT("Restart uses a fresh physical stream"),
		Backend.GetStartSensorStreamCount(), 2);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeNativeSelectionTest,
	"OpenMobile.Sensors.Attitude.NativeSelection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeNativeSelectionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend NativeBackend(TEXT("NativeAttitude"));
	NativeBackend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::Gyroscope),
		MakeCapability(EOpenMobileSensorType::Magnetometer),
		MakeCapability(EOpenMobileSensorType::Attitude)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(NativeBackend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Native =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	TestEqual(TEXT("Attitude uses the native fused stream"),
		NativeBackend.GetLastStartedPhysicalRequest().Sensor.Type,
		EOpenMobileSensorType::Attitude);
	FOpenMobileSensorsSubscriptionService::StopSubscription(
		Owner,
		Native.Handle
	);
	FinishBackend(NativeBackend);

	FOpenMobileSensorsMockBackend RawOnlyBackend(TEXT("RawOnlyAttitude"));
	RawOnlyBackend.SetSensorCapabilities({
		MakeCapability(EOpenMobileSensorType::Accelerometer),
		MakeCapability(EOpenMobileSensorType::Gyroscope),
		MakeCapability(EOpenMobileSensorType::Magnetometer)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(RawOnlyBackend);
	const FOpenMobileSensorSubscriptionResult UnsupportedFallback =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest()
		);
	TestEqual(TEXT("Undocumented plugin fusion is not selected"),
		UnsupportedFallback.Operation.Code,
		EOpenMobileSensorResultCode::Unavailable);
	TestEqual(TEXT("Raw inputs do not start hidden fusion streams"),
		RawOnlyBackend.GetStartSensorStreamCount(), 0);
	FinishBackend(RawOnlyBackend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeNormalizationQualityTest,
	"OpenMobile.Sensors.Attitude.NormalizationQuality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeNormalizationQualityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	FOpenMobileAttitudeSensorSample Normalized = MakeSample(
		1.0,
		FQuat(0.0, 0.0, 0.0, 2.0),
		EOpenMobileSensorAccuracy::High
	);
	Normalized.Header.bUnitsNormalized = false;
	TestTrue(TEXT("A finite native quaternion normalizes"),
		FOpenMobileSensorUnitConverter::NormalizeAttitudeSample(
			EOpenMobileSensorNativePlatform::Android,
			Normalized
		));
	TestTrue(TEXT("The canonical quaternion has unit length"),
		FMath::IsNearlyEqual(Normalized.Quaternion.SizeSquared(), 1.0));
	const FQuat QuarterTurn(FVector::UpVector, UE_DOUBLE_PI * 0.5);
	const FQuat Negated(
		-QuarterTurn.X,
		-QuarterTurn.Y,
		-QuarterTurn.Z,
		-QuarterTurn.W
	);
	TestTrue(TEXT("Quaternion sign does not change orientation"),
		SameRotation(QuarterTurn, Negated));
	FQuat LastRotation = FQuat::Identity;
	bool bContinuousNormalized = true;
	bool bContinuousWithoutJumps = true;
	for (int32 Degrees = 1; Degrees <= 360; ++Degrees)
	{
		FOpenMobileAttitudeSensorSample Continuous = MakeSample(
			Degrees / 60.0,
			FQuat(
				FVector::UpVector,
				FMath::DegreesToRadians(static_cast<double>(Degrees))
			),
			EOpenMobileSensorAccuracy::High
		);
		Continuous.Header.bUnitsNormalized = false;
		FOpenMobileSensorUnitConverter::NormalizeAttitudeSample(
			EOpenMobileSensorNativePlatform::Android,
			Continuous
		);
		bContinuousNormalized &= FMath::IsNearlyEqual(
				Continuous.Quaternion.SizeSquared(),
				1.0
			);
		bContinuousWithoutJumps &=
			FMath::Abs(LastRotation | Continuous.Quaternion) > 0.99;
		LastRotation = Continuous.Quaternion;
	}
	TestTrue(TEXT("Continuous rotations remain normalized"),
		bContinuousNormalized);
	TestTrue(TEXT("Continuous rotations do not jump orientation"),
		bContinuousWithoutJumps);
	TestTrue(TEXT("A full turn returns to the identity orientation"),
		SameRotation(LastRotation, FQuat::Identity));

	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeQuality"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Attitude)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest(EOpenMobileAttitudeReferenceFrame::MagneticNorth)
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileAttitudeSensorBatch Batch;
	Batch.Samples.Add(MakeSample(
		1.0,
		QuarterTurn,
		EOpenMobileSensorAccuracy::Low,
		EOpenMobileAttitudeReferenceFrame::MagneticNorth,
		true
	));
	FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		Batch
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileAttitudeSensorSample Output;
	TestTrue(TEXT("A calibrated native sample remains observable"),
		FOpenMobileSensorsSampleService::ReadLatestAttitude(
			Owner,
			Subscription.Handle,
			0,
			1.1,
			Read,
			Output
		));
	TestTrue(TEXT("The delivered quaternion keeps its rotation"),
		SameRotation(Output.Quaternion, QuarterTurn));
	TestEqual(TEXT("Native fusion provenance remains explicit"),
		Output.Header.SourceFlags,
		static_cast<int32>(EOpenMobileSensorSourceFlags::NativeFused));
	TestEqual(TEXT("Magnetic calibration trouble degrades attitude"),
		Output.Header.Fusion.Quality,
		EOpenMobileSensorFusionQuality::Degraded);
	TestTrue(TEXT("Native quality remains distinguishable"),
		Output.Header.Fusion.bHasNativeQualityReport);
	TestEqual(TEXT("No plugin-derived drift is introduced"),
		Output.Header.SourceFlags
			& static_cast<int32>(EOpenMobileSensorSourceFlags::PluginDerived),
		0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAttitudeReferenceMismatchTest,
	"OpenMobile.Sensors.Attitude.ReferenceMismatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAttitudeReferenceMismatchTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsAttitudeTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("AttitudeFrame"));
	Backend.SetSensorCapabilities(
		{MakeCapability(EOpenMobileSensorType::Attitude)}
	);
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	const FOpenMobileSensorSubscriptionResult Subscription =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			Owner,
			MakeRequest()
		);
	FOpenMobileSensorsSubscriptionService::
		ProcessPendingBackendOperationsForTests();
	FOpenMobileAttitudeSensorBatch WrongFrame;
	WrongFrame.Samples.Add(MakeSample(
		1.0,
		FQuat::Identity,
		EOpenMobileSensorAccuracy::High,
		EOpenMobileAttitudeReferenceFrame::MagneticNorth
	));
	FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		WrongFrame
	);
	FOpenMobileSensorReadResult Read;
	FOpenMobileAttitudeSensorSample Output;
	TestFalse(TEXT("A mismatched reference frame is not delivered"),
		FOpenMobileSensorsSampleService::ReadLatestAttitude(
			Owner,
			Subscription.Handle,
			0,
			1.1,
			Read,
			Output
		));
	FOpenMobileAttitudeSensorBatch MatchingFrame;
	MatchingFrame.Samples.Add(MakeSample(
		2.0,
		FQuat::Identity,
		EOpenMobileSensorAccuracy::High
	));
	FOpenMobileSensorsSampleService::PublishAttitudeBatchFromBackend(
		FOpenMobileSensorsBackendRegistry::CaptureToken(),
		Backend.GetLastStartedPhysicalHandle(),
		MatchingFrame
	);
	TestTrue(TEXT("A matching reference frame is delivered"),
		FOpenMobileSensorsSampleService::ReadLatestAttitude(
			Owner,
			Subscription.Handle,
			0,
			2.1,
			Read,
			Output
		));
	TestTrue(TEXT("Rejected frame data resets the next sample"),
		Output.Header.bStatefulProcessingReset);
	FinishBackend(Backend);
	return true;
}

#endif
