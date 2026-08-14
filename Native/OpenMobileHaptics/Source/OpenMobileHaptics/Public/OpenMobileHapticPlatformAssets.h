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

	/** Unreal needs an empty step while it restores reflected primitive arrays from an asset. */
	FOpenMobileHapticAndroidPrimitiveStep() = default;
	/** Keeps the primitive, strength, and delay together when platform data is assembled in code. */
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

	/** Unreal needs the empty form when it reconstructs saved waveform rows. */
	FOpenMobileHapticAndroidWaveformStep() = default;
	/** Builds a complete row in code, so duration and amplitude can't land in separate legacy arrays. */
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
	/** Identifies which runtime can consume the asset, preventing an override from being cooked or submitted on the wrong platform. */
	virtual EOpenMobileHapticOverridePlatform GetOverridePlatform() const
		PURE_VIRTUAL(
			UOpenMobileHapticPlatformPatternAsset::GetOverridePlatform,
			return EOpenMobileHapticOverridePlatform::None;
		);
	/** Gives cooking and capability checks one conservative OS floor for the authored representation. */
	virtual int32 GetMinimumOSVersion() const PURE_VIRTUAL(
		UOpenMobileHapticPlatformPatternAsset::GetMinimumOSVersion,
		return MAX_int32;
	);
	/** Checks the actual device capabilities as well as OS version, version alone can't prove the feature exists. */
	virtual bool Supports(
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 OSVersion
	) const PURE_VIRTUAL(
		UOpenMobileHapticPlatformPatternAsset::Supports,
		return false;
	);
	/** Collects authoring errors before the platform backend receives data it can't submit. */
	virtual bool Validate(TArray<FString>& Errors) const PURE_VIRTUAL(
		UOpenMobileHapticPlatformPatternAsset::Validate,
		return false;
	);

	/** Keeps platform-only content out of unrelated packages, which also avoids loading native formats where they're meaningless. */
	bool ShouldCookForPlatform(FName PlatformName) const;
	/** Validates the native representation before save so bad source can't quietly reach a cook. */
	virtual void PreSave(FObjectPreSaveContext SaveContext) override;
	/** Gives Unreal's cooker the same platform decision used by explicit cook checks. */
	virtual bool NeedsLoadForTargetPlatform(
		const ITargetPlatform* TargetPlatform
	) const override;

#if WITH_EDITOR
	/** Surfaces platform-format errors in the Content Browser without needing a device build. */
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
	/** Refreshes the derived Android API floor for brand-new assets before the details panel shows it. */
	virtual void PostInitProperties() override;
	/** Migrates old split waveform arrays and recomputes requirements when existing assets load. */
	virtual void PostLoad() override;

#if WITH_EDITOR
	/** Recomputes the API floor as soon as format data changes, so authors see the real requirement. */
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

	/** Pins this asset to Android for cook filtering and runtime selection. */
	virtual EOpenMobileHapticOverridePlatform GetOverridePlatform() const override
	{
		return EOpenMobileHapticOverridePlatform::Android;
	}
	/** Returns the resolved floor after format requirements and the author floor have both been applied. */
	virtual int32 GetMinimumOSVersion() const override;
	/** Requires the selected Android feature to be explicitly reported, Unknown isn't enough to submit native data. */
	virtual bool Supports(
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 OSVersion
	) const override;
	/** Checks step counts, ranges, repeat indices, and format-specific Android requirements together. */
	virtual bool Validate(TArray<FString>& Errors) const override;

private:
	/** Converts the old parallel timing and amplitude arrays once, otherwise their indices can drift during future edits. */
	void MigrateLegacyWaveform();
	/** Raises the visible API floor when the selected Android format needs a newer platform feature. */
	void RefreshResolvedMinimumAndroidAPI();
};

UCLASS()
class OPENMOBILEHAPTICS_API UOpenMobileHapticIOSPatternAsset final
	: public UOpenMobileHapticPlatformPatternAsset
{
	GENERATED_BODY()

public:
	/** Creates import metadata for new editor assets, cooked builds don't carry that editor-only object. */
	virtual void PostInitProperties() override;

	UPROPERTY(EditAnywhere, Category = "OpenMobile|Haptics|iOS", meta = (ClampMin = "13", ToolTip = "Optional iOS major-version floor for this AHAP asset. Core Haptics requires iOS 13 or newer."))
	int32 MinimumIOSMajorVersion = 13;

	/** Validates and normalizes AHAP text before storing it, raw import text never goes straight to native playback. */
	bool SetAHAPSource(const FString& Source, TArray<FString>& Errors);
	/** Installs AHAP and its resolved custom audio in one operation so references can't point at missing bytes. */
	bool SetAHAPSourceWithAudioResources(
		const FString& Source,
		const TArray<FOpenMobileHapticIOSAudioResource>& InAudioResources,
		TArray<FString>& Errors
	);

	/** Returns the validated normalized document that iOS playback and cooking both consume. */
	const FString& GetNormalizedAHAPJson() const
	{
		return AHAPJson;
	}

	/** Uses the duration found during validation, so callers don't need to parse the JSON again. */
	double GetAHAPDurationSeconds() const
	{
		return AHAPDurationSeconds;
	}

	/** Tells the backend when simple one-shot playback can't represent this AHAP's controls or events. */
	bool RequiresAdvancedPlayer() const
	{
		return bRequiresAdvancedPlayer;
	}

	/** Lets policy reject audio-bearing AHAP content where the current playback path is haptics-only. */
	bool ContainsAudioEvents() const
	{
		return bContainsAudioEvents;
	}

	/** Confirms the document produces haptic output, since an audio-only AHAP isn't a valid haptic override. */
	bool ContainsHapticEvents() const;

	/** Separates embedded custom audio from ordinary AHAP audio events because it needs packaged resource files. */
	bool ContainsCustomAudioEvents() const
	{
		return bContainsCustomAudioEvents;
	}

	/** Exposes validated imported bytes without copying them before the iOS backend registers resources. */
	const TArray<FOpenMobileHapticIOSAudioResource>& GetAudioResources() const
	{
		return AudioResources;
	}

#if WITH_EDITOR
	/** Gives import and reimport tools mutable access to the source record they own. */
	UAssetImportData* GetAssetImportData()
	{
		return AssetImportData;
	}

	/** Lets read-only editor tooling inspect import provenance without changing the asset. */
	const UAssetImportData* GetAssetImportData() const
	{
		return AssetImportData;
	}
#endif

	/** Pins this asset to iOS for cook filtering and runtime override selection. */
	virtual EOpenMobileHapticOverridePlatform GetOverridePlatform() const override
	{
		return EOpenMobileHapticOverridePlatform::IOS;
	}
	/** Never reports below Core Haptics' iOS 13 floor, even if old serialized data contains a smaller value. */
	virtual int32 GetMinimumOSVersion() const override
	{
		return FMath::Max(13, MinimumIOSMajorVersion);
	}
	/** Requires both a suitable iOS version and the exact capabilities used by this AHAP. */
	virtual bool Supports(
		const FOpenMobileHapticCapabilities& Capabilities,
		int32 OSVersion
	) const override;
	/** Rechecks normalized JSON and audio references before save or cook, imported data can still become stale. */
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
