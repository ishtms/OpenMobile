#pragma once

#include "CoreMinimal.h"
#include "Engine/CancellableAsyncAction.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileSensorAsyncActionBase.generated.h"

class UOpenMobileSensorsSubsystem;
class UWorld;

UENUM()
enum class EOpenMobileSensorAsyncTerminalState : uint8
{
	Pending,
	Succeeded,
	Cancelled,
	Failed
};

UCLASS(Abstract, BlueprintType, Transient, meta = (ExposedAsyncProxy = "AsyncAction"))
class OPENMOBILESENSORS_API UOpenMobileSensorAsyncActionBase
	: public UCancellableAsyncAction
{
	GENERATED_BODY()

public:
	virtual void Cancel() override;

	bool IsFinished() const
	{
		return TerminalState != EOpenMobileSensorAsyncTerminalState::Pending;
	}

protected:
	bool InitializeAction(const UObject* WorldContextObject);
	UOpenMobileSensorsSubsystem* GetSensorsSubsystem() const;
	void FinishSucceeded();
	void FinishFailed(FOpenMobileError Error);
	void FinishCancelled(FOpenMobileError Error);

	virtual void CancelNativeOperation() {}
	virtual void OnActionSucceeded() {}
	virtual void OnActionFailed(const FOpenMobileError& Error) {}
	virtual void OnActionCancelled(const FOpenMobileError& Error) {}

private:
	friend class UOpenMobileSensorsSubsystem;

	bool TrySetTerminalState(EOpenMobileSensorAsyncTerminalState State);
	void HandleWorldCleanup(
		UWorld* World,
		bool bSessionEnded,
		bool bCleanupResources
	);
	void HandleGameInstanceTeardown();
	void Cleanup();

	UPROPERTY(Transient)
	TObjectPtr<UObject> StoredWorldContextObject;

	TWeakObjectPtr<UOpenMobileSensorsSubsystem> Subsystem;
	TWeakObjectPtr<UWorld> TargetWorld;
	FDelegateHandle WorldCleanupHandle;
	EOpenMobileSensorAsyncTerminalState TerminalState =
		EOpenMobileSensorAsyncTerminalState::Pending;
};
