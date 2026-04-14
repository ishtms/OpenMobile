#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileAdsPrivacy.h"
#include "OpenMobileAdsTypes.h"
#include "OpenMobileAdsConfiguration.generated.h"

struct FOpenMobileAdsProviderCapabilities;
class UOpenMobileAdsSettings;

UENUM(BlueprintType)
enum class EOpenMobileAdsMaxAdContentRating : uint8
{
	Unspecified,
	General,
	ParentalGuidance,
	Teen,
	Mature
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsRequestConfiguration
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsMaxAdContentRating MaxAdContentRating =
		EOpenMobileAdsMaxAdContentRating::Unspecified;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsRetryPolicy
{
	GENERATED_BODY()

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0", ClampMax = "10")
	)
	int32 MaxRetryAttempts = 2;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0.0", Units = "s")
	)
	double InitialDelaySeconds = 1.0;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "1.0")
	)
	double BackoffMultiplier = 2.0;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0.0", Units = "s")
	)
	double MaxDelaySeconds = 30.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bUseJitter = true;

	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsFrequencyCap
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	int32 MaxImpressions = 0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (ClampMin = "0.0"))
	double WindowSeconds = 0.0;

	bool IsEnabled() const
	{
		return MaxImpressions > 0 && WindowSeconds > 0.0;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsPlatformPlacementOverride
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FString AdUnitId;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bOverrideEnabled = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (EditCondition = "bOverrideEnabled"))
	bool bEnabled = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bOverridePreload = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (EditCondition = "bOverridePreload"))
	bool bPreload = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bOverrideRefreshInterval = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (EditCondition = "bOverrideRefreshInterval", ClampMin = "0.0"))
	double RefreshIntervalSeconds = 0.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bOverrideFrequencyCap = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (EditCondition = "bOverrideFrequencyCap"))
	FOpenMobileAdsFrequencyCap FrequencyCap;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bOverrideCooldown = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (EditCondition = "bOverrideCooldown", ClampMin = "0.0"))
	double CooldownSeconds = 0.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	TMap<FName, FString> ProviderOptions;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsResolvedPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Placement;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString AdUnitId;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bPreload = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	double RefreshIntervalSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsFrequencyCap FrequencyCap;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	double CooldownSeconds = 0.0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	TMap<FName, FString> ProviderOptions;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsPlacementSettings
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FName Placement;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bEnabled = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bPreload = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (ClampMin = "0.0"))
	double RefreshIntervalSeconds = 0.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FOpenMobileAdsFrequencyCap FrequencyCap;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (ClampMin = "0.0"))
	double CooldownSeconds = 0.0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	TMap<FName, FString> ProviderOptions;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FOpenMobileAdsPlatformPlacementOverride Android;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FOpenMobileAdsPlatformPlacementOverride IOS;

	FOpenMobileAdsResolvedPlacement Resolve(EOpenMobileAdsPlatform Platform) const;
};

UENUM(BlueprintType)
enum class EOpenMobileAdsConfigurationIssueSeverity : uint8
{
	Warning,
	Error
};

UENUM(BlueprintType)
enum class EOpenMobileAdsConfigurationIssueCode : uint8
{
	EmptyPlacement,
	DuplicatePlacement,
	CaseConflict,
	MissingAndroidAdUnitId,
	MissingIOSAdUnitId,
	DuplicateAndroidAdUnitId,
	DuplicateIOSAdUnitId,
	InvalidRefreshInterval,
	RefreshNotSupported,
	InvalidFrequencyCap,
	InvalidCooldown,
	EmptyProviderOption,
	UnsupportedProviderFormat,
	UnsupportedProviderOperation,
	InvalidRetryPolicy,
	UnsafeShippingTestMode
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsConfigurationIssue
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConfigurationIssueSeverity Severity = EOpenMobileAdsConfigurationIssueSeverity::Error;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConfigurationIssueCode Code = EOpenMobileAdsConfigurationIssueCode::EmptyPlacement;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Placement;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName ConflictingPlacement;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Message;
};

class OPENMOBILEADS_API FOpenMobileAdsConfigurationValidator
{
public:
	static TArray<FOpenMobileAdsConfigurationIssue> Validate(
		const TArray<FOpenMobileAdsPlacementSettings>& Placements
	);

	static TArray<FOpenMobileAdsConfigurationIssue> ValidateProviderCapabilities(
		const TArray<FOpenMobileAdsPlacementSettings>& Placements,
		const FOpenMobileAdsProviderCapabilities& Capabilities
	);

	static TArray<FOpenMobileAdsConfigurationIssue> ValidateSettings(
		const UOpenMobileAdsSettings& Settings,
		bool bForShipping
	);
};

UCLASS(Config = Engine, DefaultConfig, meta = (DisplayName = "OpenMobile Ads"))
class OPENMOBILEADS_API UOpenMobileAdsSettings : public UDeveloperSettings
{
	GENERATED_BODY()

public:
	virtual FName GetCategoryName() const override { return TEXT("Plugins"); }
	virtual FName GetSectionName() const override { return TEXT("OpenMobile Ads"); }

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Providers")
	FName PreferredProvider;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Development",
		meta = (
			DisplayName = "Development/Test Mode",
			ToolTip = "Enables test devices, official test IDs, consent debug controls, and verbose diagnostics in non-shipping builds. Shipping builds reject this setting and force production behavior."
		)
	)
	bool bDevelopmentTestMode = false;

	static bool ResolveDevelopmentTestMode(bool bConfigured, bool bForShipping)
	{
		return bConfigured && !bForShipping;
	}

	bool IsDevelopmentTestModeEnabled() const
	{
		return ResolveDevelopmentTestMode(bDevelopmentTestMode, UE_BUILD_SHIPPING != 0);
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Reliability")
	FOpenMobileAdsRetryPolicy RetryPolicy;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Privacy")
	FOpenMobileAdsPrivacyConfiguration Privacy;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Request Configuration")
	FOpenMobileAdsRequestConfiguration RequestConfiguration;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placements")
	TArray<FOpenMobileAdsPlacementSettings> Placements;

	const FOpenMobileAdsPlacementSettings* FindPlacement(FName Placement) const;
};
