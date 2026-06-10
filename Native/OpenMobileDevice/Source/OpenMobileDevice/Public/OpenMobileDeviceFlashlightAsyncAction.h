#pragma once

#include "OpenMobileDeviceAsyncActionBase.h"
#include "OpenMobileDeviceFlashlightTypes.h"
#include "OpenMobileDeviceFlashlightAsyncAction.generated.h"

UCLASS(meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILEDEVICE_API UOpenMobileDeviceFlashlightAsyncAction final
	: public UOpenMobileDeviceAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileFlashlightOperationResult Result;

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Device",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Set Flashlight",
			ToolTip = "Asynchronously turns the flashlight off, on, or applies a normalized intensity without prompting for permission."
		)
	)
	static UOpenMobileDeviceFlashlightAsyncAction* SetFlashlight(
		const UObject* WorldContextObject,
		const FOpenMobileFlashlightRequest& Request
	);

	virtual void Activate() override;

protected:
	virtual void CancelNativeOperation() override;

private:
	void ExecuteOperation();

	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	FOpenMobileFlashlightRequest Request;
	uint64 BackendGeneration = 0;
	FGuid OperationId;
};
