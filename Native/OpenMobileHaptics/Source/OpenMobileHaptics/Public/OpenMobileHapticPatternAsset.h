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

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint32 RelativeTimeMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint16 Value = MAX_uint16;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCookedParameterCurve
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	EOpenMobileHapticCurveParameter Parameter =
		EOpenMobileHapticCurveParameter::IntensityControl;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint32 StartTimeMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticCookedCurvePoint> ControlPoints;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCookedPatternEvent
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	EOpenMobileHapticPatternEventType Type =
		EOpenMobileHapticPatternEventType::Transient;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint32 StartTimeMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint32 DurationMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint16 Intensity = MAX_uint16;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint16 Sharpness = MAX_uint16 / 2;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint16 FrequencyIntent = MAX_uint16 / 2;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticCookedPatternData
{
	GENERATED_BODY()

	static constexpr uint8 CurrentFormatVersion = 3;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint8 DataFormatVersion = CurrentFormatVersion;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint32 SourceHash = 0;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint32 DurationMicroseconds = 0;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	uint32 GranularityMicroseconds = 1000;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
	TArray<FOpenMobileHapticCookedPatternEvent> Events;

	UPROPERTY(VisibleAnywhere, Category = "Open Mobile|Haptics")
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

	UPROPERTY(EditAnywhere, Category = "Open Mobile|Haptics")
	FName Name;

	UPROPERTY(EditAnywhere, Category = "Open Mobile|Haptics", meta = (ClampMin = "0.0", Units = "s"))
	double TimeSeconds = 0.0;
};

UCLASS(BlueprintType)
class OPENMOBILEHAPTICS_API UOpenMobileHapticPatternAsset
	: public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Metadata", meta = (ClampMin = "1"))
	int32 PatternVersion = 1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Usage")
	bool bSuitableForFrequentRepetition = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Usage")
	bool bSuitableForAccessibilitySensitiveUse = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Usage")
	bool bSuitableForBackgroundPlayback = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	FName DefaultCategory = TEXT("Gameplay");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	EOpenMobileHapticChannelPriority Priority =
		EOpenMobileHapticChannelPriority::Normal;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	EOpenMobileHapticOverlapPolicy OverlapPolicy =
		EOpenMobileHapticOverlapPolicy::Replace;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	FOpenMobileHapticLoopOptions Loop;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Playback")
	EOpenMobileHapticFallbackPolicy FallbackPolicy =
		EOpenMobileHapticFallbackPolicy::Automatic;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fallback")
	EOpenMobileHapticFallbackFloor LowestAllowedFallback =
		EOpenMobileHapticFallbackFloor::BasicVibration;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fallback")
	FName PrimitiveOrPresetFallback;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fallback")
	bool bAllowSemanticFallback = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Fallback", meta = (EditCondition = "bAllowSemanticFallback"))
	EOpenMobileHapticSemanticEffect SemanticFallback =
		EOpenMobileHapticSemanticEffect::Click;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, Category = "Pattern")
	FOpenMobileHapticPattern SourcePattern;

	UPROPERTY(EditAnywhere, Category = "Pattern")
	TArray<FOpenMobileHapticPatternMarker> Markers;
#endif

	UPROPERTY(EditAnywhere, Category = "Platform Overrides")
	TSoftObjectPtr<UOpenMobileHapticAndroidPatternAsset> AndroidOverride;

	UPROPERTY(EditAnywhere, Category = "Platform Overrides")
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

	UPROPERTY(VisibleAnywhere, Category = "Cooked Pattern")
	FOpenMobileHapticCookedPatternData CookedPattern;

#if !UE_BUILD_SHIPPING
	bool bHasCookedPreviewData = false;
#endif
};
