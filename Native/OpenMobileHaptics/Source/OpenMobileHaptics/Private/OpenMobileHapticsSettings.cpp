#include "OpenMobileHapticsSettings.h"

#include "Misc/PackageName.h"

namespace OpenMobileHapticsSettingsPrivate
{
	bool IsNormalized(float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.0f && Value <= 1.0f;
	}

	bool IsFiniteRange(float Value, float Minimum, float Maximum)
	{
		return FMath::IsFinite(Value) && Value >= Minimum && Value <= Maximum;
	}
}

UOpenMobileHapticsSettings::UOpenMobileHapticsSettings()
{
	FOpenMobileHapticNamedLibrarySettings StarterLibrary;
	StarterLibrary.Name = TEXT("OpenMobileStarter");
	StarterLibrary.Asset = FSoftObjectPath(TEXT(
		"/OpenMobileHaptics/StarterPresets/OpenMobileStarterHaptics."
		"OpenMobileStarterHaptics"
	));
	NamedLibraries.Add(MoveTemp(StarterLibrary));

	auto AddChannel = [this](
		FName Name,
		EOpenMobileHapticChannelPriority Priority,
		int32 Capacity,
		float MinimumIntervalSeconds
	)
	{
		FOpenMobileHapticChannelSettings Channel;
		Channel.Name = Name;
		Channel.Priority = Priority;
		Channel.MaximumActiveHandles = Capacity;
		Channel.MaximumQueueDepth = Capacity;
		Channel.MinimumIntervalSeconds = MinimumIntervalSeconds;
		Channels.Add(MoveTemp(Channel));
	};

	AddChannel(TEXT("UI"), EOpenMobileHapticChannelPriority::Normal, 4, 0.04f);
	AddChannel(
		TEXT("Gameplay"),
		EOpenMobileHapticChannelPriority::Normal,
		8,
		0.02f
	);
	AddChannel(TEXT("Alerts"), EOpenMobileHapticChannelPriority::High, 4, 0.1f);
	AddChannel(
		TEXT("Cinematic"),
		EOpenMobileHapticChannelPriority::Normal,
		4,
		0.02f
	);
	AddChannel(
		TEXT("Critical"),
		EOpenMobileHapticChannelPriority::Critical,
		2,
		0.25f
	);
}

bool UOpenMobileHapticsSettings::Validate(TArray<FString>& OutErrors) const
{
	using namespace OpenMobileHapticsSettingsPrivate;
	OutErrors.Reset();
	auto AddError = [&OutErrors](const TCHAR* Message)
	{
		OutErrors.Emplace(Message);
	};

	if (!IsNormalized(DefaultMasterIntensity))
	{
		AddError(TEXT("Default master intensity must be finite and between 0 and 1."));
	}
	if (DefaultChannel.IsNone())
	{
		AddError(TEXT("Default channel cannot be empty."));
	}
	if (DefaultCategory.IsNone())
	{
		AddError(TEXT("Default category cannot be empty."));
	}

	TSet<FName> ChannelNames;
	for (const FOpenMobileHapticChannelSettings& Channel : Channels)
	{
		if (Channel.Name.IsNone())
		{
			AddError(TEXT("Channel names cannot be empty."));
		}
		else if (ChannelNames.Contains(Channel.Name))
		{
			AddError(TEXT("Channel names must be unique without case conflicts."));
		}
		else
		{
			ChannelNames.Add(Channel.Name);
		}
		if (static_cast<uint8>(Channel.Priority)
			> static_cast<uint8>(EOpenMobileHapticChannelPriority::Critical))
		{
			AddError(TEXT("Channel priorities must use a known priority value."));
		}
		if (Channel.MaximumActiveHandles < 0
			|| Channel.MaximumActiveHandles > MaximumActiveHandles)
		{
			AddError(TEXT("Channel active capacity exceeds the project handle limit."));
		}
		if (Channel.MaximumQueueDepth < 0
			|| Channel.MaximumQueueDepth > MaximumQueueDepthPerChannel)
		{
			AddError(TEXT("Channel queue depth exceeds the project channel limit."));
		}
		if (static_cast<uint8>(Channel.UnsupportedMixFallbackPolicy)
			> static_cast<uint8>(
				EOpenMobileHapticOverlapPolicy::MixWhenSupported)
			|| Channel.UnsupportedMixFallbackPolicy
				== EOpenMobileHapticOverlapPolicy::MixWhenSupported)
		{
			AddError(TEXT("Channel mix fallback must use a non-mixing overlap policy."));
		}
		if (!IsFiniteRange(Channel.MinimumIntervalSeconds, 0.0f, 1.0f))
		{
			AddError(TEXT("Channel minimum intervals must be finite and between 0 and 1 second."));
		}
		if (!IsNormalized(Channel.IntensityScale))
		{
			AddError(TEXT("Channel intensity scales must be finite and between 0 and 1."));
		}
	}
	if (!ChannelNames.Contains(DefaultChannel))
	{
		AddError(TEXT("Default channel must reference a configured channel."));
	}

	TSet<FName> EffectNames;
	for (const FOpenMobileHapticEffectSettings& Effect : EffectOverrides)
	{
		if (Effect.Name.IsNone())
		{
			AddError(TEXT("Effect override names cannot be empty."));
		}
		else if (EffectNames.Contains(Effect.Name))
		{
			AddError(TEXT("Effect override names must be unique without case conflicts."));
		}
		else
		{
			EffectNames.Add(Effect.Name);
		}
		if (!IsNormalized(Effect.IntensityScale))
		{
			AddError(TEXT("Effect intensity scales must be finite and between 0 and 1."));
		}
		if (!IsFiniteRange(Effect.MinimumIntervalSeconds, 0.0f, 10.0f))
		{
			AddError(TEXT("Effect minimum intervals must be finite and between 0 and 10 seconds."));
		}
	}

	TSet<FName> LibraryNames;
	for (const FOpenMobileHapticNamedLibrarySettings& Library : NamedLibraries)
	{
		if (Library.Name.IsNone())
		{
			AddError(TEXT("Named library names cannot be empty."));
		}
		else if (LibraryNames.Contains(Library.Name))
		{
			AddError(TEXT("Named library names must be unique without case conflicts."));
		}
		else
		{
			LibraryNames.Add(Library.Name);
		}

		if (Library.Asset.IsNull())
		{
			AddError(TEXT("Named libraries require an asset."));
		}
		else
		{
			const FString PackageName = Library.Asset.GetLongPackageName();
			if (PackageName.IsEmpty()
				|| !FPackageName::DoesPackageExist(PackageName))
			{
				AddError(TEXT("A named library asset does not exist."));
			}
		}
	}

	if (MaximumActiveHandles < 1 || MaximumActiveHandles > 128)
	{
		AddError(TEXT("Maximum active handles must be between 1 and 128."));
	}
	if (MaximumQueuedHandles < 1 || MaximumQueuedHandles > 256)
	{
		AddError(TEXT("Maximum queued handles must be between 1 and 256."));
	}
	if (MaximumQueueDepthPerChannel < 1
		|| MaximumQueueDepthPerChannel > 64
		|| MaximumQueueDepthPerChannel > MaximumQueuedHandles)
	{
		AddError(TEXT("Per-channel queue depth must fit within the global queue limit."));
	}
	if (!IsFiniteRange(MaximumQueuedRequestAgeSeconds, 0.01f, 30.0f))
	{
		AddError(TEXT("Maximum queued request age must be finite and between 0.01 and 30 seconds."));
	}
	if (MaximumPreparedPatterns < 1 || MaximumPreparedPatterns > 128)
	{
		AddError(TEXT("Maximum prepared patterns must be between 1 and 128."));
	}
	if (MaximumPreparedPatternMemoryKilobytes < 64
		|| MaximumPreparedPatternMemoryKilobytes > 65536)
	{
		AddError(TEXT("Prepared pattern memory must be between 64 and 65536 KB."));
	}
	if (!IsFiniteRange(PreparedPatternIdleLifetimeSeconds, 1.0f, 300.0f))
	{
		AddError(TEXT("Prepared pattern idle lifetime must be finite and between 1 and 300 seconds."));
	}
	if (MaximumDiagnosticEvents < 1 || MaximumDiagnosticEvents > 512)
	{
		AddError(TEXT("Maximum diagnostic events must be between 1 and 512."));
	}
	if (MaximumFiniteRepeatCount < 1 || MaximumFiniteRepeatCount > 1000)
	{
		AddError(TEXT("Maximum finite repeat count must be between 1 and 1000."));
	}
	if (!IsFiniteRange(MaximumContinuousDurationSeconds, 0.1f, 300.0f))
	{
		AddError(TEXT("Maximum continuous duration must be finite and between 0.1 and 300 seconds."));
	}
	if (!FMath::IsFinite(MaximumPatternEventDurationSeconds)
		|| MaximumPatternEventDurationSeconds < 0.001f
		|| MaximumPatternEventDurationSeconds > MaximumContinuousDurationSeconds)
	{
		AddError(TEXT("Maximum pattern event duration must be finite, positive, and no greater than the continuous limit."));
	}
	if (MaximumPatternEventCount < 1 || MaximumPatternEventCount > 4096)
	{
		AddError(TEXT("Maximum pattern event count must be between 1 and 4096."));
	}
	if (MaximumPatternCurveCount < 0 || MaximumPatternCurveCount > 128)
	{
		AddError(TEXT("Maximum pattern curve count must be between 0 and 128."));
	}
	if (MaximumPatternCurvePointCount < 1
		|| MaximumPatternCurvePointCount > 4096)
	{
		AddError(TEXT("Maximum pattern curve point count must be between 1 and 4096."));
	}
	if (!FMath::IsFinite(MinimumPatternGranularitySeconds)
		|| MinimumPatternGranularitySeconds < 0.0001f
		|| MinimumPatternGranularitySeconds > 1.0f
		|| MinimumPatternGranularitySeconds
			> MaximumPatternEventDurationSeconds)
	{
		AddError(TEXT("Minimum pattern granularity must be finite, between 0.0001 and 1 second, and no greater than the event duration limit."));
	}
	if (!IsFiniteRange(MinimumOneShotDurationSeconds, 0.001f, 1.0f))
	{
		AddError(TEXT("Minimum one-shot duration must be finite and between 0.001 and 1 second."));
	}
	if (!FMath::IsFinite(MaximumOneShotDurationSeconds)
		|| MaximumOneShotDurationSeconds < MinimumOneShotDurationSeconds
		|| MaximumOneShotDurationSeconds > MaximumContinuousDurationSeconds)
	{
		AddError(TEXT("Maximum one-shot duration must be finite, at least the minimum, and no greater than the continuous limit."));
	}
	if (!IsFiniteRange(DefaultMinimumIntervalSeconds, 0.0f, 1.0f))
	{
		AddError(TEXT("Default minimum interval must be finite and between 0 and 1 second."));
	}
	if (MaximumSubmissionsPerSecond < 1 || MaximumSubmissionsPerSecond > 100)
	{
		AddError(TEXT("Maximum submissions per second must be between 1 and 100."));
	}
	if (MaximumDynamicParameterUpdatesPerSecond < 1
		|| MaximumDynamicParameterUpdatesPerSecond > 240)
	{
		AddError(TEXT("Maximum dynamic parameter updates per second must be between 1 and 240."));
	}
	if (!IsFiniteRange(SelectionDebounceSeconds, 0.0f, 1.0f))
	{
		AddError(TEXT("Selection debounce must be finite and between 0 and 1 second."));
	}
	if (MaximumRecoveryAttempts < 0 || MaximumRecoveryAttempts > 8)
	{
		AddError(TEXT("Maximum recovery attempts must be between 0 and 8."));
	}

	if (BackgroundPolicy == EOpenMobileHapticBackgroundPolicy::AllowAll)
	{
		AddError(TEXT("Unrestricted background gameplay haptics are unsafe."));
	}
	if (!bEnableCustomPlayback && bEnableAndroidCustomVibration)
	{
		AddError(TEXT("Android custom vibration cannot be packaged when custom playback is disabled."));
	}
	if (!bEnableCustomPlayback && IOS.bEnableCoreHaptics)
	{
		AddError(TEXT("Core Haptics cannot be enabled when custom playback is disabled."));
	}
	if (!IOS.bEnableCoreHaptics && IOS.bPackageAHAPResources)
	{
		AddError(TEXT("AHAP resources cannot be packaged when Core Haptics is disabled."));
	}

	return OutErrors.IsEmpty();
}
