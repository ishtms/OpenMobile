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

#endif
