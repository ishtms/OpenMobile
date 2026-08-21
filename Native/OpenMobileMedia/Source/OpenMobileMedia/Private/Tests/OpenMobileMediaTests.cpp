#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "OpenMobileMediaBlueprintLibrary.h"
#include "OpenMobilePickPhotoAsyncAction.h"
#include "OpenMobileMediaPlatform.h"
#include "IOpenMobileMediaBackend.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Features/IModularFeatures.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileMediaMetadataFormattingTest,
	"OpenMobile.Media.MetadataFormatting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileMediaMetadataFormattingTest::RunTest(const FString& Parameters)
{
	static_cast<void>(Parameters);
	FOpenMobileMediaMetadata Metadata;
	Metadata.FileName = TEXT("sample.jpg");
	Metadata.MimeType = TEXT("image/jpeg");
	Metadata.FileSizeBytes = 2 * 1024 * 1024;
	Metadata.Width = 4032;
	Metadata.Height = 3024;
	Metadata.CameraMake = TEXT("Example");
	Metadata.CameraModel = TEXT("Camera");
	Metadata.Aperture = 1.8f;
	Metadata.ExposureTimeSeconds = 1.0f / 120.0f;
	Metadata.Iso = 100;

	const FString Display = UOpenMobileMediaBlueprintLibrary::FormatPhotoMetadata(Metadata).ToString();
	TestTrue(TEXT("Includes file name"), Display.Contains(TEXT("sample.jpg")));
	TestTrue(TEXT("Includes dimensions"), Display.Contains(TEXT("4032 x 3024")));
	TestTrue(TEXT("Includes camera"), Display.Contains(TEXT("Example Camera")));
	TestTrue(TEXT("Includes ISO"), Display.Contains(TEXT("ISO 100")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileMediaWorldCleanupTest,
	"OpenMobile.Media.Picker.WorldCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileMediaWorldCleanupTest::RunTest(const FString& Parameters)
{
	class FPickerBackend final : public IOpenMobileMediaBackend
	{
	public:
		FName GetBackendName() const override { return TEXT("PickerTest"); }
		bool IsAvailable() const override { return true; }
		int64 LastRequestId = 0;
		int32 Cancels = 0;
		bool LaunchPhotoPicker(int64 RequestId, FString&) override { LastRequestId = RequestId; return true; }
		void CancelPhotoPicker(int64 RequestId) override { if (RequestId == LastRequestId) ++Cancels; }
	} Backend;
	IModularFeatures::Get().RegisterModularFeature(IOpenMobileMediaBackend::GetModularFeatureName(), &Backend);
	UGameInstance* GameInstance = NewObject<UGameInstance>(GEngine);
	GameInstance->InitializeStandalone(TEXT("PickerCleanup"));
	UWorld* World = GameInstance->GetWorld();
	auto* Action = UOpenMobilePickPhotoAsyncAction::PickPhoto(World);
	Action->Activate();
	const int64 AbandonedRequestId = Backend.LastRequestId;
	FWorldDelegates::OnWorldCleanup.Broadcast(World, true, true);
	TestNull(TEXT("World cleanup releases the original context"), Action->StoredWorldContextObject.Get());
	TestTrue(TEXT("World cleanup finishes the action"), Action->bFinished);
	TestEqual(TEXT("World cleanup requests native dismissal once"), Backend.Cancels, 1);
	Action->Cancel();
	TestEqual(TEXT("Repeated cancellation is harmless"), Backend.Cancels, 1);
	FOpenMobileError Error;
	int64 NextRequestId = 0;
	int32 Picked = 0;
	TestTrue(TEXT("The abandoned picker does not block the next request"),
		FOpenMobileMediaPlatform::BeginPick(
			FOnOpenMobileNativePhotoPicked::CreateLambda([&](const FString&, const FString&) { ++Picked; }),
			{}, {}, NextRequestId, Error));
	TestTrue(TEXT("The new request has a distinct identity"), NextRequestId != AbandonedRequestId);
	const FString StalePath = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("AbandonedPickerImage.tmp"));
	FFileHelper::SaveStringToFile(TEXT("late photo"), *StalePath);
	FOpenMobileMediaPlatform::NativePicked(AbandonedRequestId, StalePath, TEXT("{}"));
	TestFalse(TEXT("Abandoned native images are deleted"), IFileManager::Get().FileExists(*StalePath));
	FOpenMobileMediaPlatform::NativeCancelled(AbandonedRequestId);
	FOpenMobileMediaPlatform::NativeError(AbandonedRequestId, TEXT("late error"));
	TestEqual(TEXT("Stale callbacks cannot complete a new request"), Picked, 0);
	FOpenMobileMediaPlatform::NativePicked(NextRequestId, TEXT(""), TEXT("{}"));
	TestEqual(TEXT("The current callback completes the new request"), Picked, 1);
	FOpenMobileMediaPlatform::NativePicked(NextRequestId, TEXT(""), TEXT("{}"));
	TestEqual(TEXT("Duplicate completion is ignored"), Picked, 1);
	GameInstance->Shutdown();
	World->DestroyWorld(true);
	GEngine->DestroyWorldContext(World);
	IModularFeatures::Get().UnregisterModularFeature(IOpenMobileMediaBackend::GetModularFeatureName(), &Backend);
	return true;
}

#endif
