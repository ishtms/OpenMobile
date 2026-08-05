#include "OpenMobileSensorOptionalValueLibrary.h"

double UOpenMobileSensorOptionalValueLibrary::GetOptionalNumberOrDefault(
	const FOpenMobileSensorOptionalNumber& Optional,
	double DefaultValue)
{
	return Optional.bAvailable ? Optional.Value : DefaultValue;
}

int64 UOpenMobileSensorOptionalValueLibrary::GetOptionalIntegerOrDefault(
	const FOpenMobileSensorOptionalInteger& Optional,
	int64 DefaultValue)
{
	return Optional.bAvailable ? Optional.Value : DefaultValue;
}

FString UOpenMobileSensorOptionalValueLibrary::GetOptionalTextOrDefault(
	const FOpenMobileSensorOptionalText& Optional,
	const FString& DefaultValue)
{
	return Optional.bAvailable ? Optional.Value : DefaultValue;
}

bool UOpenMobileSensorOptionalValueLibrary::GetOptionalBooleanOrDefault(
	const FOpenMobileSensorOptionalBoolean& Optional,
	bool DefaultValue)
{
	return Optional.bAvailable ? Optional.bValue : DefaultValue;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetOptionalNumber(
	const FOpenMobileSensorOptionalNumber& Optional,
	double& Value)
{
	Value = Optional.bAvailable ? Optional.Value : 0.0;
	return Optional.bAvailable;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetOptionalInteger(
	const FOpenMobileSensorOptionalInteger& Optional,
	int64& Value)
{
	Value = Optional.bAvailable ? Optional.Value : 0;
	return Optional.bAvailable;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetOptionalText(
	const FOpenMobileSensorOptionalText& Optional,
	FString& Value)
{
	Value = Optional.bAvailable ? Optional.Value : FString();
	return Optional.bAvailable;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetOptionalBoolean(
	const FOpenMobileSensorOptionalBoolean& Optional,
	bool& Value)
{
	Value = Optional.bAvailable && Optional.bValue;
	return Optional.bAvailable;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetPedometerDistance(
	const FOpenMobilePedometerMetrics& Metrics,
	double& DistanceMetres)
{
	DistanceMetres = Metrics.bHasDistanceMeters
		? Metrics.DistanceMeters
		: 0.0;
	return Metrics.bHasDistanceMeters;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetFloorsAscended(
	const FOpenMobilePedometerMetrics& Metrics,
	int64& Floors)
{
	Floors = Metrics.bHasFloorsAscended ? Metrics.FloorsAscended : 0;
	return Metrics.bHasFloorsAscended;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetFloorsDescended(
	const FOpenMobilePedometerMetrics& Metrics,
	int64& Floors)
{
	Floors = Metrics.bHasFloorsDescended ? Metrics.FloorsDescended : 0;
	return Metrics.bHasFloorsDescended;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetHeadingAccuracy(
	const FOpenMobileHeadingSensorSample& Sample,
	double& AccuracyDegrees)
{
	AccuracyDegrees = Sample.bHasAccuracyDegrees
		? Sample.AccuracyDegrees
		: 0.0;
	return Sample.bHasAccuracyDegrees;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetAltitudeVerticalAccuracy(
	const FOpenMobileAbsoluteAltitudeMetadata& Metadata,
	double& AccuracyMetres)
{
	AccuracyMetres = Metadata.bHasVerticalAccuracy
		? Metadata.VerticalAccuracyMeters
		: 0.0;
	return Metadata.bHasVerticalAccuracy;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetProximityDistance(
	const FOpenMobileProximitySensorSample& Sample,
	double& DistanceMetres)
{
	DistanceMetres = Sample.bHasDistanceMeters ? Sample.DistanceMeters : 0.0;
	return Sample.bHasDistanceMeters;
}

bool UOpenMobileSensorOptionalValueLibrary::TryGetProximityMaximumRange(
	const FOpenMobileProximitySensorSample& Sample,
	double& MaximumRangeMetres)
{
	MaximumRangeMetres = Sample.bHasMaximumRangeMeters
		? Sample.MaximumRangeMeters
		: 0.0;
	return Sample.bHasMaximumRangeMeters;
}
