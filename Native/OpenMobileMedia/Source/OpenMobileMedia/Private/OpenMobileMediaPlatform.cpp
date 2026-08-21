#include "OpenMobileMediaPlatform.h"

#include "Features/IModularFeatures.h"
#include "IOpenMobileMediaBackend.h"
#include "OpenMobileAsync.h"
#include "HAL/FileManager.h"

namespace OpenMobileMediaPlatformPrivate
{
	FOnOpenMobileNativePhotoPicked PickedDelegate;
	FOnOpenMobileNativePhotoPickCancelled CancelledDelegate;
	FOnOpenMobileNativePhotoPickError ErrorDelegate;
	int64 ActiveRequestId = 0;
	int64 NextRequestId = 0;
	IOpenMobileMediaBackend* ActiveBackend = nullptr;

	void ResetDelegates()
	{
		PickedDelegate.Unbind();
		CancelledDelegate.Unbind();
		ErrorDelegate.Unbind();
		ActiveRequestId = 0;
		ActiveBackend = nullptr;
	}

	IOpenMobileMediaBackend* FindBackend()
	{
		TArray<IOpenMobileMediaBackend*> Backends =
			IModularFeatures::Get().GetModularFeatureImplementations<IOpenMobileMediaBackend>(
				IOpenMobileMediaBackend::GetModularFeatureName()
			);

		IOpenMobileMediaBackend* Best = nullptr;
		for (IOpenMobileMediaBackend* Candidate : Backends)
		{
			if (!Candidate || !Candidate->IsAvailable())
			{
				continue;
			}

			const bool bHigherPriority = !Best || Candidate->GetPriority() > Best->GetPriority();
			const bool bStableTieBreak = Best
				&& Candidate->GetPriority() == Best->GetPriority()
				&& Candidate->GetBackendName().LexicalLess(Best->GetBackendName());
			if (bHigherPriority || bStableTieBreak)
			{
				Best = Candidate;
			}
		}

		return Best;
	}
}

bool FOpenMobileMediaPlatform::IsAvailable()
{
	return OpenMobileMediaPlatformPrivate::FindBackend() != nullptr;
}

bool FOpenMobileMediaPlatform::BeginPick(
	FOnOpenMobileNativePhotoPicked&& OnPicked,
	FOnOpenMobileNativePhotoPickCancelled&& OnCancelled,
	FOnOpenMobileNativePhotoPickError&& OnError,
	int64& OutRequestId,
	FOpenMobileError& OutError
)
{
	check(IsInGameThread());

	using namespace OpenMobileMediaPlatformPrivate;
	OutRequestId = 0;
	if (ActiveRequestId != 0)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::Busy,
			TEXT("A photo picker is already open.")
		);
		return false;
	}

	IOpenMobileMediaBackend* Backend = FindBackend();
	if (!Backend)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NotSupported,
			TEXT("No OpenMobile media backend is available for this platform.")
		);
		return false;
	}

	PickedDelegate = MoveTemp(OnPicked);
	CancelledDelegate = MoveTemp(OnCancelled);
	ErrorDelegate = MoveTemp(OnError);
	ActiveRequestId = ++NextRequestId;
	OutRequestId = ActiveRequestId;
	ActiveBackend = Backend;

	FString NativeError;
	if (!Backend->LaunchPhotoPicker(OutRequestId, NativeError))
	{
		if (ActiveRequestId == OutRequestId)
		{
			ResetDelegates();
		}
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::NativeFailure,
			NativeError.IsEmpty() ? TEXT("The native photo picker could not be opened.") : MoveTemp(NativeError),
			FString(),
			Backend->GetBackendName().ToString()
		);
		return false;
	}

	return true;
}

void FOpenMobileMediaPlatform::NativePicked(int64 RequestId, FString CachedImagePath, FString MetadataJson)
{
	OpenMobile::DispatchToGameThread(
		[RequestId, CachedImagePath = MoveTemp(CachedImagePath), MetadataJson = MoveTemp(MetadataJson)]() mutable
		{
			using namespace OpenMobileMediaPlatformPrivate;
			if (RequestId == 0 || ActiveRequestId != RequestId)
			{
				IFileManager::Get().Delete(*CachedImagePath, false, true, true);
				return;
			}

			FOnOpenMobileNativePhotoPicked Completion = MoveTemp(PickedDelegate);
			ResetDelegates();
			if (Completion.IsBound())
			{
				Completion.Execute(CachedImagePath, MetadataJson);
			}
			else
			{
				IFileManager::Get().Delete(*CachedImagePath, false, true, true);
			}
		}
	);
}

void FOpenMobileMediaPlatform::NativeCancelled(int64 RequestId)
{
	OpenMobile::DispatchToGameThread([RequestId]
	{
		using namespace OpenMobileMediaPlatformPrivate;
		if (RequestId == 0 || ActiveRequestId != RequestId)
		{
			return;
		}

		FOnOpenMobileNativePhotoPickCancelled Completion = MoveTemp(CancelledDelegate);
		ResetDelegates();
		Completion.ExecuteIfBound();
	});
}

void FOpenMobileMediaPlatform::NativeError(int64 RequestId, FString ErrorMessage)
{
	OpenMobile::DispatchToGameThread([RequestId, ErrorMessage = MoveTemp(ErrorMessage)]() mutable
	{
		using namespace OpenMobileMediaPlatformPrivate;
		if (RequestId == 0 || ActiveRequestId != RequestId)
		{
			return;
		}

		FOnOpenMobileNativePhotoPickError Completion = MoveTemp(ErrorDelegate);
		ResetDelegates();
		Completion.ExecuteIfBound(ErrorMessage);
	});
}

void FOpenMobileMediaPlatform::CancelPick(int64 RequestId)
{
	check(IsInGameThread());
	using namespace OpenMobileMediaPlatformPrivate;
	if (RequestId == 0 || ActiveRequestId != RequestId)
	{
		return;
	}
	IOpenMobileMediaBackend* Backend = ActiveBackend;
	ResetDelegates();
	const auto Backends = IModularFeatures::Get().GetModularFeatureImplementations<IOpenMobileMediaBackend>(
		IOpenMobileMediaBackend::GetModularFeatureName());
	if (Backends.Contains(Backend))
	{
		Backend->CancelPhotoPicker(RequestId);
	}
}
