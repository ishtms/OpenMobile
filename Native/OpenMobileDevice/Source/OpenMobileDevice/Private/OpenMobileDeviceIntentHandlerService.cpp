#include "OpenMobileDeviceIntentHandlerService.h"

#include "IOpenMobileDeviceBackend.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceIntentHandlerPolicy.h"
#include "OpenMobileDeviceSettings.h"

namespace OpenMobileDeviceIntentHandlerServicePrivate
{
	FOpenMobileIntentHandlerCheckResult MakeUnsupported(
		const FOpenMobileIntentHandlerCheckRequest& Request
	)
	{
		FOpenMobileIntentHandlerCheckResult Result;
		Result.Kind = Request.Kind;
		Result.State = EOpenMobileIntentHandlerCheckState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("No Device backend supports intent-handler checks.")
		);
		return Result;
	}

	void NormalizeResult(FOpenMobileIntentHandlerCheckResult& Result)
	{
		if (Result.State == EOpenMobileIntentHandlerCheckState::CanHandle
			|| Result.State == EOpenMobileIntentHandlerCheckState::CannotHandle)
		{
			Result.Error = {};
			return;
		}
		if (Result.State == EOpenMobileIntentHandlerCheckState::Unknown)
		{
			Result.State = EOpenMobileIntentHandlerCheckState::Failed;
		}
		if (Result.Error.IsSet())
		{
			return;
		}
		EOpenMobileErrorCode ErrorCode = EOpenMobileErrorCode::NativeFailure;
		FString Message = TEXT("The platform handler check failed.");
		if (Result.State == EOpenMobileIntentHandlerCheckState::Unsupported)
		{
			ErrorCode = EOpenMobileErrorCode::NotSupported;
			Message = TEXT("The platform does not support this handler query.");
		}
		else if (Result.State
			== EOpenMobileIntentHandlerCheckState::NotDeclared
			|| Result.State
				== EOpenMobileIntentHandlerCheckState::ConfigurationLimitExceeded)
		{
			ErrorCode = EOpenMobileErrorCode::NotConfigured;
			Message = TEXT("The handler query is not declared for this build.");
		}
		else if (Result.State
			== EOpenMobileIntentHandlerCheckState::InvalidRequest)
		{
			ErrorCode = EOpenMobileErrorCode::InvalidArgument;
			Message = TEXT("The handler query is invalid.");
		}
		Result.Error = FOpenMobileError::Make(
			ErrorCode,
			MoveTemp(Message)
		);
	}
}

FOpenMobileIntentHandlerCheckResult FOpenMobileDeviceIntentHandlerService::Check(
	const FOpenMobileIntentHandlerCheckRequest& Request
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIntentHandlerServicePrivate;
	const UOpenMobileDeviceSettings* Settings =
		GetDefault<UOpenMobileDeviceSettings>();
	FName Scheme;
	FOpenMobileIntentHandlerCheckResult Result;
	if (!FOpenMobileDeviceIntentHandlerPolicy::Validate(
		Request,
		Settings->DeclaredUrlSchemes,
		Settings->DeclaredAndroidIntentActions,
		Scheme,
		Result
	))
	{
		return Result;
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		return MakeUnsupported(Request);
	}
	Result = Backend->CheckIntentHandler(Request);
	Result.Kind = Request.Kind;
	Result.Scheme = Scheme.IsNone() ? FString() : Scheme.ToString();
	NormalizeResult(Result);
	return Result;
}
