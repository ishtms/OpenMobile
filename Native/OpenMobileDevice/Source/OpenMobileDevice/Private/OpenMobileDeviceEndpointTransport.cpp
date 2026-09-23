#include "OpenMobileDeviceEndpointTransport.h"

#include "Async/Async.h"
#include "Misc/ScopeRWLock.h"
#if OPENMOBILE_ENDPOINT_CURL
#include "HttpModule.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#endif
// OpenSSL's UI typedef collides with CoreUObject's UI namespace.
#pragma push_macro("UI")
#undef UI
#define UI OpenMobileOpenSslUI
#include "openssl/ssl.h"
#pragma pop_macro("UI")
#if PLATFORM_WINDOWS
#include "Windows/HideWindowsPlatformTypes.h"
#endif
#include "Ssl.h"
#include "curl/curl.h"
#endif

namespace OpenMobileDeviceEndpointTransportPrivate
{
	FRWLock PlatformLock;
	FOpenMobileDeviceEndpointTransport::FQuery PlatformQuery = nullptr;
	struct FWorker
	{
		TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> Cancellation;
		TFuture<void> Future;
	};
	TArray<FWorker> Workers; // Game-thread owned; workers only touch their captured state.

#if OPENMOBILE_ENDPOINT_CURL
	bool bCurlInitialized = false;
	struct FCurlState
	{
		const TAtomic<bool>& Cancellation;
		int64 MaximumBytes;
		FString Domain;
		FOpenMobileDeviceEndpointResponse Response;
	};

	size_t Receive(char*, size_t Size, size_t Count, void* UserData)
	{
		auto& State = *static_cast<FCurlState*>(UserData);
		const uint64 Remaining = State.MaximumBytes - State.Response.ResponseBytes;
		if (Size != 0 && Count > Remaining / Size)
		{
			State.Response.Evidence.bResponseTooLarge = true;
			return 0;
		}
		State.Response.ResponseBytes += Size * Count;
		return Size * Count;
	}

	int Progress(void* UserData, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
	{
		return static_cast<FCurlState*>(UserData)->Cancellation.Load() ? 1 : 0;
	}

	int VerifyCertificate(int Preverified, X509_STORE_CTX* Context)
	{
		if (!Preverified) return 0;
		SSL* Handle = static_cast<SSL*>(X509_STORE_CTX_get_ex_data(Context, SSL_get_ex_data_X509_STORE_CTX_idx()));
		auto* State = static_cast<FCurlState*>(SSL_CTX_get_app_data(SSL_get_SSL_CTX(Handle)));
		return FSslModule::Get().GetCertificateManager().VerifySslCertificates(Context, State->Domain) ? 1 : 0;
	}

	CURLcode ConfigureTls(CURL*, void* Context, void* UserData)
	{
		auto* SslContext = static_cast<SSL_CTX*>(Context);
		FSslModule::Get().GetCertificateManager().AddCertificatesToSslContext(SslContext);
		SSL_CTX_set_verify(SslContext, SSL_CTX_get_verify_mode(SslContext), VerifyCertificate);
		SSL_CTX_set_app_data(SslContext, UserData);
		return CURLE_OK;
	}

	FOpenMobileDeviceEndpointResponse QueryCurl(const FString& Endpoint,
		const FOpenMobileEndpointReachabilityOptions& Options, const TAtomic<bool>& Cancellation)
	{
		FCurlState State{Cancellation, Options.MaximumResponseBytes, {}, {}};
		CURL* Curl = curl_easy_init();
		if (!Curl)
		{
			State.Response.Evidence.TransportFailure = EOpenMobileDeviceEndpointTransportFailure::Other;
			return State.Response;
		}
		curl_easy_setopt(Curl, CURLOPT_PROTOCOLS_STR, "https");
		curl_easy_setopt(Curl, CURLOPT_REDIR_PROTOCOLS_STR, "https");
		curl_easy_setopt(Curl, CURLOPT_FOLLOWLOCATION, 0L);
		curl_easy_setopt(Curl, CURLOPT_DISALLOW_USERNAME_IN_URL, 1L);
		curl_easy_setopt(Curl, CURLOPT_TIMEOUT_MS, static_cast<long>(Options.TimeoutSeconds * 1000));
		curl_easy_setopt(Curl, CURLOPT_NOSIGNAL, 1L);
		curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYPEER, 1L);
		curl_easy_setopt(Curl, CURLOPT_SSL_VERIFYHOST, 2L);
		// Unreal supplies the trust store in ConfigureTls, including project CAs.
		curl_easy_setopt(Curl, CURLOPT_CAINFO, nullptr);
		curl_easy_setopt(Curl, CURLOPT_SSL_CTX_FUNCTION, ConfigureTls);
		curl_easy_setopt(Curl, CURLOPT_SSL_CTX_DATA, &State);
		curl_easy_setopt(Curl, CURLOPT_WRITEFUNCTION, Receive);
		curl_easy_setopt(Curl, CURLOPT_WRITEDATA, &State);
		curl_easy_setopt(Curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(Curl, CURLOPT_XFERINFOFUNCTION, Progress);
		curl_easy_setopt(Curl, CURLOPT_XFERINFODATA, &State);
		CURLcode Status = CURLE_OK;
		auto& Evidence = State.Response.Evidence;
		FString CurrentUrl = Endpoint;
		const double Deadline = FPlatformTime::Seconds() + Options.TimeoutSeconds;
		for (int32 Hop = 0; ; ++Hop)
		{
			const double Remaining = Deadline - FPlatformTime::Seconds();
			if (Cancellation.Load()) { Status = CURLE_ABORTED_BY_CALLBACK; break; }
			if (Remaining <= 0.0) { Status = CURLE_OPERATION_TIMEDOUT; break; }
			FTCHARToUTF8 Url(*CurrentUrl);
			State.Domain = FGenericPlatformHttp::GetUrlDomain(CurrentUrl);
			curl_easy_setopt(Curl, CURLOPT_URL, Url.Get());
			curl_easy_setopt(Curl, CURLOPT_TIMEOUT_MS, FMath::Max(1L, static_cast<long>(Remaining * 1000)));
			Status = curl_easy_perform(Curl);
			long HttpStatus = 0;
			curl_easy_getinfo(Curl, CURLINFO_RESPONSE_CODE, &HttpStatus);
			Evidence.StatusCode = static_cast<int32>(HttpStatus);
			Evidence.bResponseReceived = HttpStatus > 0;
			char* RedirectUrl = nullptr;
			curl_easy_getinfo(Curl, CURLINFO_REDIRECT_URL, &RedirectUrl);
			if (!RedirectUrl) break;
			Evidence.bRedirected = true;
			CurrentUrl = UTF8_TO_TCHAR(RedirectUrl);
			if (!Options.bAllowRedirects || Hop >= 5
				|| !FOpenMobileDeviceEndpointReachabilityPolicy::IsValidEndpoint(CurrentUrl))
			{
				Evidence.bRedirectRejected = true;
				break;
			}
			if (Status != CURLE_OK) break;
		}
		switch (Status)
		{
		case CURLE_OK: break;
		case CURLE_COULDNT_RESOLVE_HOST:
			Evidence.TransportFailure = EOpenMobileDeviceEndpointTransportFailure::Dns; break;
		case CURLE_COULDNT_CONNECT:
			Evidence.TransportFailure = EOpenMobileDeviceEndpointTransportFailure::Connection; break;
		case CURLE_OPERATION_TIMEDOUT:
			Evidence.TransportFailure = EOpenMobileDeviceEndpointTransportFailure::Timeout; break;
		case CURLE_ABORTED_BY_CALLBACK:
			Evidence.TransportFailure = EOpenMobileDeviceEndpointTransportFailure::Cancelled; break;
		case CURLE_SSL_CONNECT_ERROR:
		case CURLE_PEER_FAILED_VERIFICATION:
		case CURLE_SSL_CERTPROBLEM:
		case CURLE_SSL_CACERT_BADFILE:
			Evidence.TransportFailure = EOpenMobileDeviceEndpointTransportFailure::Tls; break;
		default:
			Evidence.TransportFailure = EOpenMobileDeviceEndpointTransportFailure::Other; break;
		}
		curl_easy_cleanup(Curl);
		return State.Response;
	}
#endif
}

void FOpenMobileDeviceEndpointTransport::Start(const FString& Endpoint,
	FOpenMobileEndpointReachabilityOptions Options,
	TSharedRef<TAtomic<bool>, ESPMode::ThreadSafe> Cancellation, FCompletion Completion)
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceEndpointTransportPrivate;
#if OPENMOBILE_ENDPOINT_CURL
	// Initialize curl and the certificate store before entering the worker pool.
	FHttpModule::Get();
	FSslModule::Get();
	if (!bCurlInitialized)
	{
		if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
		{
			FOpenMobileDeviceEndpointResponse Response;
			Response.Evidence.TransportFailure = EOpenMobileDeviceEndpointTransportFailure::Other;
			Completion(Response);
			return;
		}
		bCurlInitialized = true;
	}
#endif
	Workers.RemoveAll([](const FWorker& Worker) { return Worker.Future.IsReady(); });
	auto Future = Async(EAsyncExecution::ThreadPool,
		[Endpoint, Options, Cancellation, Completion = MoveTemp(Completion)]() mutable
	{
		FOpenMobileDeviceEndpointResponse Response;
		{
			FReadScopeLock Lock(PlatformLock);
			if (Cancellation->Load()) return;
			if (PlatformQuery)
			{
				Response = PlatformQuery(Endpoint, Options, *Cancellation);
			}
#if OPENMOBILE_ENDPOINT_CURL
			else
			{
				Response = QueryCurl(Endpoint, Options, *Cancellation);
			}
#endif
		}
		AsyncTask(ENamedThreads::GameThread,
			[Cancellation, Completion = MoveTemp(Completion), Response]() mutable
		{
			if (!Cancellation->Load()) Completion(Response);
		});
	});
	Workers.Add({Cancellation, MoveTemp(Future)});
}

void FOpenMobileDeviceEndpointTransport::SetPlatformQuery(FQuery Query)
{
	check(IsInGameThread());
	FWriteScopeLock Lock(OpenMobileDeviceEndpointTransportPrivate::PlatformLock);
	OpenMobileDeviceEndpointTransportPrivate::PlatformQuery = Query;
}

void FOpenMobileDeviceEndpointTransport::Shutdown()
{
	check(IsInGameThread());
	using namespace OpenMobileDeviceEndpointTransportPrivate;
	for (FWorker& Worker : Workers) Worker.Cancellation->Store(true);
	for (FWorker& Worker : Workers) Worker.Future.Wait();
	Workers.Reset();
#if OPENMOBILE_ENDPOINT_CURL
	if (bCurlInitialized)
	{
		curl_global_cleanup();
		bCurlInitialized = false;
	}
#endif
}
