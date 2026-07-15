#include "OpenMobileHapticsPrimitiveCompositionPolicy.h"

#include "OpenMobileHapticsBudgetPolicy.h"
#include "OpenMobileHapticsSettings.h"

namespace OpenMobileHapticsPrimitiveCompositionPolicyPrivate
{
	FName PrimitiveName(EOpenMobileHapticAndroidPrimitive Primitive)
	{
		switch (Primitive)
		{
		case EOpenMobileHapticAndroidPrimitive::Tick:
			return TEXT("Tick");
		case EOpenMobileHapticAndroidPrimitive::LowTick:
			return TEXT("LowTick");
		case EOpenMobileHapticAndroidPrimitive::Click:
			return TEXT("Click");
		case EOpenMobileHapticAndroidPrimitive::Thud:
			return TEXT("Thud");
		case EOpenMobileHapticAndroidPrimitive::Spin:
			return TEXT("Spin");
		case EOpenMobileHapticAndroidPrimitive::QuickRise:
			return TEXT("QuickRise");
		case EOpenMobileHapticAndroidPrimitive::SlowRise:
			return TEXT("SlowRise");
		case EOpenMobileHapticAndroidPrimitive::QuickFall:
			return TEXT("QuickFall");
		default:
			return NAME_None;
		}
	}

	EOpenMobileHapticSupportState DetailedSupport(
		FName Primitive,
		const FOpenMobileHapticCapabilities& Capabilities
	)
	{
		for (const FOpenMobileHapticNamedSupport& Entry :
			Capabilities.PrimitiveSupport)
		{
			if (Entry.Name == Primitive)
			{
				return Entry.Support;
			}
		}
		return EOpenMobileHapticSupportState::Unknown;
	}

	FOpenMobileHapticsPrimitiveCompositionResolution Unavailable(
		EOpenMobileHapticFallbackPolicy Policy,
		FName Reason
	)
	{
		FOpenMobileHapticsPrimitiveCompositionResolution Resolution;
		Resolution.Outcome =
			Policy == EOpenMobileHapticFallbackPolicy::ExactOnly
				? EOpenMobileHapticsPrimitiveCompositionOutcome::Rejected
				: EOpenMobileHapticsPrimitiveCompositionOutcome::FallbackRequired;
		Resolution.Reason = Reason;
		return Resolution;
	}

	FOpenMobileHapticsPrimitiveCompositionResolution Invalid(FName Reason)
	{
		FOpenMobileHapticsPrimitiveCompositionResolution Resolution;
		Resolution.Outcome =
			EOpenMobileHapticsPrimitiveCompositionOutcome::Rejected;
		Resolution.Reason = Reason;
		return Resolution;
	}
}

FOpenMobileHapticsPrimitiveCompositionResolution
FOpenMobileHapticsPrimitiveCompositionPolicy::Resolve(
	const UOpenMobileHapticAndroidPatternAsset& Asset,
	const FOpenMobileHapticCapabilities& Capabilities,
	int32 AndroidAPI,
	float RequestIntensity,
	EOpenMobileHapticFallbackPolicy FallbackPolicy
)
{
	using namespace OpenMobileHapticsPrimitiveCompositionPolicyPrivate;
	if (!FMath::IsFinite(RequestIntensity)
		|| RequestIntensity < 0.0f
		|| RequestIntensity > 1.0f
		|| Asset.Format != EOpenMobileHapticAndroidPatternFormat::Primitives)
	{
		return Invalid(TEXT("InvalidRequest"));
	}
	TArray<FString> Errors;
	if (!Asset.Validate(Errors))
	{
		return Invalid(TEXT("InvalidPattern"));
	}
	if (AndroidAPI < Asset.GetMinimumOSVersion())
	{
		return Unavailable(FallbackPolicy, TEXT("OSVersion"));
	}
	if (Capabilities.Primitives
		!= EOpenMobileHapticSupportState::Supported)
	{
		return Unavailable(FallbackPolicy, TEXT("PrimitiveSupport"));
	}
	const int32 ConfiguredMaximumSteps =
		FOpenMobileHapticsBudgetPolicy::ResolveMaximumPatternEvents(
			GetDefault<UOpenMobileHapticsSettings>()->MaximumPatternEventCount
		);
	const int32 MaximumSteps = Capabilities.MaximumEventCount.bKnown
		? FMath::Min(
			Capabilities.MaximumEventCount.Value,
			ConfiguredMaximumSteps
		)
		: ConfiguredMaximumSteps;
	if (Asset.Primitives.Num() > MaximumSteps)
	{
		return Invalid(TEXT("StepCount"));
	}

	FOpenMobileHapticsPrimitiveCompositionResolution Resolution;
	Resolution.Outcome = EOpenMobileHapticsPrimitiveCompositionOutcome::Ready;
	Resolution.Reason = TEXT("Supported");
	Resolution.Primitives.Reserve(Asset.Primitives.Num());
	Resolution.Scales.Reserve(Asset.Primitives.Num());
	Resolution.DelaysMilliseconds.Reserve(Asset.Primitives.Num());
	int64 TotalDelayMilliseconds = 0;
	for (const FOpenMobileHapticAndroidPrimitiveStep& Step : Asset.Primitives)
	{
		if (Step.DelayMilliseconds > MaximumStepDelayMilliseconds)
		{
			return Invalid(TEXT("StepDelay"));
		}
		TotalDelayMilliseconds += Step.DelayMilliseconds;
		if (TotalDelayMilliseconds > MaximumTotalDelayMilliseconds)
		{
			return Invalid(TEXT("TotalDelay"));
		}
		const FName Name = PrimitiveName(Step.Primitive);
		if (Name.IsNone())
		{
			return Invalid(TEXT("Primitive"));
		}
		if (DetailedSupport(Name, Capabilities)
			!= EOpenMobileHapticSupportState::Supported)
		{
			return Unavailable(FallbackPolicy, Name);
		}
		Resolution.Primitives.Add(Step.Primitive);
		Resolution.Scales.Add(Step.Scale * RequestIntensity);
		Resolution.DelaysMilliseconds.Add(Step.DelayMilliseconds);
	}
	return Resolution;
}
