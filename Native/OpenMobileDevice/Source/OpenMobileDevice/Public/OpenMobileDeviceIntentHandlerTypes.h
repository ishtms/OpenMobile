#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"
#include "OpenMobileDeviceIntentHandlerTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileIntentHandlerQueryKind : uint8
{
	Url,
	DeclaredIntent
};

UENUM(BlueprintType)
enum class EOpenMobileIntentHandlerCheckState : uint8
{
	Unknown,
	CanHandle,
	CannotHandle,
	NotDeclared,
	Unsupported,
	ConfigurationLimitExceeded,
	InvalidRequest,
	Failed
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileIntentHandlerCheckRequest
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	EOpenMobileIntentHandlerQueryKind Kind =
		EOpenMobileIntentHandlerQueryKind::Url;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Device",
		meta = (ToolTip = "One absolute http, https, or configured custom URL to check. The URL is never launched or logged.")
	)
	FString Url;

	UPROPERTY(
		EditAnywhere,
		BlueprintReadWrite,
		Category = "Open Mobile|Device",
		meta = (ToolTip = "One exact action from Declared Android Intent Actions. iOS reports this query kind as Unsupported.")
	)
	FString DeclaredIntentAction;
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileIntentHandlerCheckResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileIntentHandlerCheckState State =
		EOpenMobileIntentHandlerCheckState::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileIntentHandlerQueryKind Kind =
		EOpenMobileIntentHandlerQueryKind::Url;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FString Scheme;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	FOpenMobileError Error;
};
