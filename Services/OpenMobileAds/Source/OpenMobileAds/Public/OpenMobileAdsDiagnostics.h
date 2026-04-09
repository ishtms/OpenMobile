#pragma once

#include "CoreMinimal.h"
#include "OpenMobileAdsDiagnostics.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileAdsLogLevel : uint8
{
	Error,
	Warning,
	Info,
	Verbose,
	VeryVerbose
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsNativeDiagnostics
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString NativeCode;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString NativeMessage;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Network;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Adapter;

	bool IsSet() const
	{
		return !NativeCode.IsEmpty()
			|| !NativeMessage.IsEmpty()
			|| !Provider.IsNone()
			|| !Network.IsEmpty()
			|| !Adapter.IsEmpty();
	}
};

USTRUCT(BlueprintType)
struct OPENMOBILEADS_API FOpenMobileAdsDiagnosticRecord
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	EOpenMobileAdsLogLevel Level = EOpenMobileAdsLogLevel::Info;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FDateTime Timestamp;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Stage;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Placement;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FName Provider;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Ads")
	FString Message;
};
