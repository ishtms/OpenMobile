#include "OpenMobileSensorAxisModelActor.h"

#include "Components/ArrowComponent.h"
#include "Components/SceneComponent.h"

namespace OpenMobileSensorAxisModelActorPrivate
{
	void ConfigureArrow(
		UArrowComponent& Arrow,
		USceneComponent& Parent,
		const FVector& Direction,
		const FLinearColor& Color
	)
	{
		Arrow.SetupAttachment(&Parent);
		Arrow.SetArrowColor(Color);
		Arrow.SetArrowLength(80.0f);
		Arrow.SetArrowSize(1.5f);
		Arrow.SetHiddenInGame(false);
		Arrow.SetRelativeRotation(
			FRotationMatrix::MakeFromX(Direction).Rotator()
		);
	}
}

AOpenMobileSensorAxisModelActor::AOpenMobileSensorAxisModelActor()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(SceneRoot);
	XAxis = CreateDefaultSubobject<UArrowComponent>(TEXT("XAxis"));
	YAxis = CreateDefaultSubobject<UArrowComponent>(TEXT("YAxis"));
	ZAxis = CreateDefaultSubobject<UArrowComponent>(TEXT("ZAxis"));
	GravityArrow = CreateDefaultSubobject<UArrowComponent>(TEXT("Gravity"));
	AngularVelocityArrow = CreateDefaultSubobject<UArrowComponent>(
		TEXT("AngularVelocity")
	);

	using namespace OpenMobileSensorAxisModelActorPrivate;
	ConfigureArrow(*XAxis, *SceneRoot, FVector::ForwardVector, FLinearColor::Red);
	ConfigureArrow(*YAxis, *SceneRoot, FVector::RightVector, FLinearColor::Green);
	ConfigureArrow(*ZAxis, *SceneRoot, FVector::UpVector, FLinearColor::Blue);
	ConfigureArrow(*GravityArrow, *SceneRoot, -FVector::UpVector,
		FLinearColor::Yellow);
	ConfigureArrow(*AngularVelocityArrow, *SceneRoot, FVector::UpVector,
		FLinearColor(0.0f, 1.0f, 1.0f));
	GravityArrow->SetVisibility(false);
	AngularVelocityArrow->SetVisibility(false);
}

void AOpenMobileSensorAxisModelActor::SetSampleVectors(
	const FVector& Gravity,
	const FVector& AngularVelocity
)
{
	SetOverlayVector(*GravityArrow, Gravity);
	SetOverlayVector(*AngularVelocityArrow, AngularVelocity);
}

void AOpenMobileSensorAxisModelActor::ConnectSensorListener(
	UOpenMobileSensorListener* Listener,
	EOpenMobileSensorAxisConnectionOutcome& Outcome,
	FText& Message
)
{
	if (!IsValid(Listener))
	{
		Outcome = EOpenMobileSensorAxisConnectionOutcome::InvalidListener;
		Message = NSLOCTEXT("OpenMobileSensorsValidation", "InvalidListener",
			"Provide a valid typed Gravity or Gyroscope listener.");
		return;
	}
	if (UOpenMobileGravityListener* Gravity =
		Cast<UOpenMobileGravityListener>(Listener))
	{
		if (ConnectedGravityListener)
		{
			ConnectedGravityListener->OnSampleNative().Remove(
				GravitySampleHandle);
		}
		ConnectedGravityListener = Gravity;
		GravitySampleHandle = ConnectedGravityListener->OnSampleNative().AddUObject(
			this,
			&AOpenMobileSensorAxisModelActor::HandleGravitySample
		);
		Outcome = EOpenMobileSensorAxisConnectionOutcome::Connected;
		Message = NSLOCTEXT("OpenMobileSensorsValidation", "GravityConnected",
			"The Gravity listener is connected.");
		return;
	}
	if (UOpenMobileGyroscopeListener* Gyroscope =
		Cast<UOpenMobileGyroscopeListener>(Listener))
	{
		if (ConnectedGyroscopeListener)
		{
			ConnectedGyroscopeListener->OnSampleNative().Remove(
				GyroscopeSampleHandle);
		}
		ConnectedGyroscopeListener = Gyroscope;
		GyroscopeSampleHandle =
			ConnectedGyroscopeListener->OnSampleNative().AddUObject(
			this,
			&AOpenMobileSensorAxisModelActor::HandleGyroscopeSample
		);
		Outcome = EOpenMobileSensorAxisConnectionOutcome::Connected;
		Message = NSLOCTEXT("OpenMobileSensorsValidation", "GyroscopeConnected",
			"The Gyroscope listener is connected.");
		return;
	}
	Outcome = EOpenMobileSensorAxisConnectionOutcome::UnsupportedListener;
	Message = NSLOCTEXT("OpenMobileSensorsValidation", "UnsupportedListener",
		"This validator accepts typed Gravity and Gyroscope listeners only.");
}

void AOpenMobileSensorAxisModelActor::DisconnectSensorListeners()
{
	if (ConnectedGravityListener)
	{
		ConnectedGravityListener->OnSampleNative().Remove(GravitySampleHandle);
		ConnectedGravityListener = nullptr;
		GravitySampleHandle.Reset();
	}
	if (ConnectedGyroscopeListener)
	{
		ConnectedGyroscopeListener->OnSampleNative().Remove(
			GyroscopeSampleHandle);
		ConnectedGyroscopeListener = nullptr;
		GyroscopeSampleHandle.Reset();
	}
}

void AOpenMobileSensorAxisModelActor::EndPlay(
	const EEndPlayReason::Type EndPlayReason
)
{
	DisconnectSensorListeners();
	Super::EndPlay(EndPlayReason);
}

void AOpenMobileSensorAxisModelActor::HandleGravitySample(
	const FVector& Gravity
)
{
	SetOverlayVector(*GravityArrow, Gravity);
}

void AOpenMobileSensorAxisModelActor::HandleGyroscopeSample(
	const FVector& AngularVelocity
)
{
	SetOverlayVector(*AngularVelocityArrow, AngularVelocity);
}

void AOpenMobileSensorAxisModelActor::SetOverlayVector(
	UArrowComponent& Arrow,
	const FVector& Value
)
{
	const bool bVisible = !Value.IsNearlyZero() && !Value.ContainsNaN();
	Arrow.SetVisibility(bVisible);
	if (!bVisible)
	{
		return;
	}
	Arrow.SetRelativeRotation(
		FRotationMatrix::MakeFromX(Value.GetSafeNormal()).Rotator()
	);
	Arrow.SetArrowLength(static_cast<float>(FMath::Clamp(
		Value.Size() * 10.0,
		20.0,
		200.0
	)));
}
