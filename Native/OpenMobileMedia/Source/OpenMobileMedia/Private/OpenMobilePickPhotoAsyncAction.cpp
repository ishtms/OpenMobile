#include "OpenMobilePickPhotoAsyncAction.h"

#include "Async/Async.h"
#include "Dom/JsonObject.h"
#include "Engine/Texture2D.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"
#include "OpenMobileMediaPlatform.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace OpenMobileMediaAsyncPrivate
{
	bool ParseMetadata(const FString& Json, FOpenMobileMediaMetadata& OutMetadata, FString& OutError)
	{
		TSharedPtr<FJsonObject> Object;
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Json);
		if (!FJsonSerializer::Deserialize(Reader, Object) || !Object.IsValid())
		{
			OutError = TEXT("The platform returned invalid photo metadata.");
			return false;
		}

		OutMetadata.RawMetadataJson = Json;
		Object->TryGetStringField(TEXT("fileName"), OutMetadata.FileName);
		Object->TryGetStringField(TEXT("mimeType"), OutMetadata.MimeType);
		Object->TryGetStringField(TEXT("dateTaken"), OutMetadata.DateTaken);
		Object->TryGetStringField(TEXT("cameraMake"), OutMetadata.CameraMake);
		Object->TryGetStringField(TEXT("cameraModel"), OutMetadata.CameraModel);
		Object->TryGetStringField(TEXT("lensModel"), OutMetadata.LensModel);

		double Number = 0.0;
		if (Object->TryGetNumberField(TEXT("fileSizeBytes"), Number))
		{
			OutMetadata.FileSizeBytes = static_cast<int64>(Number);
		}
		if (Object->TryGetNumberField(TEXT("width"), Number))
		{
			OutMetadata.Width = static_cast<int32>(Number);
		}
		if (Object->TryGetNumberField(TEXT("height"), Number))
		{
			OutMetadata.Height = static_cast<int32>(Number);
		}
		if (Object->TryGetNumberField(TEXT("exifOrientation"), Number))
		{
			OutMetadata.ExifOrientation = static_cast<int32>(Number);
		}
		if (Object->TryGetNumberField(TEXT("aperture"), Number))
		{
			OutMetadata.Aperture = static_cast<float>(Number);
		}
		if (Object->TryGetNumberField(TEXT("exposureTimeSeconds"), Number))
		{
			OutMetadata.ExposureTimeSeconds = static_cast<float>(Number);
		}
		if (Object->TryGetNumberField(TEXT("iso"), Number))
		{
			OutMetadata.Iso = static_cast<int32>(Number);
		}
		if (Object->TryGetNumberField(TEXT("focalLengthMm"), Number))
		{
			OutMetadata.FocalLengthMm = static_cast<float>(Number);
		}

		Object->TryGetBoolField(TEXT("hasLocation"), OutMetadata.bHasLocation);
		if (OutMetadata.bHasLocation)
		{
			Object->TryGetNumberField(TEXT("latitude"), OutMetadata.Latitude);
			Object->TryGetNumberField(TEXT("longitude"), OutMetadata.Longitude);
		}

		return true;
	}
}

UOpenMobilePickPhotoAsyncAction* UOpenMobilePickPhotoAsyncAction::PickPhoto(const UObject* WorldContextObject)
{
	UOpenMobilePickPhotoAsyncAction* Action = NewObject<UOpenMobilePickPhotoAsyncAction>();
	Action->StoredWorldContextObject = const_cast<UObject*>(WorldContextObject);
	return Action;
}

void UOpenMobilePickPhotoAsyncAction::Activate()
{
	if (bActivated || bFinished)
	{
		return;
	}
	bActivated = true;
	Super::Activate();
	UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(
		StoredWorldContextObject.Get(), EGetWorldErrorMode::ReturnNull) : nullptr;
	if (!World || !World->GetGameInstance())
	{
		FinishWithError(FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Pick Photo requires a valid world context object.")
		));
		return;
	}

	RegisterWithGameInstance(StoredWorldContextObject.Get());
	StoredWorldContextObject.Reset();
	TargetWorld = World;
	WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
		this, &UOpenMobilePickPhotoAsyncAction::HandleWorldCleanup);

	FOpenMobileError Error;
	const bool bStarted = FOpenMobileMediaPlatform::BeginPick(
		FOnOpenMobileNativePhotoPicked::CreateUObject(this, &UOpenMobilePickPhotoAsyncAction::HandleNativePicked),
		FOnOpenMobileNativePhotoPickCancelled::CreateUObject(this, &UOpenMobilePickPhotoAsyncAction::HandleNativeCancelled),
		FOnOpenMobileNativePhotoPickError::CreateUObject(this, &UOpenMobilePickPhotoAsyncAction::HandleNativeError),
		RequestId,
		Error
	);

	if (!bStarted)
	{
		FinishWithError(Error.IsSet()
			? MoveTemp(Error)
			: FOpenMobileError::Make(
				EOpenMobileErrorCode::NativeFailure,
				TEXT("The native photo picker could not be opened.")
			));
	}
}

void UOpenMobilePickPhotoAsyncAction::HandleNativePicked(const FString& CachedImagePath, const FString& MetadataJson)
{
	if (bFinished)
	{
		IFileManager::Get().Delete(*CachedImagePath, false, true, true);
		return;
	}
	FOpenMobileMediaMetadata Metadata;
	FString ParseError;
	if (!OpenMobileMediaAsyncPrivate::ParseMetadata(MetadataJson, Metadata, ParseError))
	{
		IFileManager::Get().Delete(*CachedImagePath, false, true, true);
		FinishWithError(FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			MoveTemp(ParseError)
		));
		return;
	}

	const TWeakObjectPtr<UOpenMobilePickPhotoAsyncAction> WeakThis(this);
	Async(EAsyncExecution::ThreadPool, [WeakThis, CachedImagePath, Metadata = MoveTemp(Metadata)]() mutable
	{
		TArray<uint8> EncodedImage;
		const bool bLoaded = FFileHelper::LoadFileToArray(EncodedImage, *CachedImagePath, FILEREAD_Silent);
		IFileManager::Get().Delete(*CachedImagePath, false, true, true);

		AsyncTask(ENamedThreads::GameThread, [WeakThis, bLoaded, EncodedImage = MoveTemp(EncodedImage), Metadata = MoveTemp(Metadata)]() mutable
		{
			if (!WeakThis.IsValid() || WeakThis->bFinished)
			{
				return;
			}

			if (!bLoaded || EncodedImage.IsEmpty())
			{
				WeakThis->FinishWithError(FOpenMobileError::Make(
					EOpenMobileErrorCode::NativeFailure,
					TEXT("The selected photo could not be read from the app cache.")
				));
				return;
			}

			UTexture2D* Texture = FImageUtils::ImportBufferAsTexture2D(EncodedImage);
			if (!Texture)
			{
				WeakThis->FinishWithError(FOpenMobileError::Make(
					EOpenMobileErrorCode::NativeFailure,
					TEXT("Unreal could not decode the selected photo.")
				));
				return;
			}

			Texture->SRGB = true;
			Texture->NeverStream = true;

			FOpenMobileMediaPickResult Result;
			Result.Texture = Texture;
			Result.Metadata = MoveTemp(Metadata);
			if (WeakThis->Finish())
			{
				WeakThis->OnPicked.Broadcast(Result);
			}
		});
	});
}

void UOpenMobilePickPhotoAsyncAction::HandleNativeCancelled()
{
	Cancel();
}

void UOpenMobilePickPhotoAsyncAction::HandleNativeError(const FString& ErrorMessage)
{
	FinishWithError(FOpenMobileError::Make(
		EOpenMobileErrorCode::NativeFailure,
		ErrorMessage
	));
}

void UOpenMobilePickPhotoAsyncAction::FinishWithError(FOpenMobileError Error)
{
	if (Finish())
	{
		OnFailed.Broadcast(Error);
	}
}

bool UOpenMobilePickPhotoAsyncAction::Finish()
{
	if (bFinished)
	{
		return false;
	}
	bFinished = true;
	FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
	WorldCleanupHandle.Reset();
	StoredWorldContextObject.Reset();
	TargetWorld.Reset();
	FOpenMobileMediaPlatform::CancelPick(RequestId);
	RequestId = 0;
	SetReadyToDestroy();
	return true;
}

void UOpenMobilePickPhotoAsyncAction::Cancel()
{
	if (Finish())
	{
		OnCancelled.Broadcast();
	}
}

void UOpenMobilePickPhotoAsyncAction::HandleWorldCleanup(UWorld* World, bool, bool)
{
	if (World == TargetWorld.Get())
	{
		Cancel();
	}
}
