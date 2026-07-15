#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "IOpenMobilePermissionProvider.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorPermissionAsyncAction.h"
#include "OpenMobileSensorPermissions.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

namespace OpenMobileSensorsPermissionFlowTestsPrivate
{
	class FMockPermissionProvider final : public IOpenMobilePermissionProvider
	{
	public:
		virtual FName GetProviderName() const override
		{
			return TEXT("SensorPermissionTestProvider");
		}

		virtual bool SupportsPermission(FName Permission) const override
		{
			return Permission == MotionPermission;
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
			static_cast<void>(OutError);
			if (!SupportsPermission(Permission))
			{
				return false;
			}
			++RequestCount;
			LastRequestIdentifier = RequestIdentifier;
			LastCompletion = Completion;
			Pending.Add(RequestIdentifier, MoveTemp(Completion));
			return true;
		}

		virtual void CancelRequest(const FGuid& RequestIdentifier) override
		{
			++CancelCount;
			Pending.Remove(RequestIdentifier);
		}

		virtual void BeginShutdown() override
		{
			Pending.Reset();
		}

		void CompleteTwice(EOpenMobilePermissionStatus CompletionStatus)
		{
			FOpenMobileNativePermissionCompletion* Completion =
				Pending.Find(LastRequestIdentifier);
			if (!Completion)
			{
				return;
			}
			Completion->ExecuteIfBound(CompletionStatus, {});
			Completion->ExecuteIfBound(CompletionStatus, {});
			Pending.Remove(LastRequestIdentifier);
		}

		void CompleteLate(EOpenMobilePermissionStatus CompletionStatus)
		{
			LastCompletion.ExecuteIfBound(CompletionStatus, {});
		}

		FName MotionPermission =
			FOpenMobileSensorPermissions::GetPermissionName(
				EOpenMobileSensorPermission::MotionActivity
			);
		EOpenMobilePermissionStatus Status =
			EOpenMobilePermissionStatus::NotDetermined;
		mutable int32 StatusQueryCount = 0;
		int32 RequestCount = 0;
		int32 CancelCount = 0;
		FGuid LastRequestIdentifier;
		FOpenMobileNativePermissionCompletion LastCompletion;
		TMap<FGuid, FOpenMobileNativePermissionCompletion> Pending;
	};

	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileSensorType Type,
		FName Permission = NAME_None
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = Type;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name =
			FOpenMobileSensorTypes::GetStableName(Type);
		Capability.Availability.State = EOpenMobileCapabilityState::Available;
		Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
		Capability.MinimumFrequencyHz = 1.0;
		Capability.MaximumFrequencyHz = 100.0;
		Capability.RequiredPermission = Permission;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest(
		EOpenMobileSensorType Type
	)
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = Type;
		Request.Sensor.InstanceId = TEXT("Default");
		return Request;
	}

	void ResetServices()
	{
		FOpenMobilePermissionProviderRegistry::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPermissionStatusFlowTest,
	"OpenMobile.Sensors.Permissions.StatusFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPermissionStatusFlowTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsPermissionFlowTestsPrivate;
	ResetServices();
	FMockPermissionProvider Provider;
	TestTrue(
		TEXT("The sensor permission provider registers"),
		FOpenMobilePermissionProviderRegistry::RegisterProvider(Provider)
	);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);

	const TArray<EOpenMobilePermissionStatus> Statuses = {
		EOpenMobilePermissionStatus::NotDetermined,
		EOpenMobilePermissionStatus::Granted,
		EOpenMobilePermissionStatus::Denied,
		EOpenMobilePermissionStatus::Restricted,
		EOpenMobilePermissionStatus::PermanentlyDenied
	};
	for (EOpenMobilePermissionStatus Expected : Statuses)
	{
		Provider.Status = Expected;
		const FOpenMobilePermissionResult Result =
			Subsystem->GetPermissionStatusNative(
				EOpenMobileSensorPermission::MotionActivity
			);
		TestFalse(TEXT("A supported status query succeeds"), Result.Error.IsSet());
		TestEqual(TEXT("The normalized status is preserved"), Result.Status, Expected);
	}
	TestEqual(
		TEXT("Status queries never trigger a native prompt"),
		Provider.RequestCount,
		0
	);

	int32 FirstCompletionCount = 0;
	int32 SecondCompletionCount = 0;
	Subsystem->RequestPermissionNative(
		EOpenMobileSensorPermission::MotionActivity,
		FOnOpenMobilePermissionRequestComplete::CreateLambda(
			[&FirstCompletionCount](const FOpenMobilePermissionResult&)
			{
				++FirstCompletionCount;
			}
		)
	);
	Subsystem->RequestPermissionNative(
		EOpenMobileSensorPermission::MotionActivity,
		FOnOpenMobilePermissionRequestComplete::CreateLambda(
			[&SecondCompletionCount](const FOpenMobilePermissionResult&)
			{
				++SecondCompletionCount;
			}
		)
	);
	TestEqual(
		TEXT("Concurrent subsystem requests share one native prompt"),
		Provider.RequestCount,
		1
	);
	Provider.CompleteTwice(EOpenMobilePermissionStatus::Granted);
	TestEqual(TEXT("The first caller completes exactly once"), FirstCompletionCount, 1);
	TestEqual(TEXT("The second caller completes exactly once"), SecondCompletionCount, 1);

	TestTrue(
		TEXT("The sensor permission provider unregisters"),
		FOpenMobilePermissionProviderRegistry::UnregisterProvider(Provider)
	);
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobilePermissionProviderRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPermissionOwnerTeardownTest,
	"OpenMobile.Sensors.Permissions.OwnerTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPermissionOwnerTeardownTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsPermissionFlowTestsPrivate;
	ResetServices();
	FMockPermissionProvider Provider;
	FOpenMobilePermissionProviderRegistry::RegisterProvider(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);
	UOpenMobileSensorPermissionAsyncAction* Action =
		NewObject<UOpenMobileSensorPermissionAsyncAction>();
	Action->Subsystem = Subsystem;
	int32 CompletionCount = 0;
	FOpenMobilePermissionResult CompletionResult;
	Action->RequestHandle = Subsystem->RequestPermissionNative(
		EOpenMobileSensorPermission::MotionActivity,
		FOnOpenMobilePermissionRequestComplete::CreateLambda(
			[&CompletionCount, &CompletionResult](
				const FOpenMobilePermissionResult& Result
			)
			{
				++CompletionCount;
				CompletionResult = Result;
			}
		)
	);
	Subsystem->AsyncActions.Add(Action);
	TestTrue(TEXT("The owned request starts"), Action->RequestHandle.IsValid());

	Subsystem->Deinitialize();
	TestTrue(TEXT("Owner teardown finishes the async action"), Action->IsFinished());
	TestEqual(TEXT("Owner teardown cancels one native request"), Provider.CancelCount, 1);
	TestEqual(TEXT("Owner teardown completes the caller once"), CompletionCount, 1);
	TestEqual(
		TEXT("Owner teardown has a normalized cancellation"),
		CompletionResult.Error.Code,
		EOpenMobileErrorCode::Cancelled
	);
	Provider.CompleteLate(EOpenMobilePermissionStatus::Granted);
	TestEqual(
		TEXT("A native callback after owner teardown is ignored"),
		CompletionCount,
		1
	);

	FOpenMobilePermissionProviderRegistry::UnregisterProvider(Provider);
	FOpenMobilePermissionProviderRegistry::ResetForTests();
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsPermissionRetryPolicyTest,
	"OpenMobile.Sensors.Permissions.ExplicitSubscriptionRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsPermissionRetryPolicyTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsPermissionFlowTestsPrivate;
	ResetServices();
	FMockPermissionProvider Provider;
	FOpenMobilePermissionProviderRegistry::RegisterProvider(Provider);
	FOpenMobileSensorsMockBackend Backend(TEXT("PermissionRetry"));
	Backend.SetSensorCapabilities({
		MakeCapability(
			EOpenMobileSensorType::MotionActivity,
			Provider.MotionPermission
		),
		MakeCapability(EOpenMobileSensorType::Accelerometer)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);

	Provider.Status = EOpenMobilePermissionStatus::Denied;
	Subsystem->GetPermissionStatusNative(
		EOpenMobileSensorPermission::MotionActivity
	);
	const FOpenMobileSensorSubscriptionResult Blocked =
		Subsystem->StartSubscriptionNative(
			MakeRequest(EOpenMobileSensorType::MotionActivity)
		);
	TestEqual(
		TEXT("A denied subscription fails immediately"),
		Blocked.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::PermissionDenied
	);
	TestFalse(TEXT("A blocked subscription has no handle"), Blocked.Handle.IsValid());
	TestEqual(
		TEXT("A blocked subscription never reaches the backend"),
		Backend.GetStartSensorStreamCount(),
		0
	);

	Provider.Status = EOpenMobilePermissionStatus::Granted;
	Subsystem->GetPermissionStatusNative(
		EOpenMobileSensorPermission::MotionActivity
	);
	TestEqual(
		TEXT("A permission change does not resurrect failed work"),
		FOpenMobileSensorsSubscriptionService::GetActiveSubscriptionCountForTests(),
		0
	);
	const FOpenMobileSensorSubscriptionResult Retried =
		Subsystem->StartSubscriptionNative(
			MakeRequest(EOpenMobileSensorType::MotionActivity)
		);
	FOpenMobileSensorsSubscriptionService::ProcessPendingBackendOperationsForTests();
	TestEqual(
		TEXT("An explicit retry is accepted after permission is granted"),
		Retried.Operation.Code,
		EOpenMobileSensorResultCode::Accepted
	);
	TestEqual(TEXT("The retry starts one native stream"), Backend.GetStartSensorStreamCount(), 1);
	Subsystem->StopSubscriptionNative(Retried.Handle);

	const int32 PromptCountBeforeAccelerometer = Provider.RequestCount;
	const FOpenMobileSensorSubscriptionResult Accelerometer =
		Subsystem->StartSubscriptionNative(
			MakeRequest(EOpenMobileSensorType::Accelerometer)
		);
	FOpenMobileSensorsSubscriptionService::ProcessPendingBackendOperationsForTests();
	TestEqual(
		TEXT("A sensor without runtime permission starts normally"),
		Accelerometer.Operation.Code,
		EOpenMobileSensorResultCode::Accepted
	);
	TestEqual(
		TEXT("A permission-free sensor never prompts"),
		Provider.RequestCount,
		PromptCountBeforeAccelerometer
	);
	Subsystem->StopSubscriptionNative(Accelerometer.Handle);

	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsCapabilityService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobilePermissionProviderRegistry::UnregisterProvider(Provider);
	FOpenMobilePermissionProviderRegistry::ResetForTests();
	return true;
}

#endif
