#include "OpenMobileAdsConfiguration.h"

#include "OpenMobileAdsCapabilities.h"
#include "OpenMobileAdsOperations.h"

namespace OpenMobileAdsConfigurationPrivate
{
	void AddIssue(
		TArray<FOpenMobileAdsConfigurationIssue>& Issues,
		EOpenMobileAdsConfigurationIssueCode Code,
		FName Placement,
		FString Message,
		FName ConflictingPlacement = NAME_None,
		EOpenMobileAdsConfigurationIssueSeverity Severity =
			EOpenMobileAdsConfigurationIssueSeverity::Error
	)
	{
		FOpenMobileAdsConfigurationIssue& Issue = Issues.Emplace_GetRef();
		Issue.Severity = Severity;
		Issue.Code = Code;
		Issue.Placement = Placement;
		Issue.ConflictingPlacement = ConflictingPlacement;
		Issue.Message = MoveTemp(Message);
	}

	void ValidateProviderOptions(
		const TMap<FName, FString>& Options,
		FName Placement,
		TArray<FOpenMobileAdsConfigurationIssue>& Issues
	)
	{
		if (Options.Contains(NAME_None))
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::EmptyProviderOption,
				Placement,
				TEXT("Provider option keys must not be empty.")
			);
		}
	}

	void ValidateResolvedPolicy(
		const FOpenMobileAdsResolvedPlacement& Placement,
		TArray<FOpenMobileAdsConfigurationIssue>& Issues
	)
	{
		if (Placement.RefreshIntervalSeconds < 0.0)
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::InvalidRefreshInterval,
				Placement.Placement,
				TEXT("Refresh interval must not be negative.")
			);
		}
		else if (
			Placement.RefreshIntervalSeconds > 0.0
			&& Placement.Format != EOpenMobileAdFormat::Banner
		)
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::RefreshNotSupported,
				Placement.Placement,
				TEXT("Automatic refresh is only valid for banner placements.")
			);
		}

		const FOpenMobileAdsFrequencyCap& Cap = Placement.FrequencyCap;
		const bool bInvalidCap = Cap.MaxImpressions < 0
			|| Cap.WindowSeconds < 0.0
			|| ((Cap.MaxImpressions == 0) != (Cap.WindowSeconds == 0.0));
		if (bInvalidCap)
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::InvalidFrequencyCap,
				Placement.Placement,
				TEXT("Frequency cap count and window must both be positive or both be zero.")
			);
		}

		if (Placement.CooldownSeconds < 0.0)
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::InvalidCooldown,
				Placement.Placement,
				TEXT("Cooldown must not be negative.")
			);
		}

		if (Placement.FallbackRewardAmount < 0)
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::InvalidFallbackRewardAmount,
				Placement.Placement,
				TEXT("Fallback reward amount must not be negative.")
			);
		}
	}
}

bool FOpenMobileAdsRetryPolicy::IsValid() const
{
	return MaxRetryAttempts >= 0
		&& MaxRetryAttempts <= 10
		&& FMath::IsFinite(InitialDelaySeconds)
		&& InitialDelaySeconds >= 0.0
		&& FMath::IsFinite(BackoffMultiplier)
		&& BackoffMultiplier >= 1.0
		&& FMath::IsFinite(MaxDelaySeconds)
		&& MaxDelaySeconds >= InitialDelaySeconds;
}

EOpenMobileAdsPlatform OpenMobileAdsGetCurrentPlatform()
{
#if PLATFORM_ANDROID
	return EOpenMobileAdsPlatform::Android;
#elif PLATFORM_IOS
	return EOpenMobileAdsPlatform::IOS;
#else
	return EOpenMobileAdsPlatform::Unsupported;
#endif
}

FOpenMobileAdsResolvedPlacement FOpenMobileAdsPlacementSettings::Resolve(
	EOpenMobileAdsPlatform Platform
) const
{
	FOpenMobileAdsResolvedPlacement Result;
	Result.Placement = Placement;
	Result.Format = Format;
	Result.bEnabled = bEnabled;
	Result.bPreload = bPreload;
	Result.RefreshIntervalSeconds = RefreshIntervalSeconds;
	Result.FrequencyCap = FrequencyCap;
	Result.CooldownSeconds = CooldownSeconds;
	Result.FallbackRewardType = FallbackRewardType;
	Result.FallbackRewardAmount = FallbackRewardAmount;
	Result.ProviderOptions = ProviderOptions;

	const FOpenMobileAdsPlatformPlacementOverride* Override = nullptr;
	if (Platform == EOpenMobileAdsPlatform::Android)
	{
		Override = &Android;
	}
	else if (Platform == EOpenMobileAdsPlatform::IOS)
	{
		Override = &IOS;
	}

	if (!Override)
	{
		return Result;
	}

	Result.AdUnitId = Override->AdUnitId.TrimStartAndEnd();
	Result.bEnabled = Override->bOverrideEnabled ? Override->bEnabled : Result.bEnabled;
	Result.bPreload = Override->bOverridePreload ? Override->bPreload : Result.bPreload;
	Result.RefreshIntervalSeconds = Override->bOverrideRefreshInterval
		? Override->RefreshIntervalSeconds
		: Result.RefreshIntervalSeconds;
	Result.FrequencyCap = Override->bOverrideFrequencyCap
		? Override->FrequencyCap
		: Result.FrequencyCap;
	Result.CooldownSeconds = Override->bOverrideCooldown
		? Override->CooldownSeconds
		: Result.CooldownSeconds;
	Result.ProviderOptions.Append(Override->ProviderOptions);
	return Result;
}

TArray<FOpenMobileAdsConfigurationIssue> FOpenMobileAdsConfigurationValidator::Validate(
	const TArray<FOpenMobileAdsPlacementSettings>& Placements
)
{
	using namespace OpenMobileAdsConfigurationPrivate;

	TArray<FOpenMobileAdsConfigurationIssue> Issues;
	TMap<FName, FString> PlacementDisplayNames;
	TMap<FString, FName> AndroidIds;
	TMap<FString, FName> IOSIds;
	Issues.Reserve(Placements.Num());

	for (const FOpenMobileAdsPlacementSettings& Placement : Placements)
	{
		if (Placement.Placement.IsNone())
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::EmptyPlacement,
				NAME_None,
				TEXT("Placement name must not be empty.")
			);
		}
		else if (const FString* ExistingDisplayName = PlacementDisplayNames.Find(Placement.Placement))
		{
			const FString DisplayName = Placement.Placement.ToString();
			AddIssue(
				Issues,
				DisplayName.Equals(*ExistingDisplayName, ESearchCase::CaseSensitive)
					? EOpenMobileAdsConfigurationIssueCode::DuplicatePlacement
					: EOpenMobileAdsConfigurationIssueCode::CaseConflict,
				Placement.Placement,
				TEXT("Placement names must be unique without case-only differences."),
				Placement.Placement
			);
		}
		else
		{
			PlacementDisplayNames.Add(Placement.Placement, Placement.Placement.ToString());
		}

		const FOpenMobileAdsResolvedPlacement Android =
			Placement.Resolve(EOpenMobileAdsPlatform::Android);
		const FOpenMobileAdsResolvedPlacement IOS =
			Placement.Resolve(EOpenMobileAdsPlatform::IOS);
		ValidateResolvedPolicy(Android, Issues);
		ValidateResolvedPolicy(IOS, Issues);
		ValidateProviderOptions(Placement.ProviderOptions, Placement.Placement, Issues);
		ValidateProviderOptions(Placement.Android.ProviderOptions, Placement.Placement, Issues);
		ValidateProviderOptions(Placement.IOS.ProviderOptions, Placement.Placement, Issues);

		auto ValidateId = [&Issues, &Placement](
			const FOpenMobileAdsResolvedPlacement& Resolved,
			TMap<FString, FName>& SeenIds,
			EOpenMobileAdsConfigurationIssueCode MissingCode,
			EOpenMobileAdsConfigurationIssueCode DuplicateCode,
			const TCHAR* PlatformName
		)
		{
			if (!Resolved.bEnabled)
			{
				return;
			}
			if (Resolved.AdUnitId.IsEmpty())
			{
				AddIssue(
					Issues,
					MissingCode,
					Placement.Placement,
					FString::Printf(TEXT("%s ad-unit ID is required."), PlatformName)
				);
				return;
			}

			if (const FName* ExistingPlacement = SeenIds.Find(Resolved.AdUnitId))
			{
				AddIssue(
					Issues,
					DuplicateCode,
					Placement.Placement,
					FString::Printf(TEXT("%s ad-unit IDs must be unique."), PlatformName),
					*ExistingPlacement
				);
			}
			else
			{
				SeenIds.Add(Resolved.AdUnitId, Placement.Placement);
			}
		};

		ValidateId(
			Android,
			AndroidIds,
			EOpenMobileAdsConfigurationIssueCode::MissingAndroidAdUnitId,
			EOpenMobileAdsConfigurationIssueCode::DuplicateAndroidAdUnitId,
			TEXT("Android")
		);
		ValidateId(
			IOS,
			IOSIds,
			EOpenMobileAdsConfigurationIssueCode::MissingIOSAdUnitId,
			EOpenMobileAdsConfigurationIssueCode::DuplicateIOSAdUnitId,
			TEXT("iOS")
		);
	}

	return Issues;
}

TArray<FOpenMobileAdsConfigurationIssue>
FOpenMobileAdsConfigurationValidator::ValidateProviderCapabilities(
	const TArray<FOpenMobileAdsPlacementSettings>& Placements,
	const FOpenMobileAdsProviderCapabilities& Capabilities
)
{
	using namespace OpenMobileAdsConfigurationPrivate;
	TArray<FOpenMobileAdsConfigurationIssue> Issues;
	for (const FOpenMobileAdsPlacementSettings& Placement : Placements)
	{
		const bool bEnabledOnAnyMobilePlatform =
			Placement.Resolve(EOpenMobileAdsPlatform::Android).bEnabled
			|| Placement.Resolve(EOpenMobileAdsPlatform::IOS).bEnabled;
		if (!bEnabledOnAnyMobilePlatform)
		{
			continue;
		}

		const FOpenMobileAdFormatCapabilities* FormatCapabilities =
			Capabilities.FindFormat(Placement.Format);
		if (!FormatCapabilities)
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::UnsupportedProviderFormat,
				Placement.Placement,
				FString::Printf(
					TEXT("Provider '%s' does not support this placement format."),
					*Capabilities.Provider.ToString()
				)
			);
			continue;
		}

		auto ValidateOperation = [
			&Issues,
			&Placement,
			&Capabilities
		](bool bSupported, const TCHAR* Operation)
		{
			if (!bSupported)
			{
				AddIssue(
					Issues,
					EOpenMobileAdsConfigurationIssueCode::UnsupportedProviderOperation,
					Placement.Placement,
					FString::Printf(
						TEXT("Provider '%s' does not support %s for this placement format."),
						*Capabilities.Provider.ToString(),
						Operation
					)
				);
			}
		};
		ValidateOperation(FormatCapabilities->bCanLoad, TEXT("load"));
		ValidateOperation(FormatCapabilities->bCanShow, TEXT("show"));
		ValidateOperation(FormatCapabilities->bCanDestroy, TEXT("destroy"));
	}
	return Issues;
}

TArray<FOpenMobileAdsConfigurationIssue>
FOpenMobileAdsConfigurationValidator::ValidateSettings(
	const UOpenMobileAdsSettings& Settings,
	bool bForShipping
)
{
	using namespace OpenMobileAdsConfigurationPrivate;
	TArray<FOpenMobileAdsConfigurationIssue> Issues = Validate(Settings.Placements);
	if (!Settings.ConvenienceRewardedPlacement.IsNone())
	{
		const FOpenMobileAdsPlacementSettings* Placement = Settings.FindPlacement(
			Settings.ConvenienceRewardedPlacement
		);
		const bool bEnabledOnAnyMobilePlatform = Placement
			&& (
				Placement->Resolve(EOpenMobileAdsPlatform::Android).bEnabled
				|| Placement->Resolve(EOpenMobileAdsPlatform::IOS).bEnabled
			);
		if (
			!Placement
			|| Placement->Format != EOpenMobileAdFormat::Rewarded
			|| !bEnabledOnAnyMobilePlatform
		)
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::InvalidConvenienceRewardedPlacement,
				Settings.ConvenienceRewardedPlacement,
				TEXT("Convenience Rewarded Placement must reference an enabled rewarded placement.")
			);
		}
	}
	if (!Settings.RetryPolicy.IsValid())
	{
		AddIssue(
			Issues,
			EOpenMobileAdsConfigurationIssueCode::InvalidRetryPolicy,
			NAME_None,
			TEXT("Retry attempts must be between 0 and 10, delays must be finite and non-negative, maximum delay must not be shorter than the initial delay, and backoff must be at least 1.")
		);
	}
	if (Settings.bDevelopmentTestMode)
	{
		AddIssue(
			Issues,
			EOpenMobileAdsConfigurationIssueCode::UnsafeShippingTestMode,
			NAME_None,
			bForShipping
				? TEXT("Development/Test Mode is not allowed in shipping builds.")
				: TEXT("Development/Test Mode must be disabled before making a shipping build."),
			NAME_None,
			bForShipping
				? EOpenMobileAdsConfigurationIssueSeverity::Error
				: EOpenMobileAdsConfigurationIssueSeverity::Warning
		);
	}
	TSet<FString> SeenTestDeviceIdentifiers;
	for (int32 Index = 0; Index < Settings.TestDeviceIdentifiers.Num(); ++Index)
	{
		const FString& Identifier = Settings.TestDeviceIdentifiers[Index];
		const FString IdentifierKey = Identifier.ToLower();
		if (
			!FOpenMobileAdsDevelopmentConfiguration::IsValidTestDeviceIdentifier(Identifier)
			|| SeenTestDeviceIdentifiers.Contains(IdentifierKey)
		)
		{
			AddIssue(
				Issues,
				EOpenMobileAdsConfigurationIssueCode::InvalidTestDeviceIdentifier,
				NAME_None,
				FString::Printf(
					TEXT("Global test-device identifier at index %d is invalid or duplicated."),
					Index
				)
			);
		}
		SeenTestDeviceIdentifiers.Add(IdentifierKey);
	}
	if (bForShipping && !Settings.TestDeviceIdentifiers.IsEmpty())
	{
		AddIssue(
			Issues,
			EOpenMobileAdsConfigurationIssueCode::UnsafeShippingTestDeviceIdentifier,
			NAME_None,
			TEXT("Global test-device identifiers are not allowed in shipping builds.")
		);
	}
	return Issues;
}

const FOpenMobileAdsPlacementSettings* UOpenMobileAdsSettings::FindPlacement(
	FName Placement
) const
{
	return Placements.FindByPredicate(
		[Placement](const FOpenMobileAdsPlacementSettings& Candidate)
		{
			return Candidate.Placement == Placement;
		}
	);
}
