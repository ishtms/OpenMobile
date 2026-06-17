#pragma once

#include "OpenMobileDeviceAndroidPackageTypes.h"

class FOpenMobileDeviceAndroidPackagePolicy final
{
public:
	static constexpr int32 MaximumPackageNameCharacters = 223;
	static bool ValidateSyntax(
		const FOpenMobileAndroidPackageCheckRequest& Request,
		FOpenMobileAndroidPackageCheckResult& OutFailure
	);
	static bool ValidateDeclaration(
		const FOpenMobileAndroidPackageCheckRequest& Request,
		const TArray<FString>& DeclaredPackages,
		FOpenMobileAndroidPackageCheckResult& OutFailure
	);

	static bool Validate(
		const FOpenMobileAndroidPackageCheckRequest& Request,
		const TArray<FString>& DeclaredPackages,
		FOpenMobileAndroidPackageCheckResult& OutFailure
	);
};
