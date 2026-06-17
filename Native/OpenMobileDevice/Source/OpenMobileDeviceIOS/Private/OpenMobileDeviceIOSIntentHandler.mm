#include "OpenMobileDeviceIOSIntentHandler.h"

#import <UIKit/UIKit.h>

FOpenMobileIntentHandlerCheckResult CheckOpenMobileDeviceIOSIntentHandler(
	const FOpenMobileIntentHandlerCheckRequest& Request
)
{
	FOpenMobileIntentHandlerCheckResult Result;
	Result.Kind = Request.Kind;
	if (Request.Kind == EOpenMobileIntentHandlerQueryKind::DeclaredIntent)
	{
		Result.State = EOpenMobileIntentHandlerCheckState::Unsupported;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("iOS does not expose Android-style intent-action queries."),
			FString(),
			TEXT("IOS")
		);
		return Result;
	}
	@autoreleasepool
	{
		NSString* Value = [NSString
			stringWithUTF8String:TCHAR_TO_UTF8(*Request.Url)];
		NSURL* URL = Value ? [NSURL URLWithString:Value] : nil;
		if (!URL)
		{
			Result.State = EOpenMobileIntentHandlerCheckState::Failed;
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("iOS could not construct the declared handler URL."),
				FString(),
				TEXT("IOS")
			);
			return Result;
		}
		Result.State = [[UIApplication sharedApplication] canOpenURL:URL]
			? EOpenMobileIntentHandlerCheckState::CanHandle
			: EOpenMobileIntentHandlerCheckState::CannotHandle;
		return Result;
	}
}
