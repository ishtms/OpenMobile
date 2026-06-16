#include "OpenMobileSensorFusionQuality.h"

namespace OpenMobileSensorFusionQualityPrivate
{
	bool IsQualityValueValid(EOpenMobileSensorFusionQuality Quality)
	{
		switch (Quality)
		{
		case EOpenMobileSensorFusionQuality::Unknown:
		case EOpenMobileSensorFusionQuality::Degraded:
		case EOpenMobileSensorFusionQuality::Nominal:
			return true;
		default:
			return false;
		}
	}

	int64 GetKnownSensorMask()
	{
		static const int64 KnownSensorMask = []()
		{
			int64 Mask = 0;
			for (const EOpenMobileSensorType Sensor
				: FOpenMobileSensorTypes::GetAll())
			{
				Mask |= UOpenMobileSensorQualityLibrary::MakeInputMask(Sensor);
			}
			return Mask;
		}();
		return KnownSensorMask;
	}

	bool IsInputDegraded(
		const FOpenMobileSensorFusionInputObservation& Input
	)
	{
		return Input.bAvailable
			&& (!Input.bValid
				|| Input.bCalibrationRequired
				|| Input.Accuracy == EOpenMobileSensorAccuracy::Unreliable
				|| Input.Accuracy == EOpenMobileSensorAccuracy::Low);
	}

	FOpenMobileSensorFusionContext Evaluate(
		TConstArrayView<FOpenMobileSensorFusionInputObservation> Inputs,
		bool bHasNativeQuality,
		EOpenMobileSensorFusionQuality NativeQuality
	)
	{
		FOpenMobileSensorFusionContext Context;
		Context.bHasNativeQualityReport = bHasNativeQuality;
		Context.NativeQuality = bHasNativeQuality
			? NativeQuality
			: EOpenMobileSensorFusionQuality::Unknown;
		for (const FOpenMobileSensorFusionInputObservation& Input : Inputs)
		{
			const int64 SensorMask =
				UOpenMobileSensorQualityLibrary::MakeInputMask(Input.Sensor);
			if (SensorMask == 0)
			{
				continue;
			}
			if (Input.bExpected)
			{
				Context.ExpectedInputMask |= SensorMask;
			}
			const bool bContributed = Input.bAvailable
				&& Input.bValid
				&& Input.bContributed;
			if (bContributed)
			{
				Context.ContributingInputMask |= SensorMask;
			}
			if (Input.bExpected && !bContributed)
			{
				Context.MissingInputMask |= SensorMask;
			}
			if (IsInputDegraded(Input))
			{
				Context.DegradedInputMask |= SensorMask;
			}
		}
		if (Context.MissingInputMask != 0
			|| Context.DegradedInputMask != 0
			|| (bHasNativeQuality
				&& NativeQuality ==
					EOpenMobileSensorFusionQuality::Degraded))
		{
			Context.Quality = EOpenMobileSensorFusionQuality::Degraded;
		}
		else if (Context.ExpectedInputMask != 0
			|| Context.ContributingInputMask != 0
			|| (bHasNativeQuality
				&& NativeQuality ==
					EOpenMobileSensorFusionQuality::Nominal))
		{
			Context.Quality = EOpenMobileSensorFusionQuality::Nominal;
		}
		return Context;
	}
}

FOpenMobileSensorFusionContext
FOpenMobileSensorFusionQualityEvaluator::Evaluate(
	TConstArrayView<FOpenMobileSensorFusionInputObservation> Inputs
)
{
	return OpenMobileSensorFusionQualityPrivate::Evaluate(
		Inputs,
		false,
		EOpenMobileSensorFusionQuality::Unknown
	);
}

FOpenMobileSensorFusionContext
FOpenMobileSensorFusionQualityEvaluator::Evaluate(
	TConstArrayView<FOpenMobileSensorFusionInputObservation> Inputs,
	EOpenMobileSensorFusionQuality NativeQuality
)
{
	return OpenMobileSensorFusionQualityPrivate::Evaluate(
		Inputs,
		true,
		NativeQuality
	);
}

bool FOpenMobileSensorFusionQualityEvaluator::ValidateContext(
	const FOpenMobileSensorFusionContext& Context
)
{
	using namespace OpenMobileSensorFusionQualityPrivate;
	if (!IsQualityValueValid(Context.Quality)
		|| !IsQualityValueValid(Context.NativeQuality)
		|| (!Context.bHasNativeQualityReport
			&& Context.NativeQuality !=
				EOpenMobileSensorFusionQuality::Unknown))
	{
		return false;
	}
	const int64 KnownMask = GetKnownSensorMask();
	const int64 AllInputMasks = Context.ExpectedInputMask
		| Context.ContributingInputMask
		| Context.MissingInputMask
		| Context.DegradedInputMask;
	if ((AllInputMasks & ~KnownMask) != 0
		|| (Context.MissingInputMask
			& Context.ContributingInputMask) != 0
		|| (Context.MissingInputMask
			& ~Context.ExpectedInputMask) != 0
		|| ((Context.MissingInputMask != 0
				|| Context.DegradedInputMask != 0)
			&& Context.Quality !=
				EOpenMobileSensorFusionQuality::Degraded))
	{
		return false;
	}
	return Context.Quality != EOpenMobileSensorFusionQuality::Nominal
		|| (Context.MissingInputMask == 0
			&& Context.DegradedInputMask == 0);
}
