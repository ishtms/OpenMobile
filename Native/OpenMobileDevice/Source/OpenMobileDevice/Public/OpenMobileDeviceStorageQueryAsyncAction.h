#pragma once

#include "OpenMobileDeviceAsyncActionBase.h"
#include "OpenMobileDeviceResourceTypes.h"
#include "OpenMobileDeviceStorageQueryAsyncAction.generated.h"

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILEDEVICE_API UOpenMobileDeviceStorageQueryAsyncAction final
	: public UOpenMobileDeviceAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileStorageSnapshot Snapshot;

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Device",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Query Device Storage",
			ToolTip = "Queries the application data volume on a worker thread and returns the result on the game thread."
		)
	)
	static UOpenMobileDeviceStorageQueryAsyncAction* QueryStorage(
		const UObject* WorldContextObject
	);

	virtual void Activate() override;

protected:
	virtual void CancelNativeOperation() override;

private:
	void HandleQueryComplete(
		FOpenMobileStorageSnapshot Result,
		FOpenMobileError Error,
		bool bSucceeded,
		uint64 BackendGeneration
	);

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	TSharedPtr<TAtomic<bool>, ESPMode::ThreadSafe> CancellationFlag;
};
