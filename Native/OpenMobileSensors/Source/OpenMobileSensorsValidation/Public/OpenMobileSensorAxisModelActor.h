#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "OpenMobileSensorListener.h"
#include "OpenMobileSensorAxisModelActor.generated.h"

class UArrowComponent;
class USceneComponent;

UENUM(BlueprintType)
enum class EOpenMobileSensorAxisConnectionOutcome : uint8
{
	Connected UMETA(DisplayName = "Connected", ToolTip = "The supported listener is connected to its validation arrow."),
	InvalidListener UMETA(DisplayName = "Invalid Listener", ToolTip = "The listener reference is null or invalid."),
	UnsupportedListener UMETA(DisplayName = "Unsupported Listener", ToolTip = "The axis validator accepts only typed Gravity and Gyroscope listeners.")
};

UCLASS(BlueprintType, meta = (DisplayName = "OpenMobile Sensor Axis Validator"))
class OPENMOBILESENSORSVALIDATION_API AOpenMobileSensorAxisModelActor final
	: public AActor
{
	GENERATED_BODY()

public:
	AOpenMobileSensorAxisModelActor();

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Validation", meta = (DisplayName = "Set Validation Sample Vectors", Keywords = "OpenMobile sensors validation axes gravity gyroscope vector", ToolTip = "Updates the gravity and angular-velocity arrows using normalized OpenMobile sensor units."))
	void SetSampleVectors(
		UPARAM(DisplayName = "Gravity (m/s2)") const FVector& Gravity,
		UPARAM(DisplayName = "Angular Velocity (rad/s)") const FVector& AngularVelocity
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Validation", meta = (DisplayName = "Connect Sensor Listener", ExpandEnumAsExecs = "Outcome", Keywords = "OpenMobile sensors validation connect gravity gyroscope listener", ToolTip = "Connects a typed Gravity or Gyroscope listener for automatic validation-arrow updates. Call once for each listener."))
	void ConnectSensorListener(
		UOpenMobileSensorListener* Listener,
		EOpenMobileSensorAxisConnectionOutcome& Outcome,
		FText& Message
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Validation", meta = (DisplayName = "Disconnect Sensor Listeners", Keywords = "OpenMobile sensors validation disconnect cleanup", ToolTip = "Disconnects the current Gravity and Gyroscope listeners from this validator."))
	void DisconnectSensorListeners();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Validation", meta = (ToolTip = "Device-forward validation axis, shown in red."))
	TObjectPtr<UArrowComponent> XAxis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Validation", meta = (ToolTip = "Device-right validation axis, shown in green."))
	TObjectPtr<UArrowComponent> YAxis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Validation", meta = (ToolTip = "Device-out validation axis, shown in blue."))
	TObjectPtr<UArrowComponent> ZAxis;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Validation", meta = (ToolTip = "Latest gravity vector in metres per second squared, shown in yellow."))
	TObjectPtr<UArrowComponent> GravityArrow;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "OpenMobile|Sensors|Validation", meta = (ToolTip = "Latest angular-velocity vector in radians per second, shown in cyan."))
	TObjectPtr<UArrowComponent> AngularVelocityArrow;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void HandleGravitySample(const FVector& Gravity);
	void HandleGyroscopeSample(const FVector& AngularVelocity);

	void SetOverlayVector(UArrowComponent& Arrow, const FVector& Value);

	UPROPERTY()
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileGravityListener> ConnectedGravityListener;

	UPROPERTY(Transient)
	TObjectPtr<UOpenMobileGyroscopeListener> ConnectedGyroscopeListener;

	FDelegateHandle GravitySampleHandle;
	FDelegateHandle GyroscopeSampleHandle;
};
