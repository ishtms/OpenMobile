#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileSensorsBlueprintExamples.h"
#include "OpenMobileSensorsDemoWidget.h"
#include "OpenMobileSensorsSampleGameMode.h"
#include "OpenMobileSensorsSamplePlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsSampleHostContractTest,
	"OpenMobile.Sensors.SampleHost.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsSampleHostContractTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FOpenMobileSensorSubscriptionRequest Request =
		UOpenMobileSensorsBlueprintExamples::MakeLowRateRequest(
			EOpenMobileSensorType::Attitude,
			EOpenMobileSensorDeliveryMode::Buffered,
			EOpenMobileSensorCoordinateSpace::CurrentScreen
		);
	TestEqual(TEXT("The sample uses the UI rate preset"),
		Request.Options.RatePreset,
		EOpenMobileSensorRatePreset::UI);
	TestEqual(TEXT("The sample caps callbacks at a safe rate"),
		Request.Options.MaximumCallbackFrequencyHz,
		10.0);
	TestEqual(TEXT("The sample uses a bounded buffer"),
		Request.Options.BufferCapacitySamples,
		128);
	TestFalse(TEXT("The sample does not opt in to high-rate access"),
		Request.Options.bAllowHighSamplingRate);
	TestEqual(TEXT("The sample exposes screen compensation"),
		Request.Options.CoordinateSpace,
		EOpenMobileSensorCoordinateSpace::CurrentScreen);
	TestTrue(TEXT("Attitude requests a rotation matrix"),
		(Request.Options.AttitudeRepresentations
			& static_cast<int32>(
				EOpenMobileAttitudeRepresentation::RotationMatrix
			)) != 0);

	const FVector2D Tilt =
		UOpenMobileSensorsBlueprintExamples::ComputeTiltSteering(
			FVector(0.0, 9.80665, 0.0)
		);
	TestTrue(TEXT("Tilt steering maps device right to positive steering"),
		Tilt.Equals(FVector2D(1.0, 0.0), 1.e-6));
	const FVector2D Aim =
		UOpenMobileSensorsBlueprintExamples::ComputeGyroAimDelta(
			FVector(0.0, 0.0, PI),
			0.5
		);
	TestTrue(TEXT("Gyro aiming converts radians to degrees"),
		Aim.Equals(FVector2D(90.0, 0.0), 1.e-3));

	TestNotNull(TEXT("The sample widget is reflected"),
		UOpenMobileSensorsDemoWidget::StaticClass());
	TestNotNull(TEXT("The sample controller is reflected"),
		AOpenMobileSensorsSamplePlayerController::StaticClass());
	TestNotNull(TEXT("The sample game mode is reflected"),
		AOpenMobileSensorsSampleGameMode::StaticClass());
	TestNotNull(TEXT("The Blueprint request recipe is reflected"),
		UOpenMobileSensorsBlueprintExamples::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileSensorsBlueprintExamples,
				MakeLowRateRequest
			)
		));
	TestFalse(TEXT("The sample documents the Unreal axis convention"),
		UOpenMobileSensorsBlueprintExamples::GetAxisConvention().IsEmpty());
	TestFalse(TEXT("The sample includes Blueprint control recipes"),
		UOpenMobileSensorsBlueprintExamples::GetBlueprintRecipes().IsEmpty());
	return true;
}

#endif
