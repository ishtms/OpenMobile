#include "OpenMobileHapticsAndroidBackend.h"

#include "Android/AndroidApplication.h"
#include "Android/AndroidJNI.h"
#include "Misc/ScopeLock.h"

namespace OpenMobileHapticsAndroidBackendPrivate
{
	constexpr int32 ProbeUnavailable = -1;
	constexpr int32 HasActuator = 1 << 0;
	constexpr int32 HasSemanticFeedback = 1 << 1;
	constexpr int32 HasRichHaptics = 1 << 2;

	int32 QueryHardwareFlags()
	{
		JNIEnv* Env = FAndroidApplication::GetJavaEnv();
		jobject Activity = FAndroidApplication::GetGameActivityThis();
		if (!Env || !Activity)
		{
			return ProbeUnavailable;
		}

		FScopedJavaObject<jclass> ActivityClass(Env->GetObjectClass(Activity));
		const jmethodID Method = ActivityClass
			? Env->GetMethodID(
				*ActivityClass,
				"AndroidThunkJava_OpenMobileHapticsQueryCapabilities",
				"()I"
			)
			: nullptr;
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return ProbeUnavailable;
		}
		if (!Method)
		{
			return ProbeUnavailable;
		}

		const jint Flags = Env->CallIntMethod(Activity, Method);
		if (Env->ExceptionCheck())
		{
			Env->ExceptionClear();
			return ProbeUnavailable;
		}
		return static_cast<int32>(Flags);
	}
}

FOpenMobileHapticCapabilities
FOpenMobileHapticsAndroidBackend::ProbeHardwareCapabilities() const
{
	using namespace OpenMobileHapticsAndroidBackendPrivate;
	FOpenMobileHapticCapabilities Capabilities;
	Capabilities.BackendName = GetBackendName();
	const int32 Flags = QueryHardwareFlags();
	if (Flags == ProbeUnavailable)
	{
		Capabilities.Availability =
			EOpenMobileHapticAvailability::TemporarilyUnavailable;
		Capabilities.Detail =
			TEXT("Android's vibrator service is temporarily unavailable.");
		return Capabilities;
	}
	if ((Flags & HasActuator) == 0)
	{
		Capabilities.Availability = EOpenMobileHapticAvailability::NoActuator;
		Capabilities.BasicVibration = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.SemanticFeedback = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.RichHaptics = EOpenMobileHapticSupportState::Unsupported;
		Capabilities.Detail = TEXT("Android reports no phone vibrator.");
		return Capabilities;
	}

	Capabilities.BasicVibration = EOpenMobileHapticSupportState::Supported;
	Capabilities.SemanticFeedback =
		(Flags & HasSemanticFeedback) != 0
			? EOpenMobileHapticSupportState::Supported
			: EOpenMobileHapticSupportState::Unsupported;
	Capabilities.RichHaptics = (Flags & HasRichHaptics) != 0
		? EOpenMobileHapticSupportState::Supported
		: EOpenMobileHapticSupportState::Unsupported;
	Capabilities.Availability = (Flags & HasRichHaptics) != 0
		? EOpenMobileHapticAvailability::RichHaptics
		: (Flags & HasSemanticFeedback) != 0
			? EOpenMobileHapticAvailability::SemanticFeedback
			: EOpenMobileHapticAvailability::BasicVibration;
	Capabilities.Detail = TEXT("Android vibrator capabilities were queried without playback.");
	return Capabilities;
}

FOpenMobileHapticCapabilities
FOpenMobileHapticsAndroidBackend::GetCapabilities() const
{
	FScopeLock Lock(&CacheMutex);
	if (StableCapabilities.IsSet())
	{
		return StableCapabilities.GetValue();
	}
	FOpenMobileHapticCapabilities Capabilities = ProbeHardwareCapabilities();
	if (Capabilities.Availability
		!= EOpenMobileHapticAvailability::TemporarilyUnavailable)
	{
		StableCapabilities = Capabilities;
	}
	return Capabilities;
}
