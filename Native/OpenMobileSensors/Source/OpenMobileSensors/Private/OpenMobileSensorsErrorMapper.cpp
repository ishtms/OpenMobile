#include "OpenMobileSensorsErrorMapper.h"

#include "HAL/PlatformTime.h"

namespace OpenMobileSensorsErrorMapperPrivate
{
	struct FMapping
	{
		EOpenMobileSensorFailureReason Reason;
		EOpenMobileSensorResultCode ResultCode;
		EOpenMobileErrorCode CommonCode;
		FText Message;
		FText Correction;
	};

	FString SanitizeNativeIdentifier(const FString& Value)
	{
		if (Value.IsEmpty())
		{
			return {};
		}
		for (const TCHAR Character : Value)
		{
			if (!FChar::IsAlnum(Character)
				&& Character != TEXT('.')
				&& Character != TEXT('_')
				&& Character != TEXT('-'))
			{
				return TEXT("redacted");
			}
		}
		return Value.Left(64);
	}

	FMapping GetMapping(EOpenMobileSensorFailureReason Reason)
	{
#define OPENMOBILE_SENSOR_ERROR_TEXT(Key, Source) \
		NSLOCTEXT("OpenMobileSensorsErrors", Key, Source)
		switch (Reason)
		{
		case EOpenMobileSensorFailureReason::UnsupportedPlatform:
			return {
				Reason,
				EOpenMobileSensorResultCode::NotSupported,
				EOpenMobileErrorCode::NotSupported,
				OPENMOBILE_SENSOR_ERROR_TEXT("UnsupportedPlatformCause", "Sensors are not supported on this platform."),
				OPENMOBILE_SENSOR_ERROR_TEXT("UnsupportedPlatformCorrection", "Use an explicit development mock or a supported mobile platform.")
			};
		case EOpenMobileSensorFailureReason::UnsupportedOperation:
			return {
				Reason,
				EOpenMobileSensorResultCode::NotSupported,
				EOpenMobileErrorCode::NotSupported,
				OPENMOBILE_SENSOR_ERROR_TEXT("UnsupportedOperationCause", "The active sensor backend does not support this operation."),
				OPENMOBILE_SENSOR_ERROR_TEXT("UnsupportedOperationCorrection", "Query capabilities and choose a supported operation.")
			};
		case EOpenMobileSensorFailureReason::MissingHardware:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("MissingHardwareCause", "The requested sensor hardware is unavailable."),
				OPENMOBILE_SENSOR_ERROR_TEXT("MissingHardwareCorrection", "Query the sensor capability matrix and select an available source.")
			};
		case EOpenMobileSensorFailureReason::DerivedInputUnavailable:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("DerivedInputUnavailableCause", "A required input for the derived sensor is unavailable."),
				OPENMOBILE_SENSOR_ERROR_TEXT("DerivedInputUnavailableCorrection", "Enable an available fallback or provide the required input.")
			};
		case EOpenMobileSensorFailureReason::PermissionRequired:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("PermissionRequiredCause", "The sensor operation requires permission."),
				OPENMOBILE_SENSOR_ERROR_TEXT("PermissionRequiredCorrection", "Request the reported permission before retrying.")
			};
		case EOpenMobileSensorFailureReason::PermissionDenied:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("PermissionDeniedCause", "The sensor operation was denied permission."),
				OPENMOBILE_SENSOR_ERROR_TEXT("PermissionDeniedCorrection", "Respect the decision or direct the user to system settings when appropriate.")
			};
		case EOpenMobileSensorFailureReason::PermissionRestricted:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("PermissionRestrictedCause", "System policy restricts the required sensor permission."),
				OPENMOBILE_SENSOR_ERROR_TEXT("PermissionRestrictedCorrection", "Use a feature path that does not require the restricted permission.")
			};
		case EOpenMobileSensorFailureReason::RateLimited:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Busy,
				OPENMOBILE_SENSOR_ERROR_TEXT("RateLimitedCause", "The sensor request was rate limited."),
				OPENMOBILE_SENSOR_ERROR_TEXT("RateLimitedCorrection", "Reduce the requested rate or retry after the reported limit clears.")
			};
		case EOpenMobileSensorFailureReason::InvalidRequest:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidArgument,
				EOpenMobileErrorCode::InvalidArgument,
				OPENMOBILE_SENSOR_ERROR_TEXT("InvalidRequestCause", "The sensor request contains an invalid argument."),
				OPENMOBILE_SENSOR_ERROR_TEXT("InvalidRequestCorrection", "Correct the reported request field before retrying.")
			};
		case EOpenMobileSensorFailureReason::InvalidHandle:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidHandle,
				EOpenMobileErrorCode::InvalidArgument,
				OPENMOBILE_SENSOR_ERROR_TEXT("InvalidHandleCause", "The sensor subscription handle is invalid."),
				OPENMOBILE_SENSOR_ERROR_TEXT("InvalidHandleCorrection", "Use a handle returned by the owning Game Instance.")
			};
		case EOpenMobileSensorFailureReason::StaleHandle:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidHandle,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("StaleHandleCause", "The sensor subscription handle is stale or belongs to another owner."),
				OPENMOBILE_SENSOR_ERROR_TEXT("StaleHandleCorrection", "Start a new subscription from the owning Game Instance.")
			};
		case EOpenMobileSensorFailureReason::InvalidFrequency:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidArgument,
				EOpenMobileErrorCode::InvalidArgument,
				OPENMOBILE_SENSOR_ERROR_TEXT("InvalidFrequencyCause", "The requested sensor frequency is invalid."),
				OPENMOBILE_SENSOR_ERROR_TEXT("InvalidFrequencyCorrection", "Use a finite positive frequency within project policy limits.")
			};
		case EOpenMobileSensorFailureReason::InvalidReferenceFrame:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidArgument,
				EOpenMobileErrorCode::InvalidArgument,
				OPENMOBILE_SENSOR_ERROR_TEXT("InvalidReferenceFrameCause", "The requested attitude reference frame is invalid or unsupported."),
				OPENMOBILE_SENSOR_ERROR_TEXT("InvalidReferenceFrameCorrection", "Select a reference frame reported by the active backend.")
			};
		case EOpenMobileSensorFailureReason::PoorCalibration:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("PoorCalibrationCause", "Sensor calibration quality is below the required level."),
				OPENMOBILE_SENSOR_ERROR_TEXT("PoorCalibrationCorrection", "Prompt for calibration or accept lower-quality polling explicitly.")
			};
		case EOpenMobileSensorFailureReason::BackgroundRestricted:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("BackgroundRestrictedCause", "Background policy restricts this sensor operation."),
				OPENMOBILE_SENSOR_ERROR_TEXT("BackgroundRestrictedCorrection", "Resume in the foreground or select a supported lifecycle policy.")
			};
		case EOpenMobileSensorFailureReason::BufferOverflow:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Busy,
				OPENMOBILE_SENSOR_ERROR_TEXT("BufferOverflowCause", "The bounded sensor buffer overflowed."),
				OPENMOBILE_SENSOR_ERROR_TEXT("BufferOverflowCorrection", "Drain more often, lower the rate, or choose a larger bounded capacity.")
			};
		case EOpenMobileSensorFailureReason::MissingLocationInput:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("MissingLocationInputCause", "True-heading location input is missing."),
				OPENMOBILE_SENSOR_ERROR_TEXT("MissingLocationInputCorrection", "Provide a recent caller-owned location sample before retrying.")
			};
		case EOpenMobileSensorFailureReason::StaleLocationInput:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("StaleLocationInputCause", "True-heading location input is stale."),
				OPENMOBILE_SENSOR_ERROR_TEXT("StaleLocationInputCorrection", "Provide a recent caller-owned location sample before retrying.")
			};
		case EOpenMobileSensorFailureReason::PoorLocationAccuracy:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("PoorLocationAccuracyCause", "True-heading location accuracy is insufficient."),
				OPENMOBILE_SENSOR_ERROR_TEXT("PoorLocationAccuracyCorrection", "Provide caller-owned location with horizontal accuracy of 100 metres or better.")
			};
		case EOpenMobileSensorFailureReason::TemporarilyUnavailable:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				OPENMOBILE_SENSOR_ERROR_TEXT("TemporarilyUnavailableCause", "The sensor service is temporarily unavailable."),
				OPENMOBILE_SENSOR_ERROR_TEXT("TemporarilyUnavailableCorrection", "Wait for lifecycle or backend recovery and query capabilities again.")
			};
		case EOpenMobileSensorFailureReason::ConfigurationBlocked:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::NotConfigured,
				OPENMOBILE_SENSOR_ERROR_TEXT("ConfigurationBlockedCause", "Project configuration blocks this sensor operation."),
				OPENMOBILE_SENSOR_ERROR_TEXT("ConfigurationBlockedCorrection", "Update the Sensors project settings and rebuild the application.")
			};
		case EOpenMobileSensorFailureReason::Cancelled:
			return {
				Reason,
				EOpenMobileSensorResultCode::Cancelled,
				EOpenMobileErrorCode::Cancelled,
				OPENMOBILE_SENSOR_ERROR_TEXT("CancelledCause", "The sensor operation was cancelled."),
				OPENMOBILE_SENSOR_ERROR_TEXT("CancelledCorrection", "No correction is required for an intentional cancellation.")
			};
		case EOpenMobileSensorFailureReason::Internal:
			return {
				Reason,
				EOpenMobileSensorResultCode::Failed,
				EOpenMobileErrorCode::Internal,
				OPENMOBILE_SENSOR_ERROR_TEXT("InternalCause", "Sensors detected an inconsistent internal state."),
				OPENMOBILE_SENSOR_ERROR_TEXT("InternalCorrection", "Capture sanitized diagnostics and report the failure.")
			};
		case EOpenMobileSensorFailureReason::OperationalFailure:
		case EOpenMobileSensorFailureReason::None:
		default:
			return {
				EOpenMobileSensorFailureReason::OperationalFailure,
				EOpenMobileSensorResultCode::Failed,
				EOpenMobileErrorCode::NativeFailure,
				OPENMOBILE_SENSOR_ERROR_TEXT("OperationalFailureCause", "The native sensor operation failed."),
				OPENMOBILE_SENSOR_ERROR_TEXT("OperationalFailureCorrection", "Query capabilities and sanitized diagnostics before retrying.")
			};
		}
#undef OPENMOBILE_SENSOR_ERROR_TEXT
	}

	FText GetOperationText(EOpenMobileSensorOperation Operation)
	{
		switch (Operation)
		{
		case EOpenMobileSensorOperation::CapabilityQuery:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationCapabilityQuery", "query capabilities");
		case EOpenMobileSensorOperation::StartStream:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationStartStream", "start stream");
		case EOpenMobileSensorOperation::ReconfigureStream:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationReconfigureStream", "reconfigure stream");
		case EOpenMobileSensorOperation::StopStream:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationStopStream", "stop stream");
		case EOpenMobileSensorOperation::ReadLatest:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationReadLatest", "read latest sample");
		case EOpenMobileSensorOperation::ReadBuffer:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationReadBuffer", "read buffer");
		case EOpenMobileSensorOperation::Flush:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationFlush", "flush stream");
		case EOpenMobileSensorOperation::Permission:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationPermission", "request permission");
		case EOpenMobileSensorOperation::Calibration:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationCalibration", "calibrate sensor");
		case EOpenMobileSensorOperation::Recenter:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationRecenter", "recenter sensor");
		case EOpenMobileSensorOperation::Recording:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationRecording", "record samples");
		case EOpenMobileSensorOperation::Replay:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationReplay", "replay samples");
		case EOpenMobileSensorOperation::Lifecycle:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationLifecycle", "apply lifecycle policy");
		case EOpenMobileSensorOperation::BackendCallback:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationBackendCallback", "process backend update");
		case EOpenMobileSensorOperation::Unknown:
		default:
			return NSLOCTEXT("OpenMobileSensorsErrors", "OperationUnknown", "perform sensor operation");
		}
	}
}

FOpenMobileSensorOperationResult FOpenMobileSensorsErrorMapper::Map(
	EOpenMobileSensorFailureReason Reason,
	FString NativeDomain,
	FString NativeCode
)
{
	using namespace OpenMobileSensorsErrorMapperPrivate;
	const FMapping Mapping = GetMapping(Reason);
	FOpenMobileSensorOperationResult Result;
	Result.Code = Mapping.ResultCode;
	Result.Failure.Reason = Mapping.Reason;
	Result.Failure.NativeDomain = SanitizeNativeIdentifier(NativeDomain);
	Result.Failure.NativeCode = SanitizeNativeIdentifier(NativeCode);
	Result.Failure.Correction = Mapping.Correction.ToString();
	Result.Error = FOpenMobileError::Make(
		Mapping.CommonCode,
		Mapping.Message.ToString(),
		Result.Failure.NativeCode,
		TEXT("OpenMobileSensors")
	);
	return Result;
}

FOpenMobileSensorErrorReport FOpenMobileSensorsErrorMapper::Describe(
	const FOpenMobileSensorOperationResult& Result,
	const FOpenMobileSensorErrorContext& Context,
	double TimestampSeconds
)
{
	using namespace OpenMobileSensorsErrorMapperPrivate;
	const FMapping Mapping = GetMapping(Result.Failure.Reason);
	FOpenMobileSensorErrorReport Report;
	Report.TimestampSeconds = TimestampSeconds >= 0.0
		? TimestampSeconds
		: FPlatformTime::Seconds();
	Report.Context = Context;
	Report.Context.Sensor.InstanceId = NAME_None;
	Report.Context.BackendName = FName(*SanitizeNativeIdentifier(Context.BackendName.ToString()));
	Report.ResultCode = Result.Code;
	Report.Failure = Result.Failure;
	Report.Failure.Reason = Mapping.Reason;
	Report.Failure.NativeDomain = SanitizeNativeIdentifier(Result.Failure.NativeDomain);
	Report.Failure.NativeCode = SanitizeNativeIdentifier(Result.Failure.NativeCode);
	Report.Failure.Correction = Mapping.Correction.ToString();
	Report.Error = FOpenMobileError::Make(
		Result.Error.Code == EOpenMobileErrorCode::None
			? Mapping.CommonCode
			: Result.Error.Code,
		Mapping.Message.ToString(),
		Report.Failure.NativeCode,
		TEXT("OpenMobileSensors")
	);
	Report.LikelyCause = Mapping.Message;
	Report.Correction = Mapping.Correction;

	const FText SensorText = FText::FromName(
		FOpenMobileSensorTypes::GetStableName(Context.Sensor.Type)
	);
	const FText BackendText = Report.Context.BackendName.IsNone()
		? NSLOCTEXT("OpenMobileSensorsErrors", "NoBackend", "no backend")
		: FText::FromName(Report.Context.BackendName);
	if (Context.bHasRateContext)
	{
		Report.Summary = FText::Format(
			NSLOCTEXT(
				"OpenMobileSensorsErrors",
				"RateErrorSummary",
				"Could not {Operation} for {Sensor} through {Backend}. Requested {Requested} Hz; applied {Applied} Hz."
			),
			FFormatNamedArguments{
				{TEXT("Operation"), GetOperationText(Context.Operation)},
				{TEXT("Sensor"), SensorText},
				{TEXT("Backend"), BackendText},
				{TEXT("Requested"), Context.RequestedFrequencyHz},
				{TEXT("Applied"), Context.AppliedFrequencyHz}
			}
		);
	}
	else
	{
		Report.Summary = FText::Format(
			NSLOCTEXT(
				"OpenMobileSensorsErrors",
				"ErrorSummary",
				"Could not {Operation} for {Sensor} through {Backend}."
			),
			FFormatNamedArguments{
				{TEXT("Operation"), GetOperationText(Context.Operation)},
				{TEXT("Sensor"), SensorText},
				{TEXT("Backend"), BackendText}
			}
		);
	}
	return Report;
}

FOpenMobileSensorRuntimeError FOpenMobileSensorsErrorMapper::MakeRuntimeError(
	const FOpenMobileSensorOperationResult& Result)
{
	EOpenMobileSensorFailureReason Reason = Result.Failure.Reason;
	if (Reason == EOpenMobileSensorFailureReason::None
		&& Result.Error.IsSet())
	{
		Reason = FromCommon(Result.Error).Failure.Reason;
	}
	const FOpenMobileSensorOperationResult Mapped = Map(Reason);
	FOpenMobileSensorRuntimeError Error;
	Error.Reason = Reason;
	Error.Message = FText::FromString(Result.Error.Message.IsEmpty()
		? Mapped.Error.Message
		: Result.Error.Message);
	Error.Correction = FText::FromString(
		Result.Failure.Correction.IsEmpty()
			? Mapped.Failure.Correction
			: Result.Failure.Correction);
	switch (Reason)
	{
	case EOpenMobileSensorFailureReason::PermissionRequired:
	case EOpenMobileSensorFailureReason::RateLimited:
	case EOpenMobileSensorFailureReason::PoorCalibration:
	case EOpenMobileSensorFailureReason::BackgroundRestricted:
	case EOpenMobileSensorFailureReason::BufferOverflow:
	case EOpenMobileSensorFailureReason::MissingLocationInput:
	case EOpenMobileSensorFailureReason::StaleLocationInput:
	case EOpenMobileSensorFailureReason::PoorLocationAccuracy:
	case EOpenMobileSensorFailureReason::TemporarilyUnavailable:
	case EOpenMobileSensorFailureReason::OperationalFailure:
		Error.bRetryable = true;
		break;
	default:
		break;
	}
	return Error;
}

void FOpenMobileSensorsErrorMapper::ApplyRateAdjustmentText(
	FOpenMobileSensorRateResolution& Resolution
)
{
	switch (Resolution.AdjustmentReason)
	{
	case EOpenMobileSensorRateAdjustmentReason::ProjectPolicy:
		Resolution.AdjustmentExplanation = NSLOCTEXT("OpenMobileSensorsErrors", "RateProjectPolicyCause", "Project policy or this request did not opt in to high-rate sensor access.");
		Resolution.AdjustmentCorrection = NSLOCTEXT("OpenMobileSensorsErrors", "RateProjectPolicyCorrection", "Enable high-rate sensors in project settings and opt in on the request, or request a lower rate.");
		break;
	case EOpenMobileSensorRateAdjustmentReason::HardwareLimit:
		Resolution.AdjustmentExplanation = NSLOCTEXT("OpenMobileSensorsErrors", "RateHardwareLimitCause", "The requested rate exceeds the sensor hardware limit.");
		Resolution.AdjustmentCorrection = NSLOCTEXT("OpenMobileSensorsErrors", "RateHardwareLimitCorrection", "Request a rate at or below the reported hardware maximum.");
		break;
	case EOpenMobileSensorRateAdjustmentReason::MissingPlatformDeclaration:
		Resolution.AdjustmentExplanation = NSLOCTEXT("OpenMobileSensorsErrors", "RateDeclarationCause", "Android high-rate sensor access was not declared in the packaged application.");
		Resolution.AdjustmentCorrection = NSLOCTEXT("OpenMobileSensorsErrors", "RateDeclarationCorrection", "Enable the Android high-rate sensor declaration in project settings and rebuild the application.");
		break;
	case EOpenMobileSensorRateAdjustmentReason::OperatingSystemLimit:
		Resolution.AdjustmentExplanation = NSLOCTEXT("OpenMobileSensorsErrors", "RateOperatingSystemLimitCause", "The operating system limited the requested sensor rate.");
		Resolution.AdjustmentCorrection = NSLOCTEXT("OpenMobileSensorsErrors", "RateOperatingSystemLimitCorrection", "Use the applied rate or request a lower rate supported by the operating system.");
		break;
	case EOpenMobileSensorRateAdjustmentReason::BackendLimit:
		Resolution.AdjustmentExplanation = NSLOCTEXT("OpenMobileSensorsErrors", "RateBackendLimitCause", "The active sensor backend limited the requested rate.");
		Resolution.AdjustmentCorrection = NSLOCTEXT("OpenMobileSensorsErrors", "RateBackendLimitCorrection", "Use the applied rate or select a backend with a higher supported rate.");
		break;
	case EOpenMobileSensorRateAdjustmentReason::None:
	default:
		Resolution.AdjustmentExplanation = FText::GetEmpty();
		Resolution.AdjustmentCorrection = FText::GetEmpty();
		break;
	}
}

FOpenMobileSensorOperationResult FOpenMobileSensorsErrorMapper::FromCommon(
	const FOpenMobileError& Error
)
{
	EOpenMobileSensorFailureReason Reason;
	switch (Error.Code)
	{
	case EOpenMobileErrorCode::NotSupported:
		Reason = EOpenMobileSensorFailureReason::UnsupportedOperation;
		break;
	case EOpenMobileErrorCode::NotConfigured:
		Reason = EOpenMobileSensorFailureReason::ConfigurationBlocked;
		break;
	case EOpenMobileErrorCode::Unavailable:
		Reason = EOpenMobileSensorFailureReason::TemporarilyUnavailable;
		break;
	case EOpenMobileErrorCode::Busy:
		Reason = EOpenMobileSensorFailureReason::RateLimited;
		break;
	case EOpenMobileErrorCode::Cancelled:
		Reason = EOpenMobileSensorFailureReason::Cancelled;
		break;
	case EOpenMobileErrorCode::InvalidArgument:
		Reason = EOpenMobileSensorFailureReason::InvalidRequest;
		break;
	case EOpenMobileErrorCode::Internal:
		Reason = EOpenMobileSensorFailureReason::Internal;
		break;
	case EOpenMobileErrorCode::NativeFailure:
	case EOpenMobileErrorCode::None:
	default:
		Reason = EOpenMobileSensorFailureReason::OperationalFailure;
		break;
	}
	FOpenMobileSensorOperationResult Result = Map(
		Reason,
		{},
		Error.NativeCode
	);
	if (Error.IsSet())
	{
		Result.Error = Error;
		Result.Error.NativeCode = Result.Failure.NativeCode;
	}
	return Result;
}

FString FOpenMobileSensorsErrorMapper::FormatForLog(
	const FOpenMobileSensorOperationResult& Result,
	const FOpenMobileSensorErrorContext* Context,
	bool bShipping
)
{
	const FString SafeMessage = Map(Result.Failure.Reason).Error.Message;
	const FString& Message = bShipping || Result.Error.Message.IsEmpty()
		? SafeMessage
		: Result.Error.Message;
	FString Formatted = FString::Printf(
		TEXT("Sensors error %s: %s"),
		*UEnum::GetValueAsString(Result.Failure.Reason),
		*Message
	);
	if (!bShipping && (!Result.Failure.NativeDomain.IsEmpty()
		|| !Result.Failure.NativeCode.IsEmpty()))
	{
		Formatted += FString::Printf(
			TEXT(" [domain=%s code=%s]"),
			*Result.Failure.NativeDomain,
			*Result.Failure.NativeCode
		);
	}
	if (!bShipping && Context != nullptr)
	{
		Formatted += FString::Printf(
			TEXT(" [sensor=%s operation=%s backend=%s"),
			*FOpenMobileSensorTypes::GetStableName(Context->Sensor.Type).ToString(),
			*UEnum::GetValueAsString(Context->Operation),
			*OpenMobileSensorsErrorMapperPrivate::SanitizeNativeIdentifier(Context->BackendName.ToString())
		);
		if (Context->bHasRateContext)
		{
			Formatted += FString::Printf(
				TEXT(" requested=%.3fHz applied=%.3fHz"),
				Context->RequestedFrequencyHz,
				Context->AppliedFrequencyHz
			);
		}
		Formatted += TEXT("]");
	}
	return Formatted;
}
