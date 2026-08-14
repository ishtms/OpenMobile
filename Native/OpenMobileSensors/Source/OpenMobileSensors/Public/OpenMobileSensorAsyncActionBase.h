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
	/** Call this when the Blueprint owner no longer wants the pending operation. Terminal actions stay finished and won't broadcast again. */
	virtual void Cancel() override;

	/** Use this before retaining an async action past its callback. Finished covers success, failure, and cancellation. */
	bool IsFinished() const
	{
		return TerminalState != EOpenMobileSensorAsyncTerminalState::Pending;
	}

protected:
	/** Derived actions call this once before touching the subsystem. It captures world ownership so teardown can cancel native work safely. */
	bool InitializeAction(const UObject* WorldContextObject);

	/** You'll get the subsystem captured during initialization. Null means setup failed or cleanup already ran. */
	UOpenMobileSensorsSubsystem* GetSensorsSubsystem() const;

	/** Call this after native work succeeds. It wins the terminal state once only and then cleans up ownership. */
	void FinishSucceeded();

	/** Call this with the user-facing error when native work fails. A prior terminal result won't broadcast twice. */
	void FinishFailed(FOpenMobileError Error);

	/** Call this when owner teardown or explicit cancellation ends the work. It keeps cancellation separate from failure. */
	void FinishCancelled(FOpenMobileError Error);

	/** Override this to cancel the provider request before base cleanup forgets it. Sometimes there's no native request yet, and that's fine. */
	virtual void CancelNativeOperation() {}

	/** Override this to copy success data into the action's public event before cleanup. Don't finish the action again here. */
	virtual void OnActionSucceeded() {}

	/** Override this to broadcast the typed failure payload. The base already owns terminal-state arbitration. */
	virtual void OnActionFailed(const FOpenMobileError& Error) {}

	/** Override this to broadcast the typed cancellation payload. Native cancellation may arrive few milliseconds later, so don't emit twice. */
	virtual void OnActionCancelled(const FOpenMobileError& Error) {}

private:
	friend class UOpenMobileSensorsSubsystem;
#if WITH_DEV_AUTOMATION_TESTS
	friend class FOpenMobileSensorsPermissionOwnerTeardownTest;
#endif

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
