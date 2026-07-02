#if WITH_DEV_AUTOMATION_TESTS

#include "OpenMobileHapticAHAPFactory.h"

#include "EditorFramework/AssetImportData.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "OpenMobileHapticPlatformAssets.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAHAPFactoryTest,
	"OpenMobile.Haptics.Apple.AHAP.Importer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAHAPFactoryTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	UOpenMobileHapticAHAPFactory* Factory =
		NewObject<UOpenMobileHapticAHAPFactory>();
	TestTrue(TEXT("AHAP extension is importable"),
		Factory->FactoryCanImport(TEXT("Pattern.AHAP")));
	TestFalse(TEXT("Generic JSON is not claimed"),
		Factory->FactoryCanImport(TEXT("Pattern.json")));

	const FString Filename = FPaths::CreateTempFilename(
		FPlatformProcess::UserTempDir(),
		TEXT("OpenMobileAHAP"),
		TEXT(".ahap")
	);
	const FString Source = TEXT(
		"{\"Pattern\":[{\"Event\":{\"Time\":0,"
		"\"EventType\":\"HapticTransient\"}}],\"Version\":1}"
	);
	TestTrue(TEXT("Golden AHAP source is written"),
		FFileHelper::SaveStringToFile(Source, *Filename));
	bool bCancelled = true;
	UOpenMobileHapticIOSPatternAsset* Asset =
		Cast<UOpenMobileHapticIOSPatternAsset>(Factory->FactoryCreateFile(
			UOpenMobileHapticIOSPatternAsset::StaticClass(),
			GetTransientPackage(),
			MakeUniqueObjectName(
				GetTransientPackage(),
				UOpenMobileHapticIOSPatternAsset::StaticClass(),
				TEXT("ImportedAHAP")
			),
			RF_Transient,
			Filename,
			nullptr,
			GWarn,
			bCancelled
		));
	TestNotNull(TEXT("Golden AHAP imports"), Asset);
	TestFalse(TEXT("Successful import is not cancellation"), bCancelled);
	if (Asset)
	{
		TestEqual(TEXT("Importer stores deterministic runtime data"),
			Asset->GetNormalizedAHAPJson(),
			TEXT("{\"Version\":1,\"Pattern\":[{\"Event\":{"
				"\"EventType\":\"HapticTransient\",\"Time\":0}}]}"));
		TestNotNull(TEXT("Importer creates source provenance"),
			Asset->GetAssetImportData());
		if (Asset->GetAssetImportData())
		{
			TestEqual(TEXT("Provenance retains the source path"),
				Asset->GetAssetImportData()->GetFirstFilename(),
				FPaths::ConvertRelativePathToFull(Filename));
		}
	}

	const FString InvalidFilename = FPaths::CreateTempFilename(
		FPlatformProcess::UserTempDir(),
		TEXT("OpenMobileInvalidAHAP"),
		TEXT(".ahap")
	);
	TestTrue(TEXT("Invalid AHAP fixture is written"),
		FFileHelper::SaveStringToFile(TEXT("{invalid"), *InvalidFilename));
	AddExpectedError(
		TEXT("AHAP is not valid JSON"),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	bCancelled = true;
	TestNull(TEXT("Malformed AHAP import returns no asset"),
		Factory->FactoryCreateFile(
			UOpenMobileHapticIOSPatternAsset::StaticClass(),
			GetTransientPackage(),
			MakeUniqueObjectName(
				GetTransientPackage(),
				UOpenMobileHapticIOSPatternAsset::StaticClass(),
				TEXT("InvalidAHAP")
			),
			RF_Transient,
			InvalidFilename,
			nullptr,
			GWarn,
			bCancelled
		));
	TestFalse(TEXT("Invalid data is failure, not cancellation"), bCancelled);
	IFileManager::Get().Delete(*Filename);
	IFileManager::Get().Delete(*InvalidFilename);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileHapticsAHAPAudioFactoryTest,
	"OpenMobile.Haptics.Apple.AHAP.AudioImporter",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileHapticsAHAPAudioFactoryTest::RunTest(
	const FString& Parameters
)
{
	static_cast<void>(Parameters);
	const FString Directory = FPaths::Combine(
		FPlatformProcess::UserTempDir(),
		TEXT("OpenMobileAHAPAudio_")
			+ FGuid::NewGuid().ToString(EGuidFormats::Digits)
	);
	const FString AudioDirectory = FPaths::Combine(Directory, TEXT("Audio"));
	TestTrue(TEXT("Audio import directory is created"),
		IFileManager::Get().MakeDirectory(*AudioDirectory, true));
	const FString AudioFilename =
		FPaths::Combine(AudioDirectory, TEXT("Explosion.caf"));
	const TArray<uint8> AudioBytes = {
		static_cast<uint8>('c'),
		static_cast<uint8>('a'),
		static_cast<uint8>('f'),
		static_cast<uint8>('f'),
		0,
		1,
		0,
		0
	};
	TestTrue(TEXT("CAF fixture is written"),
		FFileHelper::SaveArrayToFile(AudioBytes, *AudioFilename));
	const FString AHAPFilename =
		FPaths::Combine(Directory, TEXT("Synchronized.ahap"));
	const FString Source = TEXT(
		"{\"Version\":1,\"Pattern\":[{\"Event\":{"
		"\"EventType\":\"AudioCustom\",\"Time\":0,\"Duration\":0.25,"
		"\"EventWaveformPath\":\"Audio/Explosion.caf\"}},{\"Event\":{"
		"\"EventType\":\"HapticTransient\",\"Time\":0}}]}"
	);
	TestTrue(TEXT("Synchronized AHAP fixture is written"),
		FFileHelper::SaveStringToFile(Source, *AHAPFilename));

	UOpenMobileHapticAHAPFactory* Factory =
		NewObject<UOpenMobileHapticAHAPFactory>();
	bool bCancelled = true;
	UOpenMobileHapticIOSPatternAsset* Asset =
		Cast<UOpenMobileHapticIOSPatternAsset>(Factory->FactoryCreateFile(
			UOpenMobileHapticIOSPatternAsset::StaticClass(),
			GetTransientPackage(),
			MakeUniqueObjectName(
				GetTransientPackage(),
				UOpenMobileHapticIOSPatternAsset::StaticClass(),
				TEXT("ImportedSynchronizedAHAP")
			),
			RF_Transient,
			AHAPFilename,
			nullptr,
			GWarn,
			bCancelled
		));
	TestNotNull(TEXT("AHAP with local custom audio imports"), Asset);
	if (Asset)
	{
		TestTrue(TEXT("Custom audio presence is retained"),
			Asset->ContainsCustomAudioEvents());
		TestEqual(TEXT("One unique audio resource is cooked"),
			Asset->GetAudioResources().Num(), 1);
		if (Asset->GetAudioResources().Num() == 1)
		{
			TestEqual(TEXT("The normalized relative path is retained"),
				Asset->GetAudioResources()[0].RelativePath,
				FString(TEXT("Audio/Explosion.caf")));
			TestTrue(TEXT("The validated source bytes are retained"),
				Asset->GetAudioResources()[0].Data == AudioBytes);
		}
		TestTrue(TEXT("The custom audio path remains in normalized AHAP"),
			Asset->GetNormalizedAHAPJson().Contains(
				TEXT("\"EventWaveformPath\":\"Audio/Explosion.caf\"")
			));
	}

	const FString MissingFilename =
		FPaths::Combine(Directory, TEXT("Missing.ahap"));
	TestTrue(TEXT("Missing-resource AHAP fixture is written"),
		FFileHelper::SaveStringToFile(
			Source.Replace(TEXT("Audio/Explosion.caf"), TEXT("Audio/Missing.caf")),
			*MissingFilename
		));
	AddExpectedError(
		TEXT("AHAP audio resource is missing"),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	bCancelled = true;
	TestNull(TEXT("A missing audio resource rejects the import"),
		Factory->FactoryCreateFile(
			UOpenMobileHapticIOSPatternAsset::StaticClass(),
			GetTransientPackage(),
			MakeUniqueObjectName(
				GetTransientPackage(),
				UOpenMobileHapticIOSPatternAsset::StaticClass(),
				TEXT("MissingSynchronizedAHAP")
			),
			RF_Transient,
			MissingFilename,
			nullptr,
			GWarn,
			bCancelled
		));
	IFileManager::Get().DeleteDirectory(*Directory, false, true);
	return true;
}

#endif
