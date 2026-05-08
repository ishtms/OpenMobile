#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceAsyncActionBase.generated.h"

class UOpenMobileDeviceSubsystem;
class UWorld;

UENUM()
enum class EOpenMobileDeviceAsyncTerminalState : uint8
{
	Pending,
	Succeeded,
	Cancelled,
	Failed
};

DECLARE_MULTICAST_DELEGATE_TwoParams(
	FOpenMobileDeviceNativeAsyncTerminal,
	EOpenMobileDeviceAsyncTerminalState,
	const FOpenMobileError&
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOpenMobileDeviceAsyncSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileDeviceAsyncCancelled,
	const FOpenMobileError&,
	Error
);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileDeviceAsyncFailed,
	const FOpenMobileError&,
	Error
);

UCLASS(BlueprintType, Transient, meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILEDEVICE_API UOpenMobileDeviceAsyncActionBase :
	public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "Success", ToolTip = "Broadcasts once when the Device operation succeeds."))
	FOpenMobileDeviceAsyncSuccess Success;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "Cancelled", ToolTip = "Broadcasts once when the Device operation is cancelled."))
	FOpenMobileDeviceAsyncCancelled Cancelled;

	UPROPERTY(BlueprintAssignable, Category = "Open Mobile|Device", meta = (DisplayName = "Failed", ToolTip = "Broadcasts once with a typed error when the Device operation fails."))
	FOpenMobileDeviceAsyncFailed Failed;

	virtual void Cancel() override;

	bool IsFinished() const
	{
		return TerminalState != EOpenMobileDeviceAsyncTerminalState::Pending;
	}

	EOpenMobileDeviceAsyncTerminalState GetTerminalState() const
	{
		return TerminalState;
	}

	FOpenMobileDeviceNativeAsyncTerminal& OnNativeTerminal()
	{
		return NativeTerminal;
	}

protected:
	bool InitializeAction(const UObject* WorldContextObject);
	void FinishSucceeded();
	void FinishFailed(FOpenMobileError Error);
	void FinishCancelled(FOpenMobileError Error);

	virtual void CancelNativeOperation() {}
	virtual void OnActionSucceeded() {}
	virtual void OnActionFailed(const FOpenMobileError& Error) {}
	virtual void OnActionCancelled(const FOpenMobileError& Error) {}

private:
	friend class UOpenMobileDeviceSubsystem;
	friend class FOpenMobileDeviceAsyncContractTest;

	bool TrySetTerminalState(EOpenMobileDeviceAsyncTerminalState State);
	void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
	void HandleGameInstanceTeardown();
	void Cleanup();

	UPROPERTY(Transient)
	TObjectPtr<UObject> StoredWorldContextObject;

	TWeakObjectPtr<UOpenMobileDeviceSubsystem> Subsystem;
	TWeakObjectPtr<UWorld> TargetWorld;
	FDelegateHandle WorldCleanupHandle;
	FOpenMobileDeviceNativeAsyncTerminal NativeTerminal;
	EOpenMobileDeviceAsyncTerminalState TerminalState =
		EOpenMobileDeviceAsyncTerminalState::Pending;
};
