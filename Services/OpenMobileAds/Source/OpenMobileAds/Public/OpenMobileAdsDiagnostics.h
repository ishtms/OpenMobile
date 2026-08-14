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

	/** Treats any native SDK detail as a real diagnostics payload even when the numeric code is empty. */
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
	/** Applies global, Ads-specific, and development-test verbosity before formatting a message. */
	static bool ShouldLog(EOpenMobileAdsLogLevel Level);
	/** Resolves verbosity from supplied values so tests and editor validation don't touch live console state. */
	static bool IsLevelEnabled(
		EOpenMobileAdsLogLevel Level,
		int32 GlobalLevel,
		int32 AdsLevel,
		bool bDevelopmentTestMode = false
	);
	/** Allows additional diagnostics only while development test mode is explicitly active. */
	static void SetDevelopmentTestMode(bool bEnabled);
	/** Registers identifiers that must be removed from every later Ads log message. */
	static void SetTestDeviceIdentifiers(const TArray<FString>& Identifiers);
	/** Reads the Ads console verbosity independently from the engine-wide log level. */
	static int32 GetAdsLevel();
	/** Removes known identifiers and credential-like values before text reaches logs or errors. */
	static FString Redact(
		const FString& Message,
		const TArray<FString>& SensitiveValues = {}
	);
	/** Redacts every free-text native diagnostics field while keeping routing names intact. */
	static FOpenMobileAdsNativeDiagnostics Redact(
		const FOpenMobileAdsNativeDiagnostics& Diagnostics,
		const TArray<FString>& SensitiveValues = {}
	);
	/** Writes a provider-neutral record only after its message has passed redaction. */
	static void Write(
		EOpenMobileAdsLogLevel Level,
		const FString& Message,
		FName Placement = NAME_None,
		FName Provider = NAME_None,
		const TArray<FString>& SensitiveValues = {}
	);
};
