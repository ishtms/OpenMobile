#include "OpenMobileSensorsPackagingValidation.h"

#include "Internationalization/Regex.h"
#include "OpenMobileSensorsSettings.h"

namespace OpenMobileSensorsPackagingValidationPrivate
{
	constexpr const TCHAR* ActivityPermission =
		TEXT("android.permission.ACTIVITY_RECOGNITION");
	constexpr const TCHAR* HighSamplingPermission =
		TEXT("android.permission.HIGH_SAMPLING_RATE_SENSORS");
	constexpr const TCHAR* IOSMotionKey = TEXT("NSMotionUsageDescription");

	void AddIssue(
		TArray<FOpenMobileSensorsPackagingIssue>& OutIssues,
		EOpenMobileSensorsPackagingIssueCode Code,
		const TCHAR* Message
	)
	{
		FOpenMobileSensorsPackagingIssue& Issue = OutIssues.AddDefaulted_GetRef();
		Issue.Code = Code;
		Issue.Message = Message;
	}

	bool HostDeclares(
		const TArray<FString>& Declarations,
		const TCHAR* Permission
	)
	{
		return Declarations.ContainsByPredicate(
			[Permission](const FString& Declaration)
			{
				return Declaration.Contains(
					Permission,
					ESearchCase::CaseSensitive
				);
			}
		);
	}

	FString DecodeXMLText(FString Value)
	{
		Value.ReplaceInline(TEXT("&quot;"), TEXT("\""));
		Value.ReplaceInline(TEXT("&apos;"), TEXT("'"));
		Value.ReplaceInline(TEXT("&lt;"), TEXT("<"));
		Value.ReplaceInline(TEXT("&gt;"), TEXT(">"));
		Value.ReplaceInline(TEXT("&amp;"), TEXT("&"));
		return Value;
	}

	bool TryGetIOSMotionUsageDescription(
		const FString& PlistData,
		FString& OutValue,
		int32& OutDeclarationCount
	)
	{
		OutValue.Reset();
		OutDeclarationCount = 0;
		const FRegexPattern KeyPattern(
			TEXT("<key>\\s*NSMotionUsageDescription\\s*</key>")
		);
		FRegexMatcher KeyMatcher(KeyPattern, PlistData);
		while (KeyMatcher.FindNext())
		{
			++OutDeclarationCount;
		}
		if (OutDeclarationCount == 0)
		{
			int32 SearchStart = 0;
			while (SearchStart < PlistData.Len())
			{
				const int32 KeyIndex = PlistData.Find(
					IOSMotionKey,
					ESearchCase::CaseSensitive,
					ESearchDir::FromStart,
					SearchStart
				);
				if (KeyIndex == INDEX_NONE)
				{
					break;
				}
				++OutDeclarationCount;
				SearchStart = KeyIndex + FCString::Strlen(IOSMotionKey);
			}
		}
		if (OutDeclarationCount == 0)
		{
			return false;
		}

		const FRegexPattern ValuePattern(
			TEXT("<key>\\s*NSMotionUsageDescription\\s*</key>\\s*<string>([\\s\\S]*?)</string>")
		);
		FRegexMatcher ValueMatcher(ValuePattern, PlistData);
		if (ValueMatcher.FindNext())
		{
			OutValue = DecodeXMLText(
				ValueMatcher.GetCaptureGroup(1)
			).TrimStartAndEnd();
		}
		return true;
	}

	void ValidateAndroid(
		const UOpenMobileSensorsSettings& Settings,
		const FOpenMobileSensorsPackagingContext& Context,
		TArray<FOpenMobileSensorsPackagingIssue>& OutIssues
	)
	{
		if (Settings.bEnablePermissionSensitiveSensors
			&& !Context.bSensorsPlatformPackagingEnabled
			&& !Context.bActivityProviderPackagingEnabled)
		{
			AddIssue(
				OutIssues,
				EOpenMobileSensorsPackagingIssueCode::MissingAndroidActivityDeclaration,
				TEXT("Permission-sensitive Sensors features are enabled, but no Android activity-recognition declaration path is enabled.")
			);
		}
		if (Settings.bAllowHighSamplingRate
			&& !Context.bSensorsPlatformPackagingEnabled)
		{
			AddIssue(
				OutIssues,
				EOpenMobileSensorsPackagingIssueCode::MissingAndroidHighSamplingDeclaration,
				TEXT("High-rate Sensors sampling is enabled, but the Sensors Android declaration path is disabled.")
			);
		}

		const auto ValidateHostDeclaration =
			[&OutIssues, &Context](
				const TCHAR* Permission,
				bool bCanonicalEnabled
			)
			{
				if (!HostDeclares(Context.AndroidHostDeclarations, Permission))
				{
					return;
				}
				AddIssue(
					OutIssues,
					bCanonicalEnabled
						? EOpenMobileSensorsPackagingIssueCode::DuplicateAndroidHostDeclaration
						: EOpenMobileSensorsPackagingIssueCode::MismatchedAndroidHostDeclaration,
					bCanonicalEnabled
						? TEXT("Remove the duplicate host Android permission. The Sensors packaging path owns this declaration.")
						: TEXT("A host Android permission contradicts the disabled Sensors project setting.")
				);
			};

		ValidateHostDeclaration(
			ActivityPermission,
			Settings.bEnablePermissionSensitiveSensors
		);
		ValidateHostDeclaration(
			HighSamplingPermission,
			Settings.bAllowHighSamplingRate
		);
	}

	void ValidateIOS(
		const UOpenMobileSensorsSettings& Settings,
		const FOpenMobileSensorsPackagingContext& Context,
		TArray<FOpenMobileSensorsPackagingIssue>& OutIssues
	)
	{
		if (!Context.bSensorsPlatformPackagingEnabled)
		{
			AddIssue(
				OutIssues,
				EOpenMobileSensorsPackagingIssueCode::MissingIOSMotionDeclaration,
				TEXT("The iOS Sensors backend is enabled without its plist packaging path.")
			);
		}
		const FString CanonicalUsage =
			Settings.IOSMotionUsageDescription.TrimStartAndEnd();
		if (CanonicalUsage.IsEmpty())
		{
			AddIssue(
				OutIssues,
				EOpenMobileSensorsPackagingIssueCode::MissingIOSMotionUsageDescription,
				TEXT("IOSMotionUsageDescription must contain user-facing purpose text for the iOS Sensors backend.")
			);
		}

		FString HostUsage;
		int32 HostDeclarationCount = 0;
		const bool bHostDeclaresMotionUsage =
			TryGetIOSMotionUsageDescription(
			Context.IOSHostAdditionalPlistData,
			HostUsage,
			HostDeclarationCount
		);
		if (Context.bIOSHostPlistFallbackRequired
			&& !bHostDeclaresMotionUsage)
		{
			AddIssue(
				OutIssues,
				EOpenMobileSensorsPackagingIssueCode::MissingIOSMotionHostFallback,
				TEXT("UE 5.8 modern Xcode packaging requires a matching NSMotionUsageDescription in iOS AdditionalPlistData because its post-build path ignores iOS UPL plist updates.")
			);
		}
		if (bHostDeclaresMotionUsage)
		{
			if (HostDeclarationCount > 1)
			{
				AddIssue(
					OutIssues,
					EOpenMobileSensorsPackagingIssueCode::DuplicateIOSMotionUsageDescription,
					TEXT("The host declares NSMotionUsageDescription more than once. Keep one value that matches OpenMobile Sensors IOSMotionUsageDescription.")
				);
			}
			const bool bMatches = HostUsage.Equals(
				CanonicalUsage,
				ESearchCase::CaseSensitive
			);
			if (!bMatches)
			{
				AddIssue(
					OutIssues,
					EOpenMobileSensorsPackagingIssueCode::MismatchedIOSMotionUsageDescription,
					TEXT("The host NSMotionUsageDescription differs from IOSMotionUsageDescription in OpenMobile Sensors settings.")
				);
			}
			else if (!Context.bIOSHostPlistFallbackRequired
				&& HostDeclarationCount == 1)
			{
				AddIssue(
					OutIssues,
					EOpenMobileSensorsPackagingIssueCode::DuplicateIOSMotionUsageDescription,
					TEXT("Remove the duplicate host NSMotionUsageDescription. OpenMobile Sensors owns this key.")
				);
			}
		}
	}
}

bool FOpenMobileSensorsPackagingValidator::Validate(
	const UOpenMobileSensorsSettings& Settings,
	const FOpenMobileSensorsPackagingContext& Context,
	TArray<FOpenMobileSensorsPackagingIssue>& OutIssues
)
{
	using namespace OpenMobileSensorsPackagingValidationPrivate;
	OutIssues.Reset();
	if (Context.Target == EOpenMobileSensorsPackagingTarget::Android)
	{
		ValidateAndroid(Settings, Context, OutIssues);
	}
	else
	{
		ValidateIOS(Settings, Context, OutIssues);
	}
	if (Context.bShipping
		&& Settings.DevelopmentInputMode !=
			EOpenMobileSensorsDevelopmentInputMode::Disabled)
	{
		AddIssue(
			OutIssues,
			EOpenMobileSensorsPackagingIssueCode::UnsafeShippingDevelopmentInput,
			TEXT("DevelopmentInputMode must be Disabled before packaging a Shipping target.")
		);
	}
	return OutIssues.IsEmpty();
}
