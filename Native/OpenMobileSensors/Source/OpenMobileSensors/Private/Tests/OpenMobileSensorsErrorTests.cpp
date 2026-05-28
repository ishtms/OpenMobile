#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBackendRegistry.h"
#include "OpenMobileSensorsErrorMapper.h"
#include "OpenMobileSensorsMockBackend.h"
#include "OpenMobileSensorsSubscriptionService.h"
#include "OpenMobileSensorsSubsystem.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsNormalizedErrorMappingTest,
	"OpenMobile.Sensors.Errors.NormalizedMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsNormalizedErrorMappingTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	struct FCase
	{
		EOpenMobileSensorFailureReason Reason;
		EOpenMobileSensorResultCode ResultCode;
		EOpenMobileErrorCode CommonCode;
	};
	const TArray<FCase> Cases = {
		{EOpenMobileSensorFailureReason::UnsupportedPlatform,
			EOpenMobileSensorResultCode::NotSupported,
			EOpenMobileErrorCode::NotSupported},
		{EOpenMobileSensorFailureReason::MissingHardware,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::DerivedInputUnavailable,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::PermissionRequired,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::PermissionDenied,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::PermissionRestricted,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::RateLimited,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Busy},
		{EOpenMobileSensorFailureReason::InvalidFrequency,
			EOpenMobileSensorResultCode::InvalidArgument,
			EOpenMobileErrorCode::InvalidArgument},
		{EOpenMobileSensorFailureReason::InvalidReferenceFrame,
			EOpenMobileSensorResultCode::InvalidArgument,
			EOpenMobileErrorCode::InvalidArgument},
		{EOpenMobileSensorFailureReason::PoorCalibration,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::BackgroundRestricted,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::BufferOverflow,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Busy},
		{EOpenMobileSensorFailureReason::StaleLocationInput,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::TemporarilyUnavailable,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::ConfigurationBlocked,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::NotConfigured},
		{EOpenMobileSensorFailureReason::OperationalFailure,
			EOpenMobileSensorResultCode::Failed,
			EOpenMobileErrorCode::NativeFailure}
	};
	for (const FCase& Case : Cases)
	{
		const FOpenMobileSensorOperationResult Result =
			FOpenMobileSensorsErrorMapper::Map(Case.Reason);
		TestEqual(TEXT("Sensor reason remains typed"),
			Result.Failure.Reason, Case.Reason);
		TestEqual(TEXT("Sensor result category is stable"),
			Result.Code, Case.ResultCode);
		TestEqual(TEXT("Common error category is stable"),
			Result.Error.Code, Case.CommonCode);
		TestFalse(TEXT("Mapped message is present"), Result.Error.Message.IsEmpty());
		TestFalse(TEXT("Mapped correction is present"),
			Result.Failure.Correction.IsEmpty());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsUnknownFutureErrorTest,
	"OpenMobile.Sensors.Errors.UnknownFutureReason",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsUnknownFutureErrorTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FOpenMobileSensorOperationResult Result =
		FOpenMobileSensorsErrorMapper::Map(
			static_cast<EOpenMobileSensorFailureReason>(255),
			TEXT("android.sensor"),
			TEXT("Future_4097")
		);
	TestEqual(TEXT("Unknown reason maps to operational failure"),
		Result.Failure.Reason,
		EOpenMobileSensorFailureReason::OperationalFailure);
	TestEqual(TEXT("Unknown reason maps to native failure"),
		Result.Error.Code,
		EOpenMobileErrorCode::NativeFailure);
	TestEqual(TEXT("Future native domain is preserved"),
		Result.Failure.NativeDomain,
		FString(TEXT("android.sensor")));
	TestEqual(TEXT("Future native code is preserved"),
		Result.Failure.NativeCode,
		FString(TEXT("Future_4097")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsMissingNativeDetailsTest,
	"OpenMobile.Sensors.Errors.MissingNativeDetails",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsMissingNativeDetailsTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FOpenMobileSensorOperationResult Result =
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::MissingHardware
		);
	TestTrue(TEXT("Missing native domain stays empty"),
		Result.Failure.NativeDomain.IsEmpty());
	TestTrue(TEXT("Missing native code stays empty"),
		Result.Failure.NativeCode.IsEmpty());
	TestTrue(TEXT("Common native code stays empty"),
		Result.Error.NativeCode.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsShippingErrorRedactionTest,
	"OpenMobile.Sensors.Errors.ShippingRedaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsShippingErrorRedactionTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FOpenMobileSensorOperationResult Safe =
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::OperationalFailure,
			TEXT("CoreMotion.Manager"),
			TEXT("CMError_42")
		);
	const FString DevelopmentLog =
		FOpenMobileSensorsErrorMapper::FormatForLog(Safe, false);
	TestTrue(TEXT("Development diagnostics include sanitized domain"),
		DevelopmentLog.Contains(TEXT("CoreMotion.Manager")));
	TestTrue(TEXT("Development diagnostics include sanitized code"),
		DevelopmentLog.Contains(TEXT("CMError_42")));
	const FString ShippingLog =
		FOpenMobileSensorsErrorMapper::FormatForLog(Safe, true);
	TestFalse(TEXT("Shipping logs omit native domain"),
		ShippingLog.Contains(TEXT("CoreMotion.Manager")));
	TestFalse(TEXT("Shipping logs omit native code"),
		ShippingLog.Contains(TEXT("CMError_42")));
	FOpenMobileSensorOperationResult SensitiveMessage = Safe;
	SensitiveMessage.Error.Message =
		TEXT("Native failure at /Users/player/private with token=secret");
	const FString SensitiveShippingLog =
		FOpenMobileSensorsErrorMapper::FormatForLog(SensitiveMessage, true);
	TestFalse(TEXT("Shipping logs omit unsanitized native messages"),
		SensitiveShippingLog.Contains(TEXT("/Users/"))
			|| SensitiveShippingLog.Contains(TEXT("secret")));

	const FOpenMobileSensorOperationResult Unsafe =
		FOpenMobileSensorsErrorMapper::Map(
			EOpenMobileSensorFailureReason::OperationalFailure,
			TEXT("/Users/player/private/CoreMotion"),
			TEXT("token\nsecret")
		);
	TestEqual(TEXT("Unsafe native domain is redacted"),
		Unsafe.Failure.NativeDomain,
		FString(TEXT("redacted")));
	TestEqual(TEXT("Unsafe native code is redacted"),
		Unsafe.Failure.NativeCode,
		FString(TEXT("redacted")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSideEffectFreeQueriesTest,
	"OpenMobile.Sensors.Errors.SideEffectFreeQueries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSideEffectFreeQueriesTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsMockBackend Backend(TEXT("SideEffectFree"));
	FOpenMobileSensorsBackendRegistry::RegisterBackend(Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileSensorsSubsystem* Subsystem =
		NewObject<UOpenMobileSensorsSubsystem>(GameInstance);

	Subsystem->GetCapabilitySnapshotNative();
	TestEqual(TEXT("Capability query reads the backend once"),
		Backend.GetCapabilityQueryCount(), 1);
	TestEqual(TEXT("Capability query creates no subscription"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(),
		0);

	FOpenMobileSensorReadResult ReadResult;
	FOpenMobileVectorSensorSample Sample;
	FOpenMobileSensorSubscriptionHandle InvalidHandle;
	Subsystem->GetLatestVectorSampleNative(
		InvalidHandle,
		0,
		ReadResult,
		Sample
	);
	TestEqual(TEXT("Latest query does not query capabilities"),
		Backend.GetCapabilityQueryCount(), 1);
	TestEqual(TEXT("Latest query creates no subscription"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(),
		0);

	FOpenMobileSensorSubscriptionRequest Request;
	Request.Sensor.Type = EOpenMobileSensorType::Accelerometer;
	const FOpenMobileSensorSubscriptionResult Subscription =
		Subsystem->StartSubscriptionNative(Request);
	Subsystem->GetLatestVectorSampleNative(
		Subscription.Handle,
		0,
		ReadResult,
		Sample
	);
	TestEqual(TEXT("Latest query preserves the accepted subscription"),
		FOpenMobileSensorsSubscriptionService::
			GetActiveSubscriptionCountForTests(),
		1);
	TestEqual(TEXT("Latest query performs no backend capability work"),
		Backend.GetCapabilityQueryCount(), 1);

	Subsystem->StopSubscriptionNative(Subscription.Handle);
	const FOpenMobileSensorOperationResult StaleStop =
		Subsystem->StopSubscriptionNative(Subscription.Handle);
	TestEqual(TEXT("Stopped handles report a typed stale reason"),
		StaleStop.Failure.Reason,
		EOpenMobileSensorFailureReason::StaleHandle);
	FOpenMobileSensorsBackendRegistry::UnregisterBackend(Backend);
	const FOpenMobileSensorSubscriptionResult Unsupported =
		Subsystem->StartSubscriptionNative(Request);
	TestEqual(TEXT("No backend reports unsupported platform"),
		Unsupported.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::UnsupportedPlatform);
	FOpenMobileSensorSubscriptionRequest InvalidRequest;
	const FOpenMobileSensorSubscriptionResult Invalid =
		Subsystem->StartSubscriptionNative(InvalidRequest);
	TestEqual(TEXT("Invalid sensor request stays distinct"),
		Invalid.Operation.Failure.Reason,
		EOpenMobileSensorFailureReason::InvalidRequest);

	Subsystem->Deinitialize();
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	FOpenMobileSensorsBackendRegistry::ResetForTests();
	return true;
}

#endif
