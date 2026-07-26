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
