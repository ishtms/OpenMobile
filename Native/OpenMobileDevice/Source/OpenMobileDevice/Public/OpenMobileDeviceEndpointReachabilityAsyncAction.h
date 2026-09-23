#pragma once

#include "OpenMobileDeviceAsyncActionBase.h"
#include "OpenMobileDeviceEndpointReachabilityTypes.h"
#include "OpenMobileDeviceEndpointReachabilityAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileEndpointReachabilityCompleted,
	const FOpenMobileEndpointReachabilityResult&,
	Result
);

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILEDEVICE_API UOpenMobileDeviceEndpointReachabilityAsyncAction final
	: public UOpenMobileDeviceAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device")
	FOpenMobileEndpointReachabilityCompleted Completed;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileEndpointReachabilityResult Result;

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Device",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Test Endpoint Reachability",
			ToolTip = "Tests one explicit HTTPS endpoint once and returns a typed result without caching a global online state."
		)
	)
	static UOpenMobileDeviceEndpointReachabilityAsyncAction*
	TestEndpointReachability(
		const UObject* WorldContextObject,
		const FString& HttpsEndpoint,
		FOpenMobileEndpointReachabilityOptions Options
	);

	virtual void Activate() override;

protected:
	virtual void CancelNativeOperation() override;
	virtual void OnActionSucceeded() override;
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	void StartHttpRequest();
	void CompleteResult(EOpenMobileEndpointReachabilityOutcome Outcome);
	void ReleaseRequestSlot();
	bool HandleTimeoutTick(float DeltaTime);
	void StopTimeoutTicker();

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	FString Endpoint;
	FOpenMobileEndpointReachabilityOptions RequestOptions;
	TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> CancellationFlag;
	FTSTicker::FDelegateHandle TimeoutTickerHandle;
	double StartedAtSeconds = 0.0;
	bool bRequestSlotAcquired = false;
};
