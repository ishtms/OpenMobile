#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsTypes.h"
#include "OpenMobileAdsCapabilities.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdsServerVerificationCharacterSet : uint8
{
	Unicode,
	Ascii UMETA(DisplayName = "ASCII")
};

UENUM(BlueprintType)
enum class EOpenMobileAdsServerVerificationOptionTiming : uint8
{
	BeforePresentation UMETA(DisplayName = "Before Presentation"),
	BeforeLoad UMETA(DisplayName = "Before Load")
};

UENUM(BlueprintType)
enum class EOpenMobileAdsServerVerificationCallbackEncoding : uint8
{
	ProviderDefined UMETA(DisplayName = "Provider Defined"),
	PercentEncodedUtf8 UMETA(DisplayName = "Percent-Encoded UTF-8")
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsServerVerificationConstraints
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads", meta = (ToolTip = "Maximum user-ID length after UTF-8 encoding. Zero means the provider documents no limit."))
	int32 MaxUserIdUtf8Bytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads", meta = (ToolTip = "Maximum custom-data length after UTF-8 encoding. Zero means the provider documents no limit."))
	int32 MaxCustomDataUtf8Bytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads", meta = (ToolTip = "Character set accepted for the per-show user ID."))
	EOpenMobileAdsServerVerificationCharacterSet UserIdCharacterSet =
		EOpenMobileAdsServerVerificationCharacterSet::Unicode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads", meta = (ToolTip = "Character set accepted for per-show custom data."))
	EOpenMobileAdsServerVerificationCharacterSet CustomDataCharacterSet =
		EOpenMobileAdsServerVerificationCharacterSet::Unicode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads", meta = (ToolTip = "Latest point at which the provider accepts server-verification options."))
	EOpenMobileAdsServerVerificationOptionTiming OptionTiming =
		EOpenMobileAdsServerVerificationOptionTiming::BeforePresentation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads", meta = (ToolTip = "Encoding applied by the provider when it adds values to the backend callback URL."))
	EOpenMobileAdsServerVerificationCallbackEncoding CallbackEncoding =
		EOpenMobileAdsServerVerificationCallbackEncoding::ProviderDefined;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdFormatCapabilities
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdFormat Format = EOpenMobileAdFormat::Rewarded;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanLoad = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanShow = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanHide = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bPreservesCachedAdOnHide = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanDestroy = true;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsPreload = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsRefresh = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsCancellation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsImpression = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsClick = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsDismiss = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsReward = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bReportsRevenue = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsServerVerification = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsServerVerificationUserId = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bSupportsServerVerificationCustomData = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsServerVerificationConstraints ServerVerificationConstraints;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bRequiresIntroduction = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	int32 MaxCachedAdsPerPlacement = 1;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	double CacheLifetimeSeconds = 0.0;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsProviderCapabilities
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString ProviderVersion;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	TArray<FOpenMobileAdFormatCapabilities> Formats;

	const FOpenMobileAdFormatCapabilities* FindFormat(EOpenMobileAdFormat Format) const
	{
		return Formats.FindByPredicate(
			[Format](const FOpenMobileAdFormatCapabilities& Candidate)
			{
				return Candidate.Format == Format;
			}
		);
	}

	bool SupportsFormat(EOpenMobileAdFormat Format) const
	{
		const FOpenMobileAdFormatCapabilities* Capabilities = FindFormat(Format);
		return Capabilities && Capabilities->bCanLoad;
	}
};
