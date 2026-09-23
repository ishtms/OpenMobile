#include "OpenMobileDeviceEndpointTransport.h"

#import <Foundation/Foundation.h>

@interface OpenMobileEndpointDelegate : NSObject <NSURLSessionDataDelegate>
{
@public
	FOpenMobileEndpointReachabilityOptions Options;
	FOpenMobileDeviceEndpointResponse Response;
	int32 RedirectCount;
	dispatch_semaphore_t Invalidated;
}
@end

@implementation OpenMobileEndpointDelegate
- (void)URLSession:(NSURLSession*)Session task:(NSURLSessionTask*)Task
	willPerformHTTPRedirection:(NSHTTPURLResponse*)Redirect
	newRequest:(NSURLRequest*)Request completionHandler:(void (^)(NSURLRequest*))Completion
{
	Response.Evidence.bRedirected = true;
	const FString Url(UTF8_TO_TCHAR(Request.URL.absoluteString.UTF8String));
	if (!Options.bAllowRedirects || ++RedirectCount > 5
		|| !FOpenMobileDeviceEndpointReachabilityPolicy::IsValidEndpoint(Url)
		|| Request.URL.user != nil || Request.URL.password != nil)
	{
		Response.Evidence.bRedirectRejected = true;
		Response.Evidence.StatusCode = static_cast<int32>(Redirect.statusCode);
		Response.Evidence.bResponseReceived = true;
		Completion(nil);
		return;
	}
	Completion(Request);
}

- (void)URLSession:(NSURLSession*)Session dataTask:(NSURLSessionDataTask*)Task
	didReceiveResponse:(NSURLResponse*)Received completionHandler:(void (^)(NSURLSessionResponseDisposition))Completion
{
	if ([Received isKindOfClass:[NSHTTPURLResponse class]])
	{
		Response.Evidence.StatusCode = static_cast<int32>(((NSHTTPURLResponse*)Received).statusCode);
		Response.Evidence.bResponseReceived = true;
	}
	Completion(NSURLSessionResponseAllow);
}

- (void)URLSession:(NSURLSession*)Session dataTask:(NSURLSessionDataTask*)Task didReceiveData:(NSData*)Data
{
	if (Data.length > static_cast<uint64>(Options.MaximumResponseBytes - Response.ResponseBytes))
	{
		Response.Evidence.bResponseTooLarge = true;
		[Task cancel];
		return;
	}
	Response.ResponseBytes += Data.length;
}

- (void)URLSession:(NSURLSession*)Session task:(NSURLSessionTask*)Task didCompleteWithError:(NSError*)Error
{
	if (Error && !Response.Evidence.bRedirectRejected && !Response.Evidence.bResponseTooLarge)
	{
		auto& Failure = Response.Evidence.TransportFailure;
		Failure = EOpenMobileDeviceEndpointTransportFailure::Other;
		if ([Error.domain isEqualToString:NSURLErrorDomain])
		{
			switch (Error.code)
			{
			case NSURLErrorCannotFindHost: case NSURLErrorDNSLookupFailed:
				Failure = EOpenMobileDeviceEndpointTransportFailure::Dns; break;
			case NSURLErrorCannotConnectToHost: case NSURLErrorNetworkConnectionLost: case NSURLErrorNotConnectedToInternet:
				Failure = EOpenMobileDeviceEndpointTransportFailure::Connection; break;
			case NSURLErrorTimedOut:
				Failure = EOpenMobileDeviceEndpointTransportFailure::Timeout; break;
			case NSURLErrorCancelled:
				Failure = EOpenMobileDeviceEndpointTransportFailure::Cancelled; break;
			case NSURLErrorSecureConnectionFailed: case NSURLErrorServerCertificateHasBadDate:
			case NSURLErrorServerCertificateUntrusted: case NSURLErrorServerCertificateHasUnknownRoot:
			case NSURLErrorServerCertificateNotYetValid: case NSURLErrorClientCertificateRejected:
			case NSURLErrorClientCertificateRequired:
				Failure = EOpenMobileDeviceEndpointTransportFailure::Tls; break;
			default: break;
			}
		}
	}
	[Session finishTasksAndInvalidate];
}

- (void)URLSession:(NSURLSession*)Session didBecomeInvalidWithError:(NSError*)Error
{
	dispatch_semaphore_signal(Invalidated);
}
@end

FOpenMobileDeviceEndpointResponse OpenMobileQueryIOSEndpoint(const FString& Endpoint,
	const FOpenMobileEndpointReachabilityOptions& Options, const TAtomic<bool>& Cancellation)
{
	@autoreleasepool
	{
		NSURL* Url = [NSURL URLWithString:[NSString stringWithUTF8String:TCHAR_TO_UTF8(*Endpoint)]];
		if (!Url || !Url.host.length)
		{
			FOpenMobileDeviceEndpointResponse Result;
			Result.Evidence.TransportFailure = EOpenMobileDeviceEndpointTransportFailure::Other;
			return Result;
		}
		OpenMobileEndpointDelegate* Delegate = [[OpenMobileEndpointDelegate alloc] init];
		Delegate->Options = Options;
		Delegate->RedirectCount = 0;
		Delegate->Invalidated = dispatch_semaphore_create(0);
		NSURLSessionConfiguration* Configuration = [NSURLSessionConfiguration ephemeralSessionConfiguration];
		Configuration.HTTPCookieStorage = nil;
		Configuration.HTTPShouldSetCookies = NO;
		Configuration.URLCredentialStorage = nil;
		Configuration.URLCache = nil;
		Configuration.requestCachePolicy = NSURLRequestReloadIgnoringLocalCacheData;
		Configuration.timeoutIntervalForRequest = Options.TimeoutSeconds;
		Configuration.timeoutIntervalForResource = Options.TimeoutSeconds;
		NSOperationQueue* Queue = [[NSOperationQueue alloc] init];
		Queue.maxConcurrentOperationCount = 1;
		NSURLSession* Session = [NSURLSession sessionWithConfiguration:Configuration delegate:Delegate delegateQueue:Queue];
		NSURLSessionDataTask* Task = [Session dataTaskWithURL:Url];
		[Task resume];
		while (dispatch_semaphore_wait(Delegate->Invalidated,
			dispatch_time(DISPATCH_TIME_NOW, 50 * NSEC_PER_MSEC)) != 0)
		{
			if (Cancellation.Load()) [Task cancel];
		}
		// Invalidation is the last delegate call, so no callback can touch the result now.
		const FOpenMobileDeviceEndpointResponse Result = Delegate->Response;
		dispatch_release(Delegate->Invalidated);
		[Queue release];
		[Delegate release];
		return Result;
	}
}
