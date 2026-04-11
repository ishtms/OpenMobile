#include "IOpenMobileAdsProvider.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsAsyncAction.h"
#include "OpenMobileAdsCapabilities.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsDiagnostics.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsOperations.h"
#include "OpenMobileAdsPrivacy.h"
#include "OpenMobileAdsResults.h"
#include "OpenMobileAdsRevenue.h"
#include "OpenMobileAdsSubsystem.h"
#include "OpenMobileAdsTypes.h"

class FOpenMobileAdsConsumerTestsModule final : public IModuleInterface
{
};

IMPLEMENT_MODULE(FOpenMobileAdsConsumerTestsModule, OpenMobileAdsConsumerTests)

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsPublicConsumerCompileTest,
	"OpenMobile.Ads.Consumer.PublicHeaders",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsPublicConsumerCompileTest::RunTest(const FString& Parameters)
{
	const FName Placement(TEXT("ConsumerReward"));
	FOpenMobileAdsLoadRequest LoadRequest;
	LoadRequest.RequestId = FGuid::NewGuid();
	LoadRequest.Placement.Placement = Placement;
	LoadRequest.Placement.Format = EOpenMobileAdFormat::Rewarded;

	FOpenMobileAdsEvent Event;
	Event.Type = EOpenMobileAdsEventType::LoadStarted;
	Event.Placement = Placement;
	Event.RequestId = LoadRequest.RequestId;

	FOpenMobileAdsErrorMappingContext ErrorContext;
	ErrorContext.Domain = EOpenMobileAdsErrorDomain::Provider;
	ErrorContext.Stage = EOpenMobileAdsFailureStage::Load;
	ErrorContext.Placement = Placement;
	ErrorContext.NativeCode = TEXT("future_code");
	const FOpenMobileAdsError Error = FOpenMobileAdsErrorMapper::FromNative(ErrorContext);

	TestEqual(TEXT("Public operations retain placement names"), Event.Placement, Placement);
	TestTrue(TEXT("Public request IDs use Unreal GUIDs"), LoadRequest.RequestId.IsValid());
	TestTrue(TEXT("Unknown public errors stay typed"), Error.IsSet());
	TestNotNull(TEXT("Subsystem type is public"), UOpenMobileAdsSubsystem::StaticClass());
	TestNotNull(TEXT("Async action type is public"), UOpenMobileAdsAsyncAction::StaticClass());
	return true;
}

#endif
