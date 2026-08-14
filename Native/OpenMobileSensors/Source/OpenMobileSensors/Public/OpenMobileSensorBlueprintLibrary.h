#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorRecording.h"
#include "OpenMobileSensorResults.h"
#include "OpenMobileSensorStreamOptions.h"
#include "OpenMobileSensorBlueprintLibrary.generated.h"

class UOpenMobileSensorListener;

UENUM(BlueprintType)
enum class EOpenMobileSensorReadOutcome : uint8
{
	NewSample UMETA(DisplayName = "New Sample", ToolTip = "A sample exists with a sequence newer than the caller's previous sequence."),
	SameSample UMETA(DisplayName = "Same Sample", ToolTip = "A cached sample exists, but its sequence has not changed."),
	NoSample UMETA(DisplayName = "No Sample", ToolTip = "The listener has not received a sample yet."),
	InvalidListener UMETA(DisplayName = "Invalid Listener", ToolTip = "The raw subscription handle is invalid or no longer owned by this Game Instance.")
};

UENUM(BlueprintType)
enum class EOpenMobileSensorListenerCleanupOutcome : uint8
{
	Stopped UMETA(DisplayName = "Stopped", ToolTip = "Every unfinished listener in the collection reached a terminal state."),
	NothingToStop UMETA(DisplayName = "Nothing to Stop", ToolTip = "The collection contained no unfinished listeners."),
	SomeFailed UMETA(DisplayName = "Some Failed", ToolTip = "One or more collection entries were invalid or did not stop.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorListenerCleanupFailure
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Listener that failed cleanup, or None for an invalid collection entry."))
	TObjectPtr<UOpenMobileSensorListener> Listener;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors", meta = (ToolTip = "Short reason this collection entry could not be stopped."))
	FText Message;
};

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorBlueprintLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Use this when one owner has several typed listeners to finish together. Recording and replay sessions won't be touched. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Listeners", meta = (DisplayName = "Stop Sensor Listeners", ExpandEnumAsExecs = "Outcome", Keywords = "OpenMobile sensors listeners collection scoped cleanup stop", AutoCreateRefTerm = "Listeners", AdvancedDisplay = "Failures", ToolTip = "Stops the distinct unfinished typed listeners in this collection. Recording and replay sessions remain controlled by their own typed session objects."))
	static void StopSensorListeners(
		const TArray<UOpenMobileSensorListener*>& Listeners,
		EOpenMobileSensorListenerCleanupOutcome& Outcome,
		TArray<UOpenMobileSensorListener*>& StoppedListeners,
		TArray<FOpenMobileSensorListenerCleanupFailure>& Failures
	);

	/** Use this instead of checking only Success. Accepted means asynchronous work started correctly also. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Was Sensor Operation Successful", Keywords = "OpenMobile sensors result success accepted", ToolTip = "Returns true for both completed success and accepted asynchronous work."))
	static bool WasSensorOperationSuccessful(
		const FOpenMobileSensorOperationResult& Result
	);

	/** Use this when Success and Accepted should follow the same execution pin. A valid asynchronous start won't fall into failure then. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Branch on Sensor Operation Result", ExpandBoolAsExecs = "ReturnValue", Keywords = "OpenMobile sensors result success accepted branch", ToolTip = "Routes both completed success and accepted asynchronous work through the True execution pin."))
	static bool BranchOnSensorOperationResult(
		const FOpenMobileSensorOperationResult& Result
	);

	/** Use this to branch on one executed raw read. It won't read changing sensor state a second time. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Branch on Sensor Read Result", ExpandEnumAsExecs = "Outcome", Keywords = "OpenMobile sensors latest read new same missing invalid branch", ToolTip = "Routes one executed raw read snapshot without re-reading time-varying sensor state."))
	static void BranchOnSensorReadResult(
		const FOpenMobileSensorReadResult& Result,
		EOpenMobileSensorReadOutcome& Outcome
	);

	/** Use this to check whether this raw subscription handle identifies an owned sensor stream. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Is Sensor Subscription Handle Valid", Keywords = "OpenMobile sensors subscription handle valid", ToolTip = "Returns whether this raw subscription handle identifies an owned sensor stream."))
	static bool IsSensorSubscriptionHandleValid(
		const FOpenMobileSensorSubscriptionHandle& Handle
	);

	/** Use this to check whether two raw subscription handles identify the same sensor stream generation. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Are Sensor Subscription Handles Equal", Keywords = "OpenMobile sensors subscription handle compare equal", ToolTip = "Returns whether two raw subscription handles identify the same sensor stream generation."))
	static bool AreSensorSubscriptionHandlesEqual(
		const FOpenMobileSensorSubscriptionHandle& A,
		const FOpenMobileSensorSubscriptionHandle& B
	);

	/** Use this to create an intentional invalid raw subscription handle for initialization or comparison. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Make Invalid Sensor Subscription Handle", Keywords = "OpenMobile sensors subscription handle invalid clear", ToolTip = "Creates an intentional invalid raw subscription handle for initialization or comparison."))
	static FOpenMobileSensorSubscriptionHandle
	MakeInvalidSensorSubscriptionHandle();

	/** Use this to clear a stored raw subscription handle. This doesn't stop an active stream. */
	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Invalidate Sensor Subscription Handle", Keywords = "OpenMobile sensors subscription handle invalidate reset clear", ToolTip = "Clears a stored raw subscription handle. This does not stop an active stream."))
	static void InvalidateSensorSubscriptionHandle(
		UPARAM(ref) FOpenMobileSensorSubscriptionHandle& Handle
	);

	/** Use this to check whether this typed handle identifies a sensor flush request. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Is Sensor Flush Handle Valid", Keywords = "OpenMobile sensors flush request handle valid", ToolTip = "Returns whether this typed handle identifies a sensor flush request."))
	static bool IsSensorFlushHandleValid(
		const FOpenMobileSensorFlushHandle& Handle
	);

	/** Use this to check whether two typed sensor flush handles identify the same request. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Are Sensor Flush Handles Equal", Keywords = "OpenMobile sensors flush request handle compare equal", ToolTip = "Returns whether two typed sensor flush handles identify the same request."))
	static bool AreSensorFlushHandlesEqual(
		const FOpenMobileSensorFlushHandle& A,
		const FOpenMobileSensorFlushHandle& B
	);

	/** Use this to check whether this typed handle identifies a historical native step-count query. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Is Historical Step Query Handle Valid", Keywords = "OpenMobile sensors native historical step query handle valid", ToolTip = "Returns whether this typed handle identifies a historical native step-count query."))
	static bool IsNativeStepCountQueryHandleValid(
		const FOpenMobileNativeStepCountQueryHandle& Handle
	);

	/** Use this to check whether two typed historical step-query handles identify the same request. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Are Historical Step Query Handles Equal", Keywords = "OpenMobile sensors native historical step query handle compare equal", ToolTip = "Returns whether two typed historical step-query handles identify the same request."))
	static bool AreNativeStepCountQueryHandlesEqual(
		const FOpenMobileNativeStepCountQueryHandle& A,
		const FOpenMobileNativeStepCountQueryHandle& B
	);

	/** You'll get the generic sample family used by an advanced raw sensor stream. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Get Sensor Sample Family", Keywords = "OpenMobile sensors sample family vector attitude scalar heading steps activity orientation proximity", ToolTip = "Returns the generic sample family used by an advanced raw sensor stream."))
	static EOpenMobileSensorSampleFamily GetSensorSampleFamily(
		EOpenMobileSensorType Sensor
	);

	/** Use this to create a raw sensor identifier. Leave Instance Id empty to select the preferred matching sensor instance. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Make Sensor Identifier", NativeMakeFunc, Keywords = "OpenMobile sensors identifier preferred instance", AdvancedDisplay = "InstanceId", ToolTip = "Creates a raw sensor identifier. Leave Instance Id empty to select the preferred matching sensor instance."))
	static FOpenMobileSensorIdentifier MakeSensorIdentifier(
		EOpenMobileSensorType Sensor,
		FName InstanceId = NAME_None
	);

	/** You'll get the sensor type and optional raw instance name. An empty instance means the preferred matching sensor. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Advanced", meta = (DisplayName = "Break Sensor Identifier", NativeBreakFunc, Keywords = "OpenMobile sensors identifier type instance preferred", AdvancedDisplay = "OutInstanceId", ToolTip = "Returns the sensor type and optional raw instance name. An empty instance means the preferred matching sensor."))
	static void BreakSensorIdentifier(
		const FOpenMobileSensorIdentifier& Identifier,
		EOpenMobileSensorType& OutSensor,
		FName& OutInstanceId
	);

	/** You'll get the stream defaults configured in Project Settings under OpenMobile Sensors. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Default Sensor Stream Options", Keywords = "OpenMobile sensors default recommended stream options project settings", ToolTip = "Returns the stream defaults configured in Project Settings under OpenMobile Sensors."))
	static FOpenMobileSensorStreamOptions GetDefaultSensorStreamOptions();

	/** You'll get the recording duration, file-size, and lifecycle policy configured in Project Settings. Add sensors directly or use a preferred Start Recording node. */
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Recording", meta = (DisplayName = "Get Default Recording Options", Keywords = "OpenMobile sensors recording defaults project settings policy limits", ToolTip = "Returns the recording duration, file-size, and lifecycle policy configured in Project Settings. Add sensors directly or use a preferred Start Recording node."))
	static FOpenMobileSensorRecordingOptions
	GetDefaultSensorRecordingOptions();
};
