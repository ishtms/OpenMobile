#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorRequestHandles.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Sensor Flush Handle"))
struct OPENMOBILESENSORS_API FOpenMobileSensorFlushHandle
{
	GENERATED_BODY()

	/** The default handle is intentionally invalid, which is handy before a flush request exists. */
	FOpenMobileSensorFlushHandle() = default;

	/** Sensors uses this when a flush request receives its GUID. Callers should normally keep the handle returned by the subsystem. */
	explicit FOpenMobileSensorFlushHandle(FGuid InIdentifier)
		: Identifier(InIdentifier)
	{
	}

	/** Use this before comparing or storing a flush request. A reset or never-issued handle returns false. */
	bool IsValid() const
	{
		return Identifier.IsValid();
	}

	/** You'll get the request GUID without copying it. Don't treat that GUID as a subscription handle. */
	const FGuid& GetIdentifier() const
	{
		return Identifier;
	}

	/** Use this after you no longer own the request token. Resetting won't cancel the flush itself. */
	void Reset()
	{
		Identifier.Invalidate();
	}

	/** This compares request identity only. Matching handles refer to the same flush attempt. */
	bool operator==(const FOpenMobileSensorFlushHandle& Other) const
	{
		return Identifier == Other.Identifier;
	}

private:
	UPROPERTY(meta = (AllowPrivateAccess = "true"))
	FGuid Identifier;
};

USTRUCT(BlueprintType, meta = (DisplayName = "Historical Step Query Handle"))
struct OPENMOBILESENSORS_API FOpenMobileNativeStepCountQueryHandle
{
	GENERATED_BODY()

	/** The default handle is intentionally invalid, which is fine before a step query exists. */
	FOpenMobileNativeStepCountQueryHandle() = default;

	/** Sensors uses this when a historical query receives its GUID. Callers should normally keep the returned handle only. */
	explicit FOpenMobileNativeStepCountQueryHandle(FGuid InIdentifier)
		: Identifier(InIdentifier)
	{
	}

	/** Use this before cancelling or comparing a historical query. A reset or never-issued handle returns false. */
	bool IsValid() const
	{
		return Identifier.IsValid();
	}

	/** You'll get the query GUID without copying it. It belongs to the step-query service only. */
	const FGuid& GetIdentifier() const
	{
		return Identifier;
	}

	/** Use this after the query has completed or ownership has ended. Resetting won't cancel native work. */
	void Reset()
	{
		Identifier.Invalidate();
	}

	/** This compares query identity only. Matching handles refer to the same historical request. */
	bool operator==(
		const FOpenMobileNativeStepCountQueryHandle& Other) const
	{
		return Identifier == Other.Identifier;
	}

private:
	UPROPERTY(meta = (AllowPrivateAccess = "true"))
	FGuid Identifier;
};
