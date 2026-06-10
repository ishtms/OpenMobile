#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticLibrary.generated.h"

class UOpenMobileHapticPatternAsset;

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticLibraryEntry
{
	GENERATED_BODY()

	FOpenMobileHapticLibraryEntry() = default;
	FOpenMobileHapticLibraryEntry(
		FName InName,
		TSoftObjectPtr<UOpenMobileHapticPatternAsset> InPattern
	)
		: Name(InName)
		, Pattern(MoveTemp(InPattern))
	{
	}

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patterns")
	FName Name;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patterns")
	TSoftObjectPtr<UOpenMobileHapticPatternAsset> Pattern;
};

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
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Patterns")
	TArray<FOpenMobileHapticLibraryEntry> Patterns;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Game Presets")
	TArray<FOpenMobileHapticGamePresetOverride> GamePresetOverrides;

	bool BuildPatternLookup(
		TMap<FName, FSoftObjectPath>& OutPatterns,
		TArray<FString>& Errors
	) const;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context
	) const override;
#endif

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
