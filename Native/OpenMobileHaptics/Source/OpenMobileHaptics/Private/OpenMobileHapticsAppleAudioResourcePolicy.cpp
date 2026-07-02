#include "OpenMobileHapticsAppleAudioResourcePolicy.h"

#include "Misc/Paths.h"

namespace OpenMobileHapticsAppleAudioResourcePolicyPrivate
{
	bool IsSupportedFormat(const FString& Path)
	{
		const FString Extension = FPaths::GetExtension(Path).ToLower();
		return Extension == TEXT("caf")
			|| Extension == TEXT("wav")
			|| Extension == TEXT("aif")
			|| Extension == TEXT("aiff");
	}

	bool NormalizePathStructure(
		const FString& Path,
		FString& OutNormalizedPath,
		const FOpenMobileHapticsAppleAudioResourceLimits& Limits
	)
	{
		OutNormalizedPath.Reset();
		if (Path.IsEmpty()
			|| Path.Len() > Limits.MaximumRelativePathCharacters
			|| !FPaths::IsRelative(Path)
			|| Path.Contains(TEXT("\\"))
			|| Path.Contains(TEXT(":")))
		{
			return false;
		}
		TArray<FString> Segments;
		Path.ParseIntoArray(Segments, TEXT("/"), false);
		if (Segments.IsEmpty())
		{
			return false;
		}
		for (const FString& Segment : Segments)
		{
			if (Segment.IsEmpty()
				|| Segment == TEXT(".")
				|| Segment == TEXT(".."))
			{
				return false;
			}
		}
		OutNormalizedPath = FString::Join(Segments, TEXT("/"));
		return true;
	}

	FOpenMobileHapticsAppleAudioResourceValidation Fail(
		EOpenMobileHapticsAppleAudioResourceError Error,
		FString RelativePath,
		int64 TotalBytes = 0
	)
	{
		FOpenMobileHapticsAppleAudioResourceValidation Result;
		Result.Error = Error;
		Result.RelativePath = MoveTemp(RelativePath);
		Result.TotalBytes = TotalBytes;
		return Result;
	}

	bool HasBytes(
		const TArray<uint8>& Data,
		int32 Offset,
		std::initializer_list<uint8> Bytes
	)
	{
		if (Offset < 0 || Data.Num() - Offset < static_cast<int32>(Bytes.size()))
		{
			return false;
		}
		int32 Index = Offset;
		for (const uint8 Byte : Bytes)
		{
			if (Data[Index++] != Byte)
			{
				return false;
			}
		}
		return true;
	}

	bool IsValidContainer(const FString& Path, const TArray<uint8>& Data)
	{
		const FString Extension = FPaths::GetExtension(Path).ToLower();
		if (Extension == TEXT("caf"))
		{
			return Data.Num() >= 8
				&& HasBytes(Data, 0, {'c', 'a', 'f', 'f'})
				&& Data[4] == 0
				&& Data[5] == 1;
		}
		if (Extension == TEXT("wav"))
		{
			return Data.Num() >= 44
				&& HasBytes(Data, 0, {'R', 'I', 'F', 'F'})
				&& HasBytes(Data, 8, {'W', 'A', 'V', 'E'})
				&& HasBytes(Data, 12, {'f', 'm', 't', ' '});
		}
		if (Extension == TEXT("aif") || Extension == TEXT("aiff"))
		{
			return Data.Num() >= 12
				&& HasBytes(Data, 0, {'F', 'O', 'R', 'M'})
				&& (HasBytes(Data, 8, {'A', 'I', 'F', 'F'})
					|| HasBytes(Data, 8, {'A', 'I', 'F', 'C'}));
		}
		return false;
	}
}

bool FOpenMobileHapticsAppleAudioResourcePolicy::NormalizeRelativePath(
	const FString& Path,
	FString& OutNormalizedPath,
	const FOpenMobileHapticsAppleAudioResourceLimits& Limits
)
{
	using namespace OpenMobileHapticsAppleAudioResourcePolicyPrivate;
	return NormalizePathStructure(Path, OutNormalizedPath, Limits)
		&& IsSupportedFormat(OutNormalizedPath);
}

FOpenMobileHapticsAppleAudioResourceValidation
FOpenMobileHapticsAppleAudioResourcePolicy::Validate(
	const TArray<FString>& ExpectedRelativePaths,
	const TArray<FOpenMobileHapticIOSAudioResource>& Resources,
	const FOpenMobileHapticsAppleAudioResourceLimits& Limits
)
{
	using namespace OpenMobileHapticsAppleAudioResourcePolicyPrivate;
	if (Limits.MaximumResourceCount < 0
		|| Limits.MaximumResourceBytes < 1
		|| Limits.MaximumTotalBytes < Limits.MaximumResourceBytes
		|| Limits.MaximumRelativePathCharacters < 1)
	{
		return Fail(
			EOpenMobileHapticsAppleAudioResourceError::ResourceCountExceeded,
			TEXT("Limits")
		);
	}
	if (ExpectedRelativePaths.Num() > Limits.MaximumResourceCount
		|| Resources.Num() > Limits.MaximumResourceCount)
	{
		return Fail(
			EOpenMobileHapticsAppleAudioResourceError::ResourceCountExceeded,
			FString()
		);
	}

	TSet<FString> Expected;
	for (const FString& Path : ExpectedRelativePaths)
	{
		FString Normalized;
		if (!NormalizePathStructure(Path, Normalized, Limits))
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::UnsafePath,
				Path
			);
		}
		if (!IsSupportedFormat(Normalized))
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::UnsupportedFormat,
				Path
			);
		}
		Expected.Add(Normalized);
	}

	TSet<FString> Found;
	int64 TotalBytes = 0;
	for (const FOpenMobileHapticIOSAudioResource& Resource : Resources)
	{
		FString Normalized;
		if (!NormalizePathStructure(
				Resource.RelativePath,
				Normalized,
				Limits
			)
			|| Normalized != Resource.RelativePath)
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::UnsafePath,
				Resource.RelativePath,
				TotalBytes
			);
		}
		if (!IsSupportedFormat(Normalized))
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::UnsupportedFormat,
				Normalized,
				TotalBytes
			);
		}
		if (Found.Contains(Normalized))
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::DuplicateResource,
				Normalized,
				TotalBytes
			);
		}
		if (!Expected.Contains(Normalized))
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::UnexpectedResource,
				Normalized,
				TotalBytes
			);
		}
		if (Resource.Data.Num() > Limits.MaximumResourceBytes)
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::ResourceTooLarge,
				Normalized,
				TotalBytes
			);
		}
		if (!IsValidContainer(Normalized, Resource.Data))
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::InvalidContainer,
				Normalized,
				TotalBytes
			);
		}
		TotalBytes += Resource.Data.Num();
		if (TotalBytes > Limits.MaximumTotalBytes)
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::TotalSizeExceeded,
				Normalized,
				TotalBytes
			);
		}
		Found.Add(Normalized);
	}
	for (const FString& Path : Expected)
	{
		if (!Found.Contains(Path))
		{
			return Fail(
				EOpenMobileHapticsAppleAudioResourceError::MissingResource,
				Path,
				TotalBytes
			);
		}
	}

	FOpenMobileHapticsAppleAudioResourceValidation Result;
	Result.bSuccess = true;
	Result.TotalBytes = TotalBytes;
	return Result;
}

FString FOpenMobileHapticsAppleAudioResourcePolicy::DescribeError(
	const FOpenMobileHapticsAppleAudioResourceValidation& Result
)
{
	const TCHAR* Description = TEXT("AHAP audio resource validation failed");
	switch (Result.Error)
	{
	case EOpenMobileHapticsAppleAudioResourceError::UnsafePath:
		Description = TEXT("AHAP audio resource path is unsafe");
		break;
	case EOpenMobileHapticsAppleAudioResourceError::UnsupportedFormat:
		Description = TEXT("AHAP audio resource format is unsupported");
		break;
	case EOpenMobileHapticsAppleAudioResourceError::InvalidContainer:
		Description = TEXT("AHAP audio resource container is invalid");
		break;
	case EOpenMobileHapticsAppleAudioResourceError::MissingResource:
		Description = TEXT("AHAP audio resource is missing");
		break;
	case EOpenMobileHapticsAppleAudioResourceError::UnexpectedResource:
		Description = TEXT("AHAP audio resource is not referenced");
		break;
	case EOpenMobileHapticsAppleAudioResourceError::DuplicateResource:
		Description = TEXT("AHAP audio resource is duplicated");
		break;
	case EOpenMobileHapticsAppleAudioResourceError::ResourceTooLarge:
		Description = TEXT("AHAP audio resource exceeds its size limit");
		break;
	case EOpenMobileHapticsAppleAudioResourceError::TotalSizeExceeded:
		Description = TEXT("AHAP audio resources exceed the total size limit");
		break;
	case EOpenMobileHapticsAppleAudioResourceError::ResourceCountExceeded:
		Description = TEXT("AHAP audio resources exceed the count limit");
		break;
	default:
		break;
	}
	return Result.RelativePath.IsEmpty()
		? FString(Description)
		: FString::Printf(
			TEXT("%s: %s"),
			*Result.RelativePath,
			Description
		);
}
