#include "OpenMobileSensorHeading.h"

bool FOpenMobileSensorHeading::FromAndroidRotationVector(
	const FQuat& RotationVector,
	double& OutHeadingDegrees
)
{
	if (RotationVector.ContainsNaN()
		|| RotationVector.SizeSquared() <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}
	FQuat Normalized = RotationVector;
	Normalized.Normalize();
	const double Matrix01 = 2.0 * (
		Normalized.X * Normalized.Y - Normalized.Z * Normalized.W
	);
	const double Matrix11 = 1.0 - 2.0 * (
		Normalized.X * Normalized.X + Normalized.Z * Normalized.Z
	);
	if (!FMath::IsFinite(Matrix01)
		|| !FMath::IsFinite(Matrix11)
		|| (FMath::IsNearlyZero(Matrix01, 1.e-8)
			&& FMath::IsNearlyZero(Matrix11, 1.e-8)))
	{
		return false;
	}
	OutHeadingDegrees = FMath::RadiansToDegrees(
		FMath::Atan2(Matrix01, Matrix11)
	);
	if (OutHeadingDegrees < 0.0)
	{
		OutHeadingDegrees += 360.0;
	}
	return FMath::IsFinite(OutHeadingDegrees)
		&& OutHeadingDegrees >= 0.0
		&& OutHeadingDegrees < 360.0;
}
