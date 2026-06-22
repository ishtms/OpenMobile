#include "OpenMobileDeviceIntentHandlerPolicy.h"

#include "Containers/StringConv.h"
#include "OpenMobileDeviceClipboardPolicy.h"
#include "OpenMobileDeviceNativeConfigurationPolicy.h"

namespace OpenMobileDeviceIntentHandlerPolicyPrivate
{
	bool IsDeclaredScheme(
		const FString& Scheme,
		const TArray<FString>& Declarations
	)
	{
		if (Scheme == TEXT("http") || Scheme == TEXT("https"))
		{
			return true;
		}
		for (const FString& Declaration : Declarations)
		{
			FString NormalizedDeclaration;
			if (FOpenMobileDeviceNativeConfigurationPolicy::TryNormalizeUrlScheme(
				Declaration,
				NormalizedDeclaration
			) && NormalizedDeclaration == Scheme)
			{
				return true;
			}
		}
		return false;
	}

	FOpenMobileIntentHandlerCheckResult MakeFailure(
		const FOpenMobileIntentHandlerCheckRequest& Request,
		EOpenMobileIntentHandlerCheckState State,
		EOpenMobileErrorCode ErrorCode,
		FString Message
	)
	{
		FOpenMobileIntentHandlerCheckResult Result;
		Result.Kind = Request.Kind;
		Result.State = State;
		Result.Error = FOpenMobileError::Make(ErrorCode, MoveTemp(Message));
		return Result;
	}
}

bool FOpenMobileDeviceIntentHandlerPolicy::Validate(
	const FOpenMobileIntentHandlerCheckRequest& Request,
	const TArray<FString>& DeclaredUrlSchemes,
	const TArray<FString>& DeclaredIntentActions,
	FName& OutScheme,
	FOpenMobileIntentHandlerCheckResult& OutFailure
)
{
	using namespace OpenMobileDeviceIntentHandlerPolicyPrivate;
	OutScheme = NAME_None;
	OutFailure = {};
	if (Request.Kind == EOpenMobileIntentHandlerQueryKind::Url)
	{
		if (!Request.DeclaredIntentAction.IsEmpty())
		{
			OutFailure = MakeFailure(
				Request,
				EOpenMobileIntentHandlerCheckState::InvalidRequest,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("A URL handler query cannot include an intent action.")
			);
			return false;
		}
		const FTCHARToUTF8 Utf8(*Request.Url, Request.Url.Len());
		if (Utf8.Length() > MaximumUrlBytes
			|| !FOpenMobileDeviceClipboardPolicy::IsAbsoluteUrl(Request.Url))
		{
			OutFailure = MakeFailure(
				Request,
				EOpenMobileIntentHandlerCheckState::InvalidRequest,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("Handler checks require a well-formed absolute URL of at most 4 KiB.")
			);
			return false;
		}
		const int32 ColonIndex = Request.Url.Find(TEXT(":"));
		FString Scheme = Request.Url.Left(ColonIndex).ToLower();
		if (!FOpenMobileDeviceNativeConfigurationPolicy::IsValidUrlScheme(Scheme))
		{
			OutFailure = MakeFailure(
				Request,
				EOpenMobileIntentHandlerCheckState::InvalidRequest,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("The URL scheme is malformed or unsafe for a handler check.")
			);
			return false;
		}
		if (Scheme != TEXT("http") && Scheme != TEXT("https")
			&& DeclaredUrlSchemes.Num() > MaximumDeclaredUrlSchemes)
		{
			OutFailure = MakeFailure(
				Request,
				EOpenMobileIntentHandlerCheckState::ConfigurationLimitExceeded,
				EOpenMobileErrorCode::NotConfigured,
				TEXT("Declared custom URL schemes exceed the 50-entry platform limit.")
			);
			return false;
		}
		if (!IsDeclaredScheme(Scheme, DeclaredUrlSchemes))
		{
			OutFailure = MakeFailure(
				Request,
				EOpenMobileIntentHandlerCheckState::NotDeclared,
				EOpenMobileErrorCode::NotConfigured,
				TEXT("The URL scheme is not declared in OpenMobile Device settings.")
			);
			return false;
		}
		OutScheme = FName(Scheme);
		return true;
	}
	if (Request.Kind == EOpenMobileIntentHandlerQueryKind::DeclaredIntent)
	{
		if (!Request.Url.IsEmpty()
			|| !FOpenMobileDeviceNativeConfigurationPolicy::IsValidAndroidIntentAction(
				Request.DeclaredIntentAction
			))
		{
			OutFailure = MakeFailure(
				Request,
				EOpenMobileIntentHandlerCheckState::InvalidRequest,
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("Declared intent checks require one exact action and no URL.")
			);
			return false;
		}
		if (DeclaredIntentActions.Num() > MaximumDeclaredIntentActions)
		{
			OutFailure = MakeFailure(
				Request,
				EOpenMobileIntentHandlerCheckState::ConfigurationLimitExceeded,
				EOpenMobileErrorCode::NotConfigured,
				TEXT("Declared Android intent actions exceed the 50-entry limit.")
			);
			return false;
		}
		const bool bIsDeclared = DeclaredIntentActions.ContainsByPredicate(
			[&Request](const FString& Declaration)
			{
				return Declaration.Equals(
					Request.DeclaredIntentAction,
					ESearchCase::CaseSensitive
				);
			}
		);
		if (!bIsDeclared)
		{
			OutFailure = MakeFailure(
				Request,
				EOpenMobileIntentHandlerCheckState::NotDeclared,
				EOpenMobileErrorCode::NotConfigured,
				TEXT("The intent action is not declared in OpenMobile Device settings.")
			);
			return false;
		}
		return true;
	}
	OutFailure = MakeFailure(
		Request,
		EOpenMobileIntentHandlerCheckState::InvalidRequest,
		EOpenMobileErrorCode::InvalidArgument,
		TEXT("The intent-handler query kind is invalid.")
	);
	return false;
}
