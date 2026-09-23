#if WITH_DEV_AUTOMATION_TESTS
#include "Async/Async.h"
#include "HAL/Event.h"
#include "IOpenMobileDeviceBackend.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileDeviceBackendRegistry.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenMobileDeviceStorageLifetimeTest,
	"OpenMobile.Device.Storage.WorkerLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenMobileDeviceStorageLifetimeTest::RunTest(const FString&)
{
	class FBackend final : public IOpenMobileDeviceBackend
	{
	public:
		FEvent* Entered = FPlatformProcess::GetSynchEventFromPool();
		FEvent* Release = FPlatformProcess::GetSynchEventFromPool();
		mutable TAtomic<bool> InQuery{false};
		bool ShutdownOverlappedQuery = false;
		FName GetBackendName() const override { return TEXT("StorageLifetimeTest"); }
		bool QueryStorageSnapshot(FOpenMobileStorageSnapshot&, FOpenMobileError&) const override
		{
			InQuery.Store(true);
			Entered->Trigger();
			Release->Wait();
			InQuery.Store(false);
			return true;
		}
		void BeginShutdown() override { ShutdownOverlappedQuery = InQuery.Load(); }
		~FBackend()
		{
			FPlatformProcess::ReturnSynchEventToPool(Entered);
			FPlatformProcess::ReturnSynchEventToPool(Release);
		}
	};
	FOpenMobileDeviceBackendRegistry::ResetForTests();
	auto Backend = MakeUnique<FBackend>();
	FOpenMobileDeviceBackendRegistry::RegisterBackend(*Backend);
	const auto Token = FOpenMobileDeviceBackendRegistry::CaptureCallbackToken();
	auto Query = Async(EAsyncExecution::ThreadPool, [&]()
	{
		FOpenMobileStorageSnapshot Snapshot;
		FOpenMobileError Error;
		return FOpenMobileDeviceBackendRegistry::QueryStorageSnapshot(Backend.Get(), Token, Snapshot, Error);
	});
	TestTrue(TEXT("Storage query entered the backend"), Backend->Entered->Wait(5000));
	auto Release = Async(EAsyncExecution::Thread, [&]()
	{
		FPlatformProcess::Sleep(0.02f);
		Backend->Release->Trigger();
	});
	FOpenMobileDeviceBackendRegistry::UnregisterBackend(*Backend);
	Release.Wait();
	TestTrue(TEXT("In-flight query completed"), Query.Get());
	TestFalse(TEXT("Native shutdown cannot overlap a query"), Backend->ShutdownOverlappedQuery);
	IOpenMobileDeviceBackend* StaleBackend = Backend.Get();
	Backend.Reset();
	FOpenMobileStorageSnapshot Snapshot;
	FOpenMobileError Error;
	TestFalse(TEXT("A queued query cannot dereference a destroyed backend"),
		FOpenMobileDeviceBackendRegistry::QueryStorageSnapshot(StaleBackend, Token, Snapshot, Error));
	return true;
}
#endif
