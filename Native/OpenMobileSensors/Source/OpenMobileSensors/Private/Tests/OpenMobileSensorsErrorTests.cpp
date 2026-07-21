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
		{EOpenMobileSensorFailureReason::MissingLocationInput,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::StaleLocationInput,
			EOpenMobileSensorResultCode::Unavailable,
			EOpenMobileErrorCode::Unavailable},
		{EOpenMobileSensorFailureReason::PoorLocationAccuracy,
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsHumanReadableErrorReportTest,
	"OpenMobile.Sensors.Errors.HumanReadableReports",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsHumanReadableErrorReportTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	for (uint8 Value = static_cast<uint8>(
			EOpenMobileSensorFailureReason::UnsupportedPlatform
		);
		Value <= static_cast<uint8>(EOpenMobileSensorFailureReason::Internal);
		++Value)
	{
		const EOpenMobileSensorFailureReason Reason =
			static_cast<EOpenMobileSensorFailureReason>(Value);
		FOpenMobileSensorErrorContext Context;
		Context.Sensor.Type = EOpenMobileSensorType::Accelerometer;
		Context.Operation = EOpenMobileSensorOperation::StartStream;
		Context.BackendName = TEXT("ContractBackend");
		const FOpenMobileSensorErrorReport Report =
			FOpenMobileSensorsErrorMapper::Describe(
				FOpenMobileSensorsErrorMapper::Map(Reason),
				Context,
				12.0
			);
		TestFalse(TEXT("Every reason has a summary"), Report.Summary.IsEmpty());
		TestFalse(TEXT("Every reason has a likely cause"),
			Report.LikelyCause.IsEmpty());
		TestFalse(TEXT("Every reason has a correction"),
			Report.Correction.IsEmpty());
	}

	FOpenMobileSensorOperationResult Unsafe =
		FOpenMobileSensorsErrorMapper::Map(
			static_cast<EOpenMobileSensorFailureReason>(255),
			TEXT("/Users/player/private"),
			TEXT("token\nsecret")
		);
	Unsafe.Error.Message = TEXT("location=/private/path token=secret");
	FOpenMobileSensorErrorContext UnsafeContext;
	UnsafeContext.Sensor.Type = EOpenMobileSensorType::TrueHeading;
	UnsafeContext.Sensor.InstanceId = TEXT("device-serial-42");
	UnsafeContext.Operation = EOpenMobileSensorOperation::BackendCallback;
	UnsafeContext.BackendName = TEXT("CoreMotion.Manager");
	UnsafeContext.bHasRateContext = true;
	UnsafeContext.RequestedFrequencyHz = 120.0;
	UnsafeContext.AppliedFrequencyHz = 60.0;
	const FOpenMobileSensorErrorReport Redacted =
		FOpenMobileSensorsErrorMapper::Describe(Unsafe, UnsafeContext, 42.0);
	TestEqual(TEXT("Unknown reasons remain stable"),
		Redacted.Failure.Reason,
		EOpenMobileSensorFailureReason::OperationalFailure);
	TestTrue(TEXT("Sensor instance identifiers are removed"),
		Redacted.Context.Sensor.InstanceId.IsNone());
	TestEqual(TEXT("Unsafe native domains are redacted"),
		Redacted.Failure.NativeDomain,
		FString(TEXT("redacted")));
	TestEqual(TEXT("Unsafe native codes are redacted"),
		Redacted.Failure.NativeCode,
		FString(TEXT("redacted")));
	TestFalse(TEXT("Raw native messages are not retained"),
		Redacted.Error.Message.Contains(TEXT("private"))
			|| Redacted.Error.Message.Contains(TEXT("secret")));
	TestTrue(TEXT("Context includes requested rate"),
		Redacted.Summary.ToString().Contains(TEXT("120")));
	TestTrue(TEXT("Context includes applied rate"),
		Redacted.Summary.ToString().Contains(TEXT("60")));

	FOpenMobileSensorRateResolution Rate;
	Rate.AdjustmentReason =
		EOpenMobileSensorRateAdjustmentReason::MissingPlatformDeclaration;
	FOpenMobileSensorsErrorMapper::ApplyRateAdjustmentText(Rate);
	TestFalse(TEXT("Rate adjustment has localized explanation"),
		Rate.AdjustmentExplanation.IsEmpty());
	TestFalse(TEXT("Rate adjustment has localized correction"),
		Rate.AdjustmentCorrection.IsEmpty());

	FOpenMobileSensorsSubscriptionService::ResetForTests();
	const FGuid OwnerA = FGuid::NewGuid();
	const FGuid OwnerB = FGuid::NewGuid();
	const FGuid HandleA = FGuid::NewGuid();
	const FGuid HandleB = FGuid::NewGuid();
	FOpenMobileSensorErrorContext ContextA = UnsafeContext;
	ContextA.SubscriptionIdentifier = HandleA;
	FOpenMobileSensorErrorContext ContextB = UnsafeContext;
	ContextB.SubscriptionIdentifier = HandleB;
	FOpenMobileSensorsSubscriptionService::RecordErrorReportForTests(
		OwnerA,
		Unsafe,
		ContextA
	);
	FOpenMobileSensorsSubscriptionService::RecordErrorReportForTests(
		OwnerB,
		Unsafe,
		ContextB
	);
	const TArray<FOpenMobileSensorErrorReport> OwnerAHandleA =
		FOpenMobileSensorsSubscriptionService::GetRecentErrorReports(
			&OwnerA,
			&HandleA
		);
	const TArray<FOpenMobileSensorErrorReport> OwnerAHandleB =
		FOpenMobileSensorsSubscriptionService::GetRecentErrorReports(
			&OwnerA,
			&HandleB
		);
	TestEqual(TEXT("Reports remain isolated to their owner and handle"),
		OwnerAHandleA.Num(), 1);
	TestEqual(TEXT("Other handles do not leak reports"),
		OwnerAHandleB.Num(), 0);
	FOpenMobileSensorsSubscriptionService::ResetForTests();
	return true;
}

#endif
