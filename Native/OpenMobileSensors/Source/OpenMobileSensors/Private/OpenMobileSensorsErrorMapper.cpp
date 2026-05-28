#include "OpenMobileSensorsErrorMapper.h"

namespace OpenMobileSensorsErrorMapperPrivate
{
	struct FMapping
	{
		EOpenMobileSensorFailureReason Reason;
		EOpenMobileSensorResultCode ResultCode;
		EOpenMobileErrorCode CommonCode;
		const TCHAR* Message;
		const TCHAR* Correction;
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
		switch (Reason)
		{
		case EOpenMobileSensorFailureReason::UnsupportedPlatform:
			return {
				Reason,
				EOpenMobileSensorResultCode::NotSupported,
				EOpenMobileErrorCode::NotSupported,
				TEXT("Sensors are not supported on this platform."),
				TEXT("Use an explicit development mock or a supported mobile platform.")
			};
		case EOpenMobileSensorFailureReason::UnsupportedOperation:
			return {
				Reason,
				EOpenMobileSensorResultCode::NotSupported,
				EOpenMobileErrorCode::NotSupported,
				TEXT("The active sensor backend does not support this operation."),
				TEXT("Query capabilities and choose a supported operation.")
			};
		case EOpenMobileSensorFailureReason::MissingHardware:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("The requested sensor hardware is unavailable."),
				TEXT("Query the sensor capability matrix and select an available source.")
			};
		case EOpenMobileSensorFailureReason::DerivedInputUnavailable:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("A required input for the derived sensor is unavailable."),
				TEXT("Enable an available fallback or provide the required input.")
			};
		case EOpenMobileSensorFailureReason::PermissionRequired:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("The sensor operation requires permission."),
				TEXT("Request the reported permission before retrying.")
			};
		case EOpenMobileSensorFailureReason::PermissionDenied:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("The sensor operation was denied permission."),
				TEXT("Respect the decision or direct the user to system settings when appropriate.")
			};
		case EOpenMobileSensorFailureReason::PermissionRestricted:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("System policy restricts the required sensor permission."),
				TEXT("Use a feature path that does not require the restricted permission.")
			};
		case EOpenMobileSensorFailureReason::RateLimited:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Busy,
				TEXT("The sensor request was rate limited."),
				TEXT("Reduce the requested rate or retry after the reported limit clears.")
			};
		case EOpenMobileSensorFailureReason::InvalidRequest:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidArgument,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The sensor request contains an invalid argument."),
				TEXT("Correct the reported request field before retrying.")
			};
		case EOpenMobileSensorFailureReason::InvalidHandle:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidHandle,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The sensor subscription handle is invalid."),
				TEXT("Use a handle returned by the owning Game Instance.")
			};
		case EOpenMobileSensorFailureReason::StaleHandle:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidHandle,
				EOpenMobileErrorCode::Unavailable,
				TEXT("The sensor subscription handle is stale or belongs to another owner."),
				TEXT("Start a new subscription from the owning Game Instance.")
			};
		case EOpenMobileSensorFailureReason::InvalidFrequency:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidArgument,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The requested sensor frequency is invalid."),
				TEXT("Use a finite positive frequency within project policy limits.")
			};
		case EOpenMobileSensorFailureReason::InvalidReferenceFrame:
			return {
				Reason,
				EOpenMobileSensorResultCode::InvalidArgument,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The requested attitude reference frame is invalid or unsupported."),
				TEXT("Select a reference frame reported by the active backend.")
			};
		case EOpenMobileSensorFailureReason::PoorCalibration:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("Sensor calibration quality is below the required level."),
				TEXT("Prompt for calibration or accept lower-quality polling explicitly.")
			};
		case EOpenMobileSensorFailureReason::BackgroundRestricted:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("Background policy restricts this sensor operation."),
				TEXT("Resume in the foreground or select a supported lifecycle policy.")
			};
		case EOpenMobileSensorFailureReason::BufferOverflow:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Busy,
				TEXT("The bounded sensor buffer overflowed."),
				TEXT("Drain more often, lower the rate, or choose a larger bounded capacity.")
			};
		case EOpenMobileSensorFailureReason::StaleLocationInput:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("True-heading location input is stale."),
				TEXT("Provide a recent caller-owned location sample before retrying.")
			};
		case EOpenMobileSensorFailureReason::TemporarilyUnavailable:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("The sensor service is temporarily unavailable."),
				TEXT("Wait for lifecycle or backend recovery and query capabilities again.")
			};
		case EOpenMobileSensorFailureReason::ConfigurationBlocked:
			return {
				Reason,
				EOpenMobileSensorResultCode::Unavailable,
				EOpenMobileErrorCode::NotConfigured,
				TEXT("Project configuration blocks this sensor operation."),
				TEXT("Update the Sensors project settings and rebuild the application.")
			};
		case EOpenMobileSensorFailureReason::Cancelled:
			return {
				Reason,
				EOpenMobileSensorResultCode::Cancelled,
				EOpenMobileErrorCode::Cancelled,
				TEXT("The sensor operation was cancelled."),
				TEXT("No correction is required for an intentional cancellation.")
			};
		case EOpenMobileSensorFailureReason::Internal:
			return {
				Reason,
				EOpenMobileSensorResultCode::Failed,
				EOpenMobileErrorCode::Internal,
				TEXT("Sensors detected an inconsistent internal state."),
				TEXT("Capture sanitized diagnostics and report the failure.")
			};
		case EOpenMobileSensorFailureReason::OperationalFailure:
		case EOpenMobileSensorFailureReason::None:
		default:
			return {
				EOpenMobileSensorFailureReason::OperationalFailure,
				EOpenMobileSensorResultCode::Failed,
				EOpenMobileErrorCode::NativeFailure,
				TEXT("The native sensor operation failed."),
				TEXT("Query capabilities and sanitized diagnostics before retrying.")
			};
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
	Result.Failure.Correction = Mapping.Correction;
	Result.Error = FOpenMobileError::Make(
		Mapping.CommonCode,
		Mapping.Message,
		Result.Failure.NativeCode,
		TEXT("OpenMobileSensors")
	);
	return Result;
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
	return Formatted;
}
