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

	/** Unreal needs the empty form when it builds reflected arrays and loads saved library entries. */
	FOpenMobileHapticLibraryEntry() = default;

	/** Use this when you're building an entry in code and want the alias and soft asset reference kept together. */
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

	/** Unreal needs this empty form while it reconstructs reflected override arrays from an asset. */
	FOpenMobileHapticGamePresetOverride() = default;

	/** Use this for code-built overrides so a preset can't be added without its configured pattern name. */
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

	/** Builds the runtime alias table and reports bad or repeated names together, so preparation can fail before playback starts. */
	bool BuildPatternLookup(
		TMap<FName, FSoftObjectPath>& OutPatterns,
		TArray<FString>& Errors
	) const;

#if WITH_EDITOR
	/** Runs the same library checks in the editor, which gives authors the errors before this asset gets cooked. */
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context
	) const override;
#endif

	/** Resolves only usable overrides and leaves the output alone when this library has no answer for the preset. */
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
