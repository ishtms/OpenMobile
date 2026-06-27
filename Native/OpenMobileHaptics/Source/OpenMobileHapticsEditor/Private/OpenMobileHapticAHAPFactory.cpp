#include "OpenMobileHapticAHAPFactory.h"

#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsAHAPPolicy.h"

UOpenMobileHapticAHAPFactory::UOpenMobileHapticAHAPFactory(
	const FObjectInitializer& ObjectInitializer
)
	: Super(ObjectInitializer)
{
	Formats.Add(TEXT("ahap;Apple Haptic and Audio Pattern"));
	SupportedClass = UOpenMobileHapticIOSPatternAsset::StaticClass();
	bEditorImport = true;
	bText = true;
}

bool UOpenMobileHapticAHAPFactory::FactoryCanImport(
	const FString& Filename
)
{
	return FPaths::GetExtension(Filename).Equals(
		TEXT("ahap"),
		ESearchCase::IgnoreCase
	);
}

UObject* UOpenMobileHapticAHAPFactory::FactoryCreateFile(
	UClass* InClass,
	UObject* InParent,
	FName InName,
	EObjectFlags Flags,
	const FString& Filename,
	const TCHAR* Parms,
	FFeedbackContext* Warn,
	bool& bOutOperationCanceled
)
{
	static_cast<void>(Parms);
	bOutOperationCanceled = false;
	if (!InClass || !InClass->IsChildOf(
		UOpenMobileHapticIOSPatternAsset::StaticClass())
		|| !FactoryCanImport(Filename))
	{
		return nullptr;
	}
	const int64 SourceSize = IFileManager::Get().FileSize(*Filename);
	if (SourceSize < 0 || SourceSize > 256 * 1024)
	{
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error,
				TEXT("AHAP source must be no larger than 256 KiB: %s"),
				*Filename);
		}
		return nullptr;
	}
	FString Source;
	if (!FFileHelper::LoadFileToString(Source, *Filename))
	{
		if (Warn)
		{
			Warn->Logf(ELogVerbosity::Error,
				TEXT("Could not read AHAP source: %s"), *Filename);
		}
		return nullptr;
	}
	const FOpenMobileHapticsAHAPNormalizationResult Normalized =
		FOpenMobileHapticsAHAPPolicy::Normalize(Source);
	if (!Normalized.bSuccess)
	{
		if (Warn)
		{
			Warn->Logf(
				ELogVerbosity::Error,
				TEXT("%s"),
				*FOpenMobileHapticsAHAPPolicy::DescribeError(Normalized)
			);
		}
		return nullptr;
	}
	UOpenMobileHapticIOSPatternAsset* Asset =
		NewObject<UOpenMobileHapticIOSPatternAsset>(
			InParent,
			InClass,
			InName,
			Flags
		);
	TArray<FString> Errors;
	if (!Asset || !Asset->SetAHAPSource(
		Normalized.Resource.NormalizedJson,
		Errors
	))
	{
		if (Warn)
		{
			for (const FString& Error : Errors)
			{
				Warn->Logf(ELogVerbosity::Error, TEXT("%s"), *Error);
			}
		}
		return nullptr;
	}
	if (UAssetImportData* ImportData = Asset->GetAssetImportData())
	{
		ImportData->Update(Filename);
	}
	return Asset;
}
