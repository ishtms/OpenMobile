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

UENUM(BlueprintType)
enum class EOpenMobileAdsProviderRequestPolicyState : uint8
{
	Allowed,
	TemporarilyBlocked,
	UserDecisionRequired,
	Blocked
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsProviderRequestContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentStatus ConsentStatus =
		EOpenMobileAdsConsentStatus::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bConsentStatusFresh = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment ChildDirectedTreatment =
		EOpenMobileAdsAgeTreatment::Unspecified;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment UnderAgeOfConsent =
		EOpenMobileAdsAgeTreatment::Unspecified;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsProviderRequestPolicy
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsProviderRequestPolicyState State =
		EOpenMobileAdsProviderRequestPolicyState::Allowed;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Explanation;
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

	UPROPERTY(
		Config,
		EditAnywhere,
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (
			DisplayName = "Under-Age-of-Consent Treatment (TFUA)",
			ToolTip = "Specifies under-age-of-consent treatment independently from the COPPA child-directed setting."
		)
	)
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
	bool bConsentStatusFresh = true;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Source;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime LastUpdated;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsConsentProviderDetails ProviderDetails;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsError Error;
};

UENUM(BlueprintType)
enum class EOpenMobileAdsCanRequestAdsBlockReason : uint8
{
	None,
	InitializationNotStarted,
	InitializationPending,
	InitializationFailed,
	ShuttingDown,
	ProviderUnavailable,
	ConsentResetting,
	ConsentFormPresenting,
	ConsentRefreshing,
	ConsentStale,
	ConsentUnknown,
	ConsentRequired,
	ConsentDenied,
	ProviderPolicy
};

UENUM(BlueprintType)
enum class EOpenMobileAdsCanRequestAdsBlockType : uint8
{
	None,
	Temporary,
	UserDecision,
	Configuration,
	Terminal
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsCanRequestAdsResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bCanRequestAds = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsCanRequestAdsBlockReason BlockReason =
		EOpenMobileAdsCanRequestAdsBlockReason::InitializationNotStarted;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsCanRequestAdsBlockType BlockType =
		EOpenMobileAdsCanRequestAdsBlockType::Temporary;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Provider;

	bool operator==(const FOpenMobileAdsCanRequestAdsResult& Other) const
	{
		return bCanRequestAds == Other.bCanRequestAds
			&& BlockReason == Other.BlockReason
			&& BlockType == Other.BlockType
			&& Explanation == Other.Explanation
			&& Provider == Other.Provider;
	}

	bool operator!=(const FOpenMobileAdsCanRequestAdsResult& Other) const
	{
		return !(*this == Other);
	}
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
	bool bStatusFresh = true;

	static FOpenMobileAdsConsentStatusUpdate BeginRefresh(FName Source);
	static FOpenMobileAdsConsentStatusUpdate BeginFormPresentation(FName Source);
	static FOpenMobileAdsConsentStatusUpdate BeginReset(FName Source);
	static FOpenMobileAdsConsentStatusUpdate Complete(
		EOpenMobileAdsConsentStatus Status,
		FName Source,
		FOpenMobileAdsConsentProviderDetails ProviderDetails = {},
		bool bStatusFresh = true
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

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsCanRequestAdsNativeEvent,
	const FOpenMobileAdsCanRequestAdsResult&
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsCanRequestAdsDynamicEvent,
	const FOpenMobileAdsCanRequestAdsResult&,
	Result
);
