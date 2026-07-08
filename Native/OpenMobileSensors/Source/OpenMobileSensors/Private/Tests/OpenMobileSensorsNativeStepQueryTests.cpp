#if WITH_DEV_AUTOMATION_TESTS

#include "IOpenMobileSensorsBackend.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileNativeStepQueryService.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"

namespace OpenMobileSensorsNativeStepQueryTestsPrivate
{
	class FQueryBackend final : public IOpenMobileSensorsBackend
	{
	public:
		virtual FName GetBackendName() const override
		{
			return TEXT("NativeStepQueryTest");
		}

		virtual TArray<FOpenMobileSensorCapability>
		GetSensorCapabilities() const override
		{
			FOpenMobileSensorCapability Capability;
			Capability.Sensor.Type = EOpenMobileSensorType::StepCounter;
			Capability.Sensor.InstanceId = TEXT("Default");
			Capability.Availability.Name =
				FOpenMobileSensorTypes::GetStableName(
					EOpenMobileSensorType::StepCounter
				);
			Capability.Availability.State =
				StepCapabilityState;
			Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
			Capability.ActiveRestriction = StepRestriction;
			Capability.RequiredPermission =
				FOpenMobileSensorPermissions::GetPermissionName(
					EOpenMobileSensorPermission::MotionActivity
				);
			return {Capability};
		}

		virtual FOpenMobileSensorOperationResult QueryNativeStepCount(
			const FGuid& RequestId,
			const FOpenMobileNativeStepCountQuery& Query,
			FOnOpenMobileNativeStepCountBackendQueryComplete&& Completion
		) override
		{
			++QueryCalls;
			Queries.Add(RequestId, Query);
			Completions.Add(RequestId, MoveTemp(Completion));
			return {EOpenMobileSensorResultCode::Accepted};
		}

		virtual bool CancelNativeStepCountQuery(
			const FGuid& RequestId
		) override
		{
			if (!Completions.Remove(RequestId))
			{
				return false;
			}
			Queries.Remove(RequestId);
			++CancelCalls;
			return true;
		}

		bool Complete(const FGuid& RequestId, int64 Count)
		{
			FOnOpenMobileNativeStepCountBackendQueryComplete Completion;
			if (!Completions.RemoveAndCopyValue(RequestId, Completion))
			{
				return false;
			}
			const FOpenMobileNativeStepCountQuery Query =
				Queries.FindAndRemoveChecked(RequestId);
			FOpenMobileStepsSensorSample Sample;
			Sample.Header.Sensor.Type = EOpenMobileSensorType::StepCounter;
			Sample.Header.Sensor.InstanceId = TEXT("Default");
			Sample.Header.TimestampSeconds = 50.0;
			Sample.Header.bValid = true;
			Sample.Count = Count;
			Sample.Origin = EOpenMobileStepCountOrigin::QueryInterval;
			Sample.OriginIdentifier = RequestId;
			Sample.bHasQueryInterval = true;
			Sample.QueryStartUnixTimeSeconds = Query.StartUnixTimeSeconds;
			Sample.QueryEndUnixTimeSeconds = Query.EndUnixTimeSeconds;
			FOpenMobileSensorOperationResult Operation;
			Operation.Code = EOpenMobileSensorResultCode::Success;
			Completion.ExecuteIfBound(Operation, Sample);
			return true;
		}

		int32 QueryCalls = 0;
		int32 CancelCalls = 0;
		EOpenMobileCapabilityState StepCapabilityState =
			EOpenMobileCapabilityState::Available;
		EOpenMobileSensorRestriction StepRestriction =
			EOpenMobileSensorRestriction::None;

	private:
		TMap<FGuid, FOpenMobileNativeStepCountQuery> Queries;
		TMap<FGuid, FOnOpenMobileNativeStepCountBackendQueryComplete> Completions;
	};

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileNativeStepQueryService::ResetForTests();
	}

	void FinishBackend(FQueryBackend& Backend)
	{
		FOpenMobileNativeStepQueryService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}

	FOpenMobileNativeStepCountQuery MakeQuery()
	{
		FOpenMobileNativeStepCountQuery Query;
		Query.StartUnixTimeSeconds = 1000.0;
		Query.EndUnixTimeSeconds = 1100.0;
		return Query;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNativeStepHistoricalQueryTest,
	"OpenMobile.Sensors.Steps.Native.HistoricalQuery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNativeStepHistoricalQueryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsNativeStepQueryTestsPrivate;
	ResetServices();
	FQueryBackend Backend;
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity),
		EOpenMobilePermissionStatus::Granted);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileNativeStepCountQueryResult Received;
	int32 Completions = 0;
	const FGuid RequestId = FOpenMobileNativeStepQueryService::Query(
		Owner,
		MakeQuery(),
		[&Received, &Completions](
			const FOpenMobileNativeStepCountQueryResult& Result)
		{
			Received = Result;
			++Completions;
		}
	);
	TestTrue(TEXT("A historical query receives a request id"),
		RequestId.IsValid());
	TestEqual(TEXT("The backend receives one query"), Backend.QueryCalls, 1);
	TestEqual(TEXT("An accepted query has not completed yet"), Completions, 0);
	TestTrue(TEXT("The backend can complete the query"),
		Backend.Complete(RequestId, 5000000000LL));
	TestEqual(TEXT("The query completes exactly once"), Completions, 1);
	TestEqual(TEXT("The request id is preserved"),
		Received.RequestId, RequestId);
	TestEqual(TEXT("A wide historical count is preserved"),
		Received.Sample.Count, 5000000000LL);
	TestEqual(TEXT("The query start is preserved"),
		Received.Sample.QueryStartUnixTimeSeconds, 1000.0);
	TestEqual(TEXT("The query end is preserved"),
		Received.Sample.QueryEndUnixTimeSeconds, 1100.0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNativeStepQueryCancellationTest,
	"OpenMobile.Sensors.Steps.Native.HistoricalQueryCancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNativeStepQueryCancellationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsNativeStepQueryTestsPrivate;
	ResetServices();
	FQueryBackend Backend;
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity),
		EOpenMobilePermissionStatus::Granted);
	const FGuid Owner = FGuid::NewGuid();
	int32 Completions = 0;
	FOpenMobileNativeStepCountQueryResult Received;
	const FGuid RequestId = FOpenMobileNativeStepQueryService::Query(
		Owner,
		MakeQuery(),
		[&Received, &Completions](
			const FOpenMobileNativeStepCountQueryResult& Result)
		{
			Received = Result;
			++Completions;
		}
	);
	TestFalse(TEXT("Another owner cannot cancel the query"),
		FOpenMobileNativeStepQueryService::Cancel(
			FGuid::NewGuid(), RequestId));
	TestTrue(TEXT("The owner can cancel the query"),
		FOpenMobileNativeStepQueryService::Cancel(Owner, RequestId));
	TestEqual(TEXT("Cancellation reaches the backend"),
		Backend.CancelCalls, 1);
	TestEqual(TEXT("Cancellation completes exactly once"), Completions, 1);
	TestEqual(TEXT("Cancellation is typed"),
		Received.Operation.Code, EOpenMobileSensorResultCode::Cancelled);
	TestFalse(TEXT("A cancelled native callback is discarded"),
		Backend.Complete(RequestId, 10));
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNativeStepQueryValidationTest,
	"OpenMobile.Sensors.Steps.Native.HistoricalQueryValidation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNativeStepQueryValidationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsNativeStepQueryTestsPrivate;
	ResetServices();
	FQueryBackend Backend;
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FGuid Owner = FGuid::NewGuid();
	FOpenMobileNativeStepCountQuery Invalid = MakeQuery();
	Invalid.EndUnixTimeSeconds = Invalid.StartUnixTimeSeconds;
	FOpenMobileNativeStepCountQueryResult InvalidResult;
	int32 InvalidCompletions = 0;
	FOpenMobileNativeStepQueryService::Query(
		Owner,
		Invalid,
		[&InvalidResult, &InvalidCompletions](
			const FOpenMobileNativeStepCountQueryResult& Result)
		{
			InvalidResult = Result;
			++InvalidCompletions;
		}
	);
	TestEqual(TEXT("An invalid range completes once"), InvalidCompletions, 1);
	TestEqual(TEXT("An invalid range is explicit"),
		InvalidResult.Operation.Code,
		EOpenMobileSensorResultCode::InvalidArgument);
	TestEqual(TEXT("An invalid range does not reach the backend"),
		Backend.QueryCalls, 0);

	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity),
		EOpenMobilePermissionStatus::NotDetermined);
	FOpenMobileNativeStepCountQueryResult RequiredResult;
	int32 RequiredCompletions = 0;
	FOpenMobileNativeStepQueryService::Query(
		Owner,
		MakeQuery(),
		[&RequiredResult, &RequiredCompletions](
			const FOpenMobileNativeStepCountQueryResult& Result)
		{
			RequiredResult = Result;
			++RequiredCompletions;
		}
	);
	TestEqual(TEXT("An undecided query completes once"),
		RequiredCompletions, 1);
	TestEqual(TEXT("Undecided motion access requires permission"),
		RequiredResult.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::PermissionRequired);

	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity),
		EOpenMobilePermissionStatus::Denied);
	FOpenMobileNativeStepCountQueryResult DeniedResult;
	int32 DeniedCompletions = 0;
	FOpenMobileNativeStepQueryService::Query(
		Owner,
		MakeQuery(),
		[&DeniedResult, &DeniedCompletions](
			const FOpenMobileNativeStepCountQueryResult& Result)
		{
			DeniedResult = Result;
			++DeniedCompletions;
		}
	);
	TestEqual(TEXT("A denied query completes once"), DeniedCompletions, 1);
	TestEqual(TEXT("Denied motion access remains explicit"),
		DeniedResult.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::PermissionDenied);

	FOpenMobileSensorsCapabilityService::NotifyPermissionStatusChanged(
		FOpenMobileSensorPermissions::GetPermissionName(
			EOpenMobileSensorPermission::MotionActivity),
		EOpenMobilePermissionStatus::Restricted);
	FOpenMobileNativeStepCountQueryResult RestrictedResult;
	int32 RestrictedCompletions = 0;
	FOpenMobileNativeStepQueryService::Query(
		Owner,
		MakeQuery(),
		[&RestrictedResult, &RestrictedCompletions](
			const FOpenMobileNativeStepCountQueryResult& Result)
		{
			RestrictedResult = Result;
			++RestrictedCompletions;
		}
	);
	TestEqual(TEXT("A restricted query completes once"),
		RestrictedCompletions, 1);
	TestEqual(TEXT("Restricted motion access remains explicit"),
		RestrictedResult.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::PermissionRestricted);
	TestEqual(TEXT("A restricted query does not reach the backend"),
		Backend.QueryCalls, 0);
	FinishBackend(Backend);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNativeStepQueryUnsupportedHardwareTest,
	"OpenMobile.Sensors.Steps.Native.HistoricalQueryUnsupportedHardware",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNativeStepQueryUnsupportedHardwareTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsNativeStepQueryTestsPrivate;
	ResetServices();
	FQueryBackend Backend;
	Backend.StepCapabilityState = EOpenMobileCapabilityState::Unavailable;
	Backend.StepRestriction = EOpenMobileSensorRestriction::MissingHardware;
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	FOpenMobileNativeStepCountQueryResult Received;
	int32 Completions = 0;
	FOpenMobileNativeStepQueryService::Query(
		FGuid::NewGuid(),
		MakeQuery(),
		[&Received, &Completions](
			const FOpenMobileNativeStepCountQueryResult& Result)
		{
			Received = Result;
			++Completions;
		}
	);
	TestEqual(TEXT("Unavailable hardware completes once"), Completions, 1);
	TestEqual(TEXT("Missing step hardware remains explicit"),
		Received.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::MissingHardware);
	TestEqual(TEXT("Missing hardware does not reach the backend"),
		Backend.QueryCalls, 0);
	FinishBackend(Backend);
	return true;
}

#endif
