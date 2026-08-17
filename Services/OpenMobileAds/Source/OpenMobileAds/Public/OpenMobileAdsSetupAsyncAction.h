#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "OpenMobileAdsInitialization.h"
#include "OpenMobileAdsPrivacy.h"
#include "OpenMobileAdsResults.h"
#include "OpenMobileAdsSetupAsyncAction.generated.h"

class UOpenMobileAdsSubsystem;
class UWorld;
#if WITH_DEV_AUTOMATION_TESTS
class FOpenMobileAdsSetupAsyncContractTest;
#endif

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsSetupResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Ads|Setup")
	FGuid RequestId;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Ads|Setup")
	FOpenMobileAdsError Error;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Ads|Setup")
	EOpenMobileAdsServiceState ServiceState =
		EOpenMobileAdsServiceState::Uninitialized;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Ads|Setup")
	FOpenMobileAdsInitializationStatusSnapshot Initialization;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Ads|Setup")
	FOpenMobileAdsPrivacySnapshot Privacy;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Ads|Setup")
	EOpenMobileAdsTrackingAuthorizationStatus TrackingAuthorization =
		EOpenMobileAdsTrackingAuthorizationStatus::Unsupported;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Ads|Setup")
	FOpenMobileAdsCanRequestAdsResult RequestEligibility;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Ads|Setup")
	FName ActiveProvider;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsSetupCompleted,
	const FOpenMobileAdsSetupResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsSetupFailed,
	const FOpenMobileAdsSetupResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsSetupCancelled,
	const FOpenMobileAdsSetupResult&,
	Result
);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILEADS_API UOpenMobileAdsSetupAsyncAction final :
	public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Setup")
	FOpenMobileAdsSetupCompleted OnCompleted;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Setup")
	FOpenMobileAdsSetupFailed OnFailed;

	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Ads|Setup")
	FOpenMobileAdsSetupCancelled OnCancelled;

	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Ads|Consent",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Refresh Ads Consent Async"
		)
	)
	static UOpenMobileAdsSetupAsyncAction* RefreshAdsConsent(
		const UObject* WorldContextObject
	);

	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Ads|Consent",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Present Ads Privacy Options Async"
		)
	)
	static UOpenMobileAdsSetupAsyncAction* PresentAdsPrivacyOptions(
		const UObject* WorldContextObject
	);

	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Ads|Consent",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Request iOS Tracking Authorization Async"
		)
	)
	static UOpenMobileAdsSetupAsyncAction* RequestTrackingAuthorization(
		const UObject* WorldContextObject
	);

	UFUNCTION(
		BlueprintCallable,
		Category = "OpenMobile|Ads|Setup",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Initialize Ads Async"
		)
	)
	static UOpenMobileAdsSetupAsyncAction* InitializeAds(
		const UObject* WorldContextObject
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Ads|Setup")
	void Cancel();

	virtual void Activate() override;

private:
#if WITH_DEV_AUTOMATION_TESTS
	friend class FOpenMobileAdsSetupAsyncContractTest;
#endif

	enum class EOperation : uint8
	{
		RefreshConsent,
		PresentPrivacyOptions,
		RequestTrackingAuthorization,
		Initialize
	};

	static UOpenMobileAdsSetupAsyncAction* Create(
		const UObject* WorldContextObject,
		EOperation Operation
	);
	void HandleInitialization(
		const FOpenMobileAdsInitializationStatusSnapshot& Status
	);
	void HandleConsent(const FOpenMobileAdsPrivacySnapshot& Privacy);
	void HandleTracking(
		FGuid CompletedRequestId,
		EOpenMobileAdsTrackingAuthorizationStatus Status
	);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void TryCompleteFromCurrentState();
	FOpenMobileAdsSetupResult MakeResult(
		const FOpenMobileAdsError& Error = {}
	) const;
	void FinishCompleted();
	void FinishFailed(const FOpenMobileAdsError& Error);
	void FinishCancelled();
	void Cleanup();
	EOpenMobileAdsFailureStage GetFailureStage() const;

	UPROPERTY(Transient)
	TObjectPtr<UObject> StoredWorldContextObject;

	TWeakObjectPtr<UOpenMobileAdsSubsystem> Subsystem;
	TWeakObjectPtr<UWorld> TargetWorld;
	EOperation Operation = EOperation::RefreshConsent;
	FGuid RequestId;
	FDelegateHandle InitializationHandle;
	FDelegateHandle ConsentHandle;
	FDelegateHandle TrackingHandle;
	FDelegateHandle WorldCleanupHandle;
	bool bFinished = false;
};
