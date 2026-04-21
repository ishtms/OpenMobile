#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsErrors.h"
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
enum class EOpenMobileAdsConsentActivity : uint8
{
	Idle,
	Refreshing,
	PresentingForm,
	Resetting
};

UENUM(BlueprintType)
enum class EOpenMobileAdsAgeTreatment : uint8
{
	Unspecified,
	No,
	Yes
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsConsentProviderDetails
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bIsAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString RawStatus;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString RawMessage;
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
	EOpenMobileAdsConsentActivity ConsentActivity = EOpenMobileAdsConsentActivity::Idle;

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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsConsentProviderDetails ProviderDetails;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsError Error;
};

enum class EOpenMobileAdsConsentStatusUpdateType : uint8
{
	RefreshStarted,
	FormPresentationStarted,
	ResetStarted,
	Completed,
	Failed
};

struct OPENMOBILEADS_API FOpenMobileAdsConsentStatusUpdate
{
	EOpenMobileAdsConsentStatusUpdateType Type =
		EOpenMobileAdsConsentStatusUpdateType::Completed;
	EOpenMobileAdsConsentStatus Status = EOpenMobileAdsConsentStatus::Unknown;
	FName Source;
	FOpenMobileAdsConsentProviderDetails ProviderDetails;
	FOpenMobileAdsError Error;

	static FOpenMobileAdsConsentStatusUpdate BeginRefresh(FName Source);
	static FOpenMobileAdsConsentStatusUpdate BeginFormPresentation(FName Source);
	static FOpenMobileAdsConsentStatusUpdate BeginReset(FName Source);
	static FOpenMobileAdsConsentStatusUpdate Complete(
		EOpenMobileAdsConsentStatus Status,
		FName Source,
		FOpenMobileAdsConsentProviderDetails ProviderDetails = {}
	);
	static FOpenMobileAdsConsentStatusUpdate Fail(
		FName Source,
		FOpenMobileAdsError Error
	);
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsConsentStatusNativeEvent,
	const FOpenMobileAdsPrivacySnapshot&
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsConsentStatusDynamicEvent,
	const FOpenMobileAdsPrivacySnapshot&,
	Status
);
