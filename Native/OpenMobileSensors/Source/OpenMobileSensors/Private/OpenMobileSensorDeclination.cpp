#include "OpenMobileSensorDeclination.h"

namespace
{
constexpr int32 ModelDegree = 12;
constexpr int32 TermCount = 91;
constexpr double ModelEpoch = 2025.0;
constexpr double ModelEndYear = 2030.0;
constexpr double Wgs84SemiMajorKilometers = 6378.137;
constexpr double Wgs84SemiMinorKilometers = 6356.7523142;
constexpr double EarthReferenceRadiusKilometers = 6371.2;

struct FCoefficient
{
	int32 N;
	int32 M;
	double G;
	double H;
	double GDot;
	double HDot;
};

constexpr FCoefficient Coefficients[] = {
	{1, 0, -29351.8, 0.0, 12.0, 0.0},
	{1, 1, -1410.8, 4545.4, 9.7, -21.5},
	{2, 0, -2556.6, 0.0, -11.6, 0.0},
	{2, 1, 2951.1, -3133.6, -5.2, -27.7},
	{2, 2, 1649.3, -815.1, -8.0, -12.1},
	{3, 0, 1361.0, 0.0, -1.3, 0.0},
	{3, 1, -2404.1, -56.6, -4.2, 4.0},
	{3, 2, 1243.8, 237.5, 0.4, -0.3},
	{3, 3, 453.6, -549.5, -15.6, -4.1},
	{4, 0, 895.0, 0.0, -1.6, 0.0},
	{4, 1, 799.5, 278.6, -2.4, -1.1},
	{4, 2, 55.7, -133.9, -6.0, 4.1},
	{4, 3, -281.1, 212.0, 5.6, 1.6},
	{4, 4, 12.1, -375.6, -7.0, -4.4},
	{5, 0, -233.2, 0.0, 0.6, 0.0},
	{5, 1, 368.9, 45.4, 1.4, -0.5},
	{5, 2, 187.2, 220.2, 0.0, 2.2},
	{5, 3, -138.7, -122.9, 0.6, 0.4},
	{5, 4, -142.0, 43.0, 2.2, 1.7},
	{5, 5, 20.9, 106.1, 0.9, 1.9},
	{6, 0, 64.4, 0.0, -0.2, 0.0},
	{6, 1, 63.8, -18.4, -0.4, 0.3},
	{6, 2, 76.9, 16.8, 0.9, -1.6},
	{6, 3, -115.7, 48.8, 1.2, -0.4},
	{6, 4, -40.9, -59.8, -0.9, 0.9},
	{6, 5, 14.9, 10.9, 0.3, 0.7},
	{6, 6, -60.7, 72.7, 0.9, 0.9},
	{7, 0, 79.5, 0.0, -0.0, 0.0},
	{7, 1, -77.0, -48.9, -0.1, 0.6},
	{7, 2, -8.8, -14.4, -0.1, 0.5},
	{7, 3, 59.3, -1.0, 0.5, -0.8},
	{7, 4, 15.8, 23.4, -0.1, 0.0},
	{7, 5, 2.5, -7.4, -0.8, -1.0},
	{7, 6, -11.1, -25.1, -0.8, 0.6},
	{7, 7, 14.2, -2.3, 0.8, -0.2},
	{8, 0, 23.2, 0.0, -0.1, 0.0},
	{8, 1, 10.8, 7.1, 0.2, -0.2},
	{8, 2, -17.5, -12.6, 0.0, 0.5},
	{8, 3, 2.0, 11.4, 0.5, -0.4},
	{8, 4, -21.7, -9.7, -0.1, 0.4},
	{8, 5, 16.9, 12.7, 0.3, -0.5},
	{8, 6, 15.0, 0.7, 0.2, -0.6},
	{8, 7, -16.8, -5.2, -0.0, 0.3},
	{8, 8, 0.9, 3.9, 0.2, 0.2},
	{9, 0, 4.6, 0.0, -0.0, 0.0},
	{9, 1, 7.8, -24.8, -0.1, -0.3},
	{9, 2, 3.0, 12.2, 0.1, 0.3},
	{9, 3, -0.2, 8.3, 0.3, -0.3},
	{9, 4, -2.5, -3.3, -0.3, 0.3},
	{9, 5, -13.1, -5.2, 0.0, 0.2},
	{9, 6, 2.4, 7.2, 0.3, -0.1},
	{9, 7, 8.6, -0.6, -0.1, -0.2},
	{9, 8, -8.7, 0.8, 0.1, 0.4},
	{9, 9, -12.9, 10.0, -0.1, 0.1},
	{10, 0, -1.3, 0.0, 0.1, 0.0},
	{10, 1, -6.4, 3.3, 0.0, 0.0},
	{10, 2, 0.2, 0.0, 0.1, -0.0},
	{10, 3, 2.0, 2.4, 0.1, -0.2},
	{10, 4, -1.0, 5.3, -0.0, 0.1},
	{10, 5, -0.6, -9.1, -0.3, -0.1},
	{10, 6, -0.9, 0.4, 0.0, 0.1},
	{10, 7, 1.5, -4.2, -0.1, 0.0},
	{10, 8, 0.9, -3.8, -0.1, -0.1},
	{10, 9, -2.7, 0.9, -0.0, 0.2},
	{10, 10, -3.9, -9.1, -0.0, -0.0},
	{11, 0, 2.9, 0.0, 0.0, 0.0},
	{11, 1, -1.5, 0.0, -0.0, -0.0},
	{11, 2, -2.5, 2.9, 0.0, 0.1},
	{11, 3, 2.4, -0.6, 0.0, -0.0},
	{11, 4, -0.6, 0.2, 0.0, 0.1},
	{11, 5, -0.1, 0.5, -0.1, -0.0},
	{11, 6, -0.6, -0.3, 0.0, -0.0},
	{11, 7, -0.1, -1.2, -0.0, 0.1},
	{11, 8, 1.1, -1.7, -0.1, -0.0},
	{11, 9, -1.0, -2.9, -0.1, 0.0},
	{11, 10, -0.2, -1.8, -0.1, 0.0},
	{11, 11, 2.6, -2.3, -0.1, 0.0},
	{12, 0, -2.0, 0.0, 0.0, 0.0},
	{12, 1, -0.2, -1.3, 0.0, -0.0},
	{12, 2, 0.3, 0.7, -0.0, 0.0},
	{12, 3, 1.2, 1.0, -0.0, -0.1},
	{12, 4, -1.3, -1.4, -0.0, 0.1},
	{12, 5, 0.6, -0.0, -0.0, -0.0},
	{12, 6, 0.6, 0.6, 0.1, -0.0},
	{12, 7, 0.5, -0.1, -0.0, -0.0},
	{12, 8, -0.1, 0.8, 0.0, 0.0},
	{12, 9, -0.4, 0.1, 0.0, -0.0},
	{12, 10, -0.2, -1.0, -0.1, -0.0},
	{12, 11, -1.3, 0.1, -0.0, 0.0},
	{12, 12, -0.7, 0.2, -0.1, -0.1}
};
static_assert(UE_ARRAY_COUNT(Coefficients) == TermCount - 1);

int32 Index(int32 N, int32 M)
{
	return N * (N + 1) / 2 + M;
}

void CalculateLegendre(
	double SinLatitude,
	double (&P)[TermCount],
	double (&DP)[TermCount]
)
{
	double Normalization[TermCount] = {};
	P[0] = 1.0;
	const double CosLatitude = FMath::Sqrt(
		(1.0 - SinLatitude) * (1.0 + SinLatitude)
	);
	for (int32 N = 1; N <= ModelDegree; ++N)
	{
		for (int32 M = 0; M <= N; ++M)
		{
			const int32 Current = Index(N, M);
			if (N == M)
			{
				const int32 Previous = Index(N - 1, M - 1);
				P[Current] = CosLatitude * P[Previous];
				DP[Current] = CosLatitude * DP[Previous]
					+ SinLatitude * P[Previous];
			}
			else if (N == 1)
			{
				P[Current] = SinLatitude * P[0];
				DP[Current] = SinLatitude * DP[0]
					- CosLatitude * P[0];
			}
			else
			{
				const int32 Previous = Index(N - 1, M);
				if (M > N - 2)
				{
					P[Current] = SinLatitude * P[Previous];
					DP[Current] = SinLatitude * DP[Previous]
						- CosLatitude * P[Previous];
				}
				else
				{
					const int32 Earlier = Index(N - 2, M);
					const double K = static_cast<double>(
						(N - 1) * (N - 1) - M * M
					) / static_cast<double>((2 * N - 1) * (2 * N - 3));
					P[Current] = SinLatitude * P[Previous]
						- K * P[Earlier];
					DP[Current] = SinLatitude * DP[Previous]
						- CosLatitude * P[Previous]
						- K * DP[Earlier];
				}
			}
		}
	}
	Normalization[0] = 1.0;
	for (int32 N = 1; N <= ModelDegree; ++N)
	{
		const int32 Zonal = Index(N, 0);
		Normalization[Zonal] = Normalization[Index(N - 1, 0)]
			* static_cast<double>(2 * N - 1) / static_cast<double>(N);
		for (int32 M = 1; M <= N; ++M)
		{
			const int32 Current = Index(N, M);
			Normalization[Current] = Normalization[Current - 1]
				* FMath::Sqrt(static_cast<double>(
					(N - M + 1) * (M == 1 ? 2 : 1)
				) / static_cast<double>(N + M));
		}
	}
	for (int32 I = 1; I < TermCount; ++I)
	{
		P[I] *= Normalization[I];
		DP[I] *= -Normalization[I];
	}
}

double CalculatePoleEastField(
	double SinLatitude,
	const double (&RadiusPower)[ModelDegree + 1],
	const double (&SinLongitude)[ModelDegree + 1],
	const double (&CosLongitude)[ModelDegree + 1],
	double YearOffset
)
{
	double P[ModelDegree + 1] = {};
	P[0] = 1.0;
	double PreviousNormalization = 1.0;
	double Result = 0.0;
	for (int32 N = 1; N <= ModelDegree; ++N)
	{
		const double NextNormalization = PreviousNormalization
			* static_cast<double>(2 * N - 1) / static_cast<double>(N);
		const double SectorNormalization = NextNormalization
			* FMath::Sqrt(static_cast<double>(2 * N) / static_cast<double>(N + 1));
		PreviousNormalization = NextNormalization;
		if (N == 1)
		{
			P[N] = P[N - 1];
		}
		else
		{
			const double K = static_cast<double>((N - 1) * (N - 1) - 1)
				/ static_cast<double>((2 * N - 1) * (2 * N - 3));
			P[N] = SinLatitude * P[N - 1] - K * P[N - 2];
		}
		const FCoefficient& Coefficient = Coefficients[Index(N, 1) - 1];
		const double G = Coefficient.G + YearOffset * Coefficient.GDot;
		const double H = Coefficient.H + YearOffset * Coefficient.HDot;
		Result += RadiusPower[N]
			* (G * SinLongitude[1] - H * CosLongitude[1])
			* P[N] * SectorNormalization;
	}
	return Result;
}
}

bool FOpenMobileSensorDeclination::CalculateWMM2025(
	double LatitudeDegrees,
	double LongitudeDegrees,
	double AltitudeMeters,
	double DecimalYear,
	FOpenMobileSensorDeclinationResult& OutResult
)
{
	if (!FMath::IsFinite(LatitudeDegrees)
		|| !FMath::IsFinite(LongitudeDegrees)
		|| !FMath::IsFinite(AltitudeMeters)
		|| !FMath::IsFinite(DecimalYear)
		|| LatitudeDegrees < -90.0
		|| LatitudeDegrees > 90.0
		|| AltitudeMeters < -1000.0
		|| AltitudeMeters > 850000.0
		|| DecimalYear < ModelEpoch
		|| DecimalYear >= ModelEndYear)
	{
		return false;
	}

	const double GeodeticLatitude = FMath::DegreesToRadians(LatitudeDegrees);
	const double SinGeodeticLatitude = FMath::Sin(GeodeticLatitude);
	const double CosGeodeticLatitude = FMath::Cos(GeodeticLatitude);
	const double EccentricitySquared = 1.0
		- FMath::Square(Wgs84SemiMinorKilometers / Wgs84SemiMajorKilometers);
	const double RadiusOfCurvature = Wgs84SemiMajorKilometers
		/ FMath::Sqrt(1.0 - EccentricitySquared
			* FMath::Square(SinGeodeticLatitude));
	const double AltitudeKilometers = AltitudeMeters / 1000.0;
	const double X = (RadiusOfCurvature + AltitudeKilometers)
		* CosGeodeticLatitude;
	const double Z = (RadiusOfCurvature * (1.0 - EccentricitySquared)
		+ AltitudeKilometers) * SinGeodeticLatitude;
	const double SphericalRadius = FMath::Sqrt(X * X + Z * Z);
	const double GeocentricLatitude = FMath::Asin(Z / SphericalRadius);
	const double SinGeocentricLatitude = FMath::Sin(GeocentricLatitude);
	const double CosGeocentricLatitude = FMath::Cos(GeocentricLatitude);

	double P[TermCount] = {};
	double DP[TermCount] = {};
	CalculateLegendre(SinGeocentricLatitude, P, DP);

	double RadiusPower[ModelDegree + 1] = {};
	double CosLongitude[ModelDegree + 1] = {};
	double SinLongitude[ModelDegree + 1] = {};
	const double RadiusRatio = EarthReferenceRadiusKilometers / SphericalRadius;
	RadiusPower[0] = RadiusRatio * RadiusRatio;
	for (int32 N = 1; N <= ModelDegree; ++N)
	{
		RadiusPower[N] = RadiusPower[N - 1] * RadiusRatio;
	}
	const double Longitude = FMath::DegreesToRadians(LongitudeDegrees);
	CosLongitude[0] = 1.0;
	if constexpr (ModelDegree >= 1)
	{
		CosLongitude[1] = FMath::Cos(Longitude);
		SinLongitude[1] = FMath::Sin(Longitude);
	}
	for (int32 M = 2; M <= ModelDegree; ++M)
	{
		CosLongitude[M] = CosLongitude[M - 1] * CosLongitude[1]
			- SinLongitude[M - 1] * SinLongitude[1];
		SinLongitude[M] = CosLongitude[M - 1] * SinLongitude[1]
			+ SinLongitude[M - 1] * CosLongitude[1];
	}

	const double YearOffset = DecimalYear - ModelEpoch;
	double NorthSpherical = 0.0;
	double EastSpherical = 0.0;
	double DownSpherical = 0.0;
	for (const FCoefficient& Coefficient : Coefficients)
	{
		const int32 I = Index(Coefficient.N, Coefficient.M);
		const double G = Coefficient.G + YearOffset * Coefficient.GDot;
		const double H = Coefficient.H + YearOffset * Coefficient.HDot;
		const double LongitudeField = G * CosLongitude[Coefficient.M]
			+ H * SinLongitude[Coefficient.M];
		DownSpherical -= RadiusPower[Coefficient.N]
			* LongitudeField * static_cast<double>(Coefficient.N + 1) * P[I];
		EastSpherical += RadiusPower[Coefficient.N]
			* (G * SinLongitude[Coefficient.M]
				- H * CosLongitude[Coefficient.M])
			* static_cast<double>(Coefficient.M) * P[I];
		NorthSpherical -= RadiusPower[Coefficient.N]
			* LongitudeField * DP[I];
	}
	if (FMath::Abs(CosGeocentricLatitude) > 1.e-10)
	{
		EastSpherical /= CosGeocentricLatitude;
	}
	else
	{
		EastSpherical = CalculatePoleEastField(
			SinGeocentricLatitude,
			RadiusPower,
			SinLongitude,
			CosLongitude,
			YearOffset
		);
	}

	const double LatitudeDifference = GeocentricLatitude - GeodeticLatitude;
	const double North = NorthSpherical * FMath::Cos(LatitudeDifference)
		- DownSpherical * FMath::Sin(LatitudeDifference);
	const double HorizontalField = FMath::Sqrt(
		North * North + EastSpherical * EastSpherical
	);
	if (!FMath::IsFinite(HorizontalField)
		|| HorizontalField <= UE_DOUBLE_SMALL_NUMBER)
	{
		return false;
	}

	OutResult.DeclinationDegrees = FMath::RadiansToDegrees(
		FMath::Atan2(EastSpherical, North)
	);
	OutResult.HorizontalFieldNanoTesla = HorizontalField;
	OutResult.EstimatedErrorDegrees = FMath::Sqrt(
		FMath::Square(0.26) + FMath::Square(5417.0 / HorizontalField)
	);
	return FMath::IsFinite(OutResult.DeclinationDegrees)
		&& FMath::IsFinite(OutResult.EstimatedErrorDegrees);
}
