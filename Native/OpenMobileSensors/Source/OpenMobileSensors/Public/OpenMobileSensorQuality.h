#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorQuality.generated.h"

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileSensorSourceFlags : uint8
{
	None = 0,
	Raw = 1 << 0,
	CalibratedNative = 1 << 1,
	NativeFused = 1 << 2,
	PluginDerived = 1 << 3,
	MagneticNorthReferenced = 1 << 4,
	TrueNorthReferenced = 1 << 5,
	Mock = 1 << 6,
	Replay = 1 << 7
};
ENUM_CLASS_FLAGS(EOpenMobileSensorSourceFlags);

UENUM(BlueprintType)
enum class EOpenMobileSensorFusionQuality : uint8
{
	Unknown,
	Degraded,
	Nominal
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorFusionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorFusionQuality Quality =
		EOpenMobileSensorFusionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasNativeQualityReport = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	EOpenMobileSensorFusionQuality NativeQuality =
		EOpenMobileSensorFusionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	bool bHasEstimatedLag = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Sensors")
	double EstimatedLagSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced", meta = (AdvancedDisplay, ToolTip = "Raw expected-input bit mask. Prefer Get Expected Fusion Inputs or Break Sensor Fusion Context."))
	int64 ExpectedInputMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced", meta = (AdvancedDisplay, ToolTip = "Raw contributing-input bit mask. Prefer Get Contributing Fusion Inputs or Break Sensor Fusion Context."))
	int64 ContributingInputMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced", meta = (AdvancedDisplay, ToolTip = "Raw missing-input bit mask. Prefer Get Missing Fusion Inputs or Break Sensor Fusion Context."))
	int64 MissingInputMask = 0;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors|Advanced", meta = (AdvancedDisplay, ToolTip = "Raw degraded-input bit mask. Prefer Get Degraded Fusion Inputs or Break Sensor Fusion Context."))
	int64 DegradedInputMask = 0;
};

UCLASS()
class OPENMOBILESENSORS_API UOpenMobileSensorQualityLibrary final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors|Values", meta = (DisplayName = "Break Sensor Fusion Context", NativeBreakFunc, Keywords = "OpenMobile sensors fusion quality expected contributing missing degraded typed inputs", AdvancedDisplay = "bOutHasNativeQuality,OutNativeQuality,bOutHasEstimatedLag,OutEstimatedLagSeconds", ToolTip = "Breaks fusion quality into typed sensor arrays. Raw masks stay on the advanced struct path."))
	static void BreakSensorFusionContext(
		const FOpenMobileSensorFusionContext& Context,
		EOpenMobileSensorFusionQuality& OutQuality,
		bool& bOutHasNativeQuality,
		EOpenMobileSensorFusionQuality& OutNativeQuality,
		bool& bOutHasEstimatedLag,
		double& OutEstimatedLagSeconds,
		TArray<EOpenMobileSensorType>& OutExpectedInputs,
		TArray<EOpenMobileSensorType>& OutContributingInputs,
		TArray<EOpenMobileSensorType>& OutMissingInputs,
		TArray<EOpenMobileSensorType>& OutDegradedInputs);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Make Sensor Input Mask", ToolTip = "Returns the fixed fusion-input bit for one sensor type."))
	static int64 MakeInputMask(EOpenMobileSensorType Sensor);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Fusion Mask Contains Sensor", ToolTip = "Returns whether a fusion-input mask contains the sensor type."))
	static bool ContainsSensor(
		int64 InputMask,
		EOpenMobileSensorType Sensor
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Expected Fusion Inputs", ToolTip = "Returns the sensor types expected by the fusion result."))
	static TArray<EOpenMobileSensorType> GetExpectedInputs(
		const FOpenMobileSensorFusionContext& Context
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Contributing Fusion Inputs", ToolTip = "Returns the sensor types that contributed to the fusion result."))
	static TArray<EOpenMobileSensorType> GetContributingInputs(
		const FOpenMobileSensorFusionContext& Context
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Missing Fusion Inputs", ToolTip = "Returns expected sensor types that did not contribute."))
	static TArray<EOpenMobileSensorType> GetMissingInputs(
		const FOpenMobileSensorFusionContext& Context
	);

	UFUNCTION(BlueprintPure, Category = "Open Mobile|Sensors", meta = (DisplayName = "Get Degraded Fusion Inputs", ToolTip = "Returns available sensor types with invalid, low-quality, or calibration-blocked input."))
	static TArray<EOpenMobileSensorType> GetDegradedInputs(
		const FOpenMobileSensorFusionContext& Context
	);
};
