#include "OpenMobileDeviceIOSClipboard.h"

#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSClipboardPrivate
{
	bool IsApplicationActive()
	{
		return [UIApplication sharedApplication].applicationState
			== UIApplicationStateActive;
	}

	FOpenMobileClipboardOperationResult MakeUnavailable()
	{
		FOpenMobileClipboardOperationResult Result;
		Result.State = EOpenMobileClipboardOperationState::Unavailable;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Unavailable,
			TEXT("iOS clipboard access requires an active application."),
			FString(),
			TEXT("IOS")
		);
		return Result;
	}

	FOpenMobileClipboardOperationResult MakeFailure()
	{
		FOpenMobileClipboardOperationResult Result;
		Result.State = EOpenMobileClipboardOperationState::Failed;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			TEXT("iOS pasteboard operation failed."),
			FString(),
			TEXT("IOS")
		);
		return Result;
	}
}

FOpenMobileClipboardOperationResult
CheckOpenMobileDeviceIOSClipboardContentTypes()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSClipboardPrivate;
	if (!IsApplicationActive())
	{
		return MakeUnavailable();
	}
	@autoreleasepool
	{
			UIPasteboard* Pasteboard = [UIPasteboard generalPasteboard];
			FOpenMobileClipboardOperationResult Result;
			Result.Content.bContentTypesAvailable = true;
			if (Pasteboard.numberOfItems == 0)
			{
				Result.State = EOpenMobileClipboardOperationState::Empty;
				Result.Content.ContentTypes.Add(
					EOpenMobileClipboardContentType::Empty
				);
				return Result;
			}
			Result.State = EOpenMobileClipboardOperationState::Succeeded;
			if (Pasteboard.hasStrings)
			{
				Result.Content.ContentTypes.Add(
					EOpenMobileClipboardContentType::Text
				);
			}
			if (Pasteboard.hasURLs)
			{
				Result.Content.ContentTypes.Add(
					EOpenMobileClipboardContentType::Url
				);
			}
			if (Result.Content.ContentTypes.IsEmpty())
			{
				Result.Content.ContentTypes.Add(
					EOpenMobileClipboardContentType::Unknown
				);
			}
			return Result;
	}
}

FOpenMobileClipboardOperationResult WriteOpenMobileDeviceIOSClipboard(
	const FOpenMobileClipboardWriteRequest& Request
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSClipboardPrivate;
	if (!IsApplicationActive())
	{
		return MakeUnavailable();
	}
	@autoreleasepool
	{
			NSString* Value = [NSString
				stringWithUTF8String:TCHAR_TO_UTF8(*Request.Value)];
			if (!Value)
			{
				return MakeFailure();
			}
			UIPasteboard* Pasteboard = [UIPasteboard generalPasteboard];
			if (Request.ContentType == EOpenMobileClipboardContentType::Text)
			{
				Pasteboard.string = Value;
			}
			else
			{
				NSURL* URL = [NSURL URLWithString:Value];
				if (!URL)
				{
					return MakeFailure();
				}
				Pasteboard.URL = URL;
			}
			FOpenMobileClipboardOperationResult Result;
			Result.State = EOpenMobileClipboardOperationState::Succeeded;
			Result.Content.bContentTypesAvailable = true;
			Result.Content.ContentTypes.Add(Request.ContentType);
			return Result;
	}
}

FOpenMobileClipboardOperationResult ReadOpenMobileDeviceIOSClipboard(
	EOpenMobileClipboardContentType ContentType
)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSClipboardPrivate;
	if (!IsApplicationActive())
	{
		return MakeUnavailable();
	}
	@autoreleasepool
	{
			UIPasteboard* Pasteboard = [UIPasteboard generalPasteboard];
			FOpenMobileClipboardOperationResult Result;
			if (Pasteboard.numberOfItems == 0)
			{
				Result.State = EOpenMobileClipboardOperationState::Empty;
				Result.Content.bContentTypesAvailable = true;
				Result.Content.ContentTypes.Add(
					EOpenMobileClipboardContentType::Empty
				);
				return Result;
			}
			const bool bTypeAvailable =
				ContentType == EOpenMobileClipboardContentType::Text
					? Pasteboard.hasStrings
					: Pasteboard.hasURLs;
			if (!bTypeAvailable)
			{
				Result.State =
					EOpenMobileClipboardOperationState::TypeUnavailable;
				Result.Content.bContentTypesAvailable = true;
				return Result;
			}
			if (ContentType == EOpenMobileClipboardContentType::Text)
			{
				NSString* Value = Pasteboard.string;
				if (!Value)
				{
					Result.State = EOpenMobileClipboardOperationState::Denied;
					Result.Error = FOpenMobileError::Make(
						EOpenMobileErrorCode::NativeFailure,
						TEXT("iOS did not grant access to clipboard text."),
						FString(),
						TEXT("IOS")
					);
					return Result;
				}
				Result.Content.Text =
					FOpenMobileDeviceOptionalString::MakeAvailable(FString(Value));
			}
			else
			{
				NSURL* URL = Pasteboard.URL;
				if (!URL)
				{
					Result.State = EOpenMobileClipboardOperationState::Denied;
					Result.Error = FOpenMobileError::Make(
						EOpenMobileErrorCode::NativeFailure,
						TEXT("iOS did not grant access to the clipboard URL."),
						FString(),
						TEXT("IOS")
					);
					return Result;
				}
				Result.Content.Url =
					FOpenMobileDeviceOptionalString::MakeAvailable(
						FString(URL.absoluteString)
					);
			}
			Result.State = EOpenMobileClipboardOperationState::Succeeded;
			Result.Content.bContentTypesAvailable = true;
			Result.Content.ContentTypes.Add(ContentType);
			return Result;
	}
}

FOpenMobileClipboardOperationResult ClearOpenMobileDeviceIOSClipboard()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceIOSClipboardPrivate;
	if (!IsApplicationActive())
	{
		return MakeUnavailable();
	}
	@autoreleasepool
	{
			UIPasteboard* Pasteboard = [UIPasteboard generalPasteboard];
			Pasteboard.items = @[];
			if (Pasteboard.numberOfItems != 0)
			{
				return MakeFailure();
			}
			FOpenMobileClipboardOperationResult Result;
			Result.State = EOpenMobileClipboardOperationState::Succeeded;
			Result.Content.bContentTypesAvailable = true;
			Result.Content.ContentTypes.Add(
				EOpenMobileClipboardContentType::Empty
			);
			return Result;
	}
}
