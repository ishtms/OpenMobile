#include "OpenMobileDeviceAndroidUserInitiatedPaste.h"

#include "OpenMobileDeviceAndroidClipboard.h"

bool BeginOpenMobileDeviceAndroidUserInitiatedPaste(
	const FOpenMobileUserInitiatedPasteRequest& Request,
	const FGuid& OperationId,
	FOpenMobileDeviceUserInitiatedPasteCompletion&& Completion,
	FOpenMobileError& OutError
)
{
	static_cast<void>(OperationId);
	OutError = {};
	const FOpenMobileClipboardOperationResult ClipboardResult =
		ReadOpenMobileDeviceAndroidClipboard(Request.ContentType);
	FOpenMobileUserInitiatedPasteResult Result;
	Result.Content = ClipboardResult.Content;
	Result.Error = ClipboardResult.Error;
	switch (ClipboardResult.State)
	{
	case EOpenMobileClipboardOperationState::Succeeded:
		Result.State = EOpenMobileUserInitiatedPasteState::Success;
		break;
	case EOpenMobileClipboardOperationState::Denied:
		Result.State = EOpenMobileUserInitiatedPasteState::Denied;
		break;
	case EOpenMobileClipboardOperationState::Unknown:
	case EOpenMobileClipboardOperationState::Empty:
	case EOpenMobileClipboardOperationState::TypeUnavailable:
	case EOpenMobileClipboardOperationState::Unavailable:
	case EOpenMobileClipboardOperationState::Unsupported:
	case EOpenMobileClipboardOperationState::InvalidValue:
	case EOpenMobileClipboardOperationState::TooLarge:
	case EOpenMobileClipboardOperationState::Failed:
		Result.State = EOpenMobileUserInitiatedPasteState::Failed;
		if (!Result.Error.IsSet())
		{
			Result.Error = FOpenMobileError::Make(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The requested clipboard content is unavailable.")
			);
		}
		break;
	}
	Completion(MoveTemp(Result));
	return true;
}

void CancelOpenMobileDeviceAndroidUserInitiatedPaste(
	const FGuid& OperationId
)
{
	static_cast<void>(OperationId);
}
