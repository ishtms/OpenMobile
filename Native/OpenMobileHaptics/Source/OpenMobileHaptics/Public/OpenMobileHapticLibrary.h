#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticLibrary.generated.h"

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticGamePresetOverride
{
	GENERATED_BODY()

	FOpenMobileHapticGamePresetOverride() = default;
	FOpenMobileHapticGamePresetOverride(
		EOpenMobileHapticGamePreset InPreset,
		FName InPatternName
	)
		: Preset(InPreset)
		, PatternName(InPatternName)
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Presets")
	EOpenMobileHapticGamePreset Preset =
		EOpenMobileHapticGamePreset::Confirm;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Presets")
	FName PatternName;
};

UCLASS(BlueprintType)
class OPENMOBILEHAPTICS_API UOpenMobileHapticLibrary : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Presets")
	TArray<FOpenMobileHapticGamePresetOverride> GamePresetOverrides;

	bool FindGamePresetOverride(
		EOpenMobileHapticGamePreset Preset,
		FName& OutPatternName
	) const
	{
		for (const FOpenMobileHapticGamePresetOverride& Override :
			GamePresetOverrides)
		{
			if (Override.Preset == Preset && !Override.PatternName.IsNone())
			{
				OutPatternName = Override.PatternName;
				return true;
			}
		}
		return false;
	}
};
