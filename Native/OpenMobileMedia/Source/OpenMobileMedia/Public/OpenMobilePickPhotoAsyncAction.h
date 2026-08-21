#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileMediaTypes.h"
#include "OpenMobilePickPhotoAsyncAction.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobilePhotoPickedEvent,
	const FOpenMobileMediaPickResult&,
	Result
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOpenMobilePhotoPickCancelledEvent);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobilePhotoPickFailedEvent,
	const FOpenMobileError&,
	Error
);

/** Opens the system photo picker and creates a display-ready Unreal texture. */
UCLASS(meta = (ExposedAsyncProxy = "Action"))
class OPENMOBILEMEDIA_API UOpenMobilePickPhotoAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Media")
	FOpenMobilePhotoPickedEvent OnPicked;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Media")
	FOpenMobilePhotoPickCancelledEvent OnCancelled;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Media")
	FOpenMobilePhotoPickFailedEvent OnFailed;

	UFUNCTION(
		BlueprintCallable,
		Category = "Open Mobile|Media",
		meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject")
	)
	static UOpenMobilePickPhotoAsyncAction* PickPhoto(const UObject* WorldContextObject);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Media", meta = (ToolTip = "Cancels this picker or pending image import. Late native results are discarded."))
	void Cancel();

	virtual void Activate() override;

private:
	friend class FOpenMobileMediaWorldCleanupTest;
	void HandleNativePicked(const FString& CachedImagePath, const FString& MetadataJson);
	void HandleNativeCancelled();
	void HandleNativeError(const FString& ErrorMessage);
	void FinishWithError(FOpenMobileError Error);

	bool Finish();
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	TWeakObjectPtr<UObject> StoredWorldContextObject;
	TWeakObjectPtr<UWorld> TargetWorld;
	FDelegateHandle WorldCleanupHandle;
	int64 RequestId = 0;
	bool bActivated = false;
	bool bFinished = false;
};
