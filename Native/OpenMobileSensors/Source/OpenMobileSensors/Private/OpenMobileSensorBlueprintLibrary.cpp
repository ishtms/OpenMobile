#include "OpenMobileSensorBlueprintLibrary.h"

#include "OpenMobileSensorsSettings.h"

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
