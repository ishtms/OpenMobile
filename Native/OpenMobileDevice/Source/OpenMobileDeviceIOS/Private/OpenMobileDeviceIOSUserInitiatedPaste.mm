#include "OpenMobileDeviceIOSUserInitiatedPaste.h"

#include "IOS/IOSAppDelegate.h"
#include "IOS/IOSView.h"
#include "OpenMobileDeviceIOSClipboard.h"

#import <UIKit/UIKit.h>

namespace OpenMobileDeviceIOSUserInitiatedPastePrivate
{
	FGuid ActiveOperationId;
	EOpenMobileClipboardContentType ActiveContentType =
		EOpenMobileClipboardContentType::Unknown;
	FOpenMobileDeviceUserInitiatedPasteCompletion ActiveCompletion;
	UIView* ActiveOverlay = nil;
	UIPasteControl* ActivePasteControl = nil;
	NSObject<UIPasteConfigurationSupporting>* ActiveTarget = nil;
	NSProgress* ActiveProgress = nil;

	void HandlePasteItemProviders(NSArray<NSItemProvider*>* ItemProviders);
	void HandleUserCancelled();

	void RunOnMainThread(dispatch_block_t Block)
	{
		if ([NSThread isMainThread])
		{
			Block();
		}
		else
		{
			dispatch_sync(dispatch_get_main_queue(), Block);
		}
	}

	FOpenMobileUserInitiatedPasteResult MakeFailure(
		EOpenMobileErrorCode ErrorCode,
		FString Message,
		FString NativeCode = FString()
	)
	{
		FOpenMobileUserInitiatedPasteResult Result;
		Result.State = EOpenMobileUserInitiatedPasteState::Failed;
		Result.Error = FOpenMobileError::Make(
			ErrorCode,
			MoveTemp(Message),
			MoveTemp(NativeCode),
			TEXT("IOS")
		);
		return Result;
	}

	void Cleanup(bool bCancelProgress)
	{
		if (ActiveProgress)
		{
			if (bCancelProgress)
			{
				[ActiveProgress cancel];
			}
			[ActiveProgress release];
			ActiveProgress = nil;
		}
		if (ActivePasteControl)
		{
			ActivePasteControl.target = nil;
		}
		if (ActiveOverlay)
		{
			[ActiveOverlay removeFromSuperview];
		}
		[ActivePasteControl release];
		ActivePasteControl = nil;
		[ActiveTarget release];
		ActiveTarget = nil;
		[ActiveOverlay release];
		ActiveOverlay = nil;
		ActiveOperationId.Invalidate();
		ActiveContentType = EOpenMobileClipboardContentType::Unknown;
		ActiveCompletion = {};
	}

	void Complete(
		const FGuid& OperationId,
		FOpenMobileUserInitiatedPasteResult Result
	)
	{
		if (OperationId != ActiveOperationId)
		{
			return;
		}
		FOpenMobileDeviceUserInitiatedPasteCompletion Completion =
			MoveTemp(ActiveCompletion);
		Cleanup(false);
		if (Completion)
		{
			Completion(MoveTemp(Result));
		}
	}

	void CancelAndComplete(const FString& Message)
	{
		if (!ActiveOperationId.IsValid())
		{
			return;
		}
		FOpenMobileDeviceUserInitiatedPasteCompletion Completion =
			MoveTemp(ActiveCompletion);
		FOpenMobileUserInitiatedPasteResult Result;
		Result.State = EOpenMobileUserInitiatedPasteState::Cancelled;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Cancelled,
			Message,
			FString(),
			TEXT("IOS")
		);
		Cleanup(true);
		if (Completion)
		{
			Completion(MoveTemp(Result));
		}
	}

	FOpenMobileUserInitiatedPasteResult FromDirectClipboardResult(
		const FOpenMobileClipboardOperationResult& ClipboardResult
	)
	{
		FOpenMobileUserInitiatedPasteResult Result;
		Result.Content = ClipboardResult.Content;
		Result.Error = ClipboardResult.Error;
		if (ClipboardResult.State
			== EOpenMobileClipboardOperationState::Succeeded)
		{
			Result.State = EOpenMobileUserInitiatedPasteState::Success;
		}
		else if (ClipboardResult.State
			== EOpenMobileClipboardOperationState::Denied)
		{
			Result.State = EOpenMobileUserInitiatedPasteState::Denied;
		}
		else
		{
			Result.State = EOpenMobileUserInitiatedPasteState::Failed;
			if (!Result.Error.IsSet())
			{
				Result.Error = FOpenMobileError::Make(
					EOpenMobileErrorCode::Unavailable,
					TEXT("The requested pasteboard content is unavailable."),
					FString(),
					TEXT("IOS")
				);
			}
		}
		return Result;
	}
}

@interface OpenMobileDevicePasteTarget : NSObject
	<UIPasteConfigurationSupporting>
{
	UIPasteConfiguration* _pasteConfiguration;
}
@property(nonatomic, copy) UIPasteConfiguration* pasteConfiguration;
- (void)cancelPaste:(id)sender;
@end

@implementation OpenMobileDevicePasteTarget

@synthesize pasteConfiguration = _pasteConfiguration;

- (void)dealloc
{
	[_pasteConfiguration release];
	[super dealloc];
}

- (BOOL)canPasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders
{
	Class ExpectedClass =
		OpenMobileDeviceIOSUserInitiatedPastePrivate::ActiveContentType
			== EOpenMobileClipboardContentType::Text
			? [NSString class]
			: [NSURL class];
	for (NSItemProvider* Provider in itemProviders)
	{
		if ([Provider canLoadObjectOfClass:ExpectedClass])
		{
			return YES;
		}
	}
	return NO;
}

- (void)pasteItemProviders:(NSArray<NSItemProvider*>*)itemProviders
{
	OpenMobileDeviceIOSUserInitiatedPastePrivate::HandlePasteItemProviders(
		itemProviders
	);
}

- (void)cancelPaste:(id)sender
{
	static_cast<void>(sender);
	OpenMobileDeviceIOSUserInitiatedPastePrivate::HandleUserCancelled();
}

@end

namespace OpenMobileDeviceIOSUserInitiatedPastePrivate
{
	void HandlePasteItemProviders(NSArray<NSItemProvider*>* ItemProviders)
	{
		check([NSThread isMainThread]);
		if (!ActiveOperationId.IsValid() || ActiveProgress)
		{
			return;
		}
		ActivePasteControl.enabled = NO;
		Class ExpectedClass =
			ActiveContentType == EOpenMobileClipboardContentType::Text
				? [NSString class]
				: [NSURL class];
		NSItemProvider* SelectedProvider = nil;
		for (NSItemProvider* Provider in ItemProviders)
		{
			if ([Provider canLoadObjectOfClass:ExpectedClass])
			{
				SelectedProvider = Provider;
				break;
			}
		}
		if (!SelectedProvider)
		{
			Complete(
				ActiveOperationId,
				MakeFailure(
					EOpenMobileErrorCode::Unavailable,
					TEXT("The system Paste control supplied no compatible item.")
				)
			);
			return;
		}
		const FGuid OperationId = ActiveOperationId;
		const EOpenMobileClipboardContentType ContentType = ActiveContentType;
		NSProgress* Progress = [SelectedProvider
			loadObjectOfClass:ExpectedClass
			completionHandler:^(id<NSItemProviderReading> Object, NSError* Error)
			{
				dispatch_async(dispatch_get_main_queue(), ^{
					if (OperationId != ActiveOperationId)
					{
						return;
					}
					if (Error || !Object)
					{
						FOpenMobileUserInitiatedPasteResult Result;
						if (Error.code == NSUserCancelledError)
						{
							Result.State =
								EOpenMobileUserInitiatedPasteState::Cancelled;
							Result.Error = FOpenMobileError::Make(
								EOpenMobileErrorCode::Cancelled,
								TEXT("The native Paste operation was cancelled."),
								FString(),
								TEXT("IOS")
							);
						}
						else
						{
							Result = MakeFailure(
								EOpenMobileErrorCode::NativeFailure,
								TEXT("The native Paste control could not load its item."),
								Error ? FString::FromInt(Error.code) : FString()
							);
						}
						Complete(OperationId, MoveTemp(Result));
						return;
					}
					FOpenMobileUserInitiatedPasteResult Result;
					Result.State = EOpenMobileUserInitiatedPasteState::Success;
					Result.Content.bContentTypesAvailable = true;
					Result.Content.ContentTypes.Add(ContentType);
					if (ContentType == EOpenMobileClipboardContentType::Text)
					{
						NSString* StringValue = (NSString*)Object;
						Result.Content.Text =
							FOpenMobileDeviceOptionalString::MakeAvailable(
								FString(StringValue)
							);
					}
					else
					{
						NSURL* URLValue = (NSURL*)Object;
						Result.Content.Url =
							FOpenMobileDeviceOptionalString::MakeAvailable(
								FString(URLValue.absoluteString)
							);
					}
					Complete(OperationId, MoveTemp(Result));
				});
			}];
		ActiveProgress = [Progress retain];
	}

	void HandleUserCancelled()
	{
		check([NSThread isMainThread]);
		if (!ActiveOperationId.IsValid())
		{
			return;
		}
		FOpenMobileUserInitiatedPasteResult Result;
		Result.State = EOpenMobileUserInitiatedPasteState::Cancelled;
		Result.Error = FOpenMobileError::Make(
			EOpenMobileErrorCode::Cancelled,
			TEXT("The native Paste control was cancelled."),
			FString(),
			TEXT("IOS")
		);
		Complete(ActiveOperationId, MoveTemp(Result));
	}

	bool PresentPasteControl(
		const FOpenMobileUserInitiatedPasteRequest& Request,
		const FGuid& OperationId,
		FOpenMobileDeviceUserInitiatedPasteCompletion&& Completion,
		FOpenMobileError& OutError
	)
	{
		if (ActiveOperationId.IsValid())
		{
			OutError = FOpenMobileError::Make(
				EOpenMobileErrorCode::Busy,
				TEXT("An iOS Paste control is already active."),
				FString(),
				TEXT("IOS")
			);
			return false;
		}
		IOSAppDelegate* AppDelegate = [IOSAppDelegate GetDelegate];
		FIOSView* View = AppDelegate.IOSView;
		UIApplication* Application = [UIApplication sharedApplication];
		if (!View || !View.window
			|| Application.applicationState != UIApplicationStateActive)
		{
			OutError = FOpenMobileError::Make(
				EOpenMobileErrorCode::Unavailable,
				TEXT("The active iOS view is unavailable for the Paste control."),
				FString(),
				TEXT("IOS")
			);
			return false;
		}

		ActiveOperationId = OperationId;
		ActiveContentType = Request.ContentType;
		ActiveCompletion = MoveTemp(Completion);
		ActiveTarget = [[OpenMobileDevicePasteTarget alloc] init];
		Class ExpectedClass = Request.ContentType
			== EOpenMobileClipboardContentType::Text
			? [NSString class]
			: [NSURL class];
		ActiveTarget.pasteConfiguration = [[[UIPasteConfiguration alloc]
			initWithTypeIdentifiersForAcceptingClass:ExpectedClass] autorelease];

		ActiveOverlay = [[UIView alloc] initWithFrame:View.bounds];
		ActiveOverlay.autoresizingMask = UIViewAutoresizingFlexibleWidth
			| UIViewAutoresizingFlexibleHeight;
		ActiveOverlay.backgroundColor =
			[UIColor colorWithWhite:0.0 alpha:0.55];
		ActiveOverlay.accessibilityViewIsModal = YES;

		UIPasteControlConfiguration* Configuration =
			[[[UIPasteControlConfiguration alloc] init] autorelease];
		Configuration.displayMode = UIPasteControlDisplayModeIconAndLabel;
		Configuration.cornerStyle = UIButtonConfigurationCornerStyleCapsule;
		Configuration.baseForegroundColor = [UIColor whiteColor];
		Configuration.baseBackgroundColor = [UIColor systemBlueColor];
		ActivePasteControl = [[UIPasteControl alloc]
			initWithConfiguration:Configuration];
		ActivePasteControl.target = ActiveTarget;
		ActivePasteControl.translatesAutoresizingMaskIntoConstraints = NO;
		[ActiveOverlay addSubview:ActivePasteControl];

		UIButton* CancelButton = [UIButton buttonWithType:UIButtonTypeSystem];
		[CancelButton setTitle:NSLocalizedString(@"Cancel", nil)
			forState:UIControlStateNormal];
		[CancelButton setTitleColor:[UIColor whiteColor]
			forState:UIControlStateNormal];
		CancelButton.translatesAutoresizingMaskIntoConstraints = NO;
		[CancelButton addTarget:ActiveTarget
			action:@selector(cancelPaste:)
			forControlEvents:UIControlEventTouchUpInside];
		[ActiveOverlay addSubview:CancelButton];

		[NSLayoutConstraint activateConstraints:@[
			[ActivePasteControl.centerXAnchor
				constraintEqualToAnchor:ActiveOverlay.centerXAnchor],
			[ActivePasteControl.centerYAnchor
				constraintEqualToAnchor:ActiveOverlay.centerYAnchor],
			[ActivePasteControl.widthAnchor constraintGreaterThanOrEqualToConstant:160.0],
			[ActivePasteControl.heightAnchor constraintGreaterThanOrEqualToConstant:56.0],
			[CancelButton.centerXAnchor
				constraintEqualToAnchor:ActiveOverlay.centerXAnchor],
			[CancelButton.topAnchor
				constraintEqualToAnchor:ActivePasteControl.bottomAnchor
				constant:20.0],
			[CancelButton.widthAnchor constraintGreaterThanOrEqualToConstant:100.0],
			[CancelButton.heightAnchor constraintGreaterThanOrEqualToConstant:44.0]
		]];
		[View addSubview:ActiveOverlay];
		return true;
	}
}

bool BeginOpenMobileDeviceIOSUserInitiatedPaste(
	const FOpenMobileUserInitiatedPasteRequest& Request,
	const FGuid& OperationId,
	FOpenMobileDeviceUserInitiatedPasteCompletion&& Completion,
	FOpenMobileError& OutError
)
{
	using namespace OpenMobileDeviceIOSUserInitiatedPastePrivate;
	check(IsInGameThread());
	OutError = {};
	if (@available(iOS 16.0, *))
	{
		__block bool bPresented = false;
		const FOpenMobileUserInitiatedPasteRequest* RequestPtr = &Request;
		const FGuid* OperationIdPtr = &OperationId;
		FOpenMobileDeviceUserInitiatedPasteCompletion* CompletionPtr =
			&Completion;
		FOpenMobileError* ErrorPtr = &OutError;
		RunOnMainThread(^
		{
			bPresented = PresentPasteControl(
				*RequestPtr,
				*OperationIdPtr,
				MoveTemp(*CompletionPtr),
				*ErrorPtr
			);
		});
		return bPresented;
	}
	Completion(FromDirectClipboardResult(
		ReadOpenMobileDeviceIOSClipboard(Request.ContentType)
	));
	return true;
}

void CancelOpenMobileDeviceIOSUserInitiatedPaste(
	const FGuid& OperationId
)
{
	using namespace OpenMobileDeviceIOSUserInitiatedPastePrivate;
	const FGuid* OperationIdPtr = &OperationId;
	RunOnMainThread(^
	{
		if (*OperationIdPtr == ActiveOperationId)
		{
			Cleanup(true);
		}
	});
}

void ShutdownOpenMobileDeviceIOSUserInitiatedPaste()
{
	using namespace OpenMobileDeviceIOSUserInitiatedPastePrivate;
	RunOnMainThread(^
	{
		CancelAndComplete(
			TEXT("The native Paste control was cancelled during iOS shutdown.")
		);
	});
}
