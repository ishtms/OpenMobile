#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticPatternAsset.generated.h"

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCookedCurvePoint
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Control-point time in integer microseconds relative to its curve start."))
	uint32 RelativeTimeMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Normalized control value quantized to an unsigned 16-bit integer."))
	uint16 Value = MAX_uint16;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCookedParameterCurve
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Parameter changed by this compiled curve."))
	EOpenMobileHapticCurveParameter Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Compiled curve start in integer microseconds from pattern start."))
	uint32 StartTimeMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Compiled, ordered control points for this parameter curve."))
	TArray<FOpenMobileHapticCookedCurvePoint> ControlPoints;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCookedPatternEvent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Compiled transient or continuous event type."))
	EOpenMobileHapticPatternEventType Type =
		EOpenMobileHapticPatternEventType::Transient;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Compiled event start in integer microseconds from pattern start."))
	uint32 StartTimeMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Compiled event duration in integer microseconds. Transient events use zero."))
	uint32 DurationMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Normalized event intensity quantized to an unsigned 16-bit integer."))
	uint16 Intensity = MAX_uint16;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Normalized event sharpness quantized to an unsigned 16-bit integer."))
	uint16 Sharpness = MAX_uint16 / 2;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Normalized frequency intent quantized to an unsigned 16-bit integer."))
	uint16 FrequencyIntent = MAX_uint16 / 2;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCookedPatternData
{
	GENERATED_BODY()

	static constexpr uint8 CurrentFormatVersion = 3;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Serialization format version for the compiled pattern payload."))
	uint8 DataFormatVersion = CurrentFormatVersion;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Hash of the authored pattern and playback metadata used to detect stale derived data."))
	uint32 SourceHash = 0;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Total compiled pattern duration in integer microseconds."))
	uint32 DurationMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Timing granularity in integer microseconds used during compilation."))
	uint32 GranularityMicroseconds = 1000;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Deterministic compiled event sequence submitted to platform backends."))
	TArray<FOpenMobileHapticCookedPatternEvent> Events;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Deterministic compiled parameter curves submitted to capable backends."))
	TArray<FOpenMobileHapticCookedParameterCurve> ParameterCurves;

	bool Serialize(FArchive& Archive);
	void Reset();
};

template<>
struct TStructOpsTypeTraits<FOpenMobileHapticCookedPatternData>
	: public TStructOpsTypeTraitsBase2<FOpenMobileHapticCookedPatternData>
{
	enum
	{
		WithSerializer = true
	};
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticPatternMarker
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ToolTip = "Editor-only marker name for animation, audio, or gameplay alignment."))
	FName Name;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Marker time in seconds from the start of the portable pattern."))
	double TimeSeconds = 0.0;
};

UCLASS(BlueprintType)
class OPENMOBILEHAPTICS_API UOpenMobileHapticPatternAsset
	: public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Metadata", meta = (ClampMin = "1", ToolTip = "Author-controlled content version used by project migration and review tools."))
	int32 PatternVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Usage", meta = (ToolTip = "Marks content reviewed for frequent repetition and comfort-sensitive use."))
	bool bSuitableForFrequentRepetition = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Usage", meta = (ToolTip = "Marks content reviewed for accessibility-sensitive contexts."))
	bool bSuitableForAccessibilitySensitiveUse = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Usage", meta = (ToolTip = "Marks content reviewed for supported background alert playback."))
	bool bSuitableForBackgroundPlayback = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Player-policy category used when a request does not supply an explicit category."))
	FName DefaultCategory = TEXT("Gameplay");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Base request priority used for overlap, interruption, and critical-feedback policy."))
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Default behavior when this asset overlaps active work on the same channel."))
	EOpenMobileHapticOverlapPolicy OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Default bounded loop policy. Use a purpose-built loop constructor for request overrides."))
	FOpenMobileHapticLoopOptions Loop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Playback", meta = (ToolTip = "Default fallback policy when the exact platform or rich representation cannot play."))
	EOpenMobileHapticFallbackPolicy FallbackPolicy =
		EOpenMobileHapticFallbackPolicy::Automatic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Fallback", meta = (ToolTip = "Lowest output quality this asset permits before the request is rejected."))
	EOpenMobileHapticFallbackFloor LowestAllowedFallback =
		EOpenMobileHapticFallbackFloor::BasicVibration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Fallback", meta = (ToolTip = "Optional platform primitive or preset name used before semantic or basic fallback."))
	FName PrimitiveOrPresetFallback;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Fallback", meta = (ToolTip = "Allows the selected portable semantic effect when richer representations are unavailable."))
	bool bAllowSemanticFallback = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "OpenMobile|Haptics|Fallback", meta = (EditCondition = "bAllowSemanticFallback", EditConditionHides, ToolTip = "Portable semantic effect used when semantic fallback is enabled."))
	EOpenMobileHapticSemanticEffect SemanticFallback =
		EOpenMobileHapticSemanticEffect::Click;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ToolTip = "Editor-authored portable event and parameter-curve source. Runtime graphs play the asset, not this internal struct."))
	FOpenMobileHapticPattern SourcePattern;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Pattern", meta = (ToolTip = "Optional named timeline markers for aligning related authored content."))
	TArray<FOpenMobileHapticPatternMarker> Markers;
#endif

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Platform Overrides", meta = (ToolTip = "Optional Android-native representation preferred when supported and prepared."))
	TSoftObjectPtr<UOpenMobileHapticAndroidPatternAsset> AndroidOverride;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Platform Overrides", meta = (ToolTip = "Optional iOS AHAP representation preferred when supported and prepared."))
	TSoftObjectPtr<UOpenMobileHapticIOSPatternAsset> IOSOverride;

	FSoftObjectPath GetOverrideForPlatform(
		EOpenMobileHapticOverridePlatform Platform
	) const;
	FSoftObjectPath GetOverrideForCurrentPlatform() const;

	const FOpenMobileHapticCookedPatternData& GetCookedPattern() const
	{
		return CookedPattern;
	}

	bool IsDerivedDataCurrent() const;
	bool RebuildDerivedData(TArray<FString>& Errors);

#if !UE_BUILD_SHIPPING
	bool InitializeCookedPreviewData(
		const FOpenMobileHapticCookedPatternData& InCookedPattern,
		const FOpenMobileHapticLoopOptions& InLoop,
		FName InCategory,
		EOpenMobileHapticFallbackPolicy InFallbackPolicy,
		EOpenMobileHapticFallbackFloor InLowestAllowedFallback,
		FName InPrimitiveOrPresetFallback,
		bool bInAllowSemanticFallback,
		EOpenMobileHapticSemanticEffect InSemanticFallback
	);
#endif

#if WITH_EDITORONLY_DATA
	void NormalizeEditorData();
#endif

	virtual void PreSave(FObjectPreSaveContext SaveContext) override;

#if WITH_EDITOR
	bool ValidateForEditor(TArray<FString>& Errors) const;
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent
	) override;
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context
	) const override;
#endif

private:
	uint32 ComputeSourceHash() const;
	bool ValidateMetadata(TArray<FString>& Errors) const;
	bool ValidatePlatformOverrides(TArray<FString>& Errors) const;

#if WITH_EDITOR
	bool ValidateEditorData(TArray<FString>& Errors) const;
#endif

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|Cooked Pattern", meta = (ToolTip = "Read-only deterministic runtime data rebuilt from the editor source before save."))
	FOpenMobileHapticCookedPatternData CookedPattern;

#if !UE_BUILD_SHIPPING
	bool bHasCookedPreviewData = false;
#endif
};
