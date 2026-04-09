#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsDiagnostics.h"
#include "OpenMobileAdsErrors.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdsErrorCode : uint8
{
	None,
	NotConfigured,
	ProviderConflict,
	ProviderUnavailable,
	UnsupportedPlatform,
	UnsupportedFormat,
	UnknownPlacement,
	InvalidPlacement,
	DisabledPlacement,
	Busy,
	NotReady,
	Cancelled,
	PrivacyBlocked,
	InvalidState,
	ProviderFailure,
	NativeFailure,
	Internal
};

UENUM(BlueprintType)
enum class EOpenMobileAdsFailureStage : uint8
{
	None,
	Configuration,
	ProviderSelection,
	Initialization,
	Load,
	Show,
	Refresh,
	Consent,
	Teardown,
	Internal
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsError
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsErrorCode Code = EOpenMobileAdsErrorCode::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsFailureStage Stage = EOpenMobileAdsFailureStage::None;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Placement;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Explanation;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString SuggestedCorrection;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	bool bRetryable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FOpenMobileAdsNativeDiagnostics NativeDiagnostics;

	bool IsSet() const
	{
		return Code != EOpenMobileAdsErrorCode::None;
	}

	static FOpenMobileAdsError Make(
		EOpenMobileAdsErrorCode InCode,
		EOpenMobileAdsFailureStage InStage,
		FName InPlacement,
		FString InExplanation,
		FName InProvider = NAME_None,
		FString InSuggestedCorrection = FString(),
		bool bInRetryable = false
	)
	{
		FOpenMobileAdsError Error;
		Error.Code = InCode;
		Error.Stage = InStage;
		Error.Placement = InPlacement;
		Error.Provider = InProvider;
		Error.Explanation = MoveTemp(InExplanation);
		Error.SuggestedCorrection = MoveTemp(InSuggestedCorrection);
		Error.bRetryable = bInRetryable;
		return Error;
	}
};
