#include "OpenMobileDeviceAndroidPackagePolicy.h"

namespace OpenMobileDeviceAndroidPackagePolicyPrivate
{
	bool IsAsciiLetter(TCHAR Character)
	{
		return (Character >= TEXT('A') && Character <= TEXT('Z'))
			|| (Character >= TEXT('a') && Character <= TEXT('z'));
	}

	bool IsAsciiDigit(TCHAR Character)
	{
		return Character >= TEXT('0') && Character <= TEXT('9');
	}

	bool IsValidPackageName(const FString& PackageName)
	{
		if (PackageName.IsEmpty()
			|| PackageName.Len()
				> FOpenMobileDeviceAndroidPackagePolicy::MaximumPackageNameCharacters)
		{
			return false;
		}
		bool bAtSegmentStart = true;
		bool bHasSeparator = false;
		for (const TCHAR Character : PackageName)
		{
			if (Character == TEXT('.'))
			{
				if (bAtSegmentStart)
				{
					return false;
				}
				bAtSegmentStart = true;
				bHasSeparator = true;
				continue;
			}
			if (bAtSegmentStart && !IsAsciiLetter(Character))
			{
				return false;
			}
			if (!IsAsciiLetter(Character) && !IsAsciiDigit(Character)
				&& Character != TEXT('_'))
			{
				return false;
			}
			bAtSegmentStart = false;
		}
		return bHasSeparator && !bAtSegmentStart;
	}
}

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
	if (!OpenMobileDeviceAndroidPackagePolicyPrivate::IsValidPackageName(
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
