#pragma once

#include "CoreMinimal.h"
#include "OpenMobileHapticsTypes.h"

class UOpenMobileHapticLibrary;

class FOpenMobileHapticsLibraryResolver final
{
public:
	/** Starts a new generation so late async completions from an older preparation can be ignored safely. */
	uint64 BeginPreparation();
	/** Publishes the loaded libraries only when their generation is current and every name resolves without conflict. */
	bool CompletePreparation(
		uint64 Generation,
		const TArray<UOpenMobileHapticLibrary*>& Libraries,
		TArray<FString>& Errors
	);
	/** Leaves lookups in a failed state after loading or validation stops, stale prepared names shouldn't survive. */
	void FailPreparation();
	/** Clears all prepared names when the owning subsystem releases its library state. */
	void Release();
	/** Resolves a prepared name without loading the asset, the caller still owns that async step. */
	bool Find(FName PatternName, FSoftObjectPath& OutPattern) const;
	/** Copies the prepared name map for diagnostics without exposing mutable resolver state. */
	void GetPreparedPatterns(
		TArray<TPair<FName, FSoftObjectPath>>& OutPatterns
	) const;
	/** Reports whether a name is usable, missing, or waiting on the current preparation generation. */
	EOpenMobileHapticNamedPatternStatus GetStatus(FName PatternName) const;
	/** Lets async owners compare their captured generation before publishing results. */
	uint64 GetGeneration() const { return Generation; }
	/** Exposes cache size for diagnostics without handing out the map itself. */
	int32 GetPreparedPatternCount() const { return PreparedPatterns.Num(); }

private:
	TMap<FName, FSoftObjectPath> PreparedPatterns;
	EOpenMobileHapticNamedPatternStatus State =
		EOpenMobileHapticNamedPatternStatus::Unprepared;
	uint64 Generation = 0;
};
