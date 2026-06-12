#include "OpenMobileDeviceClipboardPolicy.h"

#include "Containers/StringConv.h"

namespace OpenMobileDeviceClipboardPolicyPrivate
{
	bool IsSupportedValueType(EOpenMobileClipboardContentType ContentType)
	{
		return ContentType == EOpenMobileClipboardContentType::Text
			|| ContentType == EOpenMobileClipboardContentType::Url;
	}
}

bool FOpenMobileDeviceClipboardPolicy::ValidateWrite(
	const FOpenMobileClipboardWriteRequest& Request,
	FOpenMobileError& OutError
)
{
	OutError = {};
	if (!OpenMobileDeviceClipboardPolicyPrivate::IsSupportedValueType(
		Request.ContentType
	))
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Clipboard writes support only Text and Url content types.")
		);
		return false;
	}
	if (GetPayloadSizeBytes(Request.Value) > MaximumPayloadBytes)
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Clipboard payload exceeds the 256 KiB UTF-8 limit.")
		);
		return false;
	}
	for (int32 Index = 0; Index < Request.Value.Len(); ++Index)
	{
		if (Request.Value[Index] == TEXT('\0'))
		{
			OutError = FOpenMobileError::Make(
				EOpenMobileErrorCode::InvalidArgument,
				TEXT("Clipboard payload cannot contain null characters.")
			);
			return false;
		}
	}
	if (Request.ContentType == EOpenMobileClipboardContentType::Url
		&& !IsAbsoluteUrl(Request.Value))
	{
		OutError = FOpenMobileError::Make(
			EOpenMobileErrorCode::InvalidArgument,
			TEXT("Clipboard URL must be a well-formed absolute URL.")
		);
		return false;
	}
	return true;
}

bool FOpenMobileDeviceClipboardPolicy::ValidateReadType(
	EOpenMobileClipboardContentType ContentType,
	FOpenMobileError& OutError
)
{
	OutError = {};
	if (OpenMobileDeviceClipboardPolicyPrivate::IsSupportedValueType(ContentType))
	{
		return true;
	}
	OutError = FOpenMobileError::Make(
		EOpenMobileErrorCode::InvalidArgument,
		TEXT("Clipboard reads support only Text and Url content types.")
	);
	return false;
}

int32 FOpenMobileDeviceClipboardPolicy::GetPayloadSizeBytes(
	const FString& Value
)
{
	const FTCHARToUTF8 Converted(*Value, Value.Len());
	return Converted.Length();
}

bool FOpenMobileDeviceClipboardPolicy::IsAbsoluteUrl(const FString& Value)
{
	if (Value.IsEmpty())
	{
		return false;
	}
	const int32 ColonIndex = Value.Find(TEXT(":"));
	if (ColonIndex <= 0 || ColonIndex == Value.Len() - 1
		|| !FChar::IsAlpha(Value[0]))
	{
		return false;
	}
	for (int32 Index = 0; Index < Value.Len(); ++Index)
	{
		const TCHAR Character = Value[Index];
		if (FChar::IsWhitespace(Character) || FChar::IsControl(Character))
		{
			return false;
		}
		if (Index > 0 && Index < ColonIndex
			&& !FChar::IsAlnum(Character)
			&& Character != TEXT('+')
			&& Character != TEXT('-')
			&& Character != TEXT('.'))
		{
			return false;
		}
	}
	if (Value.StartsWith(TEXT("http://"), ESearchCase::IgnoreCase)
		|| Value.StartsWith(TEXT("https://"), ESearchCase::IgnoreCase))
	{
		const int32 AuthorityStart = Value.Find(TEXT("://")) + 3;
		int32 AuthorityEnd = Value.Len();
		for (int32 Index = AuthorityStart; Index < Value.Len(); ++Index)
		{
			if (Value[Index] == TEXT('/') || Value[Index] == TEXT('?')
				|| Value[Index] == TEXT('#'))
			{
				AuthorityEnd = Index;
				break;
			}
		}
		return AuthorityStart >= 3 && AuthorityEnd > AuthorityStart;
	}
	return true;
}
