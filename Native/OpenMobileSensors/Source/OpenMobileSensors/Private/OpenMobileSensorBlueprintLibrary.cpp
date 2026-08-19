#include "OpenMobileSensorBlueprintLibrary.h"

#include "OpenMobileSensorListener.h"
#include "OpenMobileSensorsSettings.h"
#include "OpenMobileSensorsRecordingService.h"

void UOpenMobileSensorBlueprintLibrary::StopSensorListeners(
	const TArray<UOpenMobileSensorListener*>& Listeners,
	EOpenMobileSensorListenerCleanupOutcome& Outcome,
	TArray<UOpenMobileSensorListener*>& StoppedListeners,
	TArray<FOpenMobileSensorListenerCleanupFailure>& Failures
)
{
	StoppedListeners.Reset();
	Failures.Reset();
	TSet<UOpenMobileSensorListener*> SeenListeners;
	for (UOpenMobileSensorListener* Listener : Listeners)
	{
		if (!IsValid(Listener))
		{
			FOpenMobileSensorListenerCleanupFailure Failure;
			Failure.Message = NSLOCTEXT(
				"OpenMobileSensorsListeners",
				"InvalidCleanupListener",
				"The listener collection contains an invalid entry."
			);
			Failures.Add(MoveTemp(Failure));
			continue;
		}
		if (SeenListeners.Contains(Listener))
		{
			continue;
		}
		SeenListeners.Add(Listener);
		if (Listener->IsFinished())
		{
			continue;
		}
		Listener->Stop();
		if (Listener->IsFinished())
		{
			StoppedListeners.Add(Listener);
		}
		else
		{
			FOpenMobileSensorListenerCleanupFailure Failure;
			Failure.Listener = Listener;
			Failure.Message = NSLOCTEXT(
				"OpenMobileSensorsListeners",
				"ListenerCleanupFailed",
				"The listener did not stop. Inspect its Error event before retrying."
			);
			Failures.Add(MoveTemp(Failure));
		}
	}
	Outcome = !Failures.IsEmpty()
		? EOpenMobileSensorListenerCleanupOutcome::SomeFailed
		: !StoppedListeners.IsEmpty()
			? EOpenMobileSensorListenerCleanupOutcome::Stopped
			: EOpenMobileSensorListenerCleanupOutcome::NothingToStop;
}

bool UOpenMobileSensorBlueprintLibrary::WasSensorOperationSuccessful(
	const FOpenMobileSensorOperationResult& Result
)
{
	return Result.IsSuccess();
}

bool UOpenMobileSensorBlueprintLibrary::BranchOnSensorOperationResult(
	const FOpenMobileSensorOperationResult& Result
)
{
	return Result.IsSuccess();
}

void UOpenMobileSensorBlueprintLibrary::BranchOnSensorReadResult(
	const FOpenMobileSensorReadResult& Result,
	EOpenMobileSensorReadOutcome& Outcome
)
{
	if (Result.Status == EOpenMobileSensorReadStatus::InvalidHandle)
	{
		Outcome = EOpenMobileSensorReadOutcome::InvalidListener;
	}
	else if (Result.Status == EOpenMobileSensorReadStatus::NoSample
		|| Result.Sequence <= 0)
	{
		Outcome = EOpenMobileSensorReadOutcome::NoSample;
	}
	else
	{
		Outcome = Result.bHasNewerSample
			? EOpenMobileSensorReadOutcome::NewSample
			: EOpenMobileSensorReadOutcome::SameSample;
	}
}

bool UOpenMobileSensorBlueprintLibrary::IsSensorSubscriptionHandleValid(
	const FOpenMobileSensorSubscriptionHandle& Handle
)
{
	return Handle.IsValid();
}

bool UOpenMobileSensorBlueprintLibrary::AreSensorSubscriptionHandlesEqual(
	const FOpenMobileSensorSubscriptionHandle& A,
	const FOpenMobileSensorSubscriptionHandle& B
)
{
	return A == B;
}

FOpenMobileSensorSubscriptionHandle
UOpenMobileSensorBlueprintLibrary::MakeInvalidSensorSubscriptionHandle()
{
	return {};
}

void UOpenMobileSensorBlueprintLibrary::InvalidateSensorSubscriptionHandle(
	FOpenMobileSensorSubscriptionHandle& Handle
)
{
	Handle.Reset();
}

bool UOpenMobileSensorBlueprintLibrary::IsSensorFlushHandleValid(
	const FOpenMobileSensorFlushHandle& Handle)
{
	return Handle.IsValid();
}

bool UOpenMobileSensorBlueprintLibrary::AreSensorFlushHandlesEqual(
	const FOpenMobileSensorFlushHandle& A,
	const FOpenMobileSensorFlushHandle& B)
{
	return A == B;
}

bool UOpenMobileSensorBlueprintLibrary::IsNativeStepCountQueryHandleValid(
	const FOpenMobileNativeStepCountQueryHandle& Handle)
{
	return Handle.IsValid();
}

bool UOpenMobileSensorBlueprintLibrary::
AreNativeStepCountQueryHandlesEqual(
	const FOpenMobileNativeStepCountQueryHandle& A,
	const FOpenMobileNativeStepCountQueryHandle& B)
{
	return A == B;
}

EOpenMobileSensorSampleFamily
UOpenMobileSensorBlueprintLibrary::GetSensorSampleFamily(
	EOpenMobileSensorType Sensor
)
{
	return FOpenMobileSensorTypes::GetSampleFamily(Sensor);
}

FOpenMobileSensorIdentifier
UOpenMobileSensorBlueprintLibrary::MakeSensorIdentifier(
	EOpenMobileSensorType Sensor,
	FName InstanceId
)
{
	FOpenMobileSensorIdentifier Identifier;
	Identifier.Type = Sensor;
	Identifier.InstanceId = InstanceId;
	return Identifier;
}

void UOpenMobileSensorBlueprintLibrary::BreakSensorIdentifier(
	const FOpenMobileSensorIdentifier& Identifier,
	EOpenMobileSensorType& OutSensor,
	FName& OutInstanceId)
{
	OutSensor = Identifier.Type;
	OutInstanceId = Identifier.InstanceId;
}

FOpenMobileSensorStreamOptions
UOpenMobileSensorBlueprintLibrary::GetDefaultSensorStreamOptions()
{
	return GetDefault<UOpenMobileSensorsSettings>()->DefaultStreamOptions;
}

FOpenMobileSensorRecordingOptions
UOpenMobileSensorBlueprintLibrary::GetDefaultSensorRecordingOptions()
{
	FOpenMobileSensorRecordingOptions Options;
	const UOpenMobileSensorsSettings* Settings =
		GetDefault<UOpenMobileSensorsSettings>();
	Options.MaximumDurationSeconds =
		Settings->MaximumRecordingDurationSeconds;
	Options.MaximumBytes = FMath::Min(
		Settings->MaximumRecordingBytes,
		256ll * 1024 * 1024);
	Options.LifecyclePolicy =
		Settings->DefaultStreamOptions.LifecyclePolicy;
	return Options;
}

bool UOpenMobileSensorBlueprintLibrary::IsSensorRecordable(EOpenMobileSensorType Sensor)
{
	return FOpenMobileSensorsRecordingService::IsSensorRecordable(Sensor);
}
