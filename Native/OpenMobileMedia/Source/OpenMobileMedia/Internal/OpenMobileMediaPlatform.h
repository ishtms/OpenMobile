#pragma once

#include "CoreMinimal.h"
#include "OpenMobileCoreTypes.h"

DECLARE_DELEGATE_TwoParams(
	FOnOpenMobileNativePhotoPicked,
	const FString& /* CachedImagePath */,
	const FString& /* MetadataJson */
);
DECLARE_DELEGATE(FOnOpenMobileNativePhotoPickCancelled);
DECLARE_DELEGATE_OneParam(FOnOpenMobileNativePhotoPickError, const FString& /* ErrorMessage */);

/** Shared request serialization and callback boundary used by platform modules. */
class OPENMOBILEMEDIA_API FOpenMobileMediaPlatform
{
public:
	static bool IsAvailable();

	static bool BeginPick(
		FOnOpenMobileNativePhotoPicked&& OnPicked,
		FOnOpenMobileNativePhotoPickCancelled&& OnCancelled,
		FOnOpenMobileNativePhotoPickError&& OnError,
		int64& OutRequestId,
		FOpenMobileError& OutError
	);

	static void CancelPick(int64 RequestId);

	/** Thread-safe native entry points; public completions always execute on the game thread. */
	static void NativePicked(int64 RequestId, FString CachedImagePath, FString MetadataJson);
	static void NativeCancelled(int64 RequestId);
	static void NativeError(int64 RequestId, FString ErrorMessage);
};
