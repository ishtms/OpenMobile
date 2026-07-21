#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "OpenMobileHapticsTypes.h"
#include "OpenMobileHapticPlatformAssets.generated.h"

class UAssetImportData;

UENUM(BlueprintType)
enum class EOpenMobileHapticOverridePlatform : uint8
{
	None,
	Android,
	IOS
};

UENUM(BlueprintType)
enum class EOpenMobileHapticAndroidPatternFormat : uint8
{
	Primitives,
	Waveform,
	BasicEnvelope,
	WaveformEnvelope
};

UENUM(BlueprintType)
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

USTRUCT(BlueprintType)
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

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android")
	EOpenMobileHapticAndroidPrimitive Primitive =
		EOpenMobileHapticAndroidPrimitive::Click;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Scale = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android", meta = (ClampMin = "0", Units = "ms"))
	int32 DelayMilliseconds = 0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticAndroidEnvelopePoint
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android", meta = (ClampMin = "0.0", Units = "s"))
	float TimeSeconds = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Amplitude = 1.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android", meta = (ClampMin = "0.0", Units = "Hz"))
	float FrequencyHz = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float Sharpness = 0.5f;
};

USTRUCT(BlueprintType)
struct OPENMOBILEHAPTICS_API FOpenMobileHapticIOSAudioResource
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "iOS")
	FString RelativePath;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "iOS")
	TArray<uint8> Data;
};

UCLASS(Abstract, BlueprintType)
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

UCLASS(BlueprintType)
class OPENMOBILEHAPTICS_API UOpenMobileHapticAndroidPatternAsset final
	: public UOpenMobileHapticPlatformPatternAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android", meta = (ClampMin = "26"))
	int32 MinimumAndroidAPI = 26;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android")
	EOpenMobileHapticAndroidPatternFormat Format =
		EOpenMobileHapticAndroidPatternFormat::Primitives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android")
	TArray<FOpenMobileHapticAndroidPrimitiveStep> Primitives;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android")
	TArray<int32> WaveformTimingsMilliseconds;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android")
	TArray<int32> WaveformAmplitudes;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android", meta = (ClampMin = "-1"))
	int32 WaveformRepeatIndex = -1;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Android")
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
};

UCLASS(BlueprintType)
class OPENMOBILEHAPTICS_API UOpenMobileHapticIOSPatternAsset final
	: public UOpenMobileHapticPlatformPatternAsset
{
	GENERATED_BODY()

public:
	virtual void PostInitProperties() override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "iOS", meta = (ClampMin = "13"))
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
	UPROPERTY(VisibleAnywhere, Category = "iOS", meta = (MultiLine = "true"))
	FString AHAPJson;

	UPROPERTY(VisibleAnywhere, Category = "iOS", meta = (Units = "s"))
	double AHAPDurationSeconds = 0.0;

	UPROPERTY(VisibleAnywhere, Category = "iOS")
	bool bRequiresAdvancedPlayer = false;

	UPROPERTY(VisibleAnywhere, Category = "iOS")
	bool bContainsAudioEvents = false;

	UPROPERTY(VisibleAnywhere, Category = "iOS")
	bool bContainsCustomAudioEvents = false;

	UPROPERTY(VisibleAnywhere, Category = "iOS")
	TArray<FOpenMobileHapticIOSAudioResource> AudioResources;

#if WITH_EDITORONLY_DATA
	UPROPERTY(VisibleAnywhere, Instanced, Category = "Import")
	TObjectPtr<UAssetImportData> AssetImportData;
#endif
};
