#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticPlatformAssets.generated.h"

class UAssetImportData;

UENUM()
enum class EOpenMobileHapticOverridePlatform : uint8
{
	None UMETA(DisplayName = "No Override"),
	Android UMETA(DisplayName = "Android"),
	IOS UMETA(DisplayName = "iOS")
};

UENUM()
enum class EOpenMobileHapticAndroidPatternFormat : uint8
{
	Primitives UMETA(DisplayName = "Primitive Composition"),
	Waveform UMETA(DisplayName = "Waveform Steps"),
	BasicEnvelope UMETA(DisplayName = "Basic Envelope"),
	WaveformEnvelope UMETA(DisplayName = "Waveform Envelope")
};

UENUM()
enum class EOpenMobileHapticAndroidPrimitive : uint8
{
	Tick,
	LowTick,
	Click,
	Thud,
	Spin,
	QuickRise,
	SlowRise,
	QuickFall
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticAndroidPrimitiveStep
{
	GENERATED_BODY()

	FOpenMobileHapticAndroidPrimitiveStep() = default;
	FOpenMobileHapticAndroidPrimitiveStep(
		EOpenMobileHapticAndroidPrimitive InPrimitive,
		float InScale,
		int32 InDelayMilliseconds
	)
		: Primitive(InPrimitive)
		, Scale(InScale)
		, DelayMilliseconds(InDelayMilliseconds)
	{
	}

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Android", meta = (ToolTip = "Android predefined primitive played by this composition step."))
	EOpenMobileHapticAndroidPrimitive Primitive =
		EOpenMobileHapticAndroidPrimitive::Click;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Android", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized strength applied to this primitive step."))
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Android", meta = (ClampMin = "0", Units = "ms", ToolTip = "Delay in milliseconds before this primitive begins."))
	int32 DelayMilliseconds = 0;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticAndroidWaveformStep
{
	GENERATED_BODY()

	FOpenMobileHapticAndroidWaveformStep() = default;
	FOpenMobileHapticAndroidWaveformStep(
		int32 InDurationMilliseconds,
		int32 InAmplitude
	)
		: DurationMilliseconds(InDurationMilliseconds)
		, Amplitude(InAmplitude)
	{
	}

	UPROPERTY(
		EditAnywhere,
		Category = "OpenMobile|Haptics|Android",
		meta = (
			ClampMin = "0",
			Units = "ms",
			ToolTip = "How long this waveform step lasts. At least one step must have a positive duration."
		)
	)
	int32 DurationMilliseconds = 0;

	UPROPERTY(
		EditAnywhere,
		Category = "OpenMobile|Haptics|Android",
		meta = (
			ClampMin = "0",
			ClampMax = "255",
			ToolTip = "Vibration amplitude from 0 for silence through 255 for maximum output."
		)
	)
	int32 Amplitude = 255;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticAndroidEnvelopePoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Android", meta = (ClampMin = "0.0", Units = "s", ToolTip = "Envelope point time in seconds from pattern start."))
	float TimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Android", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized envelope amplitude at this point."))
	float Amplitude = 1.0f;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Android", meta = (ClampMin = "0.0", Units = "Hz", ToolTip = "Requested envelope frequency in hertz when the device supports it."))
	float FrequencyHz = 0.0f;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|Android", meta = (ClampMin = "0.0", ClampMax = "1.0", ToolTip = "Normalized sharpness intent at this envelope point."))
	float Sharpness = 0.5f;
};

USTRUCT()
struct OPENMOBILEHAPTICS_API FOpenMobileHapticIOSAudioResource
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (ToolTip = "Sanitized resource path relative to the imported AHAP document."))
	FString RelativePath;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (ToolTip = "Imported audio bytes kept internal to iOS asset cooking and playback."))
	TArray<uint8> Data;
};

UCLASS(Abstract)
class OPENMOBILEHAPTICS_API UOpenMobileHapticPlatformPatternAsset
	: public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	virtual EOpenMobileHapticOverridePlatform GetOverridePlatform() const
		PURE_VIRTUAL(
			UOpenMobileHapticPlatformPatternAsset::GetOverridePlatform,
			return EOpenMobileHapticOverridePlatform::None;
		);
	virtual int32 GetMinimumOSVersion() const PURE_VIRTUAL(
		UOpenMobileHapticPlatformPatternAsset::GetMinimumOSVersion,
		return MAX_int32;
	);
	virtual bool Supports(
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 OSVersion
	) const PURE_VIRTUAL(
		UOpenMobileHapticPlatformPatternAsset::Supports,
		return false;
	);
	virtual bool Validate(TArray<FString>& Errors) const PURE_VIRTUAL(
		UOpenMobileHapticPlatformPatternAsset::Validate,
		return false;
	);

	bool ShouldCookForPlatform(FName PlatformName) const;
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	virtual bool NeedsLoadForTargetPlatform(
		const ITargetPlatform* TargetPlatform
	) const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context
	) const override;
#endif
};

UCLASS()
class OPENMOBILEHAPTICS_API UOpenMobileHapticAndroidPatternAsset final
	: public UOpenMobileHapticPlatformPatternAsset
{
	GENERATED_BODY()

public:
	virtual void PostInitProperties() override;
	virtual void PostLoad() override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(
		FPropertyChangedEvent& PropertyChangedEvent
	) override;
#endif

	UPROPERTY(
		EditAnywhere,
		Category = "OpenMobile|Haptics|Android",
		meta = (
			ClampMin = "26",
			ToolTip = "Optional project floor for this pattern. The selected format can require a newer Android API."
		)
	)
	int32 MinimumAndroidAPI = 26;

	UPROPERTY(
		VisibleAnywhere,
		Transient,
		Category = "OpenMobile|Haptics|Android",
		meta = (
			DisplayName = "Resolved Minimum Android API",
			ToolTip = "The effective Android API required by the selected format and project floor."
		)
	)
	int32 ResolvedMinimumAndroidAPI = 26;

	UPROPERTY(
		EditAnywhere,
		Category = "OpenMobile|Haptics|Android",
		meta = (ToolTip = "Selects the Android-native representation authored by this asset.")
	)
	EOpenMobileHapticAndroidPatternFormat Format =
		EOpenMobileHapticAndroidPatternFormat::Primitives;

	UPROPERTY(
		EditAnywhere,
		Category = "OpenMobile|Haptics|Android|Primitives",
		meta = (
			EditCondition = "Format == EOpenMobileHapticAndroidPatternFormat::Primitives",
			EditConditionHides,
			ToolTip = "Primitive composition steps. Android API 30 or newer is required."
		)
	)
	TArray<FOpenMobileHapticAndroidPrimitiveStep> Primitives;

	UPROPERTY(
		EditAnywhere,
		Category = "OpenMobile|Haptics|Android|Waveform",
		meta = (
			EditCondition = "Format == EOpenMobileHapticAndroidPatternFormat::Waveform",
			EditConditionHides,
			TitleProperty = "DurationMilliseconds",
			ToolTip = "Ordered waveform rows. Each row keeps its duration and amplitude together."
		)
	)
	TArray<FOpenMobileHapticAndroidWaveformStep> WaveformSteps;

	UPROPERTY(
		meta = (
			DeprecatedProperty,
			DeprecationMessage = "Waveform timings are migrated into WaveformSteps."
		)
	)
	TArray<int32> WaveformTimingsMilliseconds;

	UPROPERTY(
		meta = (
			DeprecatedProperty,
			DeprecationMessage = "Waveform amplitudes are migrated into WaveformSteps."
		)
	)
	TArray<int32> WaveformAmplitudes;

	UPROPERTY(
		EditAnywhere,
		Category = "OpenMobile|Haptics|Android|Waveform",
		meta = (
			ClampMin = "-1",
			EditCondition = "Format == EOpenMobileHapticAndroidPatternFormat::Waveform",
			EditConditionHides,
			ToolTip = "Waveform row to repeat from, or -1 to play once."
		)
	)
	int32 WaveformRepeatIndex = -1;

	UPROPERTY(
		EditAnywhere,
		Category = "OpenMobile|Haptics|Android|Envelope",
		meta = (
			EditCondition = "Format == EOpenMobileHapticAndroidPatternFormat::BasicEnvelope || Format == EOpenMobileHapticAndroidPatternFormat::WaveformEnvelope",
			EditConditionHides,
			ToolTip = "Envelope control points. Android API 36 or newer is required."
		)
	)
	TArray<FOpenMobileHapticAndroidEnvelopePoint> EnvelopePoints;

	virtual EOpenMobileHapticOverridePlatform GetOverridePlatform() const override
	{
		return EOpenMobileHapticOverridePlatform::Android;
	}
	virtual int32 GetMinimumOSVersion() const override;
	virtual bool Supports(
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 OSVersion
	) const override;
	virtual bool Validate(TArray<FString>& Errors) const override;

private:
	void MigrateLegacyWaveform();
	void RefreshResolvedMinimumAndroidAPI();
};

UCLASS()
class OPENMOBILEHAPTICS_API UOpenMobileHapticIOSPatternAsset final
	: public UOpenMobileHapticPlatformPatternAsset
{
	GENERATED_BODY()

public:
	virtual void PostInitProperties() override;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (ClampMin = "13", ToolTip = "Optional iOS major-version floor for this AHAP asset. Core Haptics requires iOS 13 or newer."))
	int32 MinimumIOSMajorVersion = 13;

	bool SetAHAPSource(const FString& Source, TArray<FString>& Errors);
	bool SetAHAPSourceWithAudioResources(
		const FString& Source,
		const TArray<FOpenMobileHapticIOSAudioResource>& InAudioResources,
		TArray<FString>& Errors
	);

	const FString& GetNormalizedAHAPJson() const
	{
		return AHAPJson;
	}

	double GetAHAPDurationSeconds() const
	{
		return AHAPDurationSeconds;
	}

	bool RequiresAdvancedPlayer() const
	{
		return bRequiresAdvancedPlayer;
	}

	bool ContainsAudioEvents() const
	{
		return bContainsAudioEvents;
	}

	bool ContainsHapticEvents() const;

	bool ContainsCustomAudioEvents() const
	{
		return bContainsCustomAudioEvents;
	}

	const TArray<FOpenMobileHapticIOSAudioResource>& GetAudioResources() const
	{
		return AudioResources;
	}

#if WITH_EDITOR
	UAssetImportData* GetAssetImportData()
	{
		return AssetImportData;
	}

	const UAssetImportData* GetAssetImportData() const
	{
		return AssetImportData;
	}
#endif

	virtual EOpenMobileHapticOverridePlatform GetOverridePlatform() const override
	{
		return EOpenMobileHapticOverridePlatform::IOS;
	}
	virtual int32 GetMinimumOSVersion() const override
	{
		return FMath::Max(13, MinimumIOSMajorVersion);
	}
	virtual bool Supports(
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 OSVersion
	) const override;
	virtual bool Validate(TArray<FString>& Errors) const override;

private:
	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (MultiLine = "true", ToolTip = "Normalized validated AHAP JSON stored for deterministic iOS playback."))
	FString AHAPJson;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (Units = "s", ToolTip = "Validated AHAP duration in seconds."))
	double AHAPDurationSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (ToolTip = "True when this AHAP needs an advanced Core Haptics player."))
	bool bRequiresAdvancedPlayer = false;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (ToolTip = "True when the AHAP contains any audio event."))
	bool bContainsAudioEvents = false;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (ToolTip = "True when the AHAP references imported custom audio resources."))
	bool bContainsCustomAudioEvents = false;

	UPROPERTY(VisibleAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (ToolTip = "Imported audio resource records used only by iOS playback and cooking."))
	TArray<FOpenMobileHapticIOSAudioResource> AudioResources;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "OpenMobile|Haptics|iOS|Import", meta = (ToolTip = "Editor import source and reimport metadata for this AHAP asset."))
	TObjectPtr<UAssetImportData> AssetImportData;
#endif
};
