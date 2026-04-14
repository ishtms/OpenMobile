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

class OPENMOBILEADS_API FOpenMobileAdsLog
{
public:
	static bool ShouldLog(EOpenMobileAdsLogLevel Level);
	static bool IsLevelEnabled(
		EOpenMobileAdsLogLevel Level,
		int32 GlobalLevel,
		int32 AdsLevel,
		bool bDevelopmentTestMode = false
	);
	static void SetDevelopmentTestMode(bool bEnabled);
	static void SetTestDeviceIdentifiers(const TArray<FString>& Identifiers);
	static int32 GetAdsLevel();
	static FString Redact(
		const FString& Message,
		const TArray<FString>& SensitiveValues = {}
	);
	static FOpenMobileAdsNativeDiagnostics Redact(
		const FOpenMobileAdsNativeDiagnostics& Diagnostics,
		const TArray<FString>& SensitiveValues = {}
	);
	static void Write(
		EOpenMobileAdsLogLevel Level,
		const FString& Message,
		FName Placement = NAME_None,
		FName Provider = NAME_None,
		const TArray<FString>& SensitiveValues = {}
	);
};
