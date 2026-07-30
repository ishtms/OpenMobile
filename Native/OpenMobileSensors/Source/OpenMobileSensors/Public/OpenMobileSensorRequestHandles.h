#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorRequestHandles.generated.h"

USTRUCT(BlueprintType, meta = (DisplayName = "Sensor Flush Handle"))
struct OPENMOBILESENSORS_API FOpenMobileSensorFlushHandle
{
	GENERATED_BODY()

	FOpenMobileSensorFlushHandle() = default;
	explicit FOpenMobileSensorFlushHandle(FGuid InIdentifier)
		: Identifier(InIdentifier)
	{
	}

	bool IsValid() const
	{
		return Identifier.IsValid();
	}

	const FGuid& GetIdentifier() const
	{
		return Identifier;
	}

	void Reset()
	{
		Identifier.Invalidate();
	}

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

	FOpenMobileNativeStepCountQueryHandle() = default;
	explicit FOpenMobileNativeStepCountQueryHandle(FGuid InIdentifier)
		: Identifier(InIdentifier)
	{
	}

	bool IsValid() const
	{
		return Identifier.IsValid();
	}

	const FGuid& GetIdentifier() const
	{
		return Identifier;
	}

	void Reset()
	{
		Identifier.Invalidate();
	}

	bool operator==(
		const FOpenMobileNativeStepCountQueryHandle& Other) const
	{
		return Identifier == Other.Identifier;
	}

private:
	UPROPERTY(meta = (AllowPrivateAccess = "true"))
	FGuid Identifier;
};
