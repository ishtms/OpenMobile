#include "OpenMobileHapticsCapabilityTester.h"

#include "Algo/Sort.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "OpenMobileHapticsSubsystem.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace OpenMobileHapticsCapabilityTesterPrivate
{
	constexpr int32 MaximumNamedSupportEntries = 64;
	constexpr int32 MaximumOutputBytes = 64 * 1024;

	template <typename EnumType>
	FString EnumName(EnumType Value)
	{
		const UEnum* Enum = StaticEnum<EnumType>();
		return Enum
			? Enum->GetNameStringByValue(static_cast<int64>(Value))
			: TEXT("Unknown");
	}

	FString SafeName(const FString& Value, bool& bTruncated)
	{
		FString Result;
		Result.Reserve(FMath::Min(Value.Len(), 64));
		for (const TCHAR Character : Value)
		{
			if (Result.Len() >= 64)
			{
				bTruncated = true;
				break;
			}
			if (FChar::IsAlnum(Character) || Character == TEXT(' ')
				|| Character == TEXT('_') || Character == TEXT('-')
				|| Character == TEXT('.'))
			{
				Result.AppendChar(Character);
			}
			else
			{
				bTruncated = true;
				Result.AppendChar(TEXT('_'));
			}
		}
		return Result;
	}

	bool IsKnownNativeSupportName(FName Name)
	{
		static const TSet<FName> Names = {
			TEXT("Click"),
			TEXT("Thud"),
			TEXT("Spin"),
			TEXT("QuickRise"),
			TEXT("SlowRise"),
			TEXT("QuickFall"),
			TEXT("Tick"),
			TEXT("LowTick"),
			TEXT("HeavyClick"),
			TEXT("DoubleClick")
		};
		return Names.Contains(Name);
	}

	void AddFeature(
		FOpenMobileHapticsCapabilityTesterSnapshot& Snapshot,
		const TCHAR* Name,
		EOpenMobileHapticSupportState State
	)
	{
		Snapshot.FeatureSupport.Add({Name, State});
	}

	void AddIntegerLimit(
		FOpenMobileHapticsCapabilityTesterSnapshot& Snapshot,
		const TCHAR* Name,
		const FOpenMobileHapticIntegerLimit& Limit
	)
	{
		Snapshot.Limits.Add({
			Name,
			Limit.bKnown,
			static_cast<double>(Limit.Value),
			TEXT("count")
		});
	}

	void AddDurationLimit(
		FOpenMobileHapticsCapabilityTesterSnapshot& Snapshot,
		const TCHAR* Name,
		const FOpenMobileHapticDurationLimit& Limit
	)
	{
		Snapshot.Limits.Add({
			Name,
			Limit.bKnown,
			Limit.Seconds,
			TEXT("seconds")
		});
	}

	FString NativeTier(const FOpenMobileHapticCapabilities& Capabilities)
	{
		const FString Backend = Capabilities.BackendName.ToString();
		const auto Supported = [](EOpenMobileHapticSupportState State)
		{
			return State == EOpenMobileHapticSupportState::Supported;
		};
		if (Backend == TEXT("Android"))
		{
			if (Supported(Capabilities.Envelopes))
			{
				return TEXT("AndroidEnvelope");
			}
			if (Supported(Capabilities.Primitives))
			{
				return TEXT("AndroidPrimitiveComposition");
			}
			if (Supported(Capabilities.WaveformTiming))
			{
				return TEXT("AndroidWaveform");
			}
			if (Supported(Capabilities.PredefinedEffects))
			{
				return TEXT("AndroidPredefined");
			}
			return Supported(Capabilities.BasicVibration)
				? TEXT("AndroidBasicVibration") : TEXT("Unavailable");
		}
		if (Backend == TEXT("IOS"))
		{
			if (Supported(Capabilities.AHAP))
			{
				return TEXT("CoreHapticsAHAP");
			}
			if (Supported(Capabilities.RichHaptics))
			{
				return TEXT("CoreHaptics");
			}
			return Supported(Capabilities.SemanticFeedback)
				? TEXT("UIKitSemantic") : TEXT("Unavailable");
		}
		return Backend.IsEmpty() ? TEXT("Unavailable") : TEXT("ProviderDefined");
	}

	FString EngineState(
		const FOpenMobileHapticCapabilities& Capabilities,
		const FOpenMobileHapticsDiagnostics& Diagnostics
	)
	{
		if (Capabilities.Availability
			== EOpenMobileHapticAvailability::TemporarilyUnavailable)
		{
			return TEXT("TemporarilyUnavailable");
		}
		if (Capabilities.Availability
			== EOpenMobileHapticAvailability::NoActuator
			|| Capabilities.Availability
				== EOpenMobileHapticAvailability::UnsupportedPlatform)
		{
			return TEXT("Unavailable");
		}
		if (Diagnostics.ActivePlaybackCount > 0)
		{
			return TEXT("Playing");
		}
		switch (Diagnostics.PreparationState)
		{
		case EOpenMobileHapticPreparationState::Preparing:
			return TEXT("Preparing");
		case EOpenMobileHapticPreparationState::Prepared:
			return TEXT("Prepared");
		case EOpenMobileHapticPreparationState::Failed:
			return TEXT("PreparationFailed");
		default:
			return TEXT("Idle");
		}
	}

	TSharedPtr<FJsonValue> SupportJson(
		const FOpenMobileHapticsCapabilityTesterSupport& Support
	)
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Support.Name);
		Item->SetStringField(TEXT("state"), EnumName(Support.State));
		return MakeShared<FJsonValueObject>(Item);
	}
}

FOpenMobileHapticsCapabilityTesterSnapshot
FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Capture(
	const UOpenMobileHapticsSubsystem& Haptics
)
{
	return Build(
		Haptics.GetHapticCapabilities(),
		Haptics.GetUserPolicy(),
		Haptics.GetDiagnostics()
	);
}

FOpenMobileHapticsCapabilityTesterSnapshot
FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Build(
	const FOpenMobileHapticCapabilities& Capabilities,
	const FOpenMobileHapticUserPolicy& Policy,
	const FOpenMobileHapticsDiagnostics& Diagnostics
)
{
	using namespace OpenMobileHapticsCapabilityTesterPrivate;
	FOpenMobileHapticsCapabilityTesterSnapshot Snapshot;
	Snapshot.Backend = Capabilities.BackendName == TEXT("Android")
		? TEXT("Android")
		: Capabilities.BackendName == TEXT("IOS")
			? TEXT("IOS") : TEXT("ProviderDefined");
	Snapshot.NativeApiTier = NativeTier(Capabilities);
	Snapshot.Availability = Capabilities.Availability;
	Snapshot.EngineState = EngineState(Capabilities, Diagnostics);
	Snapshot.PreparationState = Diagnostics.PreparationState;
	Snapshot.bPlayerEnabled = Policy.bEnabled;
	Snapshot.MasterIntensity = FMath::Clamp(
		FMath::IsFinite(Policy.MasterIntensity) ? Policy.MasterIntensity : 0.0f,
		0.0f,
		1.0f
	);
	Snapshot.ActivePlaybackCount = FMath::Max(
		0,
		Diagnostics.ActivePlaybackCount
	);
	Snapshot.QueuedPlaybackCount = FMath::Max(
		0,
		Diagnostics.QueuedPlaybackCount
	);
	Snapshot.PreparedNamedPatternCount = FMath::Max(
		0,
		Diagnostics.PreparedNamedPatternCount
	);
	Snapshot.CachedTimelineCount = FMath::Max(
		0,
		Diagnostics.Performance.TimelineCacheEntryCount
	);
	Snapshot.CachedTimelineBytes = FMath::Max<int64>(
		0,
		Diagnostics.Performance.TimelineCacheMemoryBytes
	);

	AddFeature(Snapshot, TEXT("BasicVibration"), Capabilities.BasicVibration);
	AddFeature(Snapshot, TEXT("SemanticFeedback"), Capabilities.SemanticFeedback);
	AddFeature(Snapshot, TEXT("SemanticEffects"), Capabilities.SemanticEffects);
	AddFeature(Snapshot, TEXT("RichHaptics"), Capabilities.RichHaptics);
	AddFeature(Snapshot, TEXT("AmplitudeControl"), Capabilities.AmplitudeControl);
	AddFeature(Snapshot, TEXT("PredefinedEffects"), Capabilities.PredefinedEffects);
	AddFeature(Snapshot, TEXT("WaveformTiming"), Capabilities.WaveformTiming);
	AddFeature(Snapshot, TEXT("Looping"), Capabilities.Looping);
	AddFeature(Snapshot, TEXT("Primitives"), Capabilities.Primitives);
	AddFeature(Snapshot, TEXT("Envelopes"), Capabilities.Envelopes);
	AddFeature(Snapshot, TEXT("FrequencyControl"), Capabilities.FrequencyControl);
	AddFeature(Snapshot, TEXT("TransientEvents"), Capabilities.TransientEvents);
	AddFeature(Snapshot, TEXT("ContinuousEvents"), Capabilities.ContinuousEvents);
	AddFeature(Snapshot, TEXT("DynamicParameters"), Capabilities.DynamicParameters);
	AddFeature(Snapshot, TEXT("AudioEvents"), Capabilities.AudioEvents);
	AddFeature(Snapshot, TEXT("AHAP"), Capabilities.AHAP);
	AddFeature(Snapshot, TEXT("Scheduling"), Capabilities.Scheduling);
	AddFeature(Snapshot, TEXT("Mixing"), Capabilities.Mixing);
	AddFeature(Snapshot, TEXT("BackgroundAlerts"), Capabilities.BackgroundAlerts);
	AddFeature(Snapshot, TEXT("Pause"), Capabilities.Pause);
	AddFeature(Snapshot, TEXT("Resume"), Capabilities.Resume);
	AddFeature(Snapshot, TEXT("Seek"), Capabilities.Seek);

	auto AddNamedSupport = [&Snapshot](
		const TArray<FOpenMobileHapticNamedSupport>& Source,
		TArray<FOpenMobileHapticsCapabilityTesterSupport>& Destination
	)
	{
		for (int32 Index = 0;
			Index < Source.Num() && Index < MaximumNamedSupportEntries;
			++Index)
		{
			if (!IsKnownNativeSupportName(Source[Index].Name))
			{
				Snapshot.bTruncated = true;
				continue;
			}
			Destination.Add({
				SafeName(Source[Index].Name.ToString(), Snapshot.bTruncated),
				Source[Index].Support
			});
		}
		Snapshot.bTruncated |= Source.Num() > MaximumNamedSupportEntries;
		Algo::SortBy(
			Destination,
			&FOpenMobileHapticsCapabilityTesterSupport::Name
		);
	};
	AddNamedSupport(Capabilities.PrimitiveSupport, Snapshot.PrimitiveSupport);
	AddNamedSupport(Capabilities.PresetSupport, Snapshot.PresetSupport);
	AddIntegerLimit(Snapshot, TEXT("MaximumEvents"),
		Capabilities.MaximumEventCount);
	AddIntegerLimit(Snapshot, TEXT("MaximumControlPoints"),
		Capabilities.MaximumControlPointCount);
	AddIntegerLimit(Snapshot, TEXT("MaximumQueueDepth"),
		Capabilities.MaximumQueueDepth);
	AddDurationLimit(Snapshot, TEXT("MaximumDuration"),
		Capabilities.MaximumDurationSeconds);
	AddDurationLimit(Snapshot, TEXT("MinimumTimingGranularity"),
		Capabilities.MinimumTimingGranularitySeconds);
	AddDurationLimit(Snapshot, TEXT("MaximumControlPointDuration"),
		Capabilities.MaximumControlPointDurationSeconds);
	if (Capabilities.FrequencyRange.bKnown)
	{
		Snapshot.Limits.Add({
			TEXT("MinimumFrequency"),
			true,
			Capabilities.FrequencyRange.MinimumHertz,
			TEXT("hertz")
		});
		Snapshot.Limits.Add({
			TEXT("MaximumFrequency"),
			true,
			Capabilities.FrequencyRange.MaximumHertz,
			TEXT("hertz")
		});
	}
	return Snapshot;
}

bool FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Serialize(
	const FOpenMobileHapticsCapabilityTesterSnapshot& Snapshot,
	FString& OutJson,
	FString& OutError
)
{
	using namespace OpenMobileHapticsCapabilityTesterPrivate;
	OutJson.Reset();
	OutError.Reset();
	if (Snapshot.SchemaVersion != 1 || Snapshot.Backend.Len() > 64
		|| Snapshot.NativeApiTier.Len() > 64
		|| Snapshot.EngineState.Len() > 64)
	{
		OutError = TEXT("Capability snapshot is invalid.");
		return false;
	}
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetNumberField(TEXT("schemaVersion"), Snapshot.SchemaVersion);
	Root->SetStringField(TEXT("plugin"), TEXT("OpenMobileHaptics"));
	Root->SetStringField(TEXT("backend"), Snapshot.Backend);
	Root->SetStringField(TEXT("nativeApiTier"), Snapshot.NativeApiTier);
	Root->SetStringField(TEXT("availability"), EnumName(Snapshot.Availability));
	Root->SetStringField(TEXT("engineState"), Snapshot.EngineState);
	Root->SetStringField(
		TEXT("preparationState"),
		EnumName(Snapshot.PreparationState)
	);
	Root->SetBoolField(TEXT("playerEnabled"), Snapshot.bPlayerEnabled);
	Root->SetNumberField(TEXT("masterIntensity"), Snapshot.MasterIntensity);
	Root->SetNumberField(TEXT("activePlayback"), Snapshot.ActivePlaybackCount);
	Root->SetNumberField(TEXT("queuedPlayback"), Snapshot.QueuedPlaybackCount);
	Root->SetNumberField(
		TEXT("preparedNamedPatterns"),
		Snapshot.PreparedNamedPatternCount
	);
	Root->SetNumberField(
		TEXT("cachedTimelines"),
		Snapshot.CachedTimelineCount
	);
	Root->SetStringField(
		TEXT("cachedTimelineBytes"),
		FString::Printf(TEXT("%lld"), Snapshot.CachedTimelineBytes)
	);
	Root->SetBoolField(TEXT("truncated"), Snapshot.bTruncated);

	auto SetSupportArray = [&Root](
		const TCHAR* Field,
		const TArray<FOpenMobileHapticsCapabilityTesterSupport>& Source
	)
	{
		TArray<TSharedPtr<FJsonValue>> Items;
		Items.Reserve(Source.Num());
		for (const FOpenMobileHapticsCapabilityTesterSupport& Support : Source)
		{
			Items.Add(SupportJson(Support));
		}
		Root->SetArrayField(Field, MoveTemp(Items));
	};
	SetSupportArray(TEXT("features"), Snapshot.FeatureSupport);
	SetSupportArray(TEXT("primitives"), Snapshot.PrimitiveSupport);
	SetSupportArray(TEXT("presets"), Snapshot.PresetSupport);
	TArray<TSharedPtr<FJsonValue>> Limits;
	for (const FOpenMobileHapticsCapabilityTesterLimit& Limit : Snapshot.Limits)
	{
		const TSharedRef<FJsonObject> Item = MakeShared<FJsonObject>();
		Item->SetStringField(TEXT("name"), Limit.Name);
		Item->SetStringField(TEXT("unit"), Limit.Unit);
		if (Limit.bKnown && FMath::IsFinite(Limit.Value))
		{
			Item->SetNumberField(TEXT("value"), Limit.Value);
		}
		else
		{
			Item->SetField(TEXT("value"), MakeShared<FJsonValueNull>());
		}
		Limits.Add(MakeShared<FJsonValueObject>(Item));
	}
	Root->SetArrayField(TEXT("limits"), MoveTemp(Limits));
	const TSharedRef<TJsonWriter<
		TCHAR,
		TCondensedJsonPrintPolicy<TCHAR>
	>> Writer = TJsonWriterFactory<
		TCHAR,
		TCondensedJsonPrintPolicy<TCHAR>
	>::Create(&OutJson);
	if (!FJsonSerializer::Serialize(Root, Writer) || !Writer->Close()
		|| OutJson.Len() * sizeof(TCHAR) > MaximumOutputBytes)
	{
		OutJson.Reset();
		OutError = TEXT("Capability snapshot exceeds its output limit.");
		return false;
	}
	return true;
}

FString FOpenMobileHapticsCapabilityTesterSnapshotBuilder::ToDisplayText(
	const FOpenMobileHapticsCapabilityTesterSnapshot& Snapshot
)
{
	using namespace OpenMobileHapticsCapabilityTesterPrivate;
	FString Text = FString::Printf(
		TEXT("Backend: %s\nNative API tier: %s\nAvailability: %s\nEngine: %s\nPreparation: %s\nPlayer policy: %s at %.2f\nPlayback: %d active, %d queued\nResources: %d named prepared, %d timelines cached, %lld cache bytes\n\nFeatures\n"),
		*Snapshot.Backend,
		*Snapshot.NativeApiTier,
		*EnumName(Snapshot.Availability),
		*Snapshot.EngineState,
		*EnumName(Snapshot.PreparationState),
		Snapshot.bPlayerEnabled ? TEXT("enabled") : TEXT("disabled"),
		Snapshot.MasterIntensity,
		Snapshot.ActivePlaybackCount,
		Snapshot.QueuedPlaybackCount,
		Snapshot.PreparedNamedPatternCount,
		Snapshot.CachedTimelineCount,
		Snapshot.CachedTimelineBytes
	);
	for (const FOpenMobileHapticsCapabilityTesterSupport& Support
		: Snapshot.FeatureSupport)
	{
		Text += FString::Printf(
			TEXT("%s: %s\n"),
			*Support.Name,
			*EnumName(Support.State)
		);
	}
	Text += TEXT("\nPrimitives\n");
	for (const FOpenMobileHapticsCapabilityTesterSupport& Support
		: Snapshot.PrimitiveSupport)
	{
		Text += FString::Printf(
			TEXT("%s: %s\n"),
			*Support.Name,
			*EnumName(Support.State)
		);
	}
	Text += TEXT("\nPresets\n");
	for (const FOpenMobileHapticsCapabilityTesterSupport& Support
		: Snapshot.PresetSupport)
	{
		Text += FString::Printf(
			TEXT("%s: %s\n"),
			*Support.Name,
			*EnumName(Support.State)
		);
	}
	Text += TEXT("\nLimits\n");
	for (const FOpenMobileHapticsCapabilityTesterLimit& Limit : Snapshot.Limits)
	{
		Text += Limit.bKnown
			? FString::Printf(
				TEXT("%s: %.6g %s\n"),
				*Limit.Name,
				Limit.Value,
				*Limit.Unit
			)
			: FString::Printf(TEXT("%s: unknown\n"), *Limit.Name);
	}
	return Text;
}

bool UOpenMobileHapticsCapabilityTesterLibrary::
CreateSanitizedCapabilitySnapshot(
	const UObject* WorldContextObject,
	FOpenMobileHapticsCapabilityTesterSnapshot& OutSnapshot,
	FString& OutJson,
	FString& OutError
)
{
	OutSnapshot = {};
	OutJson.Reset();
	OutError.Reset();
#if OPENMOBILE_HAPTICS_PREVIEW_ENABLED
	const UWorld* World = GEngine
		? GEngine->GetWorldFromContextObject(
			WorldContextObject,
			EGetWorldErrorMode::ReturnNull
		)
		: nullptr;
	const UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	const UOpenMobileHapticsSubsystem* Haptics = GameInstance
		? GameInstance->GetSubsystem<UOpenMobileHapticsSubsystem>()
		: nullptr;
	if (!Haptics)
	{
		OutError = TEXT("Haptics subsystem is unavailable.");
		return false;
	}
	OutSnapshot =
		FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Capture(*Haptics);
	return FOpenMobileHapticsCapabilityTesterSnapshotBuilder::Serialize(
		OutSnapshot,
		OutJson,
		OutError
	);
#else
	static_cast<void>(WorldContextObject);
	OutError = TEXT("Capability tester is available only in Development builds.");
	return false;
#endif
}
