#pragma once

#include "CoreMinimal.h"
#include "OpenMobileDeviceEndpointReachabilityTypes.generated.h"

UENUM(BlueprintType)
enum class EOpenMobileEndpointReachabilityOutcome : uint8
{
	Unknown,
	Succeeded,
	DnsFailure,
	ConnectionFailure,
	TlsFailure,
	Timeout,
	Cancelled,
	HttpResponseRejected,
	RedirectRejected,
	ResponseTooLarge,
	ConcurrencyLimitReached,
	InvalidRequest,
	TransportFailure
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileEndpointReachabilityOptions
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	float TimeoutSeconds = 10.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	int32 MinimumAcceptedStatusCode = 200;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	int32 MaximumAcceptedStatusCode = 299;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	bool bAllowRedirects = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Open Mobile|Device")
	int64 MaximumResponseBytes = 64 * 1024;
};

USTRUCT(BlueprintType)
struct OPENMOBILEDEVICE_API FOpenMobileEndpointReachabilityResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	EOpenMobileEndpointReachabilityOutcome Outcome =
		EOpenMobileEndpointReachabilityOutcome::Unknown;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bHttpStatusAvailable = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	int32 HttpStatusCode = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	bool bRedirected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	int64 ResponseBytes = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Open Mobile|Device")
	float DurationSeconds = 0.0f;

	bool IsReachable() const
	{
		return Outcome == EOpenMobileEndpointReachabilityOutcome::Succeeded;
	}
};
