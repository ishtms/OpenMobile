#if WITH_DEV_AUTOMATION_TESTS

#include "Components/ArrowComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Misc/AutomationTest.h"
#include "OpenMobileSensorAxisModelActor.h"
#include "UObject/Package.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileSensorsAxisModelBlueprintContractTest,
	"OpenMobile.Sensors.Validation.AxisModelBlueprintContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileSensorsAxisModelBlueprintContractTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	TestEqual(TEXT("The axis actor lives in the validation module"),
		AOpenMobileSensorAxisModelActor::StaticClass()->GetOutermost()->GetName(),
		FString(TEXT("/Script/OpenMobileSensorsValidation")));
#if WITH_METADATA
	const UFunction* SetVectorsFunction =
		AOpenMobileSensorAxisModelActor::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				AOpenMobileSensorAxisModelActor,
				SetSampleVectors));
	TestNotNull(TEXT("The vector update node is reflected"),
		SetVectorsFunction);
	if (SetVectorsFunction)
	{
		TestEqual(TEXT("The vector node has a guided display name"),
			SetVectorsFunction->GetMetaData(TEXT("DisplayName")),
			FString(TEXT("Set Validation Sample Vectors")));
		const FProperty* Gravity = FindFProperty<FProperty>(
			SetVectorsFunction, TEXT("Gravity"));
		const FProperty* AngularVelocity = FindFProperty<FProperty>(
			SetVectorsFunction, TEXT("AngularVelocity"));
		TestNotNull(TEXT("The gravity pin is reflected"), Gravity);
		TestNotNull(TEXT("The angular-velocity pin is reflected"),
			AngularVelocity);
		if (Gravity && AngularVelocity)
		{
			TestEqual(TEXT("The gravity pin names its unit"),
				Gravity->GetMetaData(TEXT("DisplayName")),
				FString(TEXT("Gravity (m/s2)")));
			TestEqual(TEXT("The gyroscope pin names its unit"),
				AngularVelocity->GetMetaData(TEXT("DisplayName")),
				FString(TEXT("Angular Velocity (rad/s)")));
		}
	}
	const UFunction* ConnectFunction =
		AOpenMobileSensorAxisModelActor::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				AOpenMobileSensorAxisModelActor,
				ConnectSensorListener));
	TestNotNull(TEXT("The guided listener connection is reflected"),
		ConnectFunction);
	if (ConnectFunction)
	{
		TestEqual(TEXT("Connection outcomes become execution pins"),
			ConnectFunction->GetMetaData(TEXT("ExpandEnumAsExecs")),
			FString(TEXT("Outcome")));
		TestTrue(TEXT("The connection tooltip names supported listeners"),
			ConnectFunction->GetToolTipText().ToString().Contains(
				TEXT("Gravity or Gyroscope")));
	}
	TestNotNull(TEXT("Validation listeners can be disconnected explicitly"),
		AOpenMobileSensorAxisModelActor::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				AOpenMobileSensorAxisModelActor,
				DisconnectSensorListeners)));
#endif
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(
		TEXT("OpenMobileSensorsAxisValidation"));
	UWorld* World = GameInstance->GetWorld();
	AOpenMobileSensorAxisModelActor* Actor =
		World->SpawnActor<AOpenMobileSensorAxisModelActor>();
	TestTrue(TEXT("The validation actor spawns"), IsValid(Actor));
	Actor->SetSampleVectors(FVector::ForwardVector, FVector::RightVector);
	TestTrue(TEXT("Direct gravity updates reach the arrow"),
		Actor->GravityArrow->GetVisibleFlag());
	TestTrue(TEXT("Direct gyroscope updates reach the arrow"),
		Actor->AngularVelocityArrow->GetVisibleFlag());
	Actor->SetSampleVectors(FVector::ZeroVector, FVector::ZeroVector);
	UOpenMobileGravityListener* GravityListener =
		NewObject<UOpenMobileGravityListener>(World);
	UOpenMobileGyroscopeListener* GyroscopeListener =
		NewObject<UOpenMobileGyroscopeListener>(World);
	EOpenMobileSensorAxisConnectionOutcome Outcome =
		EOpenMobileSensorAxisConnectionOutcome::InvalidListener;
	FText Message;
	Actor->ConnectSensorListener(GravityListener, Outcome, Message);
	TestEqual(TEXT("A Gravity listener connects"), Outcome,
		EOpenMobileSensorAxisConnectionOutcome::Connected);
	TestTrue(TEXT("The Gravity delegate is bound"),
		GravityListener->OnSampleNative().IsBound());
	Actor->ConnectSensorListener(GyroscopeListener, Outcome, Message);
	TestEqual(TEXT("A Gyroscope listener connects"), Outcome,
		EOpenMobileSensorAxisConnectionOutcome::Connected);
	TestTrue(TEXT("The Gyroscope delegate is bound"),
		GyroscopeListener->OnSampleNative().IsBound());
	FOpenMobileSensorSampleInfo SampleInfo;
	static_cast<void>(SampleInfo);
	GravityListener->OnSampleNative().Broadcast(FVector::ForwardVector);
	GyroscopeListener->OnSampleNative().Broadcast(FVector::RightVector);
	TestTrue(TEXT("Gravity samples reveal the gravity arrow"),
		Actor->GravityArrow->GetVisibleFlag());
	TestTrue(TEXT("Gravity samples orient the gravity arrow"),
		Actor->GravityArrow->GetRelativeRotation().Vector().Equals(
			FVector::ForwardVector));
	TestTrue(TEXT("Gyroscope samples reveal the angular-velocity arrow"),
		Actor->AngularVelocityArrow->GetVisibleFlag());
	TestTrue(TEXT("Gyroscope samples orient the angular-velocity arrow"),
		Actor->AngularVelocityArrow->GetRelativeRotation().Vector().Equals(
			FVector::RightVector));
	const FRotator GravityRotation =
		Actor->GravityArrow->GetRelativeRotation();
	Actor->DisconnectSensorListeners();
	GravityListener->OnSampleNative().Broadcast(FVector::UpVector);
	TestEqual(TEXT("Disconnect stops automatic arrow updates"),
		Actor->GravityArrow->GetRelativeRotation(), GravityRotation);
	Actor->ConnectSensorListener(nullptr, Outcome, Message);
	TestEqual(TEXT("Null listeners have an explicit outcome"), Outcome,
		EOpenMobileSensorAxisConnectionOutcome::InvalidListener);
	UOpenMobileAccelerometerListener* UnsupportedListener =
		NewObject<UOpenMobileAccelerometerListener>(World);
	Actor->ConnectSensorListener(UnsupportedListener, Outcome, Message);
	TestEqual(TEXT("Unsupported listeners have an explicit outcome"), Outcome,
		EOpenMobileSensorAxisConnectionOutcome::UnsupportedListener);
	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	return true;
}

#endif
