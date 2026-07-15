#if WITH_DEV_AUTOMATION_TESTS

#include "IOpenMobilePermissionProvider.h"
#include "Misc/AutomationTest.h"
#include "OpenMobilePermissions.h"

namespace OpenMobilePermissionsTestsPrivate
{
	class FMockPermissionProvider final : public IOpenMobilePermissionProvider
	{
	public:
		virtual FName GetProviderName() const override
		{
			return TEXT("TestProvider");
		}

		virtual bool SupportsPermission(FName Permission) const override
		{
			return Permission == SupportedPermission;
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
			LastCompletion = Completion;
			Pending.Add(RequestIdentifier, MoveTemp(Completion));
			LastRequestIdentifier = RequestIdentifier;
			return true;
		}

		virtual void CancelRequest(const FGuid& RequestIdentifier) override
		{
			++CancelCount;
			Pending.Remove(RequestIdentifier);
		}

		virtual void BeginShutdown() override
		{
			bShutdown = true;
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

		FName SupportedPermission = TEXT("OpenMobile.Test.Permission");
		EOpenMobilePermissionStatus Status =
			EOpenMobilePermissionStatus::NotDetermined;
		mutable int32 StatusQueryCount = 0;
		int32 RequestCount = 0;
		int32 CancelCount = 0;
		bool bShutdown = false;
		FGuid LastRequestIdentifier;
		FOpenMobileNativePermissionCompletion LastCompletion;
		TMap<FGuid, FOpenMobileNativePermissionCompletion> Pending;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobilePermissionsContractTest,
	"OpenMobile.Permissions.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobilePermissionsContractTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	using namespace OpenMobilePermissionsTestsPrivate;
	FOpenMobilePermissionProviderRegistry::ResetForTests();

	const FName Permission(TEXT("OpenMobile.Test.Permission"));
	const FOpenMobilePermissionResult Unsupported =
		FOpenMobilePermissions::GetStatus(Permission);
	TestEqual(
		TEXT("A missing provider is explicit"),
		Unsupported.Error.Code,
		EOpenMobileErrorCode::NotSupported
	);

	int32 InvalidCompletionCount = 0;
	FOpenMobilePermissionResult InvalidResult;
	const FOpenMobilePermissionRequestHandle InvalidHandle =
		FOpenMobilePermissions::RequestPermission(
			NAME_None,
			FOnOpenMobilePermissionRequestComplete::CreateLambda(
				[&InvalidCompletionCount, &InvalidResult](
					const FOpenMobilePermissionResult& Result
				)
				{
					++InvalidCompletionCount;
					InvalidResult = Result;
				}
			)
		);
	TestFalse(TEXT("An invalid request has no handle"), InvalidHandle.IsValid());
	TestEqual(TEXT("Invalid request completes once"), InvalidCompletionCount, 1);
	TestEqual(
		TEXT("Invalid request is typed"),
		InvalidResult.Error.Code,
		EOpenMobileErrorCode::InvalidArgument
	);

	FMockPermissionProvider Provider;
	TestTrue(
		TEXT("Permission provider registers"),
		FOpenMobilePermissionProviderRegistry::RegisterProvider(Provider)
	);
	Provider.Status = EOpenMobilePermissionStatus::Denied;
	const FOpenMobilePermissionResult Status =
		FOpenMobilePermissions::GetStatus(Permission);
	TestEqual(
		TEXT("Status is normalized"),
		Status.Status,
		EOpenMobilePermissionStatus::Denied
	);
	TestEqual(TEXT("Status query reaches the provider"), Provider.StatusQueryCount, 1);
	TestEqual(TEXT("Status query does not request"), Provider.RequestCount, 0);
	const TArray<EOpenMobilePermissionStatus> NormalizedStatuses = {
		EOpenMobilePermissionStatus::NotDetermined,
		EOpenMobilePermissionStatus::Granted,
		EOpenMobilePermissionStatus::Denied,
		EOpenMobilePermissionStatus::Restricted,
		EOpenMobilePermissionStatus::PermanentlyDenied
	};
	for (EOpenMobilePermissionStatus Expected : NormalizedStatuses)
	{
		Provider.Status = Expected;
		TestEqual(
			TEXT("Every normalized status is preserved"),
			FOpenMobilePermissions::GetStatus(Permission).Status,
			Expected
		);
	}
	TestEqual(
		TEXT("Repeated status queries remain side-effect free"),
		Provider.RequestCount,
		0
	);

	int32 CompletionCount = 0;
	FOpenMobilePermissionResult CompletionResult;
	const FOpenMobilePermissionRequestHandle CompletedHandle =
		FOpenMobilePermissions::RequestPermission(
			Permission,
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
	TestTrue(TEXT("Accepted request has a handle"), CompletedHandle.IsValid());
	Provider.CompleteTwice(EOpenMobilePermissionStatus::Granted);
	TestEqual(TEXT("Duplicate native completion is ignored"), CompletionCount, 1);
	TestEqual(
		TEXT("Completion status is preserved"),
		CompletionResult.Status,
		EOpenMobilePermissionStatus::Granted
	);

	int32 FirstSharedCompletionCount = 0;
	int32 SecondSharedCompletionCount = 0;
	const FOpenMobilePermissionRequestHandle FirstSharedHandle =
		FOpenMobilePermissions::RequestPermission(
			Permission,
			FOnOpenMobilePermissionRequestComplete::CreateLambda(
				[this, &FirstSharedCompletionCount](
					const FOpenMobilePermissionResult& Result
				)
				{
					++FirstSharedCompletionCount;
					TestEqual(
						TEXT("First shared request receives the decision"),
						Result.Status,
						EOpenMobilePermissionStatus::Granted
					);
				}
			)
		);
	const int32 RequestCountBeforeJoin = Provider.RequestCount;
	const FOpenMobilePermissionRequestHandle SecondSharedHandle =
		FOpenMobilePermissions::RequestPermission(
			Permission,
			FOnOpenMobilePermissionRequestComplete::CreateLambda(
				[this, &SecondSharedCompletionCount](
					const FOpenMobilePermissionResult& Result
				)
				{
					++SecondSharedCompletionCount;
					TestEqual(
						TEXT("Second shared request receives the decision"),
						Result.Status,
						EOpenMobilePermissionStatus::Granted
					);
				}
			)
		);
	TestTrue(TEXT("First shared request has a handle"), FirstSharedHandle.IsValid());
	TestTrue(TEXT("Second shared request has a handle"), SecondSharedHandle.IsValid());
	TestEqual(
		TEXT("Concurrent callers share one native prompt"),
		Provider.RequestCount,
		RequestCountBeforeJoin
	);
	Provider.CompleteTwice(EOpenMobilePermissionStatus::Granted);
	TestEqual(TEXT("First shared request completes once"), FirstSharedCompletionCount, 1);
	TestEqual(TEXT("Second shared request completes once"), SecondSharedCompletionCount, 1);

	int32 CancelledSharedCompletionCount = 0;
	int32 RemainingSharedCompletionCount = 0;
	const FOpenMobilePermissionRequestHandle CancelledSharedHandle =
		FOpenMobilePermissions::RequestPermission(
			Permission,
			FOnOpenMobilePermissionRequestComplete::CreateLambda(
				[this, &CancelledSharedCompletionCount](
					const FOpenMobilePermissionResult& Result
				)
				{
					++CancelledSharedCompletionCount;
					TestEqual(
						TEXT("A cancelled shared request is typed"),
						Result.Error.Code,
						EOpenMobileErrorCode::Cancelled
					);
				}
			)
		);
	FOpenMobilePermissions::RequestPermission(
		Permission,
		FOnOpenMobilePermissionRequestComplete::CreateLambda(
			[this, &RemainingSharedCompletionCount](
				const FOpenMobilePermissionResult& Result
			)
			{
				++RemainingSharedCompletionCount;
				TestEqual(
					TEXT("The remaining shared request receives the decision"),
					Result.Status,
					EOpenMobilePermissionStatus::Granted
				);
			}
		)
	);
	const int32 CancelCountBeforeSharedCancel = Provider.CancelCount;
	TestTrue(
		TEXT("One shared caller can cancel independently"),
		FOpenMobilePermissions::CancelRequest(CancelledSharedHandle)
	);
	TestEqual(
		TEXT("A remaining caller keeps the native prompt alive"),
		Provider.CancelCount,
		CancelCountBeforeSharedCancel
	);
	Provider.CompleteTwice(EOpenMobilePermissionStatus::Granted);
	TestEqual(TEXT("Cancelled shared request completes once"), CancelledSharedCompletionCount, 1);
	TestEqual(TEXT("Remaining shared request completes once"), RemainingSharedCompletionCount, 1);

	int32 CancelCompletionCount = 0;
	FOpenMobilePermissionResult CancelResult;
	const FOpenMobilePermissionRequestHandle CancelledHandle =
		FOpenMobilePermissions::RequestPermission(
			Permission,
			FOnOpenMobilePermissionRequestComplete::CreateLambda(
				[&CancelCompletionCount, &CancelResult](
					const FOpenMobilePermissionResult& Result
				)
				{
					++CancelCompletionCount;
					CancelResult = Result;
				}
			)
		);
	TestTrue(
		TEXT("Active request can be cancelled"),
		FOpenMobilePermissions::CancelRequest(CancelledHandle)
	);
	TestFalse(
		TEXT("Duplicate cancellation is idempotent"),
		FOpenMobilePermissions::CancelRequest(CancelledHandle)
	);
	TestEqual(TEXT("Cancellation completes once"), CancelCompletionCount, 1);
	TestEqual(
		TEXT("Cancellation has a stable error"),
		CancelResult.Error.Code,
		EOpenMobileErrorCode::Cancelled
	);
	Provider.CompleteLate(EOpenMobilePermissionStatus::Granted);
	TestEqual(
		TEXT("A late native callback after cancellation is ignored"),
		CancelCompletionCount,
		1
	);

	int32 UnregisteredCompletionCount = 0;
	FOpenMobilePermissionResult UnregisteredResult;
	FOpenMobilePermissions::RequestPermission(
		Permission,
		FOnOpenMobilePermissionRequestComplete::CreateLambda(
			[&UnregisteredCompletionCount, &UnregisteredResult](
				const FOpenMobilePermissionResult& Result
			)
			{
				++UnregisteredCompletionCount;
				UnregisteredResult = Result;
			}
		)
	);
	TestTrue(
		TEXT("Provider unregisters"),
		FOpenMobilePermissionProviderRegistry::UnregisterProvider(Provider)
	);
	TestTrue(TEXT("Provider shuts down"), Provider.bShutdown);
	TestEqual(
		TEXT("Provider loss completes pending requests once"),
		UnregisteredCompletionCount,
		1
	);
	TestEqual(
		TEXT("Provider loss is typed"),
		UnregisteredResult.Error.Code,
		EOpenMobileErrorCode::Unavailable
	);

	FOpenMobilePermissionProviderRegistry::ResetForTests();
	return true;
}

#endif
