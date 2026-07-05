#include "OpenMobileHapticsLibraryResolver.h"

#include "OpenMobileHapticLibrary.h"
#include "OpenMobileHapticPatternAsset.h"

uint64 FOpenMobileHapticsLibraryResolver::BeginPreparation()
{
	++Generation;
	if (Generation == 0)
	{
		++Generation;
	}
	PreparedPatterns.Reset();
	State = EOpenMobileHapticNamedPatternStatus::Loading;
	return Generation;
}

bool FOpenMobileHapticsLibraryResolver::CompletePreparation(
	uint64 CompletedGeneration,
	const TArray<UOpenMobileHapticLibrary*>& Libraries,
	TArray<FString>& Errors
)
{
	Errors.Reset();
	if (CompletedGeneration != Generation
		|| State != EOpenMobileHapticNamedPatternStatus::Loading)
	{
		return false;
	}

	TMap<FName, FSoftObjectPath> ResolvedPatterns;
	for (int32 LibraryIndex = 0; LibraryIndex < Libraries.Num(); ++LibraryIndex)
	{
		const UOpenMobileHapticLibrary* Library = Libraries[LibraryIndex];
		if (!Library)
		{
			Errors.Add(FString::Printf(
				TEXT("Library %d did not load."),
				LibraryIndex
			));
			continue;
		}

		TMap<FName, FSoftObjectPath> LibraryPatterns;
		TArray<FString> LibraryErrors;
		if (!Library->BuildPatternLookup(LibraryPatterns, LibraryErrors))
		{
			for (const FString& Error : LibraryErrors)
			{
				Errors.Add(FString::Printf(
					TEXT("Library %d: %s"),
					LibraryIndex,
					*Error
				));
			}
			continue;
		}

		for (const TPair<FName, FSoftObjectPath>& Pair : LibraryPatterns)
		{
			if (ResolvedPatterns.Contains(Pair.Key))
			{
				continue;
			}
			const UOpenMobileHapticPatternAsset* Pattern =
				Cast<UOpenMobileHapticPatternAsset>(Pair.Value.ResolveObject());
			if (!Pattern || !Pattern->IsDerivedDataCurrent())
			{
				Errors.Add(FString::Printf(
					TEXT("Library %d pattern %s did not load."),
					LibraryIndex,
					*Pair.Key.ToString()
				));
				continue;
			}
			ResolvedPatterns.Add(Pair.Key, Pair.Value);
		}
	}

	if (!Errors.IsEmpty())
	{
		PreparedPatterns.Reset();
		State = EOpenMobileHapticNamedPatternStatus::Invalid;
		return false;
	}
	PreparedPatterns = MoveTemp(ResolvedPatterns);
	State = EOpenMobileHapticNamedPatternStatus::Loaded;
	return true;
}

void FOpenMobileHapticsLibraryResolver::FailPreparation()
{
	PreparedPatterns.Reset();
	State = EOpenMobileHapticNamedPatternStatus::Invalid;
}

void FOpenMobileHapticsLibraryResolver::Release()
{
	++Generation;
	if (Generation == 0)
	{
		++Generation;
	}
	PreparedPatterns.Reset();
	State = EOpenMobileHapticNamedPatternStatus::Unprepared;
}

bool FOpenMobileHapticsLibraryResolver::Find(
	FName PatternName,
	FSoftObjectPath& OutPattern
) const
{
	if (State != EOpenMobileHapticNamedPatternStatus::Loaded)
	{
		return false;
	}
	const FSoftObjectPath* Pattern = PreparedPatterns.Find(PatternName);
	if (!Pattern)
	{
		return false;
	}
	OutPattern = *Pattern;
	return true;
}

void FOpenMobileHapticsLibraryResolver::GetPreparedPatterns(
	TArray<TPair<FName, FSoftObjectPath>>& OutPatterns
) const
{
	OutPatterns.Reset();
	if (State != EOpenMobileHapticNamedPatternStatus::Loaded)
	{
		return;
	}
	OutPatterns.Reserve(PreparedPatterns.Num());
	for (const TPair<FName, FSoftObjectPath>& Pattern : PreparedPatterns)
	{
		OutPatterns.Add(Pattern);
	}
	OutPatterns.Sort([](
		const TPair<FName, FSoftObjectPath>& Left,
		const TPair<FName, FSoftObjectPath>& Right
	)
	{
		return Left.Key.LexicalLess(Right.Key);
	});
}

EOpenMobileHapticNamedPatternStatus
FOpenMobileHapticsLibraryResolver::GetStatus(FName PatternName) const
{
	if (State == EOpenMobileHapticNamedPatternStatus::Loaded)
	{
		return PreparedPatterns.Contains(PatternName)
			? EOpenMobileHapticNamedPatternStatus::Loaded
			: EOpenMobileHapticNamedPatternStatus::Missing;
	}
	return State;
}
