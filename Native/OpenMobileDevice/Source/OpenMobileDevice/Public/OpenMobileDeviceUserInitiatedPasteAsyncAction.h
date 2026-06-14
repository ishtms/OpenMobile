#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceAsyncActionBase.h"
#include "OpenMobileDeviceUserInitiatedPasteTypes.h"
#include "OpenMobileDeviceUserInitiatedPasteAsyncAction.generated.h"

UCLASS()
class OPENMOBILEDEVICE_API UOpenMobileDeviceUserInitiatedPasteAsyncAction final :
	public UOpenMobileDeviceAsyncActionBase
{
	GENERATED_BODY()

public:
	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Device",
		meta = (
			BlueprintInternalUseOnly = "true",
			WorldContext = "WorldContextObject",
			DisplayName = "Request User Initiated Paste",
			ToolTip = "Starts one foreground paste after the caller confirms a current user action. iOS 16 or newer presents a native Paste control. Older iOS and Android can show a system paste notification."
		)
	)
	static UOpenMobileDeviceUserInitiatedPasteAsyncAction*
	RequestUserInitiatedPaste(
		UObject* WorldContextObject,
		const FOpenMobileUserInitiatedPasteRequest& Request
	);

	virtual void Activate() override;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileUserInitiatedPasteResult Result;

protected:
	virtual void CancelNativeOperation() override;
	virtual void OnActionFailed(const FOpenMobileError& Error) override;
	virtual void OnActionCancelled(const FOpenMobileError& Error) override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UObject> WorldContextObject;

	FOpenMobileUserInitiatedPasteRequest Request;
	FGuid OperationId;
};
