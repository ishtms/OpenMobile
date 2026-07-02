#include "OpenMobileHapticAHAPFactory.h"

#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenMobileHapticPlatformAssets.h"
#include "OpenMobileHapticsAHAPPolicy.h"
#include "OpenMobileHapticsAppleAudioResourcePolicy.h"

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
		FOpenMobileHapticsAHAPPolicy::Normalize(Source, {}, true);
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
	const FOpenMobileHapticsAppleAudioResourceLimits AudioLimits;
	if (Normalized.Resource.ExternalAudioResourcePaths.Num()
		> AudioLimits.MaximumResourceCount)
	{
		if (Warn)
		{
			Warn->Logf(
				ELogVerbosity::Error,
				TEXT("AHAP audio resources exceed the count limit")
			);
		}
		return nullptr;
	}
	TArray<FOpenMobileHapticIOSAudioResource> AudioResources;
	AudioResources.Reserve(
		Normalized.Resource.ExternalAudioResourcePaths.Num()
	);
	int64 TotalAudioBytes = 0;
	const FString SourceDirectory = FPaths::GetPath(Filename);
	for (const FString& RelativePath :
		Normalized.Resource.ExternalAudioResourcePaths)
	{
		const FString AudioFilename =
			FPaths::Combine(SourceDirectory, RelativePath);
		const int64 AudioSize = IFileManager::Get().FileSize(*AudioFilename);
		if (AudioSize < 0)
		{
			if (Warn)
			{
				Warn->Logf(
					ELogVerbosity::Error,
					TEXT("AHAP audio resource is missing: %s"),
					*RelativePath
				);
			}
			return nullptr;
		}
		if (AudioSize > AudioLimits.MaximumResourceBytes)
		{
			if (Warn)
			{
				Warn->Logf(
					ELogVerbosity::Error,
					TEXT("AHAP audio resource exceeds its size limit: %s"),
					*RelativePath
				);
			}
			return nullptr;
		}
		TotalAudioBytes += AudioSize;
		if (TotalAudioBytes > AudioLimits.MaximumTotalBytes)
		{
			if (Warn)
			{
				Warn->Logf(
					ELogVerbosity::Error,
					TEXT("AHAP audio resources exceed the total size limit")
				);
			}
			return nullptr;
		}
		FOpenMobileHapticIOSAudioResource& AudioResource =
			AudioResources.AddDefaulted_GetRef();
		AudioResource.RelativePath = RelativePath;
		if (!FFileHelper::LoadFileToArray(
			AudioResource.Data,
			*AudioFilename
		))
		{
			if (Warn)
			{
				Warn->Logf(
					ELogVerbosity::Error,
					TEXT("Could not read AHAP audio resource: %s"),
					*RelativePath
				);
			}
			return nullptr;
		}
	}
	UOpenMobileHapticIOSPatternAsset* Asset =
		NewObject<UOpenMobileHapticIOSPatternAsset>(
			InParent,
			InClass,
			InName,
			Flags
		);
	TArray<FString> Errors;
	if (!Asset || !Asset->SetAHAPSourceWithAudioResources(
		Normalized.Resource.NormalizedJson,
		AudioResources,
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
