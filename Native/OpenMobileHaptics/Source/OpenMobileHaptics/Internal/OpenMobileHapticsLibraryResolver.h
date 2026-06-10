#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

class UOpenMobileHapticLibrary;

class FOpenMobileHapticsLibraryResolver final
{
public:
	uint64 BeginPreparation();
	bool CompletePreparation(
		uint64 Generation,
		const TArray<UOpenMobileHapticLibrary*>& Libraries,
		TArray<FString>& Errors
	);
	void Release();
	bool Find(FName PatternName, FSoftObjectPath& OutPattern) const;
	EOpenMobileHapticNamedPatternStatus GetStatus(FName PatternName) const;
	uint64 GetGeneration() const { return Generation; }
	int32 GetPreparedPatternCount() const { return PreparedPatterns.Num(); }

private:
	TMap<FName, FSoftObjectPath> PreparedPatterns;
	EOpenMobileHapticNamedPatternStatus State =
		EOpenMobileHapticNamedPatternStatus::Unprepared;
	uint64 Generation = 0;
};
