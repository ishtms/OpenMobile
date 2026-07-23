#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticsSampleRecipes.generated.h"

class UOpenMobileHapticsSubsystem;

UCLASS()
class OPENMOBILEHAPTICSSAMPLEHOST_API UOpenMobileHapticsSampleRecipes final
	: public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "OpenMobile Sample|Haptics", meta = (WorldContext = "WorldContextObject"))
	static UOpenMobileHapticsSubsystem* GetHapticsSubsystem(
		UObject* WorldContextObject
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile Sample|Haptics", meta = (WorldContext = "WorldContextObject"))
	static FOpenMobileHapticPlaybackResult PlayPreparedPattern(
		UObject* WorldContextObject,
		FName PatternName,
		float Intensity,
		FName Channel,
		EOpenMobileHapticFallbackPolicy FallbackPolicy =
			EOpenMobileHapticFallbackPolicy::Automatic
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile Sample|Haptics", meta = (WorldContext = "WorldContextObject"))
	static FOpenMobileHapticPlaybackResult StartBoundedVehicleFeedback(
		UObject* WorldContextObject,
		float Intensity = 0.45f
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile Sample|Haptics", meta = (WorldContext = "WorldContextObject"))
	static FOpenMobileHapticPlaybackResult ScheduleWithAudioClock(
		UObject* WorldContextObject,
		FName PatternName,
		double AudioClockSeconds,
		double DelaySeconds = 0.25
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile Sample|Haptics", meta = (WorldContext = "WorldContextObject"))
	static FOpenMobileHapticPlaybackResult PlayAccessibilityConfirmation(
		UObject* WorldContextObject,
		float Intensity = 0.35f
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile Sample|Haptics", meta = (WorldContext = "WorldContextObject"))
	static FOpenMobileHapticControlResult CancelSamplePlayback(
		UObject* WorldContextObject,
		FOpenMobileHapticPlaybackHandle Handle
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile Sample|Haptics", meta = (WorldContext = "WorldContextObject"))
	static FOpenMobileHapticControlResult StopSampleChannel(
		UObject* WorldContextObject,
		FName Channel
	);

	UFUNCTION(BlueprintPure, Category = "OpenMobile Sample|Haptics")
	static bool HasRichHaptics(
		const FOpenMobileHapticCapabilities& Capabilities
	);
};
