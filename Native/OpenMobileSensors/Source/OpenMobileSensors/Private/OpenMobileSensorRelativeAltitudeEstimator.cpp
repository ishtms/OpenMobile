#include "OpenMobileSensorRelativeAltitudeEstimator.h"

#include "OpenMobileSensorQuality.h"

namespace OpenMobileSensorRelativeAltitudeEstimatorPrivate
{
	constexpr double StandardAtmosphereHeightMeters = 44330.0;
	constexpr double StandardAtmosphereExponent = 1.0 / 5.255;

	int32 Flag(EOpenMobileRelativeAltitudeQualityLimitation Limitation)
	{
		return static_cast<int32>(Limitation);
	}
}

bool FOpenMobileSensorRelativeAltitudeEstimator::Process(
	const FOpenMobileScalarSensorSample& Input,
	const FOpenMobileSensorIdentifier& OutputSensor,
	FOpenMobileScalarSensorSample& OutSample
)
{
	using namespace OpenMobileSensorRelativeAltitudeEstimatorPrivate;
	if (OutputSensor.Type != EOpenMobileSensorType::RelativeAltitude
		|| !OutputSensor.IsValid()
		|| !Input.Header.bValid
		|| !FMath::IsFinite(Input.Header.TimestampSeconds)
		|| Input.Header.TimestampSeconds < 0.0
		|| !FMath::IsFinite(Input.Value))
	{
		return false;
	}
	const bool bPressure = Input.Header.Sensor.Type ==
		EOpenMobileSensorType::BarometricPressure;
	const bool bNative = Input.Header.Sensor.Type ==
		EOpenMobileSensorType::RelativeAltitude;
	if ((!bPressure && !bNative)
		|| (bPressure && (Input.Value <= 0.0 || Input.Value > 2000.0)))
	{
		return false;
	}
	const EOpenMobileRelativeAltitudeSource Source = bPressure
		? EOpenMobileRelativeAltitudeSource::PressureBaseline
		: EOpenMobileRelativeAltitudeSource::NativePlatform;
	if (Input.Header.bStatefulProcessingReset)
	{
		Reset();
	}
	if (bHasBaseline && Source != BaselineSource)
	{
		Reset();
	}
	if (!bHasBaseline)
	{
		bHasBaseline = true;
		BaselineSource = Source;
		BaselineValue = Input.Value;
		BaselineTimestampSeconds = Input.Header.TimestampSeconds;
	}
	OutSample = Input;
	OutSample.Header.Sensor = OutputSensor;
	OutSample.Header.bStatefulProcessingReset |=
		Input.Header.TimestampSeconds == BaselineTimestampSeconds;
	OutSample.RelativeAltitude = {};
	OutSample.RelativeAltitude.Source = Source;
	OutSample.RelativeAltitude.BaselineTimestampSeconds =
		BaselineTimestampSeconds;
	if (bPressure)
	{
		const double PressureRatio = Input.Value / BaselineValue;
		OutSample.Value = StandardAtmosphereHeightMeters
			* (1.0 - FMath::Pow(
				PressureRatio,
				StandardAtmosphereExponent
			));
		OutSample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::PluginDerived
		);
		OutSample.Header.Fusion.Quality =
			EOpenMobileSensorFusionQuality::Degraded;
		const int64 PressureMask =
			UOpenMobileSensorQualityLibrary::MakeInputMask(
				EOpenMobileSensorType::BarometricPressure
			);
		OutSample.Header.Fusion.ExpectedInputMask = PressureMask;
		OutSample.Header.Fusion.ContributingInputMask = PressureMask;
		OutSample.RelativeAltitude.bHasBaselinePressure = true;
		OutSample.RelativeAltitude.BaselinePressureHectopascals =
			BaselineValue;
		OutSample.RelativeAltitude.bUsesStandardAtmosphereModel = true;
		OutSample.RelativeAltitude.QualityLimitationFlags =
			Flag(EOpenMobileRelativeAltitudeQualityLimitation::WeatherSensitive)
			| Flag(
				EOpenMobileRelativeAltitudeQualityLimitation::
					StandardAtmosphereAssumption
			);
	}
	else
	{
		OutSample.Value = Input.Value - BaselineValue;
		OutSample.Header.SourceFlags = static_cast<int32>(
			EOpenMobileSensorSourceFlags::NativeFused
		);
		OutSample.RelativeAltitude.QualityLimitationFlags = Flag(
			EOpenMobileRelativeAltitudeQualityLimitation::
				NativeModelUnspecified
		);
	}
	return FMath::IsFinite(OutSample.Value);
}

void FOpenMobileSensorRelativeAltitudeEstimator::Reset()
{
	BaselineSource = EOpenMobileRelativeAltitudeSource::None;
	BaselineValue = 0.0;
	BaselineTimestampSeconds = 0.0;
	bHasBaseline = false;
}
