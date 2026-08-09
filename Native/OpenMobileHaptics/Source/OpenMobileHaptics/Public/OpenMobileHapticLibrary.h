#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticLibrary.generated.h"

class UOpenMobileHapticPatternAsset;

USTRUCT()
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

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Patterns", meta = (ToolTip = "Stable configured alias used by typed pattern identifiers."))
	FName Name;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Patterns", meta = (ToolTip = "Portable Haptic Pattern asset resolved when this library is prepared."))
	TSoftObjectPtr<UOpenMobileHapticPatternAsset> Pattern;
};

USTRUCT()
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

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Game Presets", meta = (ToolTip = "Stable game preset whose configured pattern should be preferred."))
	EOpenMobileHapticGamePreset Preset =
		EOpenMobileHapticGamePreset::Confirm;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Game Presets", meta = (ToolTip = "Pattern alias from this library used before the preset semantic fallback."))
	FName PatternName;
};

UCLASS()
class OPENMOBILEHAPTICS_API UOpenMobileHapticLibrary : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Metadata", meta = (ClampMin = "1", ToolTip = "Author-controlled content version used by project migration and review tools."))
	int32 LibraryVersion = 1;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Patterns", meta = (TitleProperty = "Name", ToolTip = "Named portable patterns made available during library preparation."))
	TArray<FOpenMobileHapticLibraryEntry> Patterns;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Game Presets", meta = (TitleProperty = "Preset", ToolTip = "Optional prepared-pattern overrides for stable game preset nodes."))
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
