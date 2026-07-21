#include "OpenMobileHapticPatternFactory.h"

#include "OpenMobileHapticPatternAsset.h"

UOpenMobileHapticPatternFactory::UOpenMobileHapticPatternFactory()
{
	SupportedClass = UOpenMobileHapticPatternAsset::StaticClass();
	bCreateNew = true;
	bEditAfterNew = true;
}

UObject* UOpenMobileHapticPatternFactory::FactoryCreateNew(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext* Warn
)
{
	static_cast<void>(Context);
	static_cast<void>(Warn);
	UOpenMobileHapticPatternAsset* Asset =
		NewObject<UOpenMobileHapticPatternAsset>(
			InParent,
			InClass,
			InName,
			Flags | RF_Transactional
		);
	if (!Asset)
	{
		return nullptr;
	}
	FOpenMobileHapticPatternEvent Event;
	Event.Type = EOpenMobileHapticPatternEventType::Transient;
	Asset->SourcePattern.Events.Add(Event);
	TArray<FString> Errors;
	if (!Asset->RebuildDerivedData(Errors))
	{
		return nullptr;
	}
	return Asset;
}
