#include "OpenMobileMediaBlueprintLibrary.h"

#include "Internationalization/Text.h"
#include "OpenMobileMediaPlatform.h"

namespace OpenMobileMediaFormatting
{
	FString FormatBytes(const int64 Bytes)
	{
		if (Bytes <= 0)
		{
			return TEXT("Unknown");
		}

		constexpr double Kilobyte = 1024.0;
		constexpr double Megabyte = Kilobyte * 1024.0;
		if (Bytes >= static_cast<int64>(Megabyte))
		{
			return FString::Printf(TEXT("%.2f MB"), static_cast<double>(Bytes) / Megabyte);
		}

		if (Bytes >= static_cast<int64>(Kilobyte))
		{
			return FString::Printf(TEXT("%.1f KB"), static_cast<double>(Bytes) / Kilobyte);
		}

		return FString::Printf(TEXT("%lld bytes"), static_cast<long long>(Bytes));
	}
}

bool UOpenMobileMediaBlueprintLibrary::IsPhotoPickerSupported()
{
	return FOpenMobileMediaPlatform::IsAvailable();
}

FText UOpenMobileMediaBlueprintLibrary::FormatPhotoMetadata(const FOpenMobileMediaMetadata& Metadata)
{
	TArray<FString> Lines;
	Lines.Reserve(12);

	Lines.Add(FString::Printf(TEXT("Name: %s"), Metadata.FileName.IsEmpty() ? TEXT("Unknown") : *Metadata.FileName));
	Lines.Add(FString::Printf(TEXT("Type: %s"), Metadata.MimeType.IsEmpty() ? TEXT("Unknown") : *Metadata.MimeType));

	const FString Dimensions = Metadata.Width > 0 && Metadata.Height > 0
		? FString::Printf(TEXT("%d x %d px"), Metadata.Width, Metadata.Height)
		: TEXT("Unknown");
	Lines.Add(FString::Printf(TEXT("Original size: %s (%s)"), *Dimensions, *OpenMobileMediaFormatting::FormatBytes(Metadata.FileSizeBytes)));

	if (!Metadata.DateTaken.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("Captured: %s"), *Metadata.DateTaken));
	}

	FString Camera = Metadata.CameraMake;
	if (!Metadata.CameraModel.IsEmpty())
	{
		Camera = Camera.IsEmpty() ? Metadata.CameraModel : Camera + TEXT(" ") + Metadata.CameraModel;
	}
	if (!Camera.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("Camera: %s"), *Camera));
	}

	if (!Metadata.LensModel.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("Lens: %s"), *Metadata.LensModel));
	}

	TArray<FString> ExposureParts;
	if (Metadata.Aperture > 0.0f)
	{
		ExposureParts.Add(FString::Printf(TEXT("f/%.1f"), Metadata.Aperture));
	}
	if (Metadata.ExposureTimeSeconds > 0.0f)
	{
		if (Metadata.ExposureTimeSeconds < 1.0f)
		{
			const int32 Denominator = FMath::Max(1, FMath::RoundToInt(1.0f / Metadata.ExposureTimeSeconds));
			ExposureParts.Add(FString::Printf(TEXT("1/%d s"), Denominator));
		}
		else
		{
			ExposureParts.Add(FString::Printf(TEXT("%.2f s"), Metadata.ExposureTimeSeconds));
		}
	}
	if (Metadata.Iso > 0)
	{
		ExposureParts.Add(FString::Printf(TEXT("ISO %d"), Metadata.Iso));
	}
	if (Metadata.FocalLengthMm > 0.0f)
	{
		ExposureParts.Add(FString::Printf(TEXT("%.1f mm"), Metadata.FocalLengthMm));
	}
	if (!ExposureParts.IsEmpty())
	{
		Lines.Add(FString::Printf(TEXT("Exposure: %s"), *FString::Join(ExposureParts, TEXT(" | "))));
	}

	if (Metadata.bHasLocation)
	{
		Lines.Add(FString::Printf(TEXT("Location: %.6f, %.6f"), Metadata.Latitude, Metadata.Longitude));
	}

	if (Metadata.ExifOrientation > 0)
	{
		Lines.Add(FString::Printf(TEXT("EXIF orientation: %d"), Metadata.ExifOrientation));
	}

	return FText::FromString(FString::Join(Lines, TEXT("\n")));
}
