#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "OpenMobileAdsErrors.h"
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

UENUM(BlueprintType)
enum class EOpenMobileAdsDebugGeography : uint8
{
	Disabled,
	Eea UMETA(DisplayName = "EEA, UK, or Switzerland"),
	RegulatedUsState UMETA(DisplayName = "Regulated US State"),
	Other UMETA(DisplayName = "Other Region")
};

UENUM(BlueprintType)
enum class EOpenMobileAdsHideCachePolicy : uint8
{
	PreserveWhenSupported,
	Release
};

UENUM(BlueprintType)
enum class EOpenMobileAdsBannerAnchor : uint8
{
	Top,
	Bottom,
	Center
};

UENUM(BlueprintType)
enum class EOpenMobileAdsBannerHorizontalAlignment : uint8
{
	Left,
	Center,
	Right
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsBannerMargins
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (ClampMin = "0.0"))
	float Left = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (ClampMin = "0.0"))
	float Top = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (ClampMin = "0.0"))
	float Right = 0.0f;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (ClampMin = "0.0"))
	float Bottom = 0.0f;

	/** Rejects non-finite or negative insets before they reach platform banner layout code. */
	bool IsValid() const
	{
		return FMath::IsFinite(Left)
			&& FMath::IsFinite(Top)
			&& FMath::IsFinite(Right)
			&& FMath::IsFinite(Bottom)
			&& Left >= 0.0f
			&& Top >= 0.0f
			&& Right >= 0.0f
			&& Bottom >= 0.0f;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsBannerLayout
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	EOpenMobileAdsBannerAnchor Anchor = EOpenMobileAdsBannerAnchor::Bottom;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	EOpenMobileAdsBannerHorizontalAlignment HorizontalAlignment =
		EOpenMobileAdsBannerHorizontalAlignment::Center;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bRespectSafeArea = true;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FOpenMobileAdsBannerMargins Margins;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (ClampMin = "0.0"))
	float AvailableWidth = 0.0f;

	/** Validates custom width and margins while allowing zero to mean provider or viewport width. */
	bool IsValid() const
	{
		return Margins.IsValid()
			&& FMath::IsFinite(AvailableWidth)
			&& AvailableWidth >= 0.0f;
	}
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

	/** Requires finite non-negative delays and a backoff that never moves backwards. */
	bool IsValid() const;
	/** Uses a placement override when supplied and otherwise keeps the global retry limit. */
	int32 ResolveMaxRetryAttempts(int32 PlacementMaxRetryAttempts) const;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsPreloadPolicy
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bEnabled = true;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0.0", Units = "s")
	)
	double TriggerDelaySeconds = 0.0;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "1.0", Units = "s")
	)
	double RecoverableFailureDelaySeconds = 60.0;

	/** Rejects non-finite or negative scheduling values before automatic preload starts. */
	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsCooldownPolicy
{
	GENERATED_BODY()

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0.0", Units = "s")
	)
	double FullscreenCooldownSeconds = 0.0;

	/** Keeps zero as an intentional disabled cooldown while rejecting invalid time values. */
	bool IsValid() const
	{
		return FMath::IsFinite(FullscreenCooldownSeconds)
			&& FullscreenCooldownSeconds >= 0.0;
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsFrequencyCap
{
	GENERATED_BODY()

	static constexpr int32 MaximumRollingImpressions = 4096;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0", DisplayName = "Maximum Impressions Per Session")
	)
	int32 MaxSessionImpressions = 0;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0", DisplayName = "Maximum Impressions Per Rolling Window")
	)
	int32 MaxImpressions = 0;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0.0", DisplayName = "Rolling Window", Units = "s")
	)
	double WindowSeconds = 0.0;

	/** Enables the session gate only when a positive impression limit was configured. */
	bool IsSessionLimitEnabled() const
	{
		return MaxSessionImpressions > 0;
	}

	/** Requires both a positive count and time span before the rolling gate is active. */
	bool IsRollingWindowEnabled() const
	{
		return MaxImpressions > 0 && WindowSeconds > 0.0;
	}

	/** Reports whether either independent frequency rule can block presentation. */
	bool IsEnabled() const
	{
		return IsSessionLimitEnabled() || IsRollingWindowEnabled();
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsAppOpenPolicy
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bShowOnColdStart = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bShowOnForeground = true;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0.0", Units = "s")
	)
	double ColdStartPresentationWindowSeconds = 5.0;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0.0", Units = "s")
	)
	double ForegroundPresentationWindowSeconds = 2.0;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0.0", Units = "s")
	)
	double MinimumBackgroundDurationSeconds = 30.0;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ClampMin = "0.001", Units = "s")
	)
	double MaximumCacheAgeSeconds = 14400.0;

	/** Rejects invalid opportunity windows and cache ages before lifecycle callbacks use them. */
	bool IsValid() const;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsServerVerificationSettings
{
	GENERATED_BODY()

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (ToolTip = "Expect this rewarded placement to use the provider's backend server-side verification callback. Configure callback URLs and all private credentials outside the Unreal project.")
	)
	bool bEnabled = false;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (EditCondition = "bEnabled", ToolTip = "Reject a show unless its options contain a per-show server-verification user ID.")
	)
	bool bRequireUserId = false;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (EditCondition = "bEnabled", ToolTip = "Reject a show unless its options contain per-show server-verification custom data.")
	)
	bool bRequireCustomData = false;
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
	bool bOverrideAppOpenPolicy = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (EditCondition = "bOverrideAppOpenPolicy"))
	FOpenMobileAdsAppOpenPolicy AppOpenPolicy;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bOverrideServerVerification = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (EditCondition = "bOverrideServerVerification"))
	FOpenMobileAdsServerVerificationSettings ServerVerification;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	bool bOverrideBannerLayout = false;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (EditCondition = "bOverrideBannerLayout"))
	FOpenMobileAdsBannerLayout BannerLayout;

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
	FOpenMobileAdsAppOpenPolicy AppOpenPolicy;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsServerVerificationSettings ServerVerification;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsHideCachePolicy HideCachePolicy =
		EOpenMobileAdsHideCachePolicy::PreserveWhenSupported;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsBannerLayout BannerLayout;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString FallbackRewardType;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	int64 FallbackRewardAmount = 0;

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
	FOpenMobileAdsAppOpenPolicy AppOpenPolicy;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FOpenMobileAdsServerVerificationSettings ServerVerification;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	EOpenMobileAdsHideCachePolicy HideCachePolicy =
		EOpenMobileAdsHideCachePolicy::PreserveWhenSupported;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FOpenMobileAdsBannerLayout BannerLayout;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (
			ClampMin = "-1",
			ClampMax = "10",
			ToolTip = "Maximum retries after the first load attempt. Use -1 to inherit the global limit."
		)
	)
	int32 MaxRetryAttempts = -1;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FString FallbackRewardType;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Ads",
		meta = (
			ClampMin = "0",
			ToolTip = "Positive amount used only when the provider omits its reward amount. Zero disables the fallback."
		)
	)
	int64 FallbackRewardAmount = 0;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	TMap<FName, FString> ProviderOptions;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads")
	FOpenMobileAdsPlatformPlacementOverride Android;

	UPROPERTY(Config, EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Ads", meta = (DisplayName = "iOS"))
	FOpenMobileAdsPlatformPlacementOverride IOS;

	/** Applies only the current platform's explicit overrides and leaves unsupported platforms unresolved. */
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
	InvalidFallbackRewardAmount,
	EmptyProviderOption,
	UnsupportedProviderFormat,
	UnsupportedProviderOperation,
	InvalidRetryPolicy,
	UnsafeShippingTestMode,
	InvalidTestDeviceIdentifier,
	UnsafeShippingTestDeviceIdentifier,
	UnsafeShippingDebugGeography,
	InvalidTrackingUsageDescription,
	InvalidConvenienceRewardedPlacement,
	InvalidPlacementRetryLimit,
	InvalidPreloadPolicy,
	InvalidBannerLayout,
	InvalidAppOpenPolicy,
	InvalidServerVerification
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
	/** Checks placement identity, per-platform values, and cross-placement collisions without provider knowledge. */
	static TArray<FOpenMobileAdsConfigurationIssue> Validate(
		const TArray<FOpenMobileAdsPlacementSettings>& Placements
	);

	/** Adds selected-provider format and operation checks to the portable placement validation. */
	static TArray<FOpenMobileAdsConfigurationIssue> ValidateProviderCapabilities(
		const TArray<FOpenMobileAdsPlacementSettings>& Placements,
		const FOpenMobileAdsProviderCapabilities& Capabilities,
		bool bAutomaticPreloadingEnabled = true
	);

	/** Validates project settings with shipping safety rules applied to every development option. */
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
	/** Registers provider and placement settings under the shared OpenMobile project category. */
	UOpenMobileAdsSettings();

	/** Keeps every OpenMobile plugin inside one Project Settings category. */
	virtual FName GetCategoryName() const override { return TEXT("OpenMobile"); }
	/** Gives Ads its own section instead of mixing provider-specific settings into the service. */
	virtual FName GetSectionName() const override { return TEXT("OpenMobile Ads"); }

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Providers",
		meta = (
			GetOptions = "OpenMobileAds.OpenMobileAdsSubsystem.GetRegisteredAdsProviderNames",
			ToolTip = "Selects one registered Ads provider. Leave empty only when exactly one provider is enabled."
		)
	)
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

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Development",
		meta = (
			EditCondition = "bDevelopmentTestMode",
			DisplayName = "Use Official Test Ad Unit IDs",
			ToolTip = "Uses provider-owned sample ad-unit IDs in Development/Test Mode. Disable this to test configured mediation ad units while test devices and diagnostics remain enabled."
		)
	)
	bool bUseOfficialTestAdUnitIds = true;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Development",
		meta = (
			DisplayName = "Global Test Device Identifiers",
			ToolTip = "Opaque test-device identifiers shared with the selected ads provider while Development/Test Mode is enabled. Shipping builds reject non-empty values."
		)
	)
	TArray<FString> TestDeviceIdentifiers;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Development",
		meta = (
			DisplayName = "Consent Debug Geography",
			ToolTip = "Forces consent flows to use a test region only when Development/Test Mode and at least one provider test-device identifier are active."
		)
	)
	EOpenMobileAdsDebugGeography DebugGeography =
		EOpenMobileAdsDebugGeography::Disabled;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Privacy|Tracking Authorization",
		meta = (
			DisplayName = "Enable App Tracking Transparency",
			ToolTip = "Enables the caller-controlled iOS tracking authorization request. This setting never requests permission automatically."
		)
	)
	bool bEnableTrackingAuthorization = false;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Privacy|Tracking Authorization",
		meta = (
			EditCondition = "bEnableTrackingAuthorization",
			DisplayName = "Tracking Usage Description",
			ToolTip = "Project-specific text shown by the iOS tracking authorization prompt."
		)
	)
	FString TrackingUsageDescription;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Privacy|Tracking Authorization",
		meta = (
			EditCondition = "bEnableTrackingAuthorization",
			DisplayName = "Delay Ads Initialization Until Tracking Decision",
			ToolTip = "Rejects ads initialization while iOS tracking authorization remains NotDetermined. The application must request authorization explicitly and retry initialization afterward."
		)
	)
	bool bDelayAdsInitializationUntilTrackingAuthorization = true;

	/** Requires trimmed, project-specific prompt text when tracking authorization is enabled. */
	static bool IsValidTrackingUsageDescription(const FString& Description);

	/** Forces production behavior in shipping builds even when a stale config enables test mode. */
	static bool ResolveDevelopmentTestMode(bool bConfigured, bool bForShipping)
	{
		return bConfigured && !bForShipping;
	}

	/** Resolves the live build flag through the same shipping-safe rule used by validation. */
	bool IsDevelopmentTestModeEnabled() const
	{
		return ResolveDevelopmentTestMode(bDevelopmentTestMode, UE_BUILD_SHIPPING != 0);
	}

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Reliability")
	FOpenMobileAdsRetryPolicy RetryPolicy;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Reliability")
	FOpenMobileAdsRetryPolicy NoFillRetryPolicy;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Reliability|Preloading")
	FOpenMobileAdsPreloadPolicy PreloadPolicy;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Pacing|Cooldowns")
	FOpenMobileAdsCooldownPolicy CooldownPolicy;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Privacy")
	FOpenMobileAdsPrivacyConfiguration Privacy;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Request Configuration")
	FOpenMobileAdsRequestConfiguration RequestConfiguration;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Placements")
	TArray<FOpenMobileAdsPlacementSettings> Placements;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Placements",
		meta = (
			GetOptions = "OpenMobileAds.OpenMobileAdsSubsystem.GetConfiguredAdsPlacementNames",
			ToolTip = "Legacy rewarded placement used by RequestAndShowRewardedAd. Leave empty only when exactly one enabled rewarded placement exists."
		)
	)
	FName ConvenienceRewardedPlacement;

	/** Uses a separate no-fill policy so inventory misses don't share ordinary failure timing. */
	const FOpenMobileAdsRetryPolicy& GetRetryPolicyForError(
		EOpenMobileAdsErrorCode ErrorCode
	) const;

	/** Matches placement names case-insensitively while configuration validation still reports case collisions. */
	const FOpenMobileAdsPlacementSettings* FindPlacement(FName Placement) const;
};
