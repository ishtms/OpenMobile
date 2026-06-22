#include "OpenMobileDeviceNativeConfigurationPolicy.h"

#include "OpenMobileDeviceAndroidPackagePolicy.h"
#include "OpenMobileDeviceIntentHandlerPolicy.h"

namespace OpenMobileDeviceNativeConfigurationPolicyPrivate
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

	bool IsUnsafeScheme(const FString& Scheme)
	{
		return Scheme == TEXT("about") || Scheme == TEXT("blob")
			|| Scheme == TEXT("content") || Scheme == TEXT("data")
			|| Scheme == TEXT("file") || Scheme == TEXT("intent")
			|| Scheme == TEXT("javascript");
	}
}

bool FOpenMobileDeviceNativeConfigurationPolicy::IsValidUrlScheme(
	const FString& Value
)
{
	using namespace OpenMobileDeviceNativeConfigurationPolicyPrivate;
	if (Value.IsEmpty() || Value != Value.TrimStartAndEnd()
		|| !IsAsciiLetter(Value[0]))
	{
		return false;
	}
	for (int32 Index = 1; Index < Value.Len(); ++Index)
	{
		const TCHAR Character = Value[Index];
		if (!IsAsciiLetter(Character) && !IsAsciiDigit(Character)
			&& Character != TEXT('+') && Character != TEXT('-')
			&& Character != TEXT('.'))
		{
			return false;
		}
	}
	return !IsUnsafeScheme(Value.ToLower());
}

bool FOpenMobileDeviceNativeConfigurationPolicy::TryNormalizeUrlScheme(
	const FString& Value,
	FString& OutNormalized
)
{
	OutNormalized.Reset();
	if (!IsValidUrlScheme(Value))
	{
		return false;
	}
	const FString Normalized = Value.ToLower();
	if (Normalized == TEXT("http") || Normalized == TEXT("https"))
	{
		return false;
	}
	OutNormalized = Normalized;
	return true;
}

bool FOpenMobileDeviceNativeConfigurationPolicy::IsValidAndroidIntentAction(
	const FString& Value
)
{
	using namespace OpenMobileDeviceNativeConfigurationPolicyPrivate;
	if (Value.IsEmpty()
		|| Value.Len()
			> FOpenMobileDeviceIntentHandlerPolicy::MaximumIntentActionCharacters
		|| Value != Value.TrimStartAndEnd() || !Value.Contains(TEXT(".")))
	{
		return false;
	}
	bool bAtSegmentStart = true;
	for (const TCHAR Character : Value)
	{
		if (Character == TEXT('.'))
		{
			if (bAtSegmentStart)
			{
				return false;
			}
			bAtSegmentStart = true;
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
	return !bAtSegmentStart;
}

bool FOpenMobileDeviceNativeConfigurationPolicy::IsValidAndroidPackage(
	const FString& Value
)
{
	using namespace OpenMobileDeviceNativeConfigurationPolicyPrivate;
	if (Value.IsEmpty()
		|| Value.Len()
			> FOpenMobileDeviceAndroidPackagePolicy::MaximumPackageNameCharacters)
	{
		return false;
	}
	bool bAtSegmentStart = true;
	bool bHasSeparator = false;
	for (const TCHAR Character : Value)
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
