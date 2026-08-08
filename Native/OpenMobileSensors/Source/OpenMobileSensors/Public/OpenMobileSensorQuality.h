#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileSensorIdentifiers.h"
#include "OpenMobileSensorQuality.generated.h"

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileSensorSourceFlags : uint8
{
	None = 0 UMETA(DisplayName = "No Source Flags", ToolTip = "The provider did not report additional source provenance."),
	Raw = 1 << 0 UMETA(DisplayName = "Raw", ToolTip = "The sample comes from an uncalibrated or direct raw sensor stream."),
	CalibratedNative = 1 << 1 UMETA(DisplayName = "Calibrated Native", ToolTip = "The native platform reports this sample as calibrated."),
	NativeFused = 1 << 2 UMETA(DisplayName = "Native Fused", ToolTip = "The native platform fused multiple sensors to produce this sample."),
	PluginDerived = 1 << 3 UMETA(DisplayName = "Plugin Derived", ToolTip = "OpenMobile Sensors derived this sample from other sensor inputs."),
	MagneticNorthReferenced = 1 << 4 UMETA(DisplayName = "Magnetic North Referenced", ToolTip = "The sample's orientation or heading is referenced to magnetic north."),
	TrueNorthReferenced = 1 << 5 UMETA(DisplayName = "True North Referenced", ToolTip = "The sample's orientation or heading is referenced to true north."),
	Mock = 1 << 6 UMETA(DisplayName = "Development Mock", ToolTip = "A development mock provider produced this sample."),
	Replay = 1 << 7 UMETA(DisplayName = "Recording Replay", ToolTip = "A recording replay produced this sample.")
};
ENUM_CLASS_FLAGS(EOpenMobileSensorSourceFlags);

UENUM(BlueprintType)
enum class EOpenMobileSensorFusionQuality : uint8
{
	Unknown UMETA(DisplayName = "Unknown", ToolTip = "The provider did not report enough information to rate fusion quality."),
	Degraded UMETA(DisplayName = "Degraded", ToolTip = "The fused or derived value is usable with reduced confidence or known limitations."),
	Nominal UMETA(DisplayName = "Nominal", ToolTip = "The fused or derived value is operating at its expected quality.")
};

USTRUCT(BlueprintType)
struct OPENMOBILESENSORS_API FOpenMobileSensorFusionContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorFusionQuality Quality =
		EOpenMobileSensorFusionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasNativeQualityReport = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	EOpenMobileSensorFusionQuality NativeQuality =
		EOpenMobileSensorFusionQuality::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
	bool bHasEstimatedLag = false;

	UPROPERTY(BlueprintReadOnly, Category = "OpenMobile|Sensors")
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

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Make Sensor Input Mask", ToolTip = "Returns the fixed fusion-input bit for one sensor type."))
	static int64 MakeInputMask(EOpenMobileSensorType Sensor);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Fusion Mask Contains Sensor", ToolTip = "Returns whether a fusion-input mask contains the sensor type."))
	static bool ContainsSensor(
		int64 InputMask,
		EOpenMobileSensorType Sensor
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Expected Fusion Inputs", ToolTip = "Returns the sensor types expected by the fusion result."))
	static TArray<EOpenMobileSensorType> GetExpectedInputs(
		const FOpenMobileSensorFusionContext& Context
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Contributing Fusion Inputs", ToolTip = "Returns the sensor types that contributed to the fusion result."))
	static TArray<EOpenMobileSensorType> GetContributingInputs(
		const FOpenMobileSensorFusionContext& Context
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Missing Fusion Inputs", ToolTip = "Returns expected sensor types that did not contribute."))
	static TArray<EOpenMobileSensorType> GetMissingInputs(
		const FOpenMobileSensorFusionContext& Context
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile|Sensors", meta = (DisplayName = "Get Degraded Fusion Inputs", ToolTip = "Returns available sensor types with invalid, low-quality, or calibration-blocked input."))
	static TArray<EOpenMobileSensorType> GetDegradedInputs(
		const FOpenMobileSensorFusionContext& Context
	);
};
