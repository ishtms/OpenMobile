#include "OpenMobileAdsDiagnostics.h"

#include "HAL/IConsoleManager.h"
#include "Internationalization/Regex.h"
#include "OpenMobileCoreLog.h"

namespace OpenMobileAdsLogPrivate
{
	TAutoConsoleVariable<int32> CVarAdsLogLevel(
		TEXT("OpenMobile.Ads.LogLevel"),
		-1,
		TEXT("Ads log level override: -1 inherit, 0 error, 1 warning, 2 info, 3 verbose, 4 very verbose."),
		ECVF_Default
	);

	FString ReplaceSensitiveFields(const FString& Message)
	{
		static const FRegexPattern SensitiveFieldPattern(TEXT(
			"(?i)(ad[ _-]?unit[ _-]?id|device[ _-]?id|consent(?:[ _-]?data)?|custom[ _-]?data|advertising[ _-]?id|idfa|gaid)(\\s*[:=]\\s*)(\"[^\"]*\"|'[^']*'|[^\\s,;&]+)"
		));
		FRegexMatcher Matcher(SensitiveFieldPattern, Message);
		FString Result;
		int32 SourceOffset = 0;
		while (Matcher.FindNext())
		{
			const int32 ValueStart = Matcher.GetCaptureGroupBeginning(3);
			const int32 ValueEnd = Matcher.GetCaptureGroupEnding(3);
			Result.Append(Message.Mid(SourceOffset, ValueStart - SourceOffset));
			const FString Value = Matcher.GetCaptureGroup(3);
			if (
				Value.Len() >= 2
				&& (Value[0] == TEXT('\"') || Value[0] == TEXT('\''))
				&& Value[Value.Len() - 1] == Value[0]
			)
			{
				Result.AppendChar(Value[0]);
				Result.Append(TEXT("[REDACTED]"));
				Result.AppendChar(Value[0]);
			}
			else
			{
				Result.Append(TEXT("[REDACTED]"));
			}
			SourceOffset = ValueEnd;
		}
		Result.Append(Message.Mid(SourceOffset));
		return Result;
	}
}

bool FOpenMobileAdsLog::ShouldLog(EOpenMobileAdsLogLevel Level)
{
	return IsLevelEnabled(
		Level,
		FOpenMobileLogFilter::GetGlobalLevel(),
		GetAdsLevel()
	);
}

bool FOpenMobileAdsLog::IsLevelEnabled(
	EOpenMobileAdsLogLevel Level,
	int32 GlobalLevel,
	int32 AdsLevel
)
{
	const int32 EffectiveLevel = AdsLevel >= 0 ? AdsLevel : GlobalLevel;
	return EffectiveLevel >= 0
		&& static_cast<int32>(Level) <= FMath::Clamp(EffectiveLevel, -1, 4);
}

int32 FOpenMobileAdsLog::GetAdsLevel()
{
	return FMath::Clamp(
		OpenMobileAdsLogPrivate::CVarAdsLogLevel.GetValueOnAnyThread(),
		-1,
		4
	);
}

FString FOpenMobileAdsLog::Redact(
	const FString& Message,
	const TArray<FString>& SensitiveValues
)
{
	FString Result = OpenMobileAdsLogPrivate::ReplaceSensitiveFields(Message);
	for (const FString& SensitiveValue : SensitiveValues)
	{
		if (!SensitiveValue.IsEmpty())
		{
			Result.ReplaceInline(
				*SensitiveValue,
				TEXT("[REDACTED]"),
				ESearchCase::IgnoreCase
			);
		}
	}
	return Result;
}

FOpenMobileAdsNativeDiagnostics FOpenMobileAdsLog::Redact(
	const FOpenMobileAdsNativeDiagnostics& Diagnostics,
	const TArray<FString>& SensitiveValues
)
{
	FOpenMobileAdsNativeDiagnostics Result = Diagnostics;
	Result.NativeMessage = Redact(Result.NativeMessage, SensitiveValues);
	return Result;
}

void FOpenMobileAdsLog::Write(
	EOpenMobileAdsLogLevel Level,
	const FString& Message,
	FName Placement,
	FName Provider,
	const TArray<FString>& SensitiveValues
)
{
	if (!ShouldLog(Level))
	{
		return;
	}
	const FString SafeMessage = Redact(Message, SensitiveValues);
	const FString Context = FString::Printf(
		TEXT("[Ads][provider=%s][placement=%s] %s"),
		*Provider.ToString(),
		*Placement.ToString(),
		*SafeMessage
	);
	switch (Level)
	{
	case EOpenMobileAdsLogLevel::Error:
		UE_LOG(LogOpenMobile, Error, TEXT("%s"), *Context);
		break;
	case EOpenMobileAdsLogLevel::Warning:
		UE_LOG(LogOpenMobile, Warning, TEXT("%s"), *Context);
		break;
	case EOpenMobileAdsLogLevel::Info:
		UE_LOG(LogOpenMobile, Log, TEXT("%s"), *Context);
		break;
	case EOpenMobileAdsLogLevel::Verbose:
		UE_LOG(LogOpenMobile, Verbose, TEXT("%s"), *Context);
		break;
	case EOpenMobileAdsLogLevel::VeryVerbose:
		UE_LOG(LogOpenMobile, VeryVerbose, TEXT("%s"), *Context);
		break;
	}
}
