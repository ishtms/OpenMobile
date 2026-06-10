#include "OpenMobileHapticLibrary.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UOpenMobileHapticLibrary::BuildPatternLookup(
	TMap<FName, FSoftObjectPath>& OutPatterns,
	TArray<FString>& Errors
) const
{
	Errors.Reset();
	TMap<FName, FSoftObjectPath> ValidPatterns;
	ValidPatterns.Reserve(Patterns.Num());
	for (int32 Index = 0; Index < Patterns.Num(); ++Index)
	{
		const FOpenMobileHapticLibraryEntry& Entry = Patterns[Index];
		if (Entry.Name.IsNone())
		{
			Errors.Add(FString::Printf(
				TEXT("Pattern entry %d has an empty name."),
				Index
			));
			continue;
		}
		if (Entry.Pattern.IsNull())
		{
			Errors.Add(FString::Printf(
				TEXT("Pattern entry %d (%s) has no asset."),
				Index,
				*Entry.Name.ToString()
			));
			continue;
		}
		if (ValidPatterns.Contains(Entry.Name))
		{
			Errors.Add(FString::Printf(
				TEXT("Pattern entry %d conflicts with the name %s."),
				Index,
				*Entry.Name.ToString()
			));
			continue;
		}
		ValidPatterns.Add(Entry.Name, Entry.Pattern.ToSoftObjectPath());
	}

	if (!Errors.IsEmpty())
	{
		OutPatterns.Reset();
		return false;
	}
	OutPatterns = MoveTemp(ValidPatterns);
	return true;
}

#if WITH_EDITOR
EDataValidationResult UOpenMobileHapticLibrary::IsDataValid(
	FDataValidationContext& Context
) const
{
	EDataValidationResult Result = Super::IsDataValid(Context);
	TMap<FName, FSoftObjectPath> Lookup;
	TArray<FString> Errors;
	if (!BuildPatternLookup(Lookup, Errors))
	{
		for (const FString& Error : Errors)
		{
			Context.AddError(FText::FromString(Error));
		}
		return EDataValidationResult::Invalid;
	}
	return CombineDataValidationResults(Result, EDataValidationResult::Valid);
}
#endif
