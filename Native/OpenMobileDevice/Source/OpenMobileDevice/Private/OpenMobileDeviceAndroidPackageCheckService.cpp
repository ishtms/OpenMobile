#include "OpenMobileDeviceAndroidPackageCheckService.h"

#include "IOpenMobileDeviceBackend.h"
#include "OpenMobileDeviceAndroidPackagePolicy.h"
#include "OpenMobileDeviceBackendRegistry.h"
#include "OpenMobileDeviceSettings.h"

namespace OpenMobileDeviceAndroidPackageCheckServicePrivate
{
	FOpenMobileAndroidPackageCheckResult MakeUnsupported()
	{
		FOpenMobileAndroidPackageCheckResult Result;
		Result.State = EOpenMobileAndroidPackageCheckState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("No Device backend supports Android package checks.")
		);
		return Result;
	}

	void NormalizeResult(FOpenMobileAndroidPackageCheckResult& Result)
	{
		if (Result.State == EOpenMobileAndroidPackageCheckState::Installed
			|| Result.State == EOpenMobileAndroidPackageCheckState::Disabled
			|| Result.State
				== EOpenMobileAndroidPackageCheckState::NotFoundOrNotVisible)
		{
			Result.Error = {};
			return;
		}
		if (Result.State == EOpenMobileAndroidPackageCheckState::Unknown)
		{
			Result.State = EOpenMobileAndroidPackageCheckState::Failed;
		}
		if (Result.Error.IsSet())
		{
			return;
		}
		EOpenMobileErrorCode ErrorCode = EOpenMobileErrorCode::NativeFailure;
		FString Message = TEXT("The Android package check failed.");
		if (Result.State == EOpenMobileAndroidPackageCheckState::Unsupported)
		{
			ErrorCode = EOpenMobileErrorCode::NotSupported;
			Message = TEXT("The platform does not support Android package checks.");
		}
		else if (Result.State
			== EOpenMobileAndroidPackageCheckState::NotDeclared)
		{
			ErrorCode = EOpenMobileErrorCode::NotConfigured;
			Message = TEXT("The Android package is not declared for this build.");
		}
		else if (Result.State
			== EOpenMobileAndroidPackageCheckState::InvalidRequest)
		{
			ErrorCode = EOpenMobileErrorCode::InvalidArgument;
			Message = TEXT("The Android package check is invalid.");
		}
		Result.Error = FOpenMobileError::Make(
			ErrorCode,
			MoveTemp(Message)
		);
	}
}

FOpenMobileAndroidPackageCheckResult
FOpenMobileDeviceAndroidPackageCheckService::Check(
	const FOpenMobileAndroidPackageCheckRequest& Request
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceAndroidPackageCheckServicePrivate;
	const UOpenMobileDeviceSettings* Settings =
		GetDefault<UOpenMobileDeviceSettings>();
	FOpenMobileAndroidPackageCheckResult Result;
	if (!FOpenMobileDeviceAndroidPackagePolicy::ValidateSyntax(Request, Result))
	{
		return Result;
	}
	IOpenMobileDeviceBackend* Backend =
		FOpenMobileDeviceBackendRegistry::FindBackend();
	if (!Backend)
	{
		return MakeUnsupported();
	}
	const FOpenMobileDeviceCapability Capability = Backend->GetCapability(
		FOpenMobileDeviceCapabilityNames::AndroidPackageCheck
	);
	if (Capability.State != EOpenMobileCapabilityState::Available)
	{
		return MakeUnsupported();
	}
	if (!FOpenMobileDeviceAndroidPackagePolicy::ValidateDeclaration(
		Request,
		Settings->DeclaredAndroidPackages,
		Result
	))
	{
		return Result;
	}
	Result = Backend->CheckAndroidPackage(Request);
	NormalizeResult(Result);
	return Result;
}
