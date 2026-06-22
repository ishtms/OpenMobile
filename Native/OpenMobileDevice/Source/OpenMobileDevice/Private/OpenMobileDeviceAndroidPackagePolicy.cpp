#include "OpenMobileDeviceAndroidPackagePolicy.h"
#include "OpenMobileDeviceNativeConfigurationPolicy.h"

bool FOpenMobileDeviceAndroidPackagePolicy::Validate(
	const FOpenMobileAndroidPackageCheckRequest& Request,
	const TArray<FString>& DeclaredPackages,
	FOpenMobileAndroidPackageCheckResult& OutFailure
)
{
	return ValidateSyntax(Request, OutFailure)
		&& ValidateDeclaration(Request, DeclaredPackages, OutFailure);
}

bool FOpenMobileDeviceAndroidPackagePolicy::ValidateSyntax(
	const FOpenMobileAndroidPackageCheckRequest& Request,
	FOpenMobileAndroidPackageCheckResult& OutFailure
)
{
	OutFailure = {};
	if (!FOpenMobileDeviceNativeConfigurationPolicy::IsValidAndroidPackage(
		Request.PackageName
	))
	{
		OutFailure.State =
			EOpenMobileAndroidPackageCheckState::InvalidRequest;
		OutFailure.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Package checks require one valid Android application ID of at most 223 characters.")
		);
		return false;
	}
	return true;
}

bool FOpenMobileDeviceAndroidPackagePolicy::ValidateDeclaration(
	const FOpenMobileAndroidPackageCheckRequest& Request,
	const TArray<FString>& DeclaredPackages,
	FOpenMobileAndroidPackageCheckResult& OutFailure
)
{
	OutFailure = {};
	const bool bIsDeclared = DeclaredPackages.ContainsByPredicate(
		[&Request](const FString& Declaration)
		{
			return Declaration.Equals(
				Request.PackageName,
				ESearchCase::CaseSensitive
			);
		}
	);
	if (!bIsDeclared)
	{
		OutFailure.State = EOpenMobileAndroidPackageCheckState::NotDeclared;
		OutFailure.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotConfigured,
			TEXT("The Android package is not declared in OpenMobile Device settings.")
		);
		return false;
	}
	return true;
}
