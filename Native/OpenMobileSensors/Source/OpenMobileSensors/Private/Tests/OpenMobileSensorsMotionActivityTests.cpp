#if WITH_DEV_AUTOMATION_TESTS

#include "Features/IModularFeatures.h"
#include "IOpenMobileMotionActivityProvider.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileActivitySampleFilter.h"
#include "OpenMobileMotionActivityClassifier.h"
#include "OpenMobileMotionActivityProviderResolver.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsCapabilityService.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSubscriptionService.h"

namespace OpenMobileSensorsMotionActivityTestsPrivate
{
	class FProvider final : public IOpenMobileMotionActivityProvider
	{
	public:
		FProvider(
			FName InName = TEXT("TestActivityProvider"),
			uint32 InVersion = InterfaceVersion,
			bool bInNativeTransitions = false
		)
			: Name(InName)
			, Version(InVersion)
			, bNativeTransitions(bInNativeTransitions)
		{
		}

		virtual uint32 GetInterfaceVersion() const override
		{
			return Version;
		}

		virtual FName GetProviderName() const override
		{
			return Name;
		}

		virtual FOpenMobileSensorCapability GetCapability() const override
		{
			FOpenMobileSensorCapability Capability;
			Capability.Sensor.Type = EOpenMobileSensorType::MotionActivity;
			Capability.Sensor.InstanceId = TEXT("Default");
			Capability.Availability.Name = TEXT("MotionActivity");
			Capability.Availability.State =
				EOpenMobileCapabilityState::Available;
			Capability.Source = EOpenMobileSensorAvailabilitySource::Derived;
			return Capability;
		}

		virtual FOpenMobileSensorCapability
		GetTransitionCapability() const override
		{
			FOpenMobileSensorCapability Capability;
			Capability.Sensor.Type =
				EOpenMobileSensorType::ActivityTransition;
			Capability.Sensor.InstanceId = TEXT("Default");
			Capability.Availability.Name = TEXT("ActivityTransition");
			Capability.Availability.State = bNativeTransitions
				? EOpenMobileCapabilityState::Available
				: EOpenMobileCapabilityState::NotSupported;
			Capability.Source = EOpenMobileSensorAvailabilitySource::Native;
			return Capability;
		}

		virtual FOpenMobileSensorOperationResult StartStream(
			const FOpenMobileMotionActivityProviderStreamHandle& Handle,
			const FOpenMobileMotionActivityProviderRequest& Request,
			FOpenMobileMotionActivityProviderCallbacks&& Callbacks
		) override
		{
			static_cast<void>(Handle);
			static_cast<void>(Request);
			static_cast<void>(Callbacks);
			return {EOpenMobileSensorResultCode::Success};
		}

		virtual FOpenMobileSensorOperationResult ReconfigureStream(
			const FOpenMobileMotionActivityProviderStreamHandle& Handle,
			const FOpenMobileMotionActivityProviderRequest& Request
		) override
		{
			static_cast<void>(Handle);
			static_cast<void>(Request);
			return {EOpenMobileSensorResultCode::Success};
		}

		virtual void StopStream(
			const FOpenMobileMotionActivityProviderStreamHandle& Handle
		) override
		{
			static_cast<void>(Handle);
		}

	private:
		FName Name;
		uint32 Version = InterfaceVersion;
		bool bNativeTransitions = false;
	};

	FOpenMobileSensorCapability MakeCapability(
		EOpenMobileCapabilityState State
	)
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::MotionActivity;
		Capability.Sensor.InstanceId = TEXT("Default");
		Capability.Availability.Name = TEXT("MotionActivity");
		Capability.Availability.State = State;
		Capability.RequiredPermission = TEXT("MotionActivity");
		Capability.Source = EOpenMobileSensorAvailabilitySource::Derived;
		return Capability;
	}

	FOpenMobileSensorSubscriptionRequest MakeRequest()
	{
		FOpenMobileSensorSubscriptionRequest Request;
		Request.Sensor.Type = EOpenMobileSensorType::MotionActivity;
		Request.Sensor.InstanceId = TEXT("Default");
		return Request;
	}

	void ResetServices()
	{
		FOpenMobileSensorsBackendRegistry::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsSubscriptionService::ResetForTests();
	}

	void FinishBackend(FOpenMobileSensorsMockBackend& Backend)
	{
		FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
		FOpenMobileSensorsSubscriptionService::ResetForTests();
		FOpenMobileSensorsCapabilityService::ResetForTests();
		FOpenMobileSensorsBackendRegistry::ResetForTests();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMotionActivityClassificationTest,
	"OpenMobile.Sensors.Activity.Classification",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMotionActivityClassificationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileNativeMotionActivityState Native;
	Native.bStationary = true;
	Native.bWalking = true;
	Native.bRunning = true;
	Native.Confidence = 2;
	FOpenMobileActivitySensorSample Sample;
	FOpenMobileMotionActivityClassifier::Classify(Native, Sample);
	TestEqual(TEXT("Running wins an ambiguous active state"),
		Sample.Activity, EOpenMobileMotionActivity::Running);
	TestEqual(TEXT("Native high confidence is preserved"),
		Sample.Confidence, EOpenMobileActivityConfidence::High);
	TestEqual(TEXT("Every concurrent activity remains visible"),
		Sample.ConcurrentActivities.Num(), 3);
	if (Sample.ConcurrentActivities.Num() == 3)
	{
		TestEqual(TEXT("Concurrent states use stable enum order"),
			Sample.ConcurrentActivities[0],
			EOpenMobileMotionActivity::Stationary);
		TestEqual(TEXT("The walking flag remains visible"),
			Sample.ConcurrentActivities[1],
			EOpenMobileMotionActivity::Walking);
		TestEqual(TEXT("The running flag remains visible"),
			Sample.ConcurrentActivities[2],
			EOpenMobileMotionActivity::Running);
	}

	Native = {};
	Native.bUnknown = true;
	Native.bCycling = true;
	Native.bAutomotive = true;
	Native.Confidence = 1;
	FOpenMobileMotionActivityClassifier::Classify(Native, Sample);
	TestEqual(TEXT("Cycling wins a mixed vehicle transition"),
		Sample.Activity, EOpenMobileMotionActivity::Cycling);
	TestEqual(TEXT("Native medium confidence is preserved"),
		Sample.Confidence, EOpenMobileActivityConfidence::Medium);
	TestEqual(TEXT("Unknown is omitted when a known state exists"),
		Sample.ConcurrentActivities.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMotionActivityDeduplicationTest,
	"OpenMobile.Sensors.Activity.Deduplication",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMotionActivityDeduplicationTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileActivitySampleFilter Filter;
	Filter.Configure({});
	FOpenMobileActivitySensorSample Walking;
	Walking.Header.Sensor.Type = EOpenMobileSensorType::MotionActivity;
	Walking.Header.Sensor.InstanceId = TEXT("Default");
	Walking.Header.TimestampSeconds = 1.0;
	Walking.Header.bValid = true;
	Walking.Activity = EOpenMobileMotionActivity::Walking;
	Walking.Confidence = EOpenMobileActivityConfidence::Medium;
	Walking.ConcurrentActivities = {EOpenMobileMotionActivity::Walking};
	TestTrue(TEXT("The first activity state emits"), Filter.Process(Walking));
	Walking.Header.TimestampSeconds = 2.0;
	TestFalse(TEXT("A duplicate classification is suppressed"),
		Filter.Process(Walking));
	Walking.Confidence = EOpenMobileActivityConfidence::High;
	Walking.Header.TimestampSeconds = 3.0;
	TestTrue(TEXT("A confidence change emits"), Filter.Process(Walking));
	Walking.Header.bStatefulProcessingReset = true;
	Walking.Header.TimestampSeconds = 4.0;
	TestTrue(TEXT("The first state after resume emits"),
		Filter.Process(Walking));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMotionActivityProviderTest,
	"OpenMobile.Sensors.Activity.ProviderSPI",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMotionActivityProviderTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMotionActivityTestsPrivate;
	TestEqual(TEXT("The activity provider SPI is version two"),
		IOpenMobileMotionActivityProvider::InterfaceVersion, 2u);
	const FOpenMobileSensorCapability Missing =
		FOpenMobileMotionActivityProviderResolver::GetCapability();
	TestEqual(TEXT("No Android provider is reported as unsupported"),
		Missing.Availability.State,
		EOpenMobileCapabilityState::NotSupported);
	FProvider Incompatible(TEXT("IncompatibleProvider"), 1);
	IModularFeatures::Get().RegisterModularFeature(
		IOpenMobileMotionActivityProvider::GetModularFeatureName(),
		&Incompatible
	);
	TestNull(TEXT("An incompatible SPI version is ignored"),
		FOpenMobileMotionActivityProviderResolver::FindProvider());
	IModularFeatures::Get().UnregisterModularFeature(
		IOpenMobileMotionActivityProvider::GetModularFeatureName(),
		&Incompatible
	);
	FProvider Provider(TEXT("ZuluProvider"));
	FProvider Preferred(TEXT("AlphaProvider"));
	IModularFeatures::Get().RegisterModularFeature(
		IOpenMobileMotionActivityProvider::GetModularFeatureName(),
		&Provider
	);
	IModularFeatures::Get().RegisterModularFeature(
		IOpenMobileMotionActivityProvider::GetModularFeatureName(),
		&Preferred
	);
	TestEqual(TEXT("Compatible providers resolve deterministically"),
		FOpenMobileMotionActivityProviderResolver::FindProvider(),
		static_cast<IOpenMobileMotionActivityProvider*>(&Preferred));
	const FOpenMobileSensorCapability Available =
		FOpenMobileMotionActivityProviderResolver::GetCapability();
	TestEqual(TEXT("The provider capability is exposed"),
		Available.Availability.State,
		EOpenMobileCapabilityState::Available);
	IModularFeatures::Get().UnregisterModularFeature(
		IOpenMobileMotionActivityProvider::GetModularFeatureName(),
		&Preferred
	);
	IModularFeatures::Get().UnregisterModularFeature(
		IOpenMobileMotionActivityProvider::GetModularFeatureName(),
		&Provider
	);
	TestNull(TEXT("Provider unload removes the implementation"),
		FOpenMobileMotionActivityProviderResolver::FindProvider());
	TestEqual(TEXT("No provider means no native transitions"),
		FOpenMobileMotionActivityProviderResolver::GetTransitionCapability()
			.Availability.State,
		EOpenMobileCapabilityState::NotSupported);
	FProvider TransitionProvider(
		TEXT("TransitionProvider"),
		IOpenMobileMotionActivityProvider::InterfaceVersion,
		true
	);
	IModularFeatures::Get().RegisterModularFeature(
		IOpenMobileMotionActivityProvider::GetModularFeatureName(),
		&TransitionProvider
	);
	TestEqual(TEXT("A provider can advertise native transitions"),
		FOpenMobileMotionActivityProviderResolver::GetTransitionCapability()
			.Availability.State,
		EOpenMobileCapabilityState::Available);
	IModularFeatures::Get().UnregisterModularFeature(
		IOpenMobileMotionActivityProvider::GetModularFeatureName(),
		&TransitionProvider
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMotionActivityAvailabilityTest,
	"OpenMobile.Sensors.Activity.Availability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMotionActivityAvailabilityTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	using namespace OpenMobileSensorsMotionActivityTestsPrivate;
	ResetServices();
	FOpenMobileSensorsMockBackend Backend(TEXT("MotionActivityAvailability"));
	Backend.SetSensorCapabilities({
		MakeCapability(EOpenMobileCapabilityState::Denied)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	const FOpenMobileSensorSubscriptionResult Denied =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest()
		);
	TestEqual(TEXT("Denied activity access fails before native start"),
		Denied.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::PermissionDenied);
	TestEqual(TEXT("Denied activity access starts no provider stream"),
		Backend.GetStartSensorStreamCount(), 0);
	FinishBackend(Backend);

	ResetServices();
	FOpenMobileSensorsMockBackend Missing(TEXT("MotionActivityProviderAbsent"));
	Missing.SetSensorCapabilities({
		MakeCapability(EOpenMobileCapabilityState::NotSupported)
	});
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Missing);
	const FOpenMobileSensorSubscriptionResult Unsupported =
		FOpenMobileSensorsSubscriptionService::StartSubscription(
			FGuid::NewGuid(),
			MakeRequest()
		);
	TestEqual(TEXT("An absent activity provider is not supported"),
		Unsupported.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::UnsupportedPlatform);
	TestEqual(TEXT("An absent activity provider starts no stream"),
		Missing.GetStartSensorStreamCount(), 0);
	FinishBackend(Missing);
	return true;
}

#endif
