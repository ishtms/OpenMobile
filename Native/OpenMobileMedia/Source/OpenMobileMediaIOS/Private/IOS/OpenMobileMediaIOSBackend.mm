#include "OpenMobileMediaIOSBackend.h"

#include "OpenMobileMediaPlatform.h"

#if PLATFORM_IOS

#import <ImageIO/ImageIO.h>
#import <PhotosUI/PhotosUI.h>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

#import "IOS/IOSAppDelegate.h"

namespace OpenMobileIOS
{
	constexpr CGFloat MaxTextureDimension = 4096.0;

	UIViewController* TopViewController(UIViewController* Controller)
	{
		if (!Controller)
		{
			return nil;
		}

		if (Controller.presentedViewController && !Controller.presentedViewController.isBeingDismissed)
		{
			return TopViewController(Controller.presentedViewController);
		}

		if ([Controller isKindOfClass:[UINavigationController class]])
		{
			return TopViewController(((UINavigationController*)Controller).visibleViewController);
		}

		if ([Controller isKindOfClass:[UITabBarController class]])
		{
			return TopViewController(((UITabBarController*)Controller).selectedViewController);
		}

		return Controller;
	}

	NSString* StringValue(id Value)
	{
		if ([Value isKindOfClass:[NSString class]])
		{
			return (NSString*)Value;
		}
		if ([Value respondsToSelector:@selector(stringValue)])
		{
			return [Value stringValue];
		}
		return @"";
	}

	NSNumber* NumberValue(id Value)
	{
		if ([Value isKindOfClass:[NSNumber class]])
		{
			return (NSNumber*)Value;
		}
		if ([Value isKindOfClass:[NSArray class]] && [(NSArray*)Value count] > 0)
		{
			return NumberValue([(NSArray*)Value firstObject]);
		}
		return nil;
	}

	FString ToFString(NSString* String)
	{
		return String ? FString(UTF8_TO_TCHAR(String.UTF8String)) : FString();
	}

	void Fail(int64 RequestId, NSString* Message)
	{
		FOpenMobileMediaPlatform::NativeError(RequestId, ToFString(Message ?: @"The iOS photo picker failed."));
	}
}

@interface OpenMobilePhotoPickerDelegate : NSObject <PHPickerViewControllerDelegate>
@property(nonatomic, assign) int64 requestId;
@property(nonatomic, weak) PHPickerViewController* picker;
@end

static OpenMobilePhotoPickerDelegate* GOpenMobilePhotoPickerDelegate = nil;

@implementation OpenMobilePhotoPickerDelegate

- (void)picker:(PHPickerViewController*)picker didFinishPicking:(NSArray<PHPickerResult*>*)results
{
	const int64 RequestId = self.requestId;
	if (GOpenMobilePhotoPickerDelegate == self)
	{
		GOpenMobilePhotoPickerDelegate = nil;
	}
	[picker dismissViewControllerAnimated:YES completion:nil];

	if (results.count == 0)
	{
		FOpenMobileMediaPlatform::NativeCancelled(RequestId);
		return;
	}

	NSItemProvider* provider = results.firstObject.itemProvider;
	NSString* selectedTypeIdentifier = nil;
	for (NSString* identifier in provider.registeredTypeIdentifiers)
	{
		UTType* type = [UTType typeWithIdentifier:identifier];
		if (type && [type conformsToType:UTTypeImage])
		{
			selectedTypeIdentifier = identifier;
			break;
		}
	}

	if (!selectedTypeIdentifier)
	{
		OpenMobileIOS::Fail(RequestId, @"The selected item is not a supported image.");
		return;
	}

	NSString* typeIdentifier = [selectedTypeIdentifier copy];
	[provider loadDataRepresentationForTypeIdentifier:typeIdentifier completionHandler:^(NSData* data, NSError* loadError)
	{
		@autoreleasepool
		{
			if (!data || loadError)
			{
				NSString* detail = loadError.localizedDescription ?: @"No image data was returned.";
				OpenMobileIOS::Fail(RequestId, [@"The selected iOS photo could not be loaded: " stringByAppendingString:detail]);
				return;
			}

			CGImageSourceRef imageSource = CGImageSourceCreateWithData((__bridge CFDataRef)data, nullptr);
			if (!imageSource)
			{
				OpenMobileIOS::Fail(RequestId, @"iOS could not inspect the selected image format.");
				return;
			}

			NSDictionary* properties = CFBridgingRelease(CGImageSourceCopyPropertiesAtIndex(imageSource, 0, nullptr));

			NSDictionary* exif = properties[(__bridge NSString*)kCGImagePropertyExifDictionary] ?: @{};
			NSDictionary* tiff = properties[(__bridge NSString*)kCGImagePropertyTIFFDictionary] ?: @{};
			NSDictionary* gps = properties[(__bridge NSString*)kCGImagePropertyGPSDictionary] ?: @{};

			NSNumber* width = OpenMobileIOS::NumberValue(properties[(__bridge NSString*)kCGImagePropertyPixelWidth]) ?: @0;
			NSNumber* height = OpenMobileIOS::NumberValue(properties[(__bridge NSString*)kCGImagePropertyPixelHeight]) ?: @0;
			NSNumber* orientation = OpenMobileIOS::NumberValue(properties[(__bridge NSString*)kCGImagePropertyOrientation]) ?: @0;

			NSString* dateTaken = OpenMobileIOS::StringValue(exif[(__bridge NSString*)kCGImagePropertyExifDateTimeOriginal]);
			if (dateTaken.length == 0)
			{
				dateTaken = OpenMobileIOS::StringValue(tiff[(__bridge NSString*)kCGImagePropertyTIFFDateTime]);
			}

			NSString* cameraMake = OpenMobileIOS::StringValue(tiff[(__bridge NSString*)kCGImagePropertyTIFFMake]);
			NSString* cameraModel = OpenMobileIOS::StringValue(tiff[(__bridge NSString*)kCGImagePropertyTIFFModel]);
			NSString* lensModel = OpenMobileIOS::StringValue(exif[(__bridge NSString*)kCGImagePropertyExifLensModel]);
			NSNumber* aperture = OpenMobileIOS::NumberValue(exif[(__bridge NSString*)kCGImagePropertyExifFNumber]) ?: @0;
			NSNumber* exposureTime = OpenMobileIOS::NumberValue(exif[(__bridge NSString*)kCGImagePropertyExifExposureTime]) ?: @0;
			NSNumber* iso = OpenMobileIOS::NumberValue(exif[(__bridge NSString*)kCGImagePropertyExifISOSpeedRatings]) ?: @0;
			NSNumber* focalLength = OpenMobileIOS::NumberValue(exif[(__bridge NSString*)kCGImagePropertyExifFocalLength]) ?: @0;

			NSNumber* latitudeValue = OpenMobileIOS::NumberValue(gps[(__bridge NSString*)kCGImagePropertyGPSLatitude]);
			NSNumber* longitudeValue = OpenMobileIOS::NumberValue(gps[(__bridge NSString*)kCGImagePropertyGPSLongitude]);
			NSString* latitudeRef = OpenMobileIOS::StringValue(gps[(__bridge NSString*)kCGImagePropertyGPSLatitudeRef]);
			NSString* longitudeRef = OpenMobileIOS::StringValue(gps[(__bridge NSString*)kCGImagePropertyGPSLongitudeRef]);
			BOOL hasLocation = latitudeValue != nil && longitudeValue != nil;
			double latitude = latitudeValue.doubleValue;
			double longitude = longitudeValue.doubleValue;
			if ([latitudeRef caseInsensitiveCompare:@"S"] == NSOrderedSame)
			{
				latitude = -latitude;
			}
			if ([longitudeRef caseInsensitiveCompare:@"W"] == NSOrderedSame)
			{
				longitude = -longitude;
			}

			UTType* selectedType = [UTType typeWithIdentifier:typeIdentifier];
			NSString* mimeType = selectedType.preferredMIMEType ?: @"image/*";
			NSString* fileName = provider.suggestedName ?: @"Selected photo";
			if (fileName.pathExtension.length == 0 && selectedType.preferredFilenameExtension.length > 0)
			{
				fileName = [fileName stringByAppendingPathExtension:selectedType.preferredFilenameExtension];
			}

			NSDictionary* thumbnailOptions = @{
				(__bridge NSString*)kCGImageSourceCreateThumbnailFromImageAlways: @YES,
				(__bridge NSString*)kCGImageSourceCreateThumbnailWithTransform: @YES,
				(__bridge NSString*)kCGImageSourceThumbnailMaxPixelSize: @(OpenMobileIOS::MaxTextureDimension)
			};
			CGImageRef thumbnail = CGImageSourceCreateThumbnailAtIndex(
				imageSource,
				0,
				(__bridge CFDictionaryRef)thumbnailOptions
			);
			CFRelease(imageSource);

			if (!thumbnail)
			{
				OpenMobileIOS::Fail(RequestId, @"iOS could not decode the selected image format.");
				return;
			}

			UIImage* sourceImage = [[UIImage alloc] initWithCGImage:thumbnail scale:1.0 orientation:UIImageOrientationUp];
			CGImageRelease(thumbnail);
			const CGSize targetSize = CGSizeMake(
				MAX(1.0, floor(sourceImage.size.width)),
				MAX(1.0, floor(sourceImage.size.height))
			);

			UIGraphicsImageRendererFormat* rendererFormat = [UIGraphicsImageRendererFormat defaultFormat];
			rendererFormat.opaque = YES;
			rendererFormat.scale = 1.0;
			UIGraphicsImageRenderer* renderer = [[UIGraphicsImageRenderer alloc] initWithSize:targetSize format:rendererFormat];
			UIImage* normalizedImage = [renderer imageWithActions:^(UIGraphicsImageRendererContext* context)
			{
				[[UIColor blackColor] setFill];
				CGContextFillRect(context.CGContext, CGRectMake(0.0, 0.0, targetSize.width, targetSize.height));
				[sourceImage drawInRect:CGRectMake(0.0, 0.0, targetSize.width, targetSize.height)];
			}];

			NSData* jpegData = UIImageJPEGRepresentation(normalizedImage, 0.92);
			if (!jpegData)
			{
				OpenMobileIOS::Fail(RequestId, @"iOS could not prepare the selected photo for Unreal.");
				return;
			}

			NSString* cacheDirectory = [NSTemporaryDirectory() stringByAppendingPathComponent:@"open_mobile_photo_picker"];
			NSError* directoryError = nil;
			if (![[NSFileManager defaultManager] createDirectoryAtPath:cacheDirectory
											withIntermediateDirectories:YES
															 attributes:nil
																  error:&directoryError])
			{
				OpenMobileIOS::Fail(RequestId, [@"The iOS photo cache could not be created: "
					stringByAppendingString:(directoryError.localizedDescription ?: @"Unknown error")]);
				return;
			}

			NSString* outputName = [NSString stringWithFormat:@"photo_%@.jpg", NSUUID.UUID.UUIDString];
			NSString* outputPath = [cacheDirectory stringByAppendingPathComponent:outputName];
			NSError* writeError = nil;
			if (![jpegData writeToFile:outputPath options:NSDataWritingAtomic error:&writeError])
			{
				OpenMobileIOS::Fail(RequestId, [@"The selected photo could not be cached: "
					stringByAppendingString:(writeError.localizedDescription ?: @"Unknown error")]);
				return;
			}

			NSMutableDictionary* metadata = [@{
				@"fileName": fileName,
				@"mimeType": mimeType,
				@"fileSizeBytes": @(data.length),
				@"width": width,
				@"height": height,
				@"dateTaken": dateTaken,
				@"exifOrientation": orientation,
				@"cameraMake": cameraMake,
				@"cameraModel": cameraModel,
				@"lensModel": lensModel,
				@"aperture": aperture,
				@"exposureTimeSeconds": exposureTime,
				@"iso": iso,
				@"focalLengthMm": focalLength,
				@"hasLocation": @(hasLocation)
			} mutableCopy];
			if (hasLocation)
			{
				metadata[@"latitude"] = @(latitude);
				metadata[@"longitude"] = @(longitude);
			}

			NSError* jsonError = nil;
			NSData* jsonData = [NSJSONSerialization dataWithJSONObject:metadata options:0 error:&jsonError];
			NSString* json = jsonData ? [[NSString alloc] initWithData:jsonData encoding:NSUTF8StringEncoding] : nil;
			if (!json)
			{
				[[NSFileManager defaultManager] removeItemAtPath:outputPath error:nil];
				OpenMobileIOS::Fail(RequestId, [@"The photo metadata could not be encoded: "
					stringByAppendingString:(jsonError.localizedDescription ?: @"Unknown error")]);
				return;
			}

			FOpenMobileMediaPlatform::NativePicked(
				RequestId,
				OpenMobileIOS::ToFString(outputPath),
				OpenMobileIOS::ToFString(json)
			);
		}
	}];
}

@end

bool FOpenMobileMediaIOSBackend::LaunchPhotoPicker(int64 RequestId, FString& OutError)
{
	if (@available(iOS 14.0, *))
	{
		dispatch_async(dispatch_get_main_queue(), ^
		{
			UIViewController* presenter = OpenMobileIOS::TopViewController(
				(UIViewController*)[IOSAppDelegate GetDelegate].IOSController
			);
			if (!presenter)
			{
				OpenMobileIOS::Fail(RequestId, @"Unreal's iOS view controller is unavailable.");
				return;
			}

			PHPickerConfiguration* configuration = [[PHPickerConfiguration alloc] init];
			configuration.filter = PHPickerFilter.imagesFilter;
			configuration.selectionLimit = 1;
			configuration.preferredAssetRepresentationMode = PHPickerConfigurationAssetRepresentationModeCurrent;

			PHPickerViewController* picker = [[PHPickerViewController alloc] initWithConfiguration:configuration];
			GOpenMobilePhotoPickerDelegate = [[OpenMobilePhotoPickerDelegate alloc] init];
			GOpenMobilePhotoPickerDelegate.requestId = RequestId;
			GOpenMobilePhotoPickerDelegate.picker = picker;
			picker.delegate = GOpenMobilePhotoPickerDelegate;
			[presenter presentViewController:picker animated:YES completion:nil];
		});
		return true;
	}

	OutError = TEXT("The native iOS photo picker requires iOS 14 or newer.");
	return false;
}

void FOpenMobileMediaIOSBackend::CancelPhotoPicker(int64 RequestId)
{
	dispatch_async(dispatch_get_main_queue(), ^
	{
		OpenMobilePhotoPickerDelegate* Handler = GOpenMobilePhotoPickerDelegate;
		if (Handler && Handler.requestId == RequestId)
		{
			Handler.picker.delegate = nil;
			[Handler.picker dismissViewControllerAnimated:NO completion:nil];
			GOpenMobilePhotoPickerDelegate = nil;
		}
	});
}

#endif
