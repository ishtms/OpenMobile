#include "OpenMobileSensorQuality.h"

namespace OpenMobileSensorQualityPrivate
{
	TArray<EOpenMobileSensorType> GetSensors(int64 InputMask)
	{
		TArray<EOpenMobileSensorType> Sensors;
		for (const EOpenMobileSensorType Sensor : FOpenMobileSensorTypes::GetAll())
		{
			if (UOpenMobileSensorQualityLibrary::ContainsSensor(
				InputMask,
				Sensor
			))
			{
				Sensors.Add(Sensor);
			}
		}
		return Sensors;
	}
}

void UOpenMobileSensorQualityLibrary::BreakSensorFusionContext(
	const FOpenMobileSensorFusionContext& Context,
	EOpenMobileSensorFusionQuality& OutQuality,
	bool& bOutHasNativeQuality,
	EOpenMobileSensorFusionQuality& OutNativeQuality,
	bool& bOutHasEstimatedLag,
	double& OutEstimatedLagSeconds,
	TArray<EOpenMobileSensorType>& OutExpectedInputs,
	TArray<EOpenMobileSensorType>& OutContributingInputs,
	TArray<EOpenMobileSensorType>& OutMissingInputs,
	TArray<EOpenMobileSensorType>& OutDegradedInputs)
{
	OutQuality = Context.Quality;
	bOutHasNativeQuality = Context.bHasNativeQualityReport;
	OutNativeQuality = Context.NativeQuality;
	bOutHasEstimatedLag = Context.bHasEstimatedLag;
	OutEstimatedLagSeconds = Context.bHasEstimatedLag
		? Context.EstimatedLagSeconds
		: 0.0;
	OutExpectedInputs = GetExpectedInputs(Context);
	OutContributingInputs = GetContributingInputs(Context);
	OutMissingInputs = GetMissingInputs(Context);
	OutDegradedInputs = GetDegradedInputs(Context);
}

int64 UOpenMobileSensorQualityLibrary::MakeInputMask(
	EOpenMobileSensorType Sensor
)
{
	const uint8 Value = static_cast<uint8>(Sensor);
	return Sensor != EOpenMobileSensorType::Unknown && Value < 63
		? int64{1} << Value
		: 0;
}

bool UOpenMobileSensorQualityLibrary::ContainsSensor(
	int64 InputMask,
	EOpenMobileSensorType Sensor
)
{
	const int64 SensorMask = MakeInputMask(Sensor);
	return SensorMask != 0 && (InputMask & SensorMask) != 0;
}

TArray<EOpenMobileSensorType>
UOpenMobileSensorQualityLibrary::GetExpectedInputs(
	const FOpenMobileSensorFusionContext& Context
)
{
	return OpenMobileSensorQualityPrivate::GetSensors(
		Context.ExpectedInputMask
	);
}

TArray<EOpenMobileSensorType>
UOpenMobileSensorQualityLibrary::GetContributingInputs(
	const FOpenMobileSensorFusionContext& Context
)
{
	return OpenMobileSensorQualityPrivate::GetSensors(
		Context.ContributingInputMask
	);
}

TArray<EOpenMobileSensorType>
UOpenMobileSensorQualityLibrary::GetMissingInputs(
	const FOpenMobileSensorFusionContext& Context
)
{
	return OpenMobileSensorQualityPrivate::GetSensors(
		Context.MissingInputMask
	);
}

TArray<EOpenMobileSensorType>
UOpenMobileSensorQualityLibrary::GetDegradedInputs(
	const FOpenMobileSensorFusionContext& Context
)
{
	return OpenMobileSensorQualityPrivate::GetSensors(
		Context.DegradedInputMask
	);
}
