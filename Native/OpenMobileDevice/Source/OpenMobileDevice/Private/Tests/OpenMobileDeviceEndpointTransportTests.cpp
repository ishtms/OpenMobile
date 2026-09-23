#if WITH_DEV_AUTOMATION_TESTS
#include "Async/TaskGraphInterfaces.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "OpenMobileDeviceEndpointTransport.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FOpenMobileDeviceEndpointTransportTest,
	"OpenMobile.Device.EndpointReachability.HttpsTransport",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FOpenMobileDeviceEndpointTransportTest::RunTest(const FString&)
{
	FString Fixture;
	if (!FParse::Value(FCommandLine::Get(), TEXT("OpenMobileEndpointFixture="), Fixture))
	{
		AddInfo(TEXT("HTTPS integration requires -OpenMobileEndpointFixture=<fixture URL> and its trusted test CA."));
		return true;
	}
	struct FCompletionState { bool Done = false; FOpenMobileDeviceEndpointResponse Response; };
	const auto Probe = [&](const FString& Route, FOpenMobileEndpointReachabilityOptions Options)
	{
		auto State = MakeShared<FCompletionState>();
		auto Cancellation = MakeShared<TAtomic<bool>, ESPMode::ThreadSafe>(false);
		FOpenMobileDeviceEndpointTransport::Start(Fixture + Route, Options, Cancellation,
			[State](FOpenMobileDeviceEndpointResponse Response)
			{ State->Response = Response; State->Done = true; });
		const double Deadline = FPlatformTime::Seconds() + Options.TimeoutSeconds + 2.0;
		while (!State->Done && FPlatformTime::Seconds() < Deadline)
		{
			FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
			FPlatformProcess::Sleep(0.005f);
		}
		TestTrue(TEXT("Transport completes within its deadline"), State->Done);
		Cancellation->Store(true);
		return State->Response;
	};
	FOpenMobileEndpointReachabilityOptions Options;
	Options.TimeoutSeconds = 2.0f;
	using Outcome = EOpenMobileEndpointReachabilityOutcome;
	const auto Expect = [&](const TCHAR* Route, Outcome Expected)
	{
		const auto Response = Probe(Route, Options);
		TestEqual(Route, FOpenMobileDeviceEndpointReachabilityPolicy::Classify(Response.Evidence, Options), Expected);
		return Response;
	};
	Expect(TEXT("/ok"), Outcome::Succeeded);
	Expect(TEXT("/target-reset"), Outcome::Succeeded);
	Expect(TEXT("/redirect-target"), Outcome::RedirectRejected);
	TestEqual(TEXT("A rejected redirect never fetches its target"), Probe(TEXT("/target-status"), Options).Evidence.StatusCode, 204);
	Options.bAllowRedirects = true;
	Expect(TEXT("/redirect-target"), Outcome::Succeeded);
	TestEqual(TEXT("An allowed HTTPS redirect fetches its target"), Probe(TEXT("/target-status"), Options).Evidence.StatusCode, 409);
	Expect(TEXT("/redirect-http"), Outcome::RedirectRejected);
	Expect(TEXT("/redirect-credentials"), Outcome::RedirectRejected);
	Expect(TEXT("/redirect-loop"), Outcome::RedirectRejected);
	Options.MaximumResponseBytes = 1024;
	const auto Large = Expect(TEXT("/large?bytes=2048"), Outcome::ResponseTooLarge);
	TestTrue(TEXT("Response accounting stays bounded"), Large.ResponseBytes <= Options.MaximumResponseBytes);
	Expect(TEXT("/status/503"), Outcome::HttpResponseRejected);
	Options.TimeoutSeconds = 0.1f;
	Expect(TEXT("/slow?seconds=1"), Outcome::Timeout);
	FOpenMobileDeviceEndpointTransport::Shutdown();
	return true;
}
#endif
