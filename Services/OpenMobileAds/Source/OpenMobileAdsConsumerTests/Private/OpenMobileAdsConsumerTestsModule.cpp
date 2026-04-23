#include "IOpenMobileAdsProvider.h"
#include "Misc/AutomationTest.h"
#include "Modules/ModuleManager.h"
#include "OpenMobileAdsAsyncAction.h"
#include "OpenMobileAdsCapabilities.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsDiagnostics.h"
#include "OpenMobileAdsErrors.h"
#include "OpenMobileAdsEvents.h"
#include "OpenMobileAdsInitialization.h"
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
	FOpenMobileAdsInitializationRequest InitializationRequest;
	InitializationRequest.RequestId = FGuid::NewGuid();
	InitializationRequest.RequestConfiguration.MaxAdContentRating =
		EOpenMobileAdsMaxAdContentRating::General;
	FOpenMobileAdsConsentRequest ConsentRequest;
	ConsentRequest.RequestId = FGuid::NewGuid();

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
	TestTrue(TEXT("Public initialization requests use Unreal GUIDs"), InitializationRequest.RequestId.IsValid());
	TestTrue(TEXT("Public consent requests use Unreal GUIDs"), ConsentRequest.RequestId.IsValid());
	TestTrue(TEXT("Unknown public errors stay typed"), Error.IsSet());
	UClass* SubsystemClass = UOpenMobileAdsSubsystem::StaticClass();
	TestNotNull(TEXT("Subsystem type is public"), SubsystemClass);
	TestNotNull(TEXT("Initialization is callable from Blueprint"), SubsystemClass->FindFunctionByName(TEXT("InitializeAds")));
	TestNotNull(TEXT("Consent refresh is callable from Blueprint"), SubsystemClass->FindFunctionByName(TEXT("RefreshConsent")));
	TestNotNull(TEXT("Service state is readable from Blueprint"), SubsystemClass->FindFunctionByName(TEXT("GetServiceState")));
	TestNotNull(TEXT("Initialization status is readable from Blueprint"), SubsystemClass->FindFunctionByName(TEXT("GetInitializationStatus")));
	TestNotNull(TEXT("Initialization changes are exposed to Blueprint"), SubsystemClass->FindPropertyByName(TEXT("OnInitializationStatusChanged")));
	TestNotNull(TEXT("Async action type is public"), UOpenMobileAdsAsyncAction::StaticClass());
	return true;
}

#endif
