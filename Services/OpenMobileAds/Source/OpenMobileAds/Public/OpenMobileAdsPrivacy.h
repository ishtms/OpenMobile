#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsPrivacy.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdsConsentStatus : uint8
{
	Unknown,
	Required,
	Granted,
	Denied,
	NotRequired
};

UENUM(BlueprintType)
enum class EOpenMobileAdsAgeTreatment : uint8
{
	Unspecified,
	No,
	Yes
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsPrivacyConfiguration
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment ChildDirectedTreatment =
		EOpenMobileAdsAgeTreatment::Unspecified;

	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment UnderAgeOfConsent =
		EOpenMobileAdsAgeTreatment::Unspecified;

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (
			DisplayName = "Delay Provider Initialization Until Consent",
			ToolTip = "When enabled, the ads service waits for consent state before initializing the selected provider."
		)
	)
	bool bDelayProviderInitializationUntilConsent = true;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsPrivacySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentStatus ConsentStatus = EOpenMobileAdsConsentStatus::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::Unspecified;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::Unspecified;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanRequestAds = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Source;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime LastUpdated;
};
