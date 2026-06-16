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
