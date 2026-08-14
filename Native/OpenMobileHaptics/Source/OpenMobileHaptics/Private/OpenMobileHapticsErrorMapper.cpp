#include "OpenMobileHapticsErrorMapper.h"

namespace OpenMobileHapticsErrorMapperPrivate
{
	struct FMapping
	{
		EOpenMobileHapticErrorCode HapticCode;
		EOpenMobileErrorCode CommonCode;
		const TCHAR* Message;
		const TCHAR* Correction;
	};

	/** Bounds and strips native identifiers before they enter public errors or diagnostics output. */
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

	/** Keeps public code, retry hint, and user action paired for every internal failure reason. */
	FMapping GetMapping(EOpenMobileHapticsFailureReason Reason)
	{
		switch (Reason)
		{
		case EOpenMobileHapticsFailureReason::UnsupportedHardware:
			return {
				EOpenMobileHapticErrorCode::UnsupportedHardware,
				EOpenMobileErrorCode::NotSupported,
				TEXT("This device has no compatible phone haptic actuator."),
				TEXT("Use visual or audio feedback, or choose a device with haptic hardware.")
			};
		case EOpenMobileHapticsFailureReason::UnsupportedFeature:
			return {
				EOpenMobileHapticErrorCode::UnsupportedFeature,
				EOpenMobileErrorCode::NotSupported,
				TEXT("The requested haptic feature is not supported by the active backend."),
				TEXT("Allow a portable fallback or request a supported effect.")
			};
		case EOpenMobileHapticsFailureReason::DisabledPolicy:
			return {
				EOpenMobileHapticErrorCode::DisabledByPolicy,
				EOpenMobileErrorCode::Unavailable,
				TEXT("Haptics are disabled by the current player or system policy."),
				TEXT("Respect the policy or let the player enable haptics.")
			};
		case EOpenMobileHapticsFailureReason::InvalidPattern:
			return {
				EOpenMobileHapticErrorCode::InvalidPattern,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The haptic pattern contains invalid or unsupported data."),
				TEXT("Open the pattern validation results and correct the reported field.")
			};
		case EOpenMobileHapticsFailureReason::RateLimited:
			return {
				EOpenMobileHapticErrorCode::RateLimited,
				EOpenMobileErrorCode::Busy,
				TEXT("The haptic request was suppressed by a comfort rate limit."),
				TEXT("Reduce request frequency or use a less restrictive channel policy.")
			};
		case EOpenMobileHapticsFailureReason::BusyChannel:
			return {
				EOpenMobileHapticErrorCode::ChannelBusy,
				EOpenMobileErrorCode::Busy,
				TEXT("The target haptic channel cannot accept this request."),
				TEXT("Change overlap policy, priority, channel, or queue capacity.")
			};
		case EOpenMobileHapticsFailureReason::LifecycleRestricted:
			return {
				EOpenMobileHapticErrorCode::LifecycleRestricted,
				EOpenMobileErrorCode::Unavailable,
				TEXT("Application lifecycle policy does not allow this haptic request."),
				TEXT("Submit noncritical gameplay haptics only while the game is active.")
			};
		case EOpenMobileHapticsFailureReason::NotConfigured:
			return {
				EOpenMobileHapticErrorCode::NotConfigured,
				EOpenMobileErrorCode::NotConfigured,
				TEXT("This haptic path is not prepared or included in the packaged build."),
				TEXT("Preload its named library or enable the matching platform packaging option.")
			};
		case EOpenMobileHapticsFailureReason::NativeEngineFailure:
			return {
				EOpenMobileHapticErrorCode::NativeEngineFailure,
				EOpenMobileErrorCode::NativeFailure,
				TEXT("The native haptic engine could not process the request."),
				TEXT("Retry only after lifecycle recovery or select a simpler fallback.")
			};
		case EOpenMobileHapticsFailureReason::Interrupted:
			return {
				EOpenMobileHapticErrorCode::Interrupted,
				EOpenMobileErrorCode::NativeFailure,
				TEXT("Accepted haptic playback was interrupted by the platform."),
				TEXT("Wait for foreground or engine recovery before submitting again.")
			};
		case EOpenMobileHapticsFailureReason::Cancelled:
			return {
				EOpenMobileHapticErrorCode::Cancelled,
				EOpenMobileErrorCode::Cancelled,
				TEXT("Haptic playback was cancelled."),
				TEXT("No correction is required for an intentional cancellation.")
			};
		case EOpenMobileHapticsFailureReason::InvalidRequest:
			return {
				EOpenMobileHapticErrorCode::InvalidRequest,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The haptic request contains an invalid argument."),
				TEXT("Use finite bounded values and provide every required name or asset.")
			};
		case EOpenMobileHapticsFailureReason::BackendUnavailable:
			return {
				EOpenMobileHapticErrorCode::BackendUnavailable,
				EOpenMobileErrorCode::Unavailable,
				TEXT("The backend that owns this haptic request is no longer available."),
				TEXT("Query capabilities again and submit a new request.")
			};
		case EOpenMobileHapticsFailureReason::Internal:
		default:
			return {
				EOpenMobileHapticErrorCode::Internal,
				EOpenMobileErrorCode::Internal,
				TEXT("Haptics rejected an inconsistent internal result."),
				TEXT("Capture a sanitized diagnostic snapshot and report the failure.")
			};
		}
	}

	/** Recovers an internal reason from public code only when richer backend context wasn't supplied. */
	EOpenMobileHapticsFailureReason GetReason(
		EOpenMobileHapticErrorCode Code,
		EOpenMobileHapticsFailureReason Fallback
	)
	{
		switch (Code)
		{
		case EOpenMobileHapticErrorCode::UnsupportedHardware:
			return EOpenMobileHapticsFailureReason::UnsupportedHardware;
		case EOpenMobileHapticErrorCode::UnsupportedFeature:
			return EOpenMobileHapticsFailureReason::UnsupportedFeature;
		case EOpenMobileHapticErrorCode::DisabledByPolicy:
			return EOpenMobileHapticsFailureReason::DisabledPolicy;
		case EOpenMobileHapticErrorCode::InvalidPattern:
			return EOpenMobileHapticsFailureReason::InvalidPattern;
		case EOpenMobileHapticErrorCode::RateLimited:
			return EOpenMobileHapticsFailureReason::RateLimited;
		case EOpenMobileHapticErrorCode::ChannelBusy:
			return EOpenMobileHapticsFailureReason::BusyChannel;
		case EOpenMobileHapticErrorCode::LifecycleRestricted:
			return EOpenMobileHapticsFailureReason::LifecycleRestricted;
		case EOpenMobileHapticErrorCode::NotConfigured:
			return EOpenMobileHapticsFailureReason::NotConfigured;
		case EOpenMobileHapticErrorCode::NativeEngineFailure:
			return EOpenMobileHapticsFailureReason::NativeEngineFailure;
		case EOpenMobileHapticErrorCode::Interrupted:
			return EOpenMobileHapticsFailureReason::Interrupted;
		case EOpenMobileHapticErrorCode::Cancelled:
			return EOpenMobileHapticsFailureReason::Cancelled;
		case EOpenMobileHapticErrorCode::InvalidRequest:
			return EOpenMobileHapticsFailureReason::InvalidRequest;
		case EOpenMobileHapticErrorCode::BackendUnavailable:
			return EOpenMobileHapticsFailureReason::BackendUnavailable;
		case EOpenMobileHapticErrorCode::Internal:
			return EOpenMobileHapticsFailureReason::Internal;
		case EOpenMobileHapticErrorCode::None:
		default:
			return Fallback;
		}
	}
}

FOpenMobileHapticError FOpenMobileHapticsErrorMapper::Map(
	const FOpenMobileHapticsErrorContext& Context
)
{
	using namespace OpenMobileHapticsErrorMapperPrivate;
	const FMapping Mapping = GetMapping(Context.Reason);
	FOpenMobileHapticError Error = FOpenMobileHapticError::Make(
		Mapping.HapticCode,
		Mapping.CommonCode,
		Context.Stage,
		Mapping.Message
	);
	Error.NativeDomain = SanitizeNativeIdentifier(Context.NativeDomain);
	Error.NativeCode = SanitizeNativeIdentifier(Context.NativeCode);
	Error.FailedItem = Context.FailedItem;
	Error.Channel = Context.Channel;
	Error.Handle = Context.Handle;
	Error.FallbackAttempts = Context.FallbackAttempts;
	Error.Correction = Mapping.Correction;
	Error.bRejectedBeforeSubmission = !Context.bAfterAcceptance;
	Error.bInterruptedAfterAcceptance = Context.bAfterAcceptance
		&& Context.Reason == EOpenMobileHapticsFailureReason::Interrupted;
	return Error;
}

FOpenMobileHapticError FOpenMobileHapticsErrorMapper::Complete(
	FOpenMobileHapticError Error,
	const FOpenMobileHapticsErrorContext& FallbackContext
)
{
	FOpenMobileHapticsErrorContext Context = FallbackContext;
	Context.Reason = OpenMobileHapticsErrorMapperPrivate::GetReason(
		Error.Code,
		FallbackContext.Reason
	);
	Context.NativeDomain = Error.NativeDomain.IsEmpty()
		? FallbackContext.NativeDomain
		: Error.NativeDomain;
	Context.NativeCode = Error.NativeCode.IsEmpty()
		? FallbackContext.NativeCode
		: Error.NativeCode;
	Context.FailedItem = Error.FailedItem.IsNone()
		? FallbackContext.FailedItem
		: Error.FailedItem;
	Context.Channel = Error.Channel.IsNone()
		? FallbackContext.Channel
		: Error.Channel;
	Context.Handle = Error.Handle.IsValid()
		? Error.Handle
		: FallbackContext.Handle;
	Context.FallbackAttempts = Error.FallbackAttempts.IsEmpty()
		? FallbackContext.FallbackAttempts
		: Error.FallbackAttempts;

	const FOpenMobileHapticError Mapped = Map(Context);
	if (!Error.IsSet())
	{
		return Mapped;
	}
	if (Error.CommonCode == EOpenMobileErrorCode::None)
	{
		Error.CommonCode = Mapped.CommonCode;
	}
	if (Error.Stage == EOpenMobileHapticFailureStage::None)
	{
		Error.Stage = Mapped.Stage;
	}
	if (Error.Message.IsEmpty())
	{
		Error.Message = Mapped.Message;
	}
	Error.NativeDomain = Mapped.NativeDomain;
	Error.NativeCode = Mapped.NativeCode;
	Error.FailedItem = Context.FailedItem;
	Error.Channel = Context.Channel;
	Error.Handle = Context.Handle;
	Error.FallbackAttempts = MoveTemp(Context.FallbackAttempts);
	if (Error.Correction.IsEmpty())
	{
		Error.Correction = Mapped.Correction;
	}
	Error.bRejectedBeforeSubmission = Mapped.bRejectedBeforeSubmission;
	Error.bInterruptedAfterAcceptance = Mapped.bInterruptedAfterAcceptance;
	return Error;
}
