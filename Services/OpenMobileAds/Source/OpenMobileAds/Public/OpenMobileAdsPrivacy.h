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
	NotRequired,
	Obtained
};

UENUM(BlueprintType)
enum class EOpenMobileAdsGdprApplicability : uint8
{
	Unknown,
	NotApplicable,
	Applicable
};

UENUM(BlueprintType)
enum class EOpenMobileAdsConsentRequirement : uint8
{
	Unknown,
	NotRequired,
	Required
};

UENUM(BlueprintType)
enum class EOpenMobileAdsConsentRequestState : uint8
{
	Unknown,
	Blocked,
	Allowed
};

UENUM(BlueprintType)
enum class EOpenMobileAdsUsPrivacyApplicability : uint8
{
	Unknown,
	NotApplicable,
	Applicable
};

UENUM(BlueprintType)
enum class EOpenMobileAdsUsPrivacyChoice : uint8
{
	Unknown,
	OptedIn,
	OptedOut
};

UENUM(BlueprintType)
enum class EOpenMobileAdsPrivacyOptionsRequirement : uint8
{
	Unknown,
	NotRequired,
	Required
};

UENUM(BlueprintType)
enum class EOpenMobileAdsDataProcessingMode : uint8
{
	Unspecified = 0,
	ProviderManaged = 1,
	Standard = 2,
	Restricted = 3
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsUsPrivacyState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsUsPrivacyApplicability Applicability =
		EOpenMobileAdsUsPrivacyApplicability::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsUsPrivacyChoice Choice =
		EOpenMobileAdsUsPrivacyChoice::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsPrivacyOptionsRequirement PrivacyOptionsRequirement =
		EOpenMobileAdsPrivacyOptionsRequirement::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bPrivacyOptionsFormAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsDataProcessingMode DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Unspecified;
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

UENUM(BlueprintType, meta = (Bitflags))
enum class EOpenMobileAdsConsentSignal : uint8
{
	None = 0,
	Gdpr = 1 << 0,
	UsPrivacy = 1 << 1,
	ChildDirected = 1 << 2,
	UnderAgeOfConsent = 1 << 3
};
ENUM_CLASS_FLAGS(EOpenMobileAdsConsentSignal)

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsConsentSignals
{
	GENERATED_BODY()

	static constexpr int32 AllSignalMask =
		static_cast<int32>(EOpenMobileAdsConsentSignal::Gdpr)
		| static_cast<int32>(EOpenMobileAdsConsentSignal::UsPrivacy)
		| static_cast<int32>(EOpenMobileAdsConsentSignal::ChildDirected)
		| static_cast<int32>(EOpenMobileAdsConsentSignal::UnderAgeOfConsent);

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentStatus ConsentStatus =
		EOpenMobileAdsConsentStatus::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsGdprApplicability GdprApplicability =
		EOpenMobileAdsGdprApplicability::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentRequirement ConsentRequirement =
		EOpenMobileAdsConsentRequirement::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentRequestState ConsentRequestState =
		EOpenMobileAdsConsentRequestState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bConsentStatusFresh = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsUsPrivacyState UsPrivacy;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment ChildDirectedTreatment =
		EOpenMobileAdsAgeTreatment::Unspecified;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsAgeTreatment UnderAgeOfConsent =
		EOpenMobileAdsAgeTreatment::Unspecified;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Source;

	int32 GetConfiguredSignalMask() const;
	int32 GetRequiredSignalMask() const;
	int32 GetChangedSignalMask(
		const FOpenMobileAdsConsentSignals& Other
	) const;

	bool operator==(const FOpenMobileAdsConsentSignals& Other) const;
	bool operator!=(const FOpenMobileAdsConsentSignals& Other) const
	{
		return !(*this == Other);
	}
};

UENUM(BlueprintType)
enum class EOpenMobileAdsConsentSignalConsumerType : uint8
{
	Provider,
	Network,
	Adapter
};

UENUM(BlueprintType)
enum class EOpenMobileAdsConsentSignalDeliveryState : uint8
{
	NotRequired,
	Applied,
	Unconfirmed,
	Unsupported,
	Failed
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsConsentSignalDeliveryStatus
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentSignalConsumerType Type =
		EOpenMobileAdsConsentSignalConsumerType::Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Name;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Parent;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileAds.EOpenMobileAdsConsentSignal")
	)
	int32 ConfiguredSignals = 0;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileAds.EOpenMobileAdsConsentSignal")
	)
	int32 RequiredSignals = 0;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileAds.EOpenMobileAdsConsentSignal")
	)
	int32 AppliedSignals = 0;

	UPROPERTY(
		BlueprintReadOnly,
		Category = "Open Mobile|Ads",
		meta = (Bitmask, BitmaskEnum = "/Script/OpenMobileAds.EOpenMobileAdsConsentSignal")
	)
	int32 ConfirmedSignals = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bRuntimeUpdate = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentSignalDeliveryState State =
		EOpenMobileAdsConsentSignalDeliveryState::NotRequired;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsError Error;
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsConsentSignalDeliverySnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime LastUpdated;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	TArray<FOpenMobileAdsConsentSignalDeliveryStatus> Consumers;

	const FOpenMobileAdsConsentSignalDeliveryStatus* Find(
		EOpenMobileAdsConsentSignalConsumerType Type,
		FName Name,
		FName Parent = NAME_None
	) const;
};

struct OPENMOBILEADS_API FOpenMobileAdsConsentSignalApplyResult
{
	int32 AppliedSignals = 0;
	int32 ConfirmedSignals = 0;
	FOpenMobileAdsError Error;

	static FOpenMobileAdsConsentSignalApplyResult Applied(
		int32 AppliedSignals,
		int32 ConfirmedSignals
	);
};

DECLARE_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsConsentSignalDeliveryNativeEvent,
	const FOpenMobileAdsConsentSignalDeliverySnapshot&
);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FOpenMobileAdsConsentSignalDeliveryDynamicEvent,
	const FOpenMobileAdsConsentSignalDeliverySnapshot&,
	Status
);

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

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsUsPrivacyState UsPrivacy;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsConsentSignals ConsentSignals;
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
	EOpenMobileAdsGdprApplicability GdprApplicability =
		EOpenMobileAdsGdprApplicability::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentRequirement ConsentRequirement =
		EOpenMobileAdsConsentRequirement::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsConsentRequestState ConsentRequestState =
		EOpenMobileAdsConsentRequestState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsUsPrivacyState UsPrivacy;

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
	FDateTime ConsentExpiresAt;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bRestoredFromProviderStorage = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsConsentProviderDetails ProviderDetails;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsError Error;

	bool IsConsentStatusFreshAt(FDateTime Now) const
	{
		return bConsentStatusFresh
			&& (
				ConsentExpiresAt == FDateTime()
				|| Now < ConsentExpiresAt
			);
	}
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
	ProviderPolicy,
	ConsentProviderBlocked,
	PrivacySignalInvalid
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
	EOpenMobileAdsGdprApplicability GdprApplicability =
		EOpenMobileAdsGdprApplicability::Unknown;
	EOpenMobileAdsConsentRequirement Requirement =
		EOpenMobileAdsConsentRequirement::Unknown;
	EOpenMobileAdsConsentRequestState RequestState =
		EOpenMobileAdsConsentRequestState::Unknown;
	FOpenMobileAdsUsPrivacyState UsPrivacy;
	FName Source;
	FOpenMobileAdsConsentProviderDetails ProviderDetails;
	FOpenMobileAdsError Error;
	bool bStatusFresh = true;
	FDateTime ExpiresAt;
	bool bRestoredFromProviderStorage = false;

	static FOpenMobileAdsConsentStatusUpdate BeginRefresh(FName Source);
	static FOpenMobileAdsConsentStatusUpdate BeginFormPresentation(FName Source);
	static FOpenMobileAdsConsentStatusUpdate BeginReset(FName Source);
	static FOpenMobileAdsConsentStatusUpdate Complete(
		EOpenMobileAdsConsentStatus Status,
		FName Source,
		FOpenMobileAdsConsentProviderDetails ProviderDetails = {},
		bool bStatusFresh = true,
		FDateTime ExpiresAt = {},
		bool bRestoredFromProviderStorage = false
	);
	static FOpenMobileAdsConsentStatusUpdate CompleteProviderState(
		EOpenMobileAdsConsentStatus Status,
		EOpenMobileAdsGdprApplicability GdprApplicability,
		EOpenMobileAdsConsentRequirement Requirement,
		EOpenMobileAdsConsentRequestState RequestState,
		FName Source,
		FOpenMobileAdsConsentProviderDetails ProviderDetails = {},
		bool bStatusFresh = true,
		FDateTime ExpiresAt = {},
		bool bRestoredFromProviderStorage = false
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
