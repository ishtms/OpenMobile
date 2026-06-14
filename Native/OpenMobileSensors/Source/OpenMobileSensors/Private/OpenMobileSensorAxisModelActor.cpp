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
	GravityArrow = CreateDefaultSubobject<UArrowComponent>(
		TEXT("Gravity")
	);
	AngularVelocityArrow = CreateDefaultSubobject<UArrowComponent>(
		TEXT("AngularVelocity")
	);

	using namespace OpenMobileSensorAxisModelActorPrivate;
	ConfigureArrow(*XAxis, *SceneRoot, FVector::ForwardVector, FLinearColor::Red);
	ConfigureArrow(*YAxis, *SceneRoot, FVector::RightVector, FLinearColor::Green);
	ConfigureArrow(*ZAxis, *SceneRoot, FVector::UpVector, FLinearColor::Blue);
	ConfigureArrow(
		*GravityArrow,
		*SceneRoot,
		-FVector::UpVector,
		FLinearColor::Yellow
	);
	ConfigureArrow(
		*AngularVelocityArrow,
		*SceneRoot,
		FVector::UpVector,
		FLinearColor(0.0f, 1.0f, 1.0f)
	);
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
