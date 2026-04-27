#include "Async/Async.h"
#include "Async/TaskGraphInterfaces.h"
#include "Containers/Ticker.h"
#include "Engine/GameInstance.h"
#include "Features/IModularFeatures.h"
#include "HAL/PlatformMisc.h"
#include "IOpenMobileAdsConsentSignalConsumer.h"
#include "IOpenMobileAdsProvider.h"
#include "IOpenMobileAdsTrackingAuthorizationBackend.h"
#include "Misc/App.h"
#include "Misc/AutomationTest.h"
#include "Misc/CoreDelegates.h"
#include "OpenMobileAdsAsyncAction.h"
#include "OpenMobileAdsCanShowPolicy.h"
#include "OpenMobileAdsClock.h"
#include "OpenMobileAdsConfiguration.h"
#include "OpenMobileAdsConnectivityPolicy.h"
#include "OpenMobileAdsFullscreenLifecycle.h"
#include "OpenMobileAdsRetry.h"
#include "OpenMobileAdsSubsystem.h"
#include "OpenMobileAdsTrackingAuthorizationPlatform.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace OpenMobileAdsProviderContractTests
{
	class FControlledAdsClock final : public IOpenMobileAdsClock
	{
	public:
		virtual FDateTime UtcNow() const override
		{
			return CurrentUtc;
		}

		virtual double MonotonicSeconds() const override
		{
			return CurrentMonotonicSeconds;
		}

		void Advance(double Seconds)
		{
			CurrentUtc += FTimespan::FromSeconds(Seconds);
			CurrentMonotonicSeconds += Seconds;
		}

		void ShiftWallClock(double Seconds)
		{
			CurrentUtc += FTimespan::FromSeconds(Seconds);
		}

		FDateTime CurrentUtc = FDateTime(2035, 4, 5, 12, 0, 0);
		double CurrentMonotonicSeconds = 1000.0;
	};

	class FControlledRetryScheduler final : public IOpenMobileAdsRetryScheduler
	{
	public:
		virtual FOpenMobileAdsRetryScheduleHandle Schedule(
			double DelaySeconds,
			TFunction<void()>&& Callback
		) override
		{
			FOpenMobileAdsRetryScheduleHandle Handle;
			Handle.Value = NextHandle++;
			FTask& Task = Tasks.Emplace_GetRef();
			Task.Handle = Handle;
			Task.DueTime = CurrentTime + DelaySeconds;
			Task.Callback = MoveTemp(Callback);
			ScheduledDelays.Add(DelaySeconds);
			return Handle;
		}

		virtual void Cancel(
			FOpenMobileAdsRetryScheduleHandle& Handle
		) override
		{
			Tasks.RemoveAll(
				[Handle](const FTask& Task)
				{
					return Task.Handle == Handle;
				}
			);
			Handle.Reset();
		}

		void AdvanceBy(double DeltaSeconds)
		{
			CurrentTime += DeltaSeconds;
			while (true)
			{
				int32 NextTaskIndex = INDEX_NONE;
				double NextDueTime = TNumericLimits<double>::Max();
				for (int32 Index = 0; Index < Tasks.Num(); ++Index)
				{
					if (
						Tasks[Index].DueTime <= CurrentTime
						&& Tasks[Index].DueTime < NextDueTime
					)
					{
						NextTaskIndex = Index;
						NextDueTime = Tasks[Index].DueTime;
					}
				}
				if (NextTaskIndex == INDEX_NONE)
				{
					return;
				}
				TFunction<void()> Callback =
					MoveTemp(Tasks[NextTaskIndex].Callback);
				Tasks.RemoveAtSwap(NextTaskIndex, EAllowShrinking::No);
				Callback();
			}
		}

		int32 NumPending() const
		{
			return Tasks.Num();
		}

		TArray<double> ScheduledDelays;

	private:
		struct FTask
		{
			FOpenMobileAdsRetryScheduleHandle Handle;
			double DueTime = 0.0;
			TFunction<void()> Callback;
		};

		TArray<FTask> Tasks;
		double CurrentTime = 0.0;
		uint64 NextHandle = 1;
	};

	class FControlledRetryRandomSource final
		: public IOpenMobileAdsRetryRandomSource
	{
	public:
		virtual double NextUnit() override
		{
			return NextValue;
		}

		double NextValue = 0.0;
	};

	class FMockFullscreenLifecycleTarget final
		: public IOpenMobileAdsFullscreenLifecycleTarget
	{
	public:
		virtual void Apply() override { ++ApplyCalls; }
		virtual void RestoreGameplay() override { ++RestoreGameplayCalls; }
		virtual void RestoreFocus() override { ++RestoreFocusCalls; }

		int32 ApplyCalls = 0;
		int32 RestoreGameplayCalls = 0;
		int32 RestoreFocusCalls = 0;
	};

	class FMockProvider final : public IOpenMobileAdsProvider
	{
	public:
		explicit FMockProvider(FName InName, bool bInSupported = true)
			: Name(InName)
			, bSupported(bInSupported)
		{
			FOpenMobileAdFormatCapabilities Rewarded;
			Rewarded.Format = EOpenMobileAdFormat::Rewarded;
			Rewarded.bCanLoad = true;
			Rewarded.bCanShow = true;
			Rewarded.bReportsDismiss = true;
			Rewarded.bReportsReward = true;
			Capabilities.Provider = Name;
			Capabilities.Formats.Add(Rewarded);
		}

		virtual FName GetProviderName() const override { return Name; }
		virtual bool IsSupported() const override { return bSupported; }
		virtual FOpenMobileAdsProviderCapabilities GetCapabilities() const override
		{
			return Capabilities;
		}
		virtual FOpenMobileAdsProviderRequestPolicy GetRequestPolicy(
			const FOpenMobileAdsProviderRequestContext& Context
		) const override
		{
			if (
				bBlockChildDirectedRequests
				&& Context.ChildDirectedTreatment == EOpenMobileAdsAgeTreatment::Yes
			)
			{
				FOpenMobileAdsProviderRequestPolicy Policy;
				Policy.State = EOpenMobileAdsProviderRequestPolicyState::Blocked;
				Policy.Explanation =
					TEXT("The mock provider does not accept child-directed requests.");
				return Policy;
			}
			if (
				bBlockUnderAgeRequests
				&& Context.UnderAgeOfConsent == EOpenMobileAdsAgeTreatment::Yes
			)
			{
				FOpenMobileAdsProviderRequestPolicy Policy;
				Policy.State = EOpenMobileAdsProviderRequestPolicyState::Blocked;
				Policy.Explanation =
					TEXT("The mock provider does not accept under-age requests.");
				return Policy;
			}
			return RequestPolicy;
		}
		virtual FName GetConsentProviderName() const override
		{
			return ConsentProviderName;
		}
		virtual bool SupportsPrivacyOptionsForm() const override
		{
			return bSupportsPrivacyOptionsForm;
		}
		virtual bool SupportsConsentResetForTesting() const override
		{
			return bSupportsConsentResetForTesting;
		}
		virtual bool ResetConsentForTesting(
			FOpenMobileAdsError& OutError
		) override
		{
			++ConsentResetCalls;
			if (!bAcceptConsentReset)
			{
				OutError = ConsentResetError;
				return false;
			}
			return true;
		}
		virtual int32 GetSupportedConsentSignalMask() const override
		{
			return SupportedConsentSignalMask;
		}
		virtual int32 GetConfirmableConsentSignalMask() const override
		{
			return ConfirmableConsentSignalMask;
		}
		virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
		{
			return RuntimeConsentSignalMask;
		}
		virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
			const FOpenMobileAdsConsentSignals& Signals,
			int32 SignalMask
		) override
		{
			++ConsentSignalCalls;
			LastConsentSignals = Signals;
			LastConsentSignalMask = SignalMask;
			if (Sequence)
			{
				ConsentSignalSequence = ++*Sequence;
			}
			return FOpenMobileAdsConsentSignalApplyResult::Applied(
				SignalMask,
				SignalMask & ConfirmableConsentSignalMask
			);
		}
		virtual bool RefreshConsent(
			const FOpenMobileAdsConsentRequest& Request,
			TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++ConsentRefreshCalls;
			LastConsentRequest = Request;
			ConsentRefreshSink = CompletionSink;
			if (!bAcceptConsentRefresh)
			{
				OutError = ConsentRejection;
				ConsentRefreshSink.Reset();
				return false;
			}
			return true;
		}
		virtual bool PresentRequiredConsentForm(
			const FOpenMobileAdsConsentRequest& Request,
			TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++ConsentFormCalls;
			LastConsentFormRequest = Request;
			ConsentFormSink = CompletionSink;
			if (!bAcceptConsentForm)
			{
				OutError = ConsentRejection;
				ConsentFormSink.Reset();
				return false;
			}
			return true;
		}
		virtual bool PresentPrivacyOptionsForm(
			const FOpenMobileAdsConsentRequest& Request,
			TSharedRef<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++PrivacyOptionsFormCalls;
			LastPrivacyOptionsRequest = Request;
			PrivacyOptionsFormSink = CompletionSink;
			if (!bAcceptPrivacyOptionsForm)
			{
				OutError = ConsentRejection;
				PrivacyOptionsFormSink.Reset();
				return false;
			}
			return true;
		}
		virtual void CancelConsent(FGuid RequestId) override
		{
			CancelledConsentRequests.Add(RequestId);
		}

		virtual bool Initialize(
			const FOpenMobileAdsInitializationRequest& Request,
			TSharedRef<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> CompletionSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++InitializationCalls;
			if (Sequence)
			{
				InitializationSequence = ++*Sequence;
			}
			LastInitializationRequest = Request;
			InitializationSink = CompletionSink;
			if (!bAcceptInitialization)
			{
				OutError = InitializationRejection;
				return false;
			}
			return true;
		}

		virtual bool Load(
			const FOpenMobileAdsLoadRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++LoadCalls;
			LastLoadRequest = Request;
			LoadSink = EventSink;
			if (!bAcceptLoad)
			{
				OutError = LoadRejection;
				LoadSink.Reset();
				return false;
			}
			return true;
		}

		virtual bool Show(
			const FOpenMobileAdsShowRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++ShowCalls;
			LastShowRequest = Request;
			ShowSink = EventSink;
			if (!bAcceptShow)
			{
				OutError = ShowRejection;
				ShowSink.Reset();
				return false;
			}
			return true;
		}

		virtual bool Destroy(
			const FOpenMobileAdsDestroyRequest& Request,
			TSharedRef<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> EventSink,
			FOpenMobileAdsError& OutError
		) override
		{
			++DestroyCalls;
			LastDestroyRequest = Request;
			DestroySink = EventSink;
			if (!bAcceptDestroy)
			{
				OutError = DestroyRejection;
				DestroySink.Reset();
				return false;
			}
			return true;
		}

		virtual bool RequestAndShowRewardedAd(
			FOpenMobileRewardedAdCallbacks&& Callbacks,
			FOpenMobileError& OutError
		) override
		{
			++LegacyRewardedRequestCalls;
			return false;
		}

		virtual void Cancel(FGuid RequestId) override
		{
			CancelledRequests.Add(RequestId);
		}

		virtual void ReleaseCachedAd(FGuid CachedAdId) override
		{
			ReleasedCachedAds.Add(CachedAdId);
		}

		virtual void Shutdown() override
		{
			++ShutdownCalls;
		}

		void CompleteInitialization(FOpenMobileAdsError Error = FOpenMobileAdsError())
		{
			if (InitializationSink)
			{
				InitializationSink->Complete(MoveTemp(Error));
			}
		}

		void ReportInitializationStatus(
			FOpenMobileAdsInitializationComponentStatus Status
		)
		{
			if (InitializationSink)
			{
				InitializationSink->UpdateStatus(MoveTemp(Status));
			}
		}

		void CompleteConsentRefresh(FOpenMobileAdsConsentStatusUpdate Update)
		{
			if (ConsentRefreshSink)
			{
				ConsentRefreshSink->Complete(MoveTemp(Update));
			}
		}

		void CompleteConsentForm(FOpenMobileAdsConsentStatusUpdate Update)
		{
			if (ConsentFormSink)
			{
				ConsentFormSink->Complete(MoveTemp(Update));
			}
		}

		void FailConsentRefresh(FOpenMobileAdsError Error)
		{
			if (ConsentRefreshSink)
			{
				ConsentRefreshSink->Fail(MoveTemp(Error));
			}
		}

		void FailConsentForm(FOpenMobileAdsError Error)
		{
			if (ConsentFormSink)
			{
				ConsentFormSink->Fail(MoveTemp(Error));
			}
		}

		void CompletePrivacyOptionsForm(FOpenMobileAdsConsentStatusUpdate Update)
		{
			if (PrivacyOptionsFormSink)
			{
				PrivacyOptionsFormSink->Complete(MoveTemp(Update));
			}
		}

		void FailPrivacyOptionsForm(FOpenMobileAdsError Error)
		{
			if (PrivacyOptionsFormSink)
			{
				PrivacyOptionsFormSink->Fail(MoveTemp(Error));
			}
		}

		FName Name;
		bool bSupported = true;
		bool bAcceptInitialization = true;
		bool bAcceptLoad = true;
		bool bAcceptShow = true;
		bool bAcceptDestroy = true;
		bool bBlockChildDirectedRequests = false;
		bool bBlockUnderAgeRequests = false;
		bool bAcceptConsentRefresh = true;
		bool bAcceptConsentForm = true;
		bool bSupportsPrivacyOptionsForm = false;
		bool bAcceptPrivacyOptionsForm = true;
		bool bSupportsConsentResetForTesting = false;
		bool bAcceptConsentReset = true;
		int32 SupportedConsentSignalMask = 0;
		int32 ConfirmableConsentSignalMask = 0;
		int32 RuntimeConsentSignalMask = 0;
		int32* Sequence = nullptr;
		int32 ConsentSignalSequence = 0;
		int32 InitializationSequence = 0;
		int32 InitializationCalls = 0;
		int32 LoadCalls = 0;
		int32 ShowCalls = 0;
		int32 DestroyCalls = 0;
		int32 LegacyRewardedRequestCalls = 0;
		int32 ShutdownCalls = 0;
		int32 ConsentRefreshCalls = 0;
		int32 ConsentFormCalls = 0;
		int32 PrivacyOptionsFormCalls = 0;
		int32 ConsentResetCalls = 0;
		int32 ConsentSignalCalls = 0;
		int32 LastConsentSignalMask = 0;
		FName ConsentProviderName;
		FOpenMobileAdsProviderCapabilities Capabilities;
		FOpenMobileAdsProviderRequestPolicy RequestPolicy;
		FOpenMobileAdsInitializationRequest LastInitializationRequest;
		FOpenMobileAdsError InitializationRejection;
		FOpenMobileAdsError LoadRejection;
		FOpenMobileAdsError ShowRejection;
		FOpenMobileAdsError DestroyRejection;
		FOpenMobileAdsLoadRequest LastLoadRequest;
		FOpenMobileAdsShowRequest LastShowRequest;
		FOpenMobileAdsDestroyRequest LastDestroyRequest;
		FOpenMobileAdsConsentRequest LastConsentRequest;
		FOpenMobileAdsConsentRequest LastConsentFormRequest;
		FOpenMobileAdsConsentRequest LastPrivacyOptionsRequest;
		FOpenMobileAdsConsentSignals LastConsentSignals;
		FOpenMobileAdsError ConsentRejection;
		FOpenMobileAdsError ConsentResetError;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> LoadSink;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShowSink;
		TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> DestroySink;
		TSharedPtr<IOpenMobileAdsProviderInitializationSink, ESPMode::ThreadSafe> InitializationSink;
		TSharedPtr<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> ConsentRefreshSink;
		TSharedPtr<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> ConsentFormSink;
		TSharedPtr<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe> PrivacyOptionsFormSink;
		TArray<FGuid> CancelledRequests;
		TArray<FGuid> CancelledConsentRequests;
		TArray<FGuid> ReleasedCachedAds;
	};

	class FMockConsentSignalConsumer final
		: public IOpenMobileAdsConsentSignalConsumer
	{
	public:
		FMockConsentSignalConsumer(
			FName InProvider,
			EOpenMobileAdsConsentSignalConsumerType InType,
			FName InName,
			FName InParent = NAME_None
		)
			: Provider(InProvider)
			, Type(InType)
			, Name(InName)
			, Parent(InParent)
		{
		}

		virtual FName GetOwningProviderName() const override { return Provider; }
		virtual EOpenMobileAdsConsentSignalConsumerType GetConsumerType() const override
		{
			return Type;
		}
		virtual FName GetConsumerName() const override { return Name; }
		virtual FName GetParentName() const override { return Parent; }
		virtual int32 GetSupportedConsentSignalMask() const override
		{
			return SupportedSignalMask;
		}
		virtual int32 GetConfirmableConsentSignalMask() const override
		{
			return ConfirmableSignalMask;
		}
		virtual int32 GetRuntimeUpdatableConsentSignalMask() const override
		{
			return RuntimeSignalMask;
		}
		virtual FOpenMobileAdsConsentSignalApplyResult ApplyConsentSignals(
			const FOpenMobileAdsConsentSignals& Signals,
			int32 SignalMask
		) override
		{
			++Calls;
			LastSignals = Signals;
			LastSignalMask = SignalMask;
			if (Sequence)
			{
				LastSequence = ++*Sequence;
			}
			if (ConsentSignalError.IsSet())
			{
				FOpenMobileAdsConsentSignalApplyResult Result;
				Result.Error = ConsentSignalError;
				return Result;
			}
			return FOpenMobileAdsConsentSignalApplyResult::Applied(
				SignalMask,
				SignalMask & ConfirmableSignalMask
			);
		}

		FName Provider;
		EOpenMobileAdsConsentSignalConsumerType Type;
		FName Name;
		FName Parent;
		int32 SupportedSignalMask = 0;
		int32 ConfirmableSignalMask = 0;
		int32 RuntimeSignalMask = 0;
		int32 Calls = 0;
		int32 LastSignalMask = 0;
		int32* Sequence = nullptr;
		int32 LastSequence = 0;
		FOpenMobileAdsConsentSignals LastSignals;
		FOpenMobileAdsError ConsentSignalError;
	};

	class FScopedConsentSignalConsumerRegistration
	{
	public:
		explicit FScopedConsentSignalConsumerRegistration(
			IOpenMobileAdsConsentSignalConsumer& InConsumer
		)
			: Consumer(InConsumer)
		{
			IModularFeatures::Get().RegisterModularFeature(
				IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
				&Consumer
			);
		}

		~FScopedConsentSignalConsumerRegistration()
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsConsentSignalConsumer::GetModularFeatureName(),
				&Consumer
			);
		}

	private:
		IOpenMobileAdsConsentSignalConsumer& Consumer;
	};

	class FScopedProviderRegistration
	{
	public:
		explicit FScopedProviderRegistration(IOpenMobileAdsProvider& InProvider)
			: Provider(InProvider)
		{
			IModularFeatures::Get().RegisterModularFeature(
				IOpenMobileAdsProvider::GetModularFeatureName(),
				&Provider
			);
		}

		~FScopedProviderRegistration()
		{
			Unregister();
		}

		void Unregister()
		{
			if (!bRegistered)
			{
				return;
			}
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsProvider::GetModularFeatureName(),
				&Provider
			);
			bRegistered = false;
		}

	private:
		IOpenMobileAdsProvider& Provider;
		bool bRegistered = true;
	};

	class FMockTrackingAuthorizationBackend final
		: public IOpenMobileAdsTrackingAuthorizationBackend
	{
	public:
		virtual FName GetBackendName() const override { return TEXT("MockATT"); }
		virtual bool IsAvailable() const override { return true; }
		virtual EOpenMobileAdsTrackingAuthorizationStatus GetStatus() const override
		{
			return Status;
		}
		virtual bool HasNonZeroAdvertisingIdentifier() const override
		{
			++AdvertisingIdentifierReads;
			return bHasNonZeroAdvertisingIdentifier;
		}
		virtual bool RequestAuthorization(
			TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)>&& InCompletion,
			FString& OutError
		) override
		{
			++RequestCalls;
			if (!bAcceptRequest)
			{
				OutError = TEXT("Mock ATT request rejected.");
				return false;
			}
			Completion = MoveTemp(InCompletion);
			return true;
		}

		void Complete(EOpenMobileAdsTrackingAuthorizationStatus InStatus)
		{
			Status = InStatus;
			TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)> Callback =
				MoveTemp(Completion);
			Callback(InStatus);
		}

		EOpenMobileAdsTrackingAuthorizationStatus Status =
			EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined;
		TFunction<void(EOpenMobileAdsTrackingAuthorizationStatus)> Completion;
		int32 RequestCalls = 0;
		mutable int32 AdvertisingIdentifierReads = 0;
		bool bAcceptRequest = true;
		bool bHasNonZeroAdvertisingIdentifier = false;
	};

	class FScopedTrackingAuthorizationBackendRegistration
	{
	public:
		explicit FScopedTrackingAuthorizationBackendRegistration(
			IOpenMobileAdsTrackingAuthorizationBackend& InBackend
		)
			: Backend(InBackend)
		{
			IModularFeatures::Get().RegisterModularFeature(
				IOpenMobileAdsTrackingAuthorizationBackend::GetModularFeatureName(),
				&Backend
			);
		}

		~FScopedTrackingAuthorizationBackendRegistration()
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IOpenMobileAdsTrackingAuthorizationBackend::GetModularFeatureName(),
				&Backend
			);
		}

	private:
		IOpenMobileAdsTrackingAuthorizationBackend& Backend;
	};

	class FScopedSettings
	{
	public:
		FScopedSettings()
		{
			Settings = GetMutableDefault<UOpenMobileAdsSettings>();
			SavedProvider = Settings->PreferredProvider;
			bSavedDevelopmentTestMode = Settings->bDevelopmentTestMode;
			SavedTestDeviceIdentifiers = Settings->TestDeviceIdentifiers;
			SavedDebugGeography = Settings->DebugGeography;
			bSavedEnableTrackingAuthorization =
				Settings->bEnableTrackingAuthorization;
			SavedTrackingUsageDescription = Settings->TrackingUsageDescription;
			bSavedDelayAdsInitializationUntilTrackingAuthorization =
				Settings->bDelayAdsInitializationUntilTrackingAuthorization;
			SavedRetryPolicy = Settings->RetryPolicy;
			SavedNoFillRetryPolicy = Settings->NoFillRetryPolicy;
			Settings->RetryPolicy.MaxRetryAttempts = 0;
			Settings->NoFillRetryPolicy.MaxRetryAttempts = 0;
			SavedPrivacy = Settings->Privacy;
			SavedRequestConfiguration = Settings->RequestConfiguration;
			SavedPlacements = Settings->Placements;
			SavedConvenienceRewardedPlacement =
				Settings->ConvenienceRewardedPlacement;
		}

		~FScopedSettings()
		{
			Settings->PreferredProvider = SavedProvider;
			Settings->bDevelopmentTestMode = bSavedDevelopmentTestMode;
			Settings->TestDeviceIdentifiers = MoveTemp(SavedTestDeviceIdentifiers);
			Settings->DebugGeography = SavedDebugGeography;
			Settings->bEnableTrackingAuthorization =
				bSavedEnableTrackingAuthorization;
			Settings->TrackingUsageDescription =
				MoveTemp(SavedTrackingUsageDescription);
			Settings->bDelayAdsInitializationUntilTrackingAuthorization =
				bSavedDelayAdsInitializationUntilTrackingAuthorization;
			Settings->RetryPolicy = SavedRetryPolicy;
			Settings->NoFillRetryPolicy = SavedNoFillRetryPolicy;
			Settings->Privacy = SavedPrivacy;
			Settings->RequestConfiguration = SavedRequestConfiguration;
			Settings->Placements = MoveTemp(SavedPlacements);
			Settings->ConvenienceRewardedPlacement =
				SavedConvenienceRewardedPlacement;
		}

		UOpenMobileAdsSettings* Settings = nullptr;

	private:
		FName SavedProvider;
		bool bSavedDevelopmentTestMode = false;
		TArray<FString> SavedTestDeviceIdentifiers;
		EOpenMobileAdsDebugGeography SavedDebugGeography =
			EOpenMobileAdsDebugGeography::Disabled;
		bool bSavedEnableTrackingAuthorization = false;
		FString SavedTrackingUsageDescription;
		bool bSavedDelayAdsInitializationUntilTrackingAuthorization = true;
		FOpenMobileAdsRetryPolicy SavedRetryPolicy;
		FOpenMobileAdsRetryPolicy SavedNoFillRetryPolicy;
		FOpenMobileAdsPrivacyConfiguration SavedPrivacy;
		FOpenMobileAdsRequestConfiguration SavedRequestConfiguration;
		TArray<FOpenMobileAdsPlacementSettings> SavedPlacements;
		FName SavedConvenienceRewardedPlacement;
	};

	void DrainGameThreadTasks()
	{
		FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	}

	void TickCoreTicker(float DeltaTime = 0.0f)
	{
		FTSTicker::GetCoreTicker().Tick(DeltaTime);
		DrainGameThreadTasks();
	}

	FOpenMobileAdsPlacementSettings& AddRewardedPlacement(
		UOpenMobileAdsSettings& Settings,
		FName PlacementName,
		int32 MaxRetryAttempts = -1
	)
	{
		FOpenMobileAdsPlacementSettings& Placement =
			Settings.Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.MaxRetryAttempts = MaxRetryAttempts;
		Placement.Android.AdUnitId = FString::Printf(
			TEXT("android-%s"),
			*PlacementName.ToString()
		);
		Placement.IOS.AdUnitId = FString::Printf(
			TEXT("ios-%s"),
			*PlacementName.ToString()
		);
		return Placement;
	}

	void SubmitLoadFailure(
		FMockProvider& Provider,
		EOpenMobileAdsErrorCode Code,
		bool bRetryable
	)
	{
		FOpenMobileAdsEvent Failed;
		Failed.Type = EOpenMobileAdsEventType::LoadFailed;
		Failed.Error = FOpenMobileAdsError::Make(
			Code,
			EOpenMobileAdsFailureStage::Load,
			NAME_None,
			TEXT("The mock load attempt failed."),
			Provider.Name,
			FString(),
			bRetryable
		);
		Provider.LoadSink->Submit(MoveTemp(Failed));
		DrainGameThreadTasks();
	}

	bool InitializeSuccessfully(
		UOpenMobileAdsSubsystem& Subsystem,
		FMockProvider& Provider,
		bool bAllowAdRequests = true
	)
	{
		const FOpenMobileAdsOperationResult Result = Subsystem.InitializeAds();
		if (!Result.bAccepted)
		{
			return false;
		}
		Provider.CompleteInitialization();
		DrainGameThreadTasks();
		if (Subsystem.GetServiceState() != EOpenMobileAdsServiceState::Ready)
		{
			return false;
		}
		if (bAllowAdRequests)
		{
			FOpenMobileAdsPrivacySnapshot Privacy;
			Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::NotRequired;
			Privacy.bCanRequestAds = true;
			Privacy.Source = TEXT("MockConsent");
			Subsystem.UpdatePrivacySnapshot(MoveTemp(Privacy));
		}
		return true;
	}

	const FOpenMobileAdsInitializationComponentStatus* FindInitializationComponent(
		const FOpenMobileAdsInitializationStatusSnapshot& Snapshot,
		EOpenMobileAdsInitializationComponentType Type,
		FName Name
	)
	{
		return Snapshot.Components.FindByPredicate(
			[Type, Name](const FOpenMobileAdsInitializationComponentStatus& Component)
			{
				return Component.Type == Type && Component.Name == Name;
			}
		);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsOfflinePolicyContractTest,
	"OpenMobile.Ads.Reliability.Offline.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsOfflinePolicyContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	TestEqual(
		TEXT("Caller work is rejected while the platform is offline"),
		FOpenMobileAdsConnectivityPolicy::Evaluate(
			ENetworkConnectionType::None,
			EOpenMobileAdsNetworkWorkOrigin::CallerInitiated
		),
		EOpenMobileAdsNetworkWorkDecision::RejectOffline
	);
	TestEqual(
		TEXT("Automatic work waits for a connectivity transition"),
		FOpenMobileAdsConnectivityPolicy::Evaluate(
			ENetworkConnectionType::AirplaneMode,
			EOpenMobileAdsNetworkWorkOrigin::Automatic
		),
		EOpenMobileAdsNetworkWorkDecision::DeferUntilConnectionChange
	);
	TestEqual(
		TEXT("Unknown reachability is not treated as proven offline"),
		FOpenMobileAdsConnectivityPolicy::Evaluate(
			ENetworkConnectionType::Unknown,
			EOpenMobileAdsNetworkWorkOrigin::CallerInitiated
		),
		EOpenMobileAdsNetworkWorkDecision::Start
	);

	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		false;
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("OfflineReward");
	Placement.Android.AdUnitId = TEXT("android-offline-unit");
	Placement.IOS.AdUnitId = TEXT("ios-offline-unit");
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before offline load checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	const ENetworkConnectionType PreviousConnectionType =
		FPlatformMisc::GetNetworkConnectionType();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(
		ENetworkConnectionType::None
	);
	const FOpenMobileAdsOperationResult Offline =
		Subsystem->LoadAd(TEXT("OfflineReward"));
	TestFalse(TEXT("A definite offline state rejects a load"), Offline.bAccepted);
	TestEqual(
		TEXT("Offline loads have a distinct error"),
		Offline.Error.Code,
		EOpenMobileAdsErrorCode::Offline
	);
	TestTrue(TEXT("Offline loads can resume after a transition"), Offline.Error.bRetryable);
	TestEqual(TEXT("Offline loads do not reach the provider"), Provider.LoadCalls, 0);

	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(
		ENetworkConnectionType::AirplaneMode
	);
	const FOpenMobileAdsOperationResult AirplaneMode =
		Subsystem->LoadAd(TEXT("OfflineReward"));
	TestFalse(TEXT("Airplane mode rejects a load"), AirplaneMode.bAccepted);
	TestEqual(TEXT("Airplane mode does not reach the provider"), Provider.LoadCalls, 0);

	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(
		ENetworkConnectionType::Unknown
	);
	TestTrue(
		TEXT("An unknown connection lets the provider determine reachability"),
		Subsystem->LoadAd(TEXT("OfflineReward")).bAccepted
	);
	TestEqual(TEXT("Potential connectivity reaches the provider"), Provider.LoadCalls, 1);

	Subsystem->Deinitialize();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(PreviousConnectionType);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsNoFillRecoveryContractTest,
	"OpenMobile.Ads.Reliability.NoFill.Recovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsNoFillRecoveryContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		false;
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("NoFillReward");
	Placement.Android.AdUnitId = TEXT("android-no-fill-unit");
	Placement.IOS.AdUnitId = TEXT("ios-no-fill-unit");
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TArray<FOpenMobileAdsEvent> LoadEvents;
	Subsystem->OnNativeAdsEvent().AddLambda(
		[&LoadEvents](const FOpenMobileAdsEvent& Event)
		{
			if (
				Event.Type == EOpenMobileAdsEventType::LoadFailed
				|| Event.Type == EOpenMobileAdsEventType::Loaded
			)
			{
				LoadEvents.Add(Event);
			}
		}
	);
	TestTrue(
		TEXT("The provider initializes before no-fill recovery"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	TestTrue(
		TEXT("The first no-fill load starts"),
		Subsystem->LoadAd(TEXT("NoFillReward")).bAccepted
	);
	FOpenMobileAdsErrorMappingContext NoFillContext;
	NoFillContext.Domain = EOpenMobileAdsErrorDomain::Mediation;
	NoFillContext.Stage = EOpenMobileAdsFailureStage::Load;
	NoFillContext.Placement = TEXT("NoFillReward");
	NoFillContext.Provider = TEXT("MockAds");
	NoFillContext.Network = TEXT("MockNetwork");
	NoFillContext.Adapter = TEXT("MockAdapter");
	NoFillContext.NativeCode = TEXT("no_fill");
	FOpenMobileAdsEvent NoFill;
	NoFill.Type = EOpenMobileAdsEventType::LoadFailed;
	NoFill.Error = FOpenMobileAdsErrorMapper::FromNative(NoFillContext);
	Provider.LoadSink->Submit(MoveTemp(NoFill));
	DrainGameThreadTasks();

	const FOpenMobileAdsPlacementStatus Failed =
		Subsystem->GetPlacementStatus(TEXT("NoFillReward"));
	TestEqual(
		TEXT("No fill leaves the placement recoverable"),
		Failed.State,
		EOpenMobileAdPlacementState::Failed
	);
	TestEqual(
		TEXT("No fill is retained as the last placement result"),
		Failed.LastError.Code,
		EOpenMobileAdsErrorCode::NoFill
	);
	TestEqual(TEXT("One no-fill event is broadcast"), LoadEvents.Num(), 1);
	if (LoadEvents.Num() == 1)
	{
		TestEqual(
			TEXT("No-fill events preserve the mediated network"),
			LoadEvents[0].Network,
			FString(TEXT("MockNetwork"))
		);
		TestEqual(
			TEXT("No-fill events preserve the provider"),
			LoadEvents[0].Error.Provider,
			FName(TEXT("MockAds"))
		);
		TestEqual(
			TEXT("No-fill events preserve the adapter"),
			LoadEvents[0].Error.NativeDiagnostics.Adapter,
			FString(TEXT("MockAdapter"))
		);
	}

	TestTrue(
		TEXT("A placement can load again after no fill"),
		Subsystem->LoadAd(TEXT("NoFillReward")).bAccepted
	);
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = FGuid::NewGuid();
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	const FOpenMobileAdsPlacementStatus Recovered =
		Subsystem->GetPlacementStatus(TEXT("NoFillReward"));
	TestEqual(
		TEXT("A successful retry recovers readiness"),
		Recovered.State,
		EOpenMobileAdPlacementState::Ready
	);
	TestFalse(
		TEXT("A successful retry clears the no-fill error"),
		Recovered.LastError.IsSet()
	);
	TestEqual(TEXT("Recovery broadcasts the loaded event"), LoadEvents.Num(), 2);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRetryAttemptLimitContractTest,
	"OpenMobile.Ads.Reliability.Retry.AttemptLimits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRetryAttemptLimitContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	AddExpectedError(
		TEXT("The mock load attempt failed."),
		EAutomationExpectedErrorFlags::Contains,
		4
	);
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		false;
	ScopedSettings.Settings->RetryPolicy.MaxRetryAttempts = 2;
	ScopedSettings.Settings->RetryPolicy.InitialDelaySeconds = 0.0;
	ScopedSettings.Settings->RetryPolicy.bUseJitter = false;
	ScopedSettings.Settings->NoFillRetryPolicy.MaxRetryAttempts = 1;
	ScopedSettings.Settings->NoFillRetryPolicy.InitialDelaySeconds = 0.0;
	ScopedSettings.Settings->NoFillRetryPolicy.bUseJitter = false;
	ScopedSettings.Settings->Placements.Reset();
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("GlobalRetry"));
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("LimitedRetry"), 1);
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("NoRetry"), 0);
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("NoFillRetry"));
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("TerminalRetry"));

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TArray<FOpenMobileAdsEvent> Events;
	Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);
	TestTrue(
		TEXT("The provider initializes before retry limit checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	const FOpenMobileAdsOperationResult Global =
		Subsystem->LoadAd(TEXT("GlobalRetry"));
	TestTrue(TEXT("The globally limited load starts"), Global.bAccepted);
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TestEqual(TEXT("A retryable failure stays internal"), Events.Num(), 1);
	TestEqual(
		TEXT("A pending retry keeps the placement loading"),
		Subsystem->GetPlacementStatus(TEXT("GlobalRetry")).State,
		EOpenMobileAdPlacementState::Loading
	);
	TickCoreTicker();
	TestEqual(TEXT("The first global retry reaches the provider"), Provider.LoadCalls, 2);
	TestEqual(TEXT("Retries retain the public request ID"), Provider.LastLoadRequest.RequestId, Global.RequestId);
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TickCoreTicker();
	TestEqual(TEXT("The second global retry reaches the provider"), Provider.LoadCalls, 3);
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TestEqual(TEXT("The global limit emits one terminal failure"), Events.Num(), 2);
	TestEqual(
		TEXT("The global limit leaves the placement failed"),
		Subsystem->GetPlacementStatus(TEXT("GlobalRetry")).State,
		EOpenMobileAdPlacementState::Failed
	);

	Events.Reset();
	const FOpenMobileAdsOperationResult Limited =
		Subsystem->LoadAd(TEXT("LimitedRetry"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TickCoreTicker();
	TestEqual(TEXT("The placement limit permits one retry"), Provider.LoadCalls, 5);
	TestEqual(TEXT("The placement retry keeps the request ID"), Provider.LastLoadRequest.RequestId, Limited.RequestId);
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TickCoreTicker();
	TestEqual(TEXT("The placement limit blocks a second retry"), Provider.LoadCalls, 5);
	TestEqual(TEXT("The placement limit emits one terminal failure"), Events.Num(), 2);

	Events.Reset();
	Subsystem->LoadAd(TEXT("NoRetry"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TickCoreTicker();
	TestEqual(TEXT("A zero placement limit disables retries"), Provider.LoadCalls, 6);
	TestEqual(TEXT("A zero limit still emits a terminal failure"), Events.Num(), 2);

	Events.Reset();
	Subsystem->LoadAd(TEXT("NoFillRetry"));
	SubmitLoadFailure(Provider, EOpenMobileAdsErrorCode::NoFill, true);
	TickCoreTicker();
	TestEqual(TEXT("No fill uses its dedicated single retry"), Provider.LoadCalls, 8);
	SubmitLoadFailure(Provider, EOpenMobileAdsErrorCode::NoFill, true);
	TickCoreTicker();
	TestEqual(TEXT("No fill does not use the larger general limit"), Provider.LoadCalls, 8);
	TestEqual(TEXT("Exhausted no fill emits one terminal failure"), Events.Num(), 2);

	Events.Reset();
	Subsystem->LoadAd(TEXT("TerminalRetry"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		false
	);
	TickCoreTicker();
	TestEqual(TEXT("Terminal failures do not retry"), Provider.LoadCalls, 9);
	TestEqual(TEXT("Terminal failures broadcast immediately"), Events.Num(), 2);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRetryCancellationContractTest,
	"OpenMobile.Ads.Reliability.Retry.Cancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRetryCancellationContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	AddExpectedError(
		TEXT("The current consent decision does not allow ad requests."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	AddExpectedError(
		TEXT("The ads provider was unregistered during an active placement operation."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		false;
	ScopedSettings.Settings->RetryPolicy.MaxRetryAttempts = 2;
	ScopedSettings.Settings->RetryPolicy.InitialDelaySeconds = 0.0;
	ScopedSettings.Settings->RetryPolicy.bUseJitter = false;
	ScopedSettings.Settings->Placements.Reset();
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("CancelRetry"));
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("SuccessRetry"));
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("DestroyRetry"));
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("ConsentRetry"));
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("ShutdownRetry"));

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before retry cancellation checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	const FOpenMobileAdsOperationResult Cancelled =
		Subsystem->LoadAd(TEXT("CancelRetry"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TestTrue(
		TEXT("A pending retry can be cancelled by request ID"),
		Subsystem->CancelRequest(Cancelled.RequestId).bAccepted
	);
	TickCoreTicker();
	TestEqual(TEXT("Cancellation prevents the provider retry"), Provider.LoadCalls, 1);

	const FOpenMobileAdsOperationResult Success =
		Subsystem->LoadAd(TEXT("SuccessRetry"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TickCoreTicker();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = FGuid::NewGuid();
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	TickCoreTicker();
	TestEqual(TEXT("Success stops further retries"), Provider.LoadCalls, 3);
	TestEqual(TEXT("The successful retry keeps the request ID"), Provider.LastLoadRequest.RequestId, Success.RequestId);
	TestTrue(TEXT("A successful retry restores readiness"), Subsystem->IsReady(TEXT("SuccessRetry")));

	Subsystem->LoadAd(TEXT("DestroyRetry"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TestTrue(
		TEXT("Destroy supersedes a pending retry"),
		Subsystem->DestroyAd(TEXT("DestroyRetry")).bAccepted
	);
	TickCoreTicker();
	TestEqual(TEXT("Destroy prevents the provider retry"), Provider.LoadCalls, 4);
	FOpenMobileAdsEvent Destroyed;
	Destroyed.Type = EOpenMobileAdsEventType::Destroyed;
	Provider.DestroySink->Submit(MoveTemp(Destroyed));
	DrainGameThreadTasks();

	Subsystem->LoadAd(TEXT("ConsentRetry"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	FOpenMobileAdsPrivacySnapshot Privacy = Subsystem->GetPrivacySnapshot();
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Denied;
	Privacy.bCanRequestAds = false;
	TestTrue(
		TEXT("Consent can change while a retry is pending"),
		Subsystem->UpdatePrivacySnapshot(MoveTemp(Privacy)).bAccepted
	);
	DrainGameThreadTasks();
	TickCoreTicker();
	TestEqual(TEXT("Consent loss prevents the provider retry"), Provider.LoadCalls, 5);
	TestEqual(
		TEXT("Consent loss terminates the pending retry"),
		Subsystem->GetPlacementStatus(TEXT("ConsentRetry")).LastError.Code,
		EOpenMobileAdsErrorCode::PrivacyBlocked
	);

	Privacy = Subsystem->GetPrivacySnapshot();
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::NotRequired;
	Privacy.bCanRequestAds = true;
	Subsystem->UpdatePrivacySnapshot(MoveTemp(Privacy));
	Subsystem->LoadAd(TEXT("ShutdownRetry"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	Subsystem->Deinitialize();
	TickCoreTicker();
	TestEqual(TEXT("Shutdown prevents the provider retry"), Provider.LoadCalls, 6);

	Registration.Unregister();
	FMockProvider ReplacementProvider(TEXT("MockAds"));
	FScopedProviderRegistration ReplacementRegistration(ReplacementProvider);
	UOpenMobileAdsSubsystem* ReplacementSubsystem =
		NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	TestTrue(
		TEXT("A replacement provider initializes for provider-loss checks"),
		InitializeSuccessfully(*ReplacementSubsystem, ReplacementProvider)
	);
	ReplacementSubsystem->LoadAd(TEXT("CancelRetry"));
	SubmitLoadFailure(
		ReplacementProvider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	ReplacementRegistration.Unregister();
	DrainGameThreadTasks();
	TickCoreTicker();
	TestEqual(
		TEXT("Provider loss prevents the provider retry"),
		ReplacementProvider.LoadCalls,
		1
	);
	TestEqual(
		TEXT("Provider loss terminates the pending retry"),
		ReplacementSubsystem->GetPlacementStatus(TEXT("CancelRetry")).LastError.Code,
		EOpenMobileAdsErrorCode::ProviderUnavailable
	);
	ReplacementSubsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsBackoffSchedulingContractTest,
	"OpenMobile.Ads.Reliability.Backoff.Scheduling",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsBackoffSchedulingContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		false;
	ScopedSettings.Settings->RetryPolicy.MaxRetryAttempts = 3;
	ScopedSettings.Settings->RetryPolicy.InitialDelaySeconds = 2.0;
	ScopedSettings.Settings->RetryPolicy.BackoffMultiplier = 2.0;
	ScopedSettings.Settings->RetryPolicy.MaxDelaySeconds = 5.0;
	ScopedSettings.Settings->RetryPolicy.bUseJitter = true;
	ScopedSettings.Settings->Placements.Reset();
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("BackoffA"));
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("BackoffB"));

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	const TSharedRef<FControlledRetryScheduler> Scheduler =
		MakeShared<FControlledRetryScheduler>();
	const TSharedRef<FControlledRetryRandomSource> Random =
		MakeShared<FControlledRetryRandomSource>();
	Random->NextValue = 1.0;
	FOpenMobileAdsRetryTestAccess::SetDependencies(
		*Subsystem,
		Scheduler,
		Random
	);
	TestTrue(
		TEXT("The provider initializes with controlled retry timing"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	const FOpenMobileAdsOperationResult First =
		Subsystem->LoadAd(TEXT("BackoffA"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TestEqual(
		TEXT("The first failure uses the initial delay"),
		Scheduler->ScheduledDelays.Last(),
		2.0
	);
	Scheduler->AdvanceBy(1.999);
	DrainGameThreadTasks();
	TestEqual(TEXT("The retry waits for the exact boundary"), Provider.LoadCalls, 1);
	Scheduler->AdvanceBy(0.001);
	DrainGameThreadTasks();
	TestEqual(TEXT("The retry starts at the delay boundary"), Provider.LoadCalls, 2);

	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TestEqual(
		TEXT("The second failure applies exponential backoff"),
		Scheduler->ScheduledDelays.Last(),
		4.0
	);

	Subsystem->LoadAd(TEXT("BackoffB"));
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TestEqual(
		TEXT("Another placement starts with an independent delay"),
		Scheduler->ScheduledDelays.Last(),
		2.0
	);
	TestEqual(TEXT("Both placements have pending work"), Scheduler->NumPending(), 2);
	TestTrue(
		TEXT("Cancelling one placement removes only its timer"),
		Subsystem->CancelRequest(First.RequestId).bAccepted
	);
	DrainGameThreadTasks();
	TestEqual(TEXT("One placement remains scheduled"), Scheduler->NumPending(), 1);

	Scheduler->AdvanceBy(1.999);
	DrainGameThreadTasks();
	TestEqual(TEXT("The independent retry also respects its boundary"), Provider.LoadCalls, 3);
	Scheduler->AdvanceBy(0.001);
	DrainGameThreadTasks();
	TestEqual(TEXT("The independent retry reaches the provider"), Provider.LoadCalls, 4);
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = FGuid::NewGuid();
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	TestTrue(TEXT("The independent placement becomes ready"), Subsystem->IsReady(TEXT("BackoffB")));

	Random->NextValue = 0.0;
	FOpenMobileAdsLoadOptions ReloadOptions;
	ReloadOptions.bForceReload = true;
	Subsystem->LoadAd(TEXT("BackoffB"), ReloadOptions);
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TestEqual(
		TEXT("A new load resets the backoff before minimum jitter"),
		Scheduler->ScheduledDelays.Last(),
		1.0
	);
	Subsystem->Deinitialize();
	TestEqual(TEXT("Shutdown cancels controlled retry work"), Scheduler->NumPending(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsLoadPolicyContractTest,
	"OpenMobile.Ads.ProviderContract.Load.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsLoadPolicyContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent = true;
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ContinueReward");
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");
	FOpenMobileAdsPlacementSettings& Disabled =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Disabled.Placement = TEXT("DisabledReward");
	Disabled.bEnabled = false;
	Disabled.Android.AdUnitId = TEXT("android-disabled-unit");
	Disabled.IOS.AdUnitId = TEXT("ios-disabled-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before placement operations"),
		InitializeSuccessfully(*Subsystem, Provider, false)
	);

	const FOpenMobileAdsOperationResult PrivacyBlocked =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestFalse(TEXT("Unknown consent rejects the load"), PrivacyBlocked.bAccepted);
	TestEqual(
		TEXT("Privacy rejection is typed"),
		PrivacyBlocked.Error.Code,
		EOpenMobileAdsErrorCode::PrivacyBlocked
	);
	TestFalse(TEXT("Rejected loads have no request ID"), PrivacyBlocked.RequestId.IsValid());
	TestEqual(TEXT("Privacy rejection does not reach the provider"), Provider.LoadCalls, 0);
	TestEqual(
		TEXT("Disabled placement wins before privacy policy"),
		Subsystem->LoadAd(TEXT("DisabledReward")).Error.Code,
		EOpenMobileAdsErrorCode::DisabledPlacement
	);

	FOpenMobileAdsPrivacySnapshot Privacy;
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Privacy.bCanRequestAds = true;
	Privacy.Source = TEXT("MockConsent");
	Subsystem->UpdatePrivacySnapshot(Privacy);

	const FOpenMobileAdsOperationResult First =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestTrue(TEXT("An allowed named placement starts loading"), First.bAccepted);
	TestTrue(TEXT("An accepted load returns a request ID"), First.RequestId.IsValid());
	TestEqual(TEXT("The provider receives one load"), Provider.LoadCalls, 1);
	TestEqual(
		TEXT("The provider receives the named placement"),
		Provider.LastLoadRequest.Placement.Placement,
		FName(TEXT("ContinueReward"))
	);

	const FOpenMobileAdsOperationResult LoadingDuplicate =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestFalse(TEXT("A duplicate in-flight load is rejected"), LoadingDuplicate.bAccepted);
	TestEqual(
		TEXT("An in-flight duplicate is busy"),
		LoadingDuplicate.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);
	TestEqual(TEXT("A duplicate does not call the provider"), Provider.LoadCalls, 1);

	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = FGuid::NewGuid();
	Provider.LoadSink->Submit(Loaded);
	DrainGameThreadTasks();
	const FOpenMobileAdsOperationResult CachedDuplicate =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestFalse(TEXT("A cached placement is not loaded again by default"), CachedDuplicate.bAccepted);
	TestEqual(
		TEXT("A cached duplicate is busy"),
		CachedDuplicate.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);

	FOpenMobileAdsLoadOptions ForceReload;
	ForceReload.bForceReload = true;
	const FOpenMobileAdsOperationResult Forced =
		Subsystem->LoadAd(TEXT("ContinueReward"), ForceReload);
	TestTrue(TEXT("Force reload replaces the ready load operation"), Forced.bAccepted);
	TestNotEqual(TEXT("Force reload gets a new request ID"), Forced.RequestId, First.RequestId);
	TestEqual(TEXT("Force reload reaches the provider once"), Provider.LoadCalls, 2);
	FOpenMobileAdsEvent ReplacementFailed;
	ReplacementFailed.Type = EOpenMobileAdsEventType::LoadFailed;
	ReplacementFailed.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Load,
		TEXT("ContinueReward"),
		TEXT("Replacement load failed.")
	);
	AddExpectedError(
		TEXT("Replacement load failed."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.LoadSink->Submit(MoveTemp(ReplacementFailed));
	DrainGameThreadTasks();
	TestTrue(
		TEXT("A failed force reload preserves the previously ready ad"),
		Subsystem->IsReady(TEXT("ContinueReward"))
	);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsReloadContractTest,
	"OpenMobile.Ads.ProviderContract.Reload.States",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsReloadContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent = false;
	ScopedSettings.Settings->RetryPolicy.MaxRetryAttempts = 1;
	ScopedSettings.Settings->RetryPolicy.InitialDelaySeconds = 0.0;
	ScopedSettings.Settings->RetryPolicy.MaxDelaySeconds = 0.0;
	ScopedSettings.Settings->RetryPolicy.bUseJitter = false;
	ScopedSettings.Settings->Placements.Reset();
	AddRewardedPlacement(*ScopedSettings.Settings, TEXT("ReloadReward"));

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	const TSharedRef<FControlledRetryScheduler> Scheduler =
		MakeShared<FControlledRetryScheduler>();
	const TSharedRef<FControlledRetryRandomSource> Random =
		MakeShared<FControlledRetryRandomSource>();
	FOpenMobileAdsRetryTestAccess::SetDependencies(
		*Subsystem,
		Scheduler,
		Random
	);
	TestTrue(
		TEXT("The provider initializes before reload checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	const FOpenMobileAdsOperationResult IdleReload =
		Subsystem->ReloadAd(TEXT("ReloadReward"));
	TestTrue(TEXT("Reload starts from idle"), IdleReload.bAccepted);
	TestTrue(
		TEXT("Reload reaches the provider as an explicit replacement"),
		Provider.LastLoadRequest.Options.bForceReload
	);
	const FGuid FirstCachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent FirstLoaded;
	FirstLoaded.Type = EOpenMobileAdsEventType::Loaded;
	FirstLoaded.CachedAdId = FirstCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(FirstLoaded));
	DrainGameThreadTasks();
	TestTrue(TEXT("Idle reload can become ready"), Subsystem->IsReady(TEXT("ReloadReward")));

	const FOpenMobileAdsOperationResult ReadyReload =
		Subsystem->ReloadAd(TEXT("ReloadReward"));
	TestTrue(TEXT("Reload replaces a ready cache"), ReadyReload.bAccepted);
	TestEqual(
		TEXT("The ready cache remains selected while its replacement loads"),
		Subsystem->GetPlacementStatus(TEXT("ReloadReward")).CachedAdId,
		FirstCachedAdId
	);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe>
		SupersededSink = Provider.LoadSink;
	const FOpenMobileAdsOperationResult LoadingReload =
		Subsystem->ReloadAd(TEXT("ReloadReward"));
	TestTrue(TEXT("Reload replaces an in-flight load"), LoadingReload.bAccepted);
	TestNotEqual(
		TEXT("The replacement owns a new public request ID"),
		LoadingReload.RequestId,
		ReadyReload.RequestId
	);
	TestTrue(
		TEXT("The provider cancels the superseded native load"),
		Provider.CancelledRequests.Contains(ReadyReload.RequestId)
	);

	if (SupersededSink)
	{
		FOpenMobileAdsEvent StaleLoaded;
		StaleLoaded.Type = EOpenMobileAdsEventType::Loaded;
		StaleLoaded.CachedAdId = FGuid::NewGuid();
		SupersededSink->Submit(MoveTemp(StaleLoaded));
	}
	DrainGameThreadTasks();
	const FOpenMobileAdsPlacementStatus AfterStaleCallback =
		Subsystem->GetPlacementStatus(TEXT("ReloadReward"));
	TestEqual(
		TEXT("A stale load cannot replace the active request"),
		AfterStaleCallback.ActiveRequestId,
		LoadingReload.RequestId
	);
	TestEqual(
		TEXT("A stale load cannot replace the preserved cache"),
		AfterStaleCallback.CachedAdId,
		FirstCachedAdId
	);

	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NativeFailure,
		true
	);
	TestEqual(TEXT("Reload uses the normal retry scheduler"), Scheduler->NumPending(), 1);
	Scheduler->AdvanceBy(0.0);
	DrainGameThreadTasks();
	TestEqual(
		TEXT("A reload retry keeps the replacement request ID"),
		Provider.LastLoadRequest.RequestId,
		LoadingReload.RequestId
	);
	const FGuid ReplacementCachedAdId = FGuid::NewGuid();
	const FDateTime ReplacementExpiresAt =
		FDateTime::UtcNow() + FTimespan::FromMinutes(30.0);
	FOpenMobileAdsEvent ReplacementLoaded;
	ReplacementLoaded.Type = EOpenMobileAdsEventType::Loaded;
	ReplacementLoaded.CachedAdId = ReplacementCachedAdId;
	ReplacementLoaded.CacheExpiresAt = ReplacementExpiresAt;
	Provider.LoadSink->Submit(MoveTemp(ReplacementLoaded));
	DrainGameThreadTasks();
	const FOpenMobileAdsPlacementStatus ReplacementStatus =
		Subsystem->GetPlacementStatus(TEXT("ReloadReward"));
	TestEqual(
		TEXT("Reload applies the normal cache identity policy"),
		ReplacementStatus.CachedAdId,
		ReplacementCachedAdId
	);
	TestEqual(
		TEXT("Reload applies the normal expiration policy"),
		ReplacementStatus.ExpiresAt,
		ReplacementExpiresAt
	);
	TestEqual(TEXT("Successful reload releases the prior cache"), Provider.ReleasedCachedAds.Num(), 1);

	const FOpenMobileAdsOperationResult Show =
		Subsystem->ShowAd(TEXT("ReloadReward"));
	TestTrue(TEXT("The reloaded cache can begin showing"), Show.bAccepted);
	const int32 LoadsBeforeVisibleReload = Provider.LoadCalls;
	const FOpenMobileAdsOperationResult VisibleReload =
		Subsystem->ReloadAd(TEXT("ReloadReward"));
	TestFalse(TEXT("Reload does not interrupt a visible ad"), VisibleReload.bAccepted);
	TestEqual(TEXT("Visible reload is busy"), VisibleReload.Error.Code, EOpenMobileAdsErrorCode::Busy);
	TestEqual(TEXT("Visible reload does not reach the provider"), Provider.LoadCalls, LoadsBeforeVisibleReload);
	FOpenMobileAdsEvent Dismissed;
	Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
	Provider.ShowSink->Submit(MoveTemp(Dismissed));
	DrainGameThreadTasks();

	const FOpenMobileAdsOperationResult FailureReload =
		Subsystem->ReloadAd(TEXT("ReloadReward"));
	TestTrue(TEXT("Reload starts after the visible ad closes"), FailureReload.bAccepted);
	AddExpectedError(
		TEXT("The mock load attempt failed."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	SubmitLoadFailure(
		Provider,
		EOpenMobileAdsErrorCode::NotConfigured,
		false
	);
	TestEqual(
		TEXT("A terminal reload failure enters failed state"),
		Subsystem->GetPlacementStatus(TEXT("ReloadReward")).State,
		EOpenMobileAdPlacementState::Failed
	);
	TestTrue(
		TEXT("Reload starts from failed state"),
		Subsystem->ReloadAd(TEXT("ReloadReward")).bAccepted
	);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsReadinessPolicyContractTest,
	"OpenMobile.Ads.ProviderContract.Readiness.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsReadinessPolicyContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	ScopedSettings.Settings->ConvenienceRewardedPlacement = NAME_None;
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ReadyReward");
	Placement.Android.AdUnitId = TEXT("android-ready-unit");
	Placement.IOS.AdUnitId = TEXT("ios-ready-unit");
	FOpenMobileAdsPlacementSettings& DisabledPlacement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	DisabledPlacement.Placement = TEXT("DisabledReward");
	DisabledPlacement.bEnabled = false;
	DisabledPlacement.Android.AdUnitId = TEXT("android-disabled-unit");
	DisabledPlacement.IOS.AdUnitId = TEXT("ios-disabled-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestFalse(TEXT("An unloaded placement is not ready"), Subsystem->IsReady(TEXT("ReadyReward")));
	TestEqual(
		TEXT("Unknown placement precedes initialization"),
		Subsystem->CanShow(TEXT("MissingReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::UnknownPlacement
	);
	TestEqual(
		TEXT("Disabled placement precedes initialization"),
		Subsystem->CanShow(TEXT("DisabledReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::Disabled
	);
	TestEqual(
		TEXT("Initialization blocks before cache state"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::NotInitialized
	);
	TestTrue(
		TEXT("The provider initializes for readiness checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	Provider.bSupported = false;
	TestEqual(
		TEXT("An unavailable provider blocks before cache state"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::ProviderUnavailable
	);
	Provider.bSupported = true;
	Provider.Capabilities.Formats[0].bCanShow = false;
	TestEqual(
		TEXT("Unsupported show format blocks before cache state"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::UnsupportedFormat
	);
	Provider.Capabilities.Formats[0].bCanShow = true;
	TestEqual(
		TEXT("An eligible but unloaded placement reports not loaded"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::NotLoaded
	);

	TestTrue(
		TEXT("The placement starts loading"),
		Subsystem->LoadAd(TEXT("ReadyReward")).bAccepted
	);
	FOpenMobileAdsPrivacySnapshot Privacy = Subsystem->GetPrivacySnapshot();
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Required;
	Privacy.bCanRequestAds = false;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	TestFalse(TEXT("Loading does not count as ready"), Subsystem->IsReady(TEXT("ReadyReward")));
	TestEqual(
		TEXT("Privacy policy wins while loading"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::PrivacyBlocked
	);

	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Privacy.bCanRequestAds = true;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	TestEqual(
		TEXT("Loading is reported after privacy allows ads"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::Loading
	);

	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = FGuid::NewGuid();
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	TestTrue(TEXT("A valid cached ad is ready"), Subsystem->IsReady(TEXT("ReadyReward")));
	TestTrue(TEXT("An eligible cached ad can show"), Subsystem->CanShow(TEXT("ReadyReward")).bCanShow);

	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Denied;
	Privacy.bCanRequestAds = false;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	TestTrue(TEXT("Privacy does not change cached readiness"), Subsystem->IsReady(TEXT("ReadyReward")));
	TestEqual(
		TEXT("Privacy blocks an otherwise ready ad"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::PrivacyBlocked
	);

	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Privacy.bCanRequestAds = true;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	const ENetworkConnectionType PreviousConnectionType =
		FPlatformMisc::GetNetworkConnectionType();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(ENetworkConnectionType::None);
	TestEqual(
		TEXT("No platform connection blocks showing"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::Offline
	);
	TestTrue(TEXT("Connectivity does not change cached readiness"), Subsystem->IsReady(TEXT("ReadyReward")));
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(ENetworkConnectionType::Unknown);
	TestTrue(TEXT("Unknown connectivity stays eligible"), Subsystem->CanShow(TEXT("ReadyReward")).bCanShow);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	const FOpenMobileAdsPlacementStatus StatusBeforeQueries =
		Subsystem->GetPlacementStatus(TEXT("ReadyReward"));
	TestEqual(
		TEXT("Background state blocks showing"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::LifecycleConflict
	);
	TestEqual(
		TEXT("Repeated background queries keep the same decision"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::LifecycleConflict
	);
	const FOpenMobileAdsPlacementStatus StatusAfterQueries =
		Subsystem->GetPlacementStatus(TEXT("ReadyReward"));
	TestEqual(
		TEXT("CanShow does not change placement state"),
		StatusAfterQueries.State,
		StatusBeforeQueries.State
	);
	TestEqual(
		TEXT("CanShow does not change cache identity"),
		StatusAfterQueries.CachedAdId,
		StatusBeforeQueries.CachedAdId
	);
	TestEqual(
		TEXT("CanShow does not change cache expiration"),
		StatusAfterQueries.ExpiresAt,
		StatusBeforeQueries.ExpiresAt
	);
	TestTrue(TEXT("Lifecycle policy does not change cached readiness"), Subsystem->IsReady(TEXT("ReadyReward")));
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	TestTrue(TEXT("Foreground state restores eligibility"), Subsystem->CanShow(TEXT("ReadyReward")).bCanShow);
	FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
	TestEqual(
		TEXT("Inactive state blocks showing"),
		Subsystem->CanShow(TEXT("ReadyReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::LifecycleConflict
	);
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	TestTrue(TEXT("Reactivation restores eligibility"), Subsystem->CanShow(TEXT("ReadyReward")).bCanShow);

	Subsystem->Deinitialize();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(PreviousConnectionType);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCanShowDecisionContractTest,
	"OpenMobile.Ads.ProviderContract.Readiness.Decision",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCanShowDecisionContractTest::RunTest(const FString& Parameters)
{
	auto MakeEligibleContext = []()
	{
		FOpenMobileAdsCanShowPolicyContext Context;
		Context.bPlacementConfigured = true;
		Context.bPlacementEnabled = true;
		Context.ServiceState = EOpenMobileAdsServiceState::Ready;
		Context.bProviderAvailable = true;
		Context.bFormatSupported = true;
		Context.bPrivacyAllowed = true;
		Context.PlacementState = EOpenMobileAdPlacementState::Ready;
		Context.bHasCachedAd = true;
		return Context;
	};
	auto TestReason = [this](
		const TCHAR* What,
		const FOpenMobileAdsCanShowPolicyContext& Context,
		EOpenMobileAdsCanShowBlockReason Expected
	)
	{
		const FOpenMobileAdsCanShowResult Result =
			FOpenMobileAdsCanShowPolicy::Evaluate(Context);
		TestEqual(What, Result.BlockReason, Expected);
		TestFalse(TEXT("A blocked decision cannot show"), Result.bCanShow);
		TestFalse(TEXT("A blocked decision includes an explanation"), Result.Explanation.IsEmpty());
	};

	FOpenMobileAdsCanShowPolicyContext Context = MakeEligibleContext();
	Context.bPlacementConfigured = false;
	Context.bPlacementEnabled = false;
	Context.ServiceState = EOpenMobileAdsServiceState::Uninitialized;
	Context.bProviderAvailable = false;
	Context.bFormatSupported = false;
	Context.bPrivacyAllowed = false;
	Context.PlacementState = EOpenMobileAdPlacementState::Loading;
	Context.bHasCachedAd = false;
	Context.bExpired = true;
	Context.bFrequencyCapped = true;
	Context.bCooldownActive = true;
	Context.bOffline = true;
	Context.bLifecycleConflict = true;
	TestReason(
		TEXT("Unknown placement has highest precedence"),
		Context,
		EOpenMobileAdsCanShowBlockReason::UnknownPlacement
	);

	Context = MakeEligibleContext();
	Context.bPlacementEnabled = false;
	TestReason(TEXT("Disabled placement is typed"), Context, EOpenMobileAdsCanShowBlockReason::Disabled);
	Context = MakeEligibleContext();
	Context.ServiceState = EOpenMobileAdsServiceState::Initializing;
	Context.bPrivacyAllowed = false;
	Context.PlacementState = EOpenMobileAdPlacementState::Loading;
	TestReason(
		TEXT("Initialization precedes privacy and loading"),
		Context,
		EOpenMobileAdsCanShowBlockReason::NotInitialized
	);
	Context = MakeEligibleContext();
	Context.bProviderAvailable = false;
	TestReason(
		TEXT("Provider availability is typed"),
		Context,
		EOpenMobileAdsCanShowBlockReason::ProviderUnavailable
	);
	Context = MakeEligibleContext();
	Context.bFormatSupported = false;
	TestReason(
		TEXT("Unsupported format is typed"),
		Context,
		EOpenMobileAdsCanShowBlockReason::UnsupportedFormat
	);
	Context = MakeEligibleContext();
	Context.bPrivacyAllowed = false;
	Context.PlacementState = EOpenMobileAdPlacementState::Loading;
	TestReason(
		TEXT("Privacy precedes loading"),
		Context,
		EOpenMobileAdsCanShowBlockReason::PrivacyBlocked
	);
	Context = MakeEligibleContext();
	Context.PlacementState = EOpenMobileAdPlacementState::Loading;
	Context.bHasCachedAd = false;
	TestReason(TEXT("Loading is typed"), Context, EOpenMobileAdsCanShowBlockReason::Loading);
	Context = MakeEligibleContext();
	Context.bHasCachedAd = false;
	TestReason(TEXT("Missing cache is typed"), Context, EOpenMobileAdsCanShowBlockReason::NotLoaded);
	Context = MakeEligibleContext();
	Context.bExpired = true;
	Context.bFrequencyCapped = true;
	TestReason(
		TEXT("Expiration precedes pacing policy"),
		Context,
		EOpenMobileAdsCanShowBlockReason::Expired
	);

	const FDateTime CapEndsAt(2030, 1, 2);
	Context = MakeEligibleContext();
	Context.bFrequencyCapped = true;
	Context.bCooldownActive = true;
	Context.bOffline = true;
	Context.bLifecycleConflict = true;
	Context.FrequencyCapEndsAt = CapEndsAt;
	FOpenMobileAdsCanShowResult Result = FOpenMobileAdsCanShowPolicy::Evaluate(Context);
	TestEqual(
		TEXT("Frequency cap precedes cooldown and runtime state"),
		Result.BlockReason,
		EOpenMobileAdsCanShowBlockReason::FrequencyCap
	);
	TestEqual(TEXT("Frequency cap returns its next eligible time"), Result.NextEligibleAt, CapEndsAt);

	const FDateTime CooldownEndsAt(2030, 1, 3);
	Context = MakeEligibleContext();
	Context.bCooldownActive = true;
	Context.bOffline = true;
	Context.bLifecycleConflict = true;
	Context.CooldownEndsAt = CooldownEndsAt;
	Result = FOpenMobileAdsCanShowPolicy::Evaluate(Context);
	TestEqual(
		TEXT("Cooldown precedes connectivity and lifecycle"),
		Result.BlockReason,
		EOpenMobileAdsCanShowBlockReason::Cooldown
	);
	TestEqual(TEXT("Cooldown returns its next eligible time"), Result.NextEligibleAt, CooldownEndsAt);

	Context = MakeEligibleContext();
	Context.bOffline = true;
	Context.bLifecycleConflict = true;
	TestReason(
		TEXT("Connectivity precedes lifecycle"),
		Context,
		EOpenMobileAdsCanShowBlockReason::Offline
	);
	Context = MakeEligibleContext();
	Context.bLifecycleConflict = true;
	TestReason(
		TEXT("Lifecycle conflict is typed"),
		Context,
		EOpenMobileAdsCanShowBlockReason::LifecycleConflict
	);

	Result = FOpenMobileAdsCanShowPolicy::Evaluate(MakeEligibleContext());
	TestTrue(TEXT("An eligible decision can show"), Result.bCanShow);
	TestEqual(
		TEXT("An eligible decision has no block reason"),
		Result.BlockReason,
		EOpenMobileAdsCanShowBlockReason::None
	);
	TestTrue(TEXT("An eligible decision has no explanation"), Result.Explanation.IsEmpty());
	TestEqual(TEXT("An eligible decision has no pacing deadline"), Result.NextEligibleAt, FDateTime());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsShowPolicyContractTest,
	"OpenMobile.Ads.ProviderContract.Show.Policy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsShowPolicyContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ShowReward");
	Placement.Android.AdUnitId = TEXT("android-show-unit");
	Placement.IOS.AdUnitId = TEXT("ios-show-unit");
	FOpenMobileAdsPlacementSettings& OtherPlacement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	OtherPlacement.Placement = TEXT("OtherReward");
	OtherPlacement.Android.AdUnitId = TEXT("android-other-unit");
	OtherPlacement.IOS.AdUnitId = TEXT("ios-other-unit");
	FOpenMobileAdsPlacementSettings& StalePlacement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	StalePlacement.Placement = TEXT("StaleReward");
	StalePlacement.Android.AdUnitId = TEXT("android-stale-unit");
	StalePlacement.IOS.AdUnitId = TEXT("ios-stale-unit");

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].CacheLifetimeSeconds = 60.0;
	FOpenMobileAdFormatCapabilities Interstitial;
	Interstitial.Format = EOpenMobileAdFormat::Interstitial;
	Interstitial.bCanShow = true;
	Provider.Capabilities.Formats.Add(Interstitial);
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before show checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	auto LoadPlacement = [this, Subsystem, &Provider](
		FName PlacementName,
		FGuid CachedAdId,
		FDateTime Timestamp = FDateTime()
	)
	{
		TestTrue(TEXT("The placement starts loading"), Subsystem->LoadAd(PlacementName).bAccepted);
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = CachedAdId;
		Loaded.Timestamp = Timestamp;
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
	};
	const FGuid ShowCachedAdId = FGuid::NewGuid();
	const FGuid OtherCachedAdId = FGuid::NewGuid();
	LoadPlacement(TEXT("ShowReward"), ShowCachedAdId);
	LoadPlacement(TEXT("OtherReward"), OtherCachedAdId);
	TestTrue(TEXT("The loaded placement is ready"), Subsystem->IsReady(TEXT("ShowReward")));

	FOpenMobileAdsPrivacySnapshot Privacy = Subsystem->GetPrivacySnapshot();
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Denied;
	Privacy.bCanRequestAds = false;
	Subsystem->UpdatePrivacySnapshot(MoveTemp(Privacy));
	const FOpenMobileAdsOperationResult Rejected = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Revoked consent rejects showing"), Rejected.bAccepted);
	TestEqual(
		TEXT("The show-time privacy rejection is typed"),
		Rejected.Error.Code,
		EOpenMobileAdsErrorCode::PrivacyBlocked
	);
	TestEqual(TEXT("A policy rejection does not reach the provider"), Provider.ShowCalls, 0);
	TestTrue(TEXT("A policy rejection preserves the ready cache"), Subsystem->IsReady(TEXT("ShowReward")));
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Privacy.bCanRequestAds = true;
	Subsystem->UpdatePrivacySnapshot(MoveTemp(Privacy));

	const ENetworkConnectionType PreviousConnectionType =
		FPlatformMisc::GetNetworkConnectionType();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(ENetworkConnectionType::AirplaneMode);
	const FOpenMobileAdsOperationResult Offline = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Offline state rejects showing"), Offline.bAccepted);
	TestEqual(TEXT("Offline rejection is typed"), Offline.Error.Code, EOpenMobileAdsErrorCode::Offline);
	TestEqual(TEXT("Offline rejection does not reach the provider"), Provider.ShowCalls, 0);
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(ENetworkConnectionType::Unknown);

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	const FOpenMobileAdsOperationResult Background = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Background state rejects showing"), Background.bAccepted);
	TestEqual(TEXT("Lifecycle rejection is typed"), Background.Error.Code, EOpenMobileAdsErrorCode::InvalidState);
	TestEqual(TEXT("Lifecycle rejection does not reach the provider"), Provider.ShowCalls, 0);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	Provider.bSupported = false;
	const FOpenMobileAdsOperationResult ProviderUnavailable =
		Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Provider availability is rechecked at show time"), ProviderUnavailable.bAccepted);
	TestEqual(
		TEXT("Provider availability rejection is typed"),
		ProviderUnavailable.Error.Code,
		EOpenMobileAdsErrorCode::UnsupportedPlatform
	);
	TestEqual(TEXT("Unavailable provider does not receive a show"), Provider.ShowCalls, 0);
	TestTrue(TEXT("Provider availability rejection preserves the cache"), Subsystem->IsReady(TEXT("ShowReward")));
	Provider.bSupported = true;

	ScopedSettings.Settings->Placements[0].Format = EOpenMobileAdFormat::Interstitial;
	const FOpenMobileAdsOperationResult WrongFormat = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("A cache for the old placement format is rejected"), WrongFormat.bAccepted);
	TestEqual(TEXT("Wrong-format cache rejection is typed"), WrongFormat.Error.Code, EOpenMobileAdsErrorCode::NotReady);
	TestEqual(TEXT("Wrong-format cache does not reach the provider"), Provider.ShowCalls, 0);
	TestTrue(TEXT("Wrong-format rejection preserves the cached ad"), Subsystem->IsReady(TEXT("ShowReward")));
	ScopedSettings.Settings->Placements[0].Format = EOpenMobileAdFormat::Rewarded;

	LoadPlacement(
		TEXT("StaleReward"),
		FGuid::NewGuid(),
		FDateTime::UtcNow() - FTimespan::FromSeconds(120.0)
	);
	const FOpenMobileAdsOperationResult Stale = Subsystem->ShowAd(TEXT("StaleReward"));
	TestFalse(TEXT("An expired cache is rejected at show time"), Stale.bAccepted);
	TestEqual(TEXT("Expired cache rejection is typed"), Stale.Error.Code, EOpenMobileAdsErrorCode::NotReady);
	TestEqual(TEXT("Expired cache does not reach the provider"), Provider.ShowCalls, 0);

	TArray<FOpenMobileAdsEvent> ShowEvents;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&ShowEvents](const FOpenMobileAdsEvent& Event)
		{
			ShowEvents.Add(Event);
		}
	);
	Provider.bAcceptShow = false;
	Provider.ShowRejection = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Show,
		TEXT("ShowReward"),
		TEXT("The mock provider rejected presentation."),
		Provider.Name
	);
	const FOpenMobileAdsOperationResult ProviderRejected =
		Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("Provider rejection is immediate"), ProviderRejected.bAccepted);
	TestEqual(
		TEXT("Provider rejection preserves its typed error"),
		ProviderRejected.Error.Code,
		EOpenMobileAdsErrorCode::ProviderFailure
	);
	TestFalse(TEXT("Immediate rejection has no request ID"), ProviderRejected.RequestId.IsValid());
	TestEqual(TEXT("Provider rejection attempts one presentation"), Provider.ShowCalls, 1);
	TestTrue(TEXT("Provider rejection restores the ready cache"), Subsystem->IsReady(TEXT("ShowReward")));
	DrainGameThreadTasks();
	TestTrue(TEXT("Provider rejection does not emit an asynchronous event"), ShowEvents.IsEmpty());

	Provider.bAcceptShow = true;
	const FOpenMobileAdsOperationResult Accepted = Subsystem->ShowAd(TEXT("ShowReward"));
	TestTrue(TEXT("An eligible ready ad is accepted once"), Accepted.bAccepted);
	TestTrue(TEXT("An accepted show has a request ID"), Accepted.RequestId.IsValid());
	TestEqual(TEXT("The provider receives the accepted request ID"), Provider.LastShowRequest.RequestId, Accepted.RequestId);
	TestEqual(TEXT("The provider receives the requested placement"), Provider.LastShowRequest.Placement, FName(TEXT("ShowReward")));
	TestEqual(TEXT("The provider receives the cached ad identity"), Provider.LastShowRequest.CachedAdId, ShowCachedAdId);
	TestEqual(TEXT("The provider receives the cached ad format"), Provider.LastShowRequest.Format, EOpenMobileAdFormat::Rewarded);
	TestFalse(TEXT("An accepted ad is no longer ready for reuse"), Subsystem->IsReady(TEXT("ShowReward")));
	const FOpenMobileAdsOperationResult Reused = Subsystem->ShowAd(TEXT("ShowReward"));
	TestFalse(TEXT("The same accepted cached ad cannot show twice"), Reused.bAccepted);
	TestEqual(TEXT("A reused cache rejection is typed"), Reused.Error.Code, EOpenMobileAdsErrorCode::NotReady);
	TestEqual(TEXT("A reused cache is not submitted twice"), Provider.ShowCalls, 2);

	const FOpenMobileAdsOperationResult Concurrent = Subsystem->ShowAd(TEXT("OtherReward"));
	TestFalse(TEXT("A concurrent full-screen show is rejected"), Concurrent.bAccepted);
	TestEqual(TEXT("Concurrent show rejection is typed"), Concurrent.Error.Code, EOpenMobileAdsErrorCode::InvalidState);
	TestEqual(TEXT("Concurrent rejection does not reach the provider"), Provider.ShowCalls, 2);
	TestTrue(TEXT("Concurrent rejection preserves the other cache"), Subsystem->IsReady(TEXT("OtherReward")));
	DrainGameThreadTasks();
	TestEqual(TEXT("Accepted show emits one service event"), ShowEvents.Num(), 1);
	if (ShowEvents.Num() == 1)
	{
		TestEqual(TEXT("The immediate service event is show accepted"), ShowEvents[0].Type, EOpenMobileAdsEventType::ShowAccepted);
	}

	FOpenMobileAdsEvent Failed;
	Failed.Type = EOpenMobileAdsEventType::Failed;
	Failed.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Show,
		TEXT("ShowReward"),
		TEXT("The mock presentation failed asynchronously.")
	);
	AddExpectedError(
		TEXT("The mock presentation failed asynchronously."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.ShowSink->Submit(MoveTemp(Failed));
	DrainGameThreadTasks();
	TestEqual(TEXT("Asynchronous failure follows acceptance"), ShowEvents.Num(), 2);
	if (ShowEvents.Num() == 2)
	{
		TestEqual(TEXT("Presentation failure is asynchronous"), ShowEvents[1].Type, EOpenMobileAdsEventType::Failed);
		TestEqual(TEXT("Presentation failure keeps the request ID"), ShowEvents[1].RequestId, Accepted.RequestId);
		TestEqual(TEXT("Presentation failure keeps the cache ID"), ShowEvents[1].CachedAdId, ShowCachedAdId);
	}
	TestFalse(TEXT("Asynchronous failure consumes the failed cache"), Subsystem->IsReady(TEXT("ShowReward")));

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);

	Subsystem->Deinitialize();
	FCoreDelegates::OnNetworkConnectionChanged.Broadcast(PreviousConnectionType);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsFullscreenLifecycleContractTest,
	"OpenMobile.Ads.ProviderContract.Fullscreen.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsFullscreenLifecycleContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("LifecycleReward");
	Placement.Format = EOpenMobileAdFormat::Rewarded;
	Placement.Android.AdUnitId = TEXT("android-lifecycle-reward");
	Placement.IOS.AdUnitId = TEXT("ios-lifecycle-reward");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before full-screen lifecycle checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	auto LoadReady = [this, Subsystem, &Provider]()
	{
		TestTrue(
			TEXT("The lifecycle placement starts loading"),
			Subsystem->LoadAd(TEXT("LifecycleReward")).bAccepted
		);
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = FGuid::NewGuid();
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
		TestTrue(
			TEXT("The lifecycle placement becomes ready"),
			Subsystem->IsReady(TEXT("LifecycleReward"))
		);
	};
	auto Dismiss = [&Provider]()
	{
		FOpenMobileAdsEvent Dismissed;
		Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
		Provider.ShowSink->Submit(MoveTemp(Dismissed));
	};

	const float PreviousVolume = FApp::GetVolumeMultiplier();
	FApp::SetVolumeMultiplier(0.37f);
	bool bVolumeRestoredBeforeDismiss = false;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&bVolumeRestoredBeforeDismiss](const FOpenMobileAdsEvent& Event)
		{
			if (Event.Type == EOpenMobileAdsEventType::Dismissed)
			{
				bVolumeRestoredBeforeDismiss = FMath::IsNearlyEqual(
					FApp::GetVolumeMultiplier(),
					0.37f
				);
			}
		}
	);

	LoadReady();
	const FOpenMobileAdsOperationResult Show =
		Subsystem->ShowAd(TEXT("LifecycleReward"));
	TestTrue(TEXT("The lifecycle show is accepted"), Show.bAccepted);
	TestTrue(
		TEXT("Accepted full-screen presentation mutes Unreal audio"),
		FMath::IsNearlyZero(FApp::GetVolumeMultiplier())
	);
	FCoreDelegates::ApplicationWillDeactivateDelegate.Broadcast();
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	TestTrue(
		TEXT("Reactivation cannot resume audio over an active ad"),
		FMath::IsNearlyZero(FApp::GetVolumeMultiplier())
	);
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	TestTrue(
		TEXT("Foreground entry cannot resume audio over an active ad"),
		FMath::IsNearlyZero(FApp::GetVolumeMultiplier())
	);
	Dismiss();
	DrainGameThreadTasks();
	TestTrue(
		TEXT("Dismissal restores the prior audio level"),
		FMath::IsNearlyEqual(FApp::GetVolumeMultiplier(), 0.37f)
	);
	TestTrue(
		TEXT("Lifecycle restoration precedes the dismiss event"),
		bVolumeRestoredBeforeDismiss
	);

	FApp::SetVolumeMultiplier(0.0f);
	LoadReady();
	TestTrue(
		TEXT("A show can start while gameplay audio is already muted"),
		Subsystem->ShowAd(TEXT("LifecycleReward")).bAccepted
	);
	Dismiss();
	DrainGameThreadTasks();
	TestTrue(
		TEXT("Dismissal preserves a pre-existing mute"),
		FMath::IsNearlyZero(FApp::GetVolumeMultiplier())
	);

	FApp::SetVolumeMultiplier(0.42f);
	LoadReady();
	const FOpenMobileAdsOperationResult MissingDismiss =
		Subsystem->ShowAd(TEXT("LifecycleReward"));
	TestTrue(TEXT("The missing-dismiss show starts"), MissingDismiss.bAccepted);
	TestTrue(
		TEXT("The missing-dismiss show owns the audio mute"),
		FMath::IsNearlyZero(FApp::GetVolumeMultiplier())
	);
	TestTrue(
		TEXT("Explicit cancellation recovers a missing dismiss"),
		Subsystem->CancelRequest(MissingDismiss.RequestId).bAccepted
	);
	TestTrue(
		TEXT("Cancellation restores only the ads-owned mute"),
		FMath::IsNearlyEqual(FApp::GetVolumeMultiplier(), 0.42f)
	);
	DrainGameThreadTasks();

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	FApp::SetVolumeMultiplier(PreviousVolume);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsFullscreenCoordinatorContractTest,
	"OpenMobile.Ads.ProviderContract.Fullscreen.Coordinator",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsFullscreenCoordinatorContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	TUniquePtr<FMockFullscreenLifecycleTarget> Target =
		MakeUnique<FMockFullscreenLifecycleTarget>();
	FMockFullscreenLifecycleTarget* TargetState = Target.Get();
	FOpenMobileAdsFullscreenLifecycleCoordinator Coordinator(MoveTemp(Target));

	const FGuid ConsentOwner = FGuid::NewGuid();
	TestTrue(
		TEXT("Consent can reserve the shared full-screen surface"),
		Coordinator.TryReserve(
			EOpenMobileAdsFullscreenSurface::Consent,
			ConsentOwner
		)
	);
	TestFalse(
		TEXT("An ad cannot overlap reserved consent UI"),
		Coordinator.TryReserve(
			EOpenMobileAdsFullscreenSurface::Ad,
			FGuid::NewGuid()
		)
	);
	TestFalse(
		TEXT("Another consent form cannot overlap reserved consent UI"),
		Coordinator.TryReserve(
			EOpenMobileAdsFullscreenSurface::Consent,
			FGuid::NewGuid()
		)
	);
	TestFalse(
		TEXT("Inspector UI cannot overlap reserved consent UI"),
		Coordinator.TryReserve(
			EOpenMobileAdsFullscreenSurface::Inspector,
			FGuid::NewGuid()
		)
	);
	TestTrue(
		TEXT("Reserved consent can end before presentation"),
		Coordinator.End(
			EOpenMobileAdsFullscreenSurface::Consent,
			ConsentOwner
		)
	);
	TestEqual(TEXT("Reservation alone does not alter gameplay"), TargetState->ApplyCalls, 0);

	const FGuid AdOwner = FGuid::NewGuid();
	TestTrue(
		TEXT("An ad can reserve the released full-screen surface"),
		Coordinator.TryReserve(EOpenMobileAdsFullscreenSurface::Ad, AdOwner)
	);
	TestTrue(
		TEXT("The owning ad can begin presentation"),
		Coordinator.BeginPresentation(EOpenMobileAdsFullscreenSurface::Ad, AdOwner)
	);
	TestEqual(TEXT("Presentation applies lifecycle state once"), TargetState->ApplyCalls, 1);
	Coordinator.SetApplicationActive(false);
	TestEqual(TEXT("Interruption reapplies lifecycle state"), TargetState->ApplyCalls, 2);
	TestFalse(
		TEXT("A mismatched owner cannot restore another surface"),
		Coordinator.End(
			EOpenMobileAdsFullscreenSurface::Ad,
			FGuid::NewGuid()
		)
	);
	TestTrue(
		TEXT("The owning ad can end while interrupted"),
		Coordinator.End(EOpenMobileAdsFullscreenSurface::Ad, AdOwner)
	);
	TestEqual(TEXT("Ending restores gameplay while interrupted"), TargetState->RestoreGameplayCalls, 1);
	TestEqual(TEXT("Focus waits until the application is active"), TargetState->RestoreFocusCalls, 0);
	TestFalse(
		TEXT("Inactive applications reject new full-screen reservations"),
		Coordinator.TryReserve(
			EOpenMobileAdsFullscreenSurface::Inspector,
			FGuid::NewGuid()
		)
	);
	Coordinator.SetApplicationActive(true);
	TestEqual(TEXT("Reactivation restores deferred focus"), TargetState->RestoreFocusCalls, 1);
	TestFalse(
		TEXT("Duplicate terminal callbacks do not restore twice"),
		Coordinator.End(EOpenMobileAdsFullscreenSurface::Ad, AdOwner)
	);

	const FGuid InspectorOwner = FGuid::NewGuid();
	TestTrue(
		TEXT("Inspector UI can reserve the shared full-screen surface"),
		Coordinator.TryReserve(
			EOpenMobileAdsFullscreenSurface::Inspector,
			InspectorOwner
		)
	);
	TestTrue(
		TEXT("Inspector reservation can end without presentation"),
		Coordinator.End(
			EOpenMobileAdsFullscreenSurface::Inspector,
			InspectorOwner
		)
	);
	TestEqual(TEXT("Unused reservations do not restore gameplay"), TargetState->RestoreGameplayCalls, 1);

	const FGuid TrackingAuthorizationOwner = FGuid::NewGuid();
	TestTrue(
		TEXT("Tracking authorization can reserve the shared full-screen surface"),
		Coordinator.TryReserve(
			EOpenMobileAdsFullscreenSurface::TrackingAuthorization,
			TrackingAuthorizationOwner
		)
	);
	TestFalse(
		TEXT("Consent UI cannot overlap tracking authorization"),
		Coordinator.TryReserve(
			EOpenMobileAdsFullscreenSurface::Consent,
			FGuid::NewGuid()
		)
	);
	TestTrue(
		TEXT("Tracking authorization releases its reservation"),
		Coordinator.End(
			EOpenMobileAdsFullscreenSurface::TrackingAuthorization,
			TrackingAuthorizationOwner
		)
	);

	const FGuid ShutdownOwner = FGuid::NewGuid();
	TestTrue(
		TEXT("A final ad can reserve before teardown"),
		Coordinator.TryReserve(
			EOpenMobileAdsFullscreenSurface::Ad,
			ShutdownOwner
		)
	);
	TestTrue(
		TEXT("A final ad can present before teardown"),
		Coordinator.BeginPresentation(
			EOpenMobileAdsFullscreenSurface::Ad,
			ShutdownOwner
		)
	);
	Coordinator.Shutdown();
	TestFalse(TEXT("Shutdown releases the shared surface"), Coordinator.IsOccupied());
	TestEqual(TEXT("Shutdown restores owned gameplay state"), TargetState->RestoreGameplayCalls, 2);
	TestEqual(TEXT("Shutdown restores captured focus state"), TargetState->RestoreFocusCalls, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRewardedConvenienceContractTest,
	"OpenMobile.Ads.ProviderContract.Rewarded.Convenience",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRewardedConvenienceContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	ScopedSettings.Settings->ConvenienceRewardedPlacement = NAME_None;
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ConvenienceReward");
	Placement.Format = EOpenMobileAdFormat::Rewarded;
	Placement.Android.AdUnitId = TEXT("android-convenience-reward");
	Placement.IOS.AdUnitId = TEXT("ios-convenience-reward");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before convenience checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			if (Event.Placement == TEXT("ConvenienceReward"))
			{
				Events.Add(Event);
			}
		}
	);
	TestTrue(
		TEXT("The rewarded convenience operation starts"),
		Subsystem->RequestAndShowRewardedAd()
	);
	TestEqual(
		TEXT("The convenience operation uses reusable loading"),
		Provider.LoadCalls,
		1
	);
	TestEqual(
		TEXT("The convenience operation bypasses the provider combined API"),
		Provider.LegacyRewardedRequestCalls,
		0
	);

	if (Provider.LoadSink)
	{
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = FGuid::NewGuid();
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("The reusable load advances into one reusable show"),
		Provider.ShowCalls,
		1
	);
	TestEqual(
		TEXT("The convenience operation waits for native presentation"),
		Subsystem->GetState(),
		EOpenMobileRewardedAdState::Loading
	);

	if (Provider.ShowSink)
	{
		FOpenMobileAdsEvent Shown;
		Shown.Type = EOpenMobileAdsEventType::Shown;
		Provider.ShowSink->Submit(MoveTemp(Shown));
		DrainGameThreadTasks();
		TestEqual(
			TEXT("Native presentation updates the convenience state"),
			Subsystem->GetState(),
			EOpenMobileRewardedAdState::Showing
		);

		FOpenMobileAdsEvent Reward;
		Reward.Type = EOpenMobileAdsEventType::RewardEarned;
		Reward.bHasReward = true;
		Reward.Reward.Type = TEXT("coin");
		Reward.Reward.Amount = 5;
		Provider.ShowSink->Submit(Reward);
		Provider.ShowSink->Submit(MoveTemp(Reward));
		FOpenMobileAdsEvent Dismissed;
		Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
		Provider.ShowSink->Submit(MoveTemp(Dismissed));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("Dismissal finishes the convenience operation"),
		Subsystem->GetState(),
		EOpenMobileRewardedAdState::Idle
	);
	TestEqual(
		TEXT("Completed rewarded flow emits one ordered reward"),
		Events.Num(),
		6
	);
	if (Events.Num() == 6)
	{
		const EOpenMobileAdsEventType ExpectedTypes[] = {
			EOpenMobileAdsEventType::LoadStarted,
			EOpenMobileAdsEventType::Loaded,
			EOpenMobileAdsEventType::ShowAccepted,
			EOpenMobileAdsEventType::Shown,
			EOpenMobileAdsEventType::RewardEarned,
			EOpenMobileAdsEventType::Dismissed
		};
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(ExpectedTypes); ++Index)
		{
			TestEqual(
				TEXT("Completed rewarded callbacks retain contract order"),
				Events[Index].Type,
				ExpectedTypes[Index]
			);
		}
	}

	FOpenMobileAdsPlacementSettings& OtherPlacement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	OtherPlacement.Placement = TEXT("OtherReward");
	OtherPlacement.Format = EOpenMobileAdFormat::Rewarded;
	OtherPlacement.Android.AdUnitId = TEXT("android-other-reward");
	OtherPlacement.IOS.AdUnitId = TEXT("ios-other-reward");
	AddExpectedError(
		TEXT("More than one enabled rewarded placement exists."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	TestFalse(
		TEXT("Multiple rewarded placements require an explicit convenience choice"),
		Subsystem->RequestAndShowRewardedAd()
	);
	TestEqual(
		TEXT("Ambiguous convenience selection does not start native loading"),
		Provider.LoadCalls,
		1
	);
	ScopedSettings.Settings->ConvenienceRewardedPlacement =
		TEXT("ConvenienceReward");
	TestTrue(
		TEXT("Configured convenience placement starts among multiple rewards"),
		Subsystem->RequestAndShowRewardedAd()
	);
	TestEqual(
		TEXT("Configured convenience placement is selected deterministically"),
		Provider.LastLoadRequest.Placement.Placement,
		FName(TEXT("ConvenienceReward"))
	);
	if (Provider.LoadSink)
	{
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = FGuid::NewGuid();
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
	}
	if (Provider.ShowSink)
	{
		FOpenMobileAdsEvent Shown;
		Shown.Type = EOpenMobileAdsEventType::Shown;
		Provider.ShowSink->Submit(MoveTemp(Shown));
		FOpenMobileAdsEvent Dismissed;
		Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
		Provider.ShowSink->Submit(MoveTemp(Dismissed));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("Skipped rewarded flow emits no reward callback"),
		Events.Num(),
		11
	);
	if (Events.Num() == 11)
	{
		TestEqual(
			TEXT("Skipped rewarded flow ends with dismissal"),
			Events.Last().Type,
			EOpenMobileAdsEventType::Dismissed
		);
	}
	TestEqual(
		TEXT("Skipped rewarded flow returns convenience state to idle"),
		Subsystem->GetState(),
		EOpenMobileRewardedAdState::Idle
	);

	TestTrue(
		TEXT("Convenience flow accepts a load that later fails"),
		Subsystem->RequestAndShowRewardedAd()
	);
	AddExpectedError(
		TEXT("Convenience rewarded load failed."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	if (Provider.LoadSink)
	{
		FOpenMobileAdsEvent LoadFailed;
		LoadFailed.Type = EOpenMobileAdsEventType::LoadFailed;
		LoadFailed.Error = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::NativeFailure,
			EOpenMobileAdsFailureStage::Load,
			TEXT("ConvenienceReward"),
			TEXT("Convenience rewarded load failed."),
			Provider.Name
		);
		Provider.LoadSink->Submit(MoveTemp(LoadFailed));
		DrainGameThreadTasks();
	}
	TestEqual(TEXT("Load failure emits its terminal callback"), Events.Num(), 13);
	if (Events.Num() == 13)
	{
		TestEqual(
			TEXT("Load failure is terminal for its convenience attempt"),
			Events.Last().Type,
			EOpenMobileAdsEventType::LoadFailed
		);
	}
	TestEqual(
		TEXT("Load failure returns convenience state to idle"),
		Subsystem->GetState(),
		EOpenMobileRewardedAdState::Idle
	);

	TestTrue(
		TEXT("Convenience flow accepts a show that later fails"),
		Subsystem->RequestAndShowRewardedAd()
	);
	if (Provider.LoadSink)
	{
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = FGuid::NewGuid();
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
	}
	AddExpectedError(
		TEXT("Convenience rewarded show failed."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	if (Provider.ShowSink)
	{
		FOpenMobileAdsEvent ShowFailed;
		ShowFailed.Type = EOpenMobileAdsEventType::Failed;
		ShowFailed.Error = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::NativeFailure,
			EOpenMobileAdsFailureStage::Show,
			TEXT("ConvenienceReward"),
			TEXT("Convenience rewarded show failed."),
			Provider.Name
		);
		Provider.ShowSink->Submit(MoveTemp(ShowFailed));
		DrainGameThreadTasks();
	}
	TestEqual(TEXT("Show failure emits its terminal callback"), Events.Num(), 17);
	if (Events.Num() == 17)
	{
		TestEqual(
			TEXT("Show failure is terminal for its convenience attempt"),
			Events.Last().Type,
			EOpenMobileAdsEventType::Failed
		);
	}
	TestEqual(
		TEXT("Show failure returns convenience state to idle"),
		Subsystem->GetState(),
		EOpenMobileRewardedAdState::Idle
	);

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsShowCallbackContractTest,
	"OpenMobile.Ads.ProviderContract.Show.Callback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsShowCallbackContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ShowCallback");
	Placement.Format = EOpenMobileAdFormat::Rewarded;
	Placement.Android.AdUnitId = TEXT("android-show-callback");
	Placement.IOS.AdUnitId = TEXT("ios-show-callback");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before show callback checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	const FOpenMobileAdsOperationResult Load =
		Subsystem->LoadAd(TEXT("ShowCallback"));
	TestTrue(TEXT("The placement loads before show callback checks"), Load.bAccepted);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	TestTrue(TEXT("The placement is ready before show callback checks"), Subsystem->IsReady(TEXT("ShowCallback")));

	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);

	Provider.bAcceptShow = false;
	Provider.ShowRejection = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Show,
		TEXT("ShowCallback"),
		TEXT("The mock provider rejected presentation."),
		Provider.Name
	);
	const FOpenMobileAdsOperationResult Rejected =
		Subsystem->ShowAd(TEXT("ShowCallback"));
	TestFalse(TEXT("Provider show rejection is immediate"), Rejected.bAccepted);
	TestFalse(TEXT("Immediate show rejection has no request ID"), Rejected.RequestId.IsValid());
	TestEqual(TEXT("Immediate show rejection preserves its error"), Rejected.Error.Code, EOpenMobileAdsErrorCode::ProviderFailure);
	TestTrue(TEXT("Immediate show rejection preserves readiness"), Subsystem->IsReady(TEXT("ShowCallback")));
	DrainGameThreadTasks();
	TestTrue(TEXT("Immediate show rejection broadcasts no callback"), Events.IsEmpty());

	Provider.bAcceptShow = true;
	const FOpenMobileAdsOperationResult Accepted =
		Subsystem->ShowAd(TEXT("ShowCallback"));
	TestTrue(TEXT("The show callback request is accepted"), Accepted.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShowSink =
		Provider.ShowSink;
	FOpenMobileAdsEvent DuplicateAccepted;
	DuplicateAccepted.Type = EOpenMobileAdsEventType::ShowAccepted;
	DuplicateAccepted.Provider = TEXT("WrongProvider");
	DuplicateAccepted.Placement = TEXT("WrongPlacement");
	DuplicateAccepted.Format = EOpenMobileAdFormat::Banner;
	DuplicateAccepted.RequestId = FGuid::NewGuid();
	DuplicateAccepted.CachedAdId = FGuid::NewGuid();
	ShowSink->Submit(MoveTemp(DuplicateAccepted));
	FOpenMobileAdsEvent Shown;
	Shown.Type = EOpenMobileAdsEventType::Shown;
	Shown.Provider = TEXT("WrongProvider");
	Shown.Placement = TEXT("WrongPlacement");
	Shown.Format = EOpenMobileAdFormat::Banner;
	Shown.RequestId = FGuid::NewGuid();
	Shown.CachedAdId = FGuid::NewGuid();
	ShowSink->Submit(Shown);
	ShowSink->Submit(MoveTemp(Shown));
	DrainGameThreadTasks();

	TestEqual(TEXT("A visible show broadcasts one acceptance and one shown result"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		const FOpenMobileAdsEvent& AcceptedEvent = Events[0];
		const FOpenMobileAdsEvent& ShownEvent = Events[1];
		TestEqual(TEXT("The first show event is accepted"), AcceptedEvent.Type, EOpenMobileAdsEventType::ShowAccepted);
		TestEqual(TEXT("The provider visibility event is shown"), ShownEvent.Type, EOpenMobileAdsEventType::Shown);
		for (const FOpenMobileAdsEvent* Event : {&AcceptedEvent, &ShownEvent})
		{
			TestEqual(TEXT("Show callback has the normalized placement"), Event->Placement, FName(TEXT("ShowCallback")));
			TestEqual(TEXT("Show callback has the normalized format"), Event->Format, EOpenMobileAdFormat::Rewarded);
			TestEqual(TEXT("Show callback has the normalized provider"), Event->Provider, Provider.Name);
			TestEqual(TEXT("Show callback has the accepted request ID"), Event->RequestId, Accepted.RequestId);
			TestEqual(TEXT("Show callback has the cached ad ID"), Event->CachedAdId, CachedAdId);
			TestEqual(TEXT("Show callback reports showing state"), Event->PlacementState, EOpenMobileAdPlacementState::Showing);
		}
		TestTrue(TEXT("Show callback sequence is increasing"), AcceptedEvent.Sequence < ShownEvent.Sequence);
	}

	FOpenMobileAdsEvent Failed;
	Failed.Type = EOpenMobileAdsEventType::Failed;
	Failed.Provider = TEXT("WrongProvider");
	Failed.Placement = TEXT("WrongPlacement");
	Failed.Format = EOpenMobileAdFormat::Banner;
	Failed.RequestId = FGuid::NewGuid();
	Failed.CachedAdId = FGuid::NewGuid();
	Failed.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Show,
		TEXT("WrongPlacement"),
		TEXT("The native presentation failed."),
		TEXT("WrongProvider")
	);
	AddExpectedError(
		TEXT("The native presentation failed."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	ShowSink->Submit(MoveTemp(Failed));
	DrainGameThreadTasks();

	TestEqual(TEXT("Asynchronous show failure follows acceptance"), Events.Num(), 3);
	if (Events.Num() == 3)
	{
		const FOpenMobileAdsEvent& FailedEvent = Events[2];
		TestEqual(TEXT("The asynchronous terminal event is failed"), FailedEvent.Type, EOpenMobileAdsEventType::Failed);
		TestEqual(TEXT("Show failure has the normalized placement"), FailedEvent.Placement, FName(TEXT("ShowCallback")));
		TestEqual(TEXT("Show failure has the normalized format"), FailedEvent.Format, EOpenMobileAdFormat::Rewarded);
		TestEqual(TEXT("Show failure has the normalized provider"), FailedEvent.Provider, Provider.Name);
		TestEqual(TEXT("Show failure has the accepted request ID"), FailedEvent.RequestId, Accepted.RequestId);
		TestEqual(TEXT("Show failure has the cached ad ID"), FailedEvent.CachedAdId, CachedAdId);
		TestEqual(TEXT("Show failure reports failed state"), FailedEvent.PlacementState, EOpenMobileAdPlacementState::Failed);
		TestEqual(TEXT("Show failure normalizes the error placement"), FailedEvent.Error.Placement, FName(TEXT("ShowCallback")));
		TestEqual(TEXT("Show failure normalizes the error provider"), FailedEvent.Error.Provider, Provider.Name);
	}
	TestFalse(TEXT("Asynchronous show failure consumes readiness"), Subsystem->IsReady(TEXT("ShowCallback")));
	TestEqual(TEXT("Asynchronous show failure releases the native cache"), Provider.ReleasedCachedAds.Num(), 1);

	FOpenMobileAdsEvent LateDismissed;
	LateDismissed.Type = EOpenMobileAdsEventType::Dismissed;
	ShowSink->Submit(MoveTemp(LateDismissed));
	FOpenMobileAdsEvent LateShown;
	LateShown.Type = EOpenMobileAdsEventType::Shown;
	ShowSink->Submit(MoveTemp(LateShown));
	DrainGameThreadTasks();
	TestEqual(TEXT("Callbacks after show failure are ignored"), Events.Num(), 3);

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsImpressionCallbackContractTest,
	"OpenMobile.Ads.ProviderContract.Impression.Callback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsImpressionCallbackContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ImpressionCallback");
	Placement.Format = EOpenMobileAdFormat::Rewarded;
	Placement.Android.AdUnitId = TEXT("android-impression-callback");
	Placement.IOS.AdUnitId = TEXT("ios-impression-callback");
	Placement.FrequencyCap.MaxImpressions = 1;
	Placement.FrequencyCap.WindowSeconds = 3600.0;
	Placement.CooldownSeconds = 300.0;

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].bReportsImpression = true;
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before impression callback checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	TestTrue(
		TEXT("The placement loads before impression callback checks"),
		Subsystem->LoadAd(TEXT("ImpressionCallback")).bAccepted
	);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	TestTrue(
		TEXT("The placement is ready before impression callback checks"),
		Subsystem->IsReady(TEXT("ImpressionCallback"))
	);

	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult Show =
		Subsystem->ShowAd(TEXT("ImpressionCallback"));
	TestTrue(TEXT("The impression callback show is accepted"), Show.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShowSink =
		Provider.ShowSink;

	FOpenMobileAdsEvent Shown;
	Shown.Type = EOpenMobileAdsEventType::Shown;
	ShowSink->Submit(MoveTemp(Shown));
	FOpenMobileAdsEvent Impression;
	Impression.Type = EOpenMobileAdsEventType::Impression;
	Impression.Placement = TEXT("WrongPlacement");
	Impression.Format = EOpenMobileAdFormat::Banner;
	Impression.Provider = TEXT("WrongProvider");
	Impression.Network = TEXT("MockNetwork");
	Impression.RequestId = FGuid::NewGuid();
	Impression.CachedAdId = FGuid::NewGuid();
	ShowSink->Submit(Impression);
	FOpenMobileAdsEvent Clicked;
	Clicked.Type = EOpenMobileAdsEventType::Clicked;
	ShowSink->Submit(MoveTemp(Clicked));
	FOpenMobileAdsEvent RevenuePaid;
	RevenuePaid.Type = EOpenMobileAdsEventType::RevenuePaid;
	ShowSink->Submit(MoveTemp(RevenuePaid));
	FOpenMobileAdsEvent RewardEarned;
	RewardEarned.Type = EOpenMobileAdsEventType::RewardEarned;
	ShowSink->Submit(MoveTemp(RewardEarned));
	ShowSink->Submit(MoveTemp(Impression));
	FOpenMobileAdsEvent Dismissed;
	Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
	ShowSink->Submit(MoveTemp(Dismissed));
	FOpenMobileAdsEvent LateImpression;
	LateImpression.Type = EOpenMobileAdsEventType::Impression;
	ShowSink->Submit(MoveTemp(LateImpression));
	DrainGameThreadTasks();

	const EOpenMobileAdsEventType ExpectedTypes[] = {
		EOpenMobileAdsEventType::ShowAccepted,
		EOpenMobileAdsEventType::Shown,
		EOpenMobileAdsEventType::Impression,
		EOpenMobileAdsEventType::Clicked,
		EOpenMobileAdsEventType::RevenuePaid,
		EOpenMobileAdsEventType::RewardEarned,
		EOpenMobileAdsEventType::Dismissed
	};
	const int32 ExpectedEventCount = static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes));
	TestEqual(TEXT("The show emits one ordered impression lifecycle"), Events.Num(), ExpectedEventCount);
	if (Events.Num() == ExpectedEventCount)
	{
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			TestEqual(TEXT("The impression lifecycle preserves provider order"), Events[Index].Type, ExpectedTypes[Index]);
			if (Index > 0)
			{
				TestTrue(TEXT("The impression lifecycle sequence is increasing"), Events[Index - 1].Sequence < Events[Index].Sequence);
			}
		}

		const FOpenMobileAdsEvent& ImpressionEvent = Events[2];
		TestEqual(TEXT("The impression has the normalized placement"), ImpressionEvent.Placement, FName(TEXT("ImpressionCallback")));
		TestEqual(TEXT("The impression has the normalized format"), ImpressionEvent.Format, EOpenMobileAdFormat::Rewarded);
		TestEqual(TEXT("The impression has the normalized provider"), ImpressionEvent.Provider, Provider.Name);
		TestEqual(TEXT("The impression preserves the provider network"), ImpressionEvent.Network, FString(TEXT("MockNetwork")));
		TestEqual(TEXT("The impression has the show request ID"), ImpressionEvent.RequestId, Show.RequestId);
		TestEqual(TEXT("The impression has the shown cache ID"), ImpressionEvent.CachedAdId, CachedAdId);
		TestEqual(TEXT("The impression reports showing state"), ImpressionEvent.PlacementState, EOpenMobileAdPlacementState::Showing);
		TestTrue(TEXT("The impression has a service timestamp"), ImpressionEvent.Timestamp != FDateTime());
	}

	TestTrue(
		TEXT("A replacement ad can load after dismissal"),
		Subsystem->LoadAd(TEXT("ImpressionCallback")).bAccepted
	);
	FOpenMobileAdsEvent ReplacementLoaded;
	ReplacementLoaded.Type = EOpenMobileAdsEventType::Loaded;
	ReplacementLoaded.CachedAdId = FGuid::NewGuid();
	Provider.LoadSink->Submit(MoveTemp(ReplacementLoaded));
	DrainGameThreadTasks();
	const FOpenMobileAdsCanShowResult FrequencyCapped =
		Subsystem->CanShow(TEXT("ImpressionCallback"));
	TestFalse(TEXT("The accepted impression activates the frequency cap"), FrequencyCapped.bCanShow);
	TestEqual(
		TEXT("The frequency cap is the first pacing block"),
		FrequencyCapped.BlockReason,
		EOpenMobileAdsCanShowBlockReason::FrequencyCap
	);
	TestTrue(TEXT("The frequency cap reports its next eligible time"), FrequencyCapped.NextEligibleAt > FDateTime::UtcNow());

	ScopedSettings.Settings->Placements[0].FrequencyCap = FOpenMobileAdsFrequencyCap();
	const FOpenMobileAdsCanShowResult CoolingDown =
		Subsystem->CanShow(TEXT("ImpressionCallback"));
	TestFalse(TEXT("The accepted impression activates the cooldown"), CoolingDown.bCanShow);
	TestEqual(
		TEXT("Cooldown remains when the frequency cap is disabled"),
		CoolingDown.BlockReason,
		EOpenMobileAdsCanShowBlockReason::Cooldown
	);
	TestTrue(TEXT("The cooldown reports its next eligible time"), CoolingDown.NextEligibleAt > FDateTime::UtcNow());

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsClickCallbackContractTest,
	"OpenMobile.Ads.ProviderContract.Click.Callback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsClickCallbackContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ClickCallback");
	Placement.Format = EOpenMobileAdFormat::Rewarded;
	Placement.Android.AdUnitId = TEXT("android-click-callback");
	Placement.IOS.AdUnitId = TEXT("ios-click-callback");

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].bReportsClick = true;
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before click callback checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	const FOpenMobileAdsProviderCapabilities Capabilities =
		Subsystem->GetProviderCapabilities();
	const FOpenMobileAdFormatCapabilities* RewardedCapabilities =
		Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded);
	TestTrue(
		TEXT("The provider SPI advertises rewarded click callbacks"),
		RewardedCapabilities && RewardedCapabilities->bReportsClick
	);

	TestTrue(
		TEXT("The placement loads before click callback checks"),
		Subsystem->LoadAd(TEXT("ClickCallback")).bAccepted
	);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();

	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);
	const FOpenMobileAdsOperationResult Show =
		Subsystem->ShowAd(TEXT("ClickCallback"));
	TestTrue(TEXT("The click callback show is accepted"), Show.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShowSink =
		Provider.ShowSink;

	FOpenMobileAdsEvent Shown;
	Shown.Type = EOpenMobileAdsEventType::Shown;
	ShowSink->Submit(MoveTemp(Shown));
	FOpenMobileAdsEvent Clicked;
	Clicked.Type = EOpenMobileAdsEventType::Clicked;
	Clicked.Placement = TEXT("WrongPlacement");
	Clicked.Format = EOpenMobileAdFormat::Banner;
	Clicked.Provider = TEXT("WrongProvider");
	Clicked.Network = TEXT("MockNetwork");
	Clicked.RequestId = FGuid::NewGuid();
	Clicked.CachedAdId = FGuid::NewGuid();
	ShowSink->Submit(Clicked);
	ShowSink->Submit(MoveTemp(Clicked));
	FOpenMobileAdsEvent Dismissed;
	Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
	ShowSink->Submit(MoveTemp(Dismissed));
	FOpenMobileAdsEvent LateClick;
	LateClick.Type = EOpenMobileAdsEventType::Clicked;
	LateClick.Network = TEXT("LateNetwork");
	ShowSink->Submit(MoveTemp(LateClick));
	DrainGameThreadTasks();

	const EOpenMobileAdsEventType ExpectedTypes[] = {
		EOpenMobileAdsEventType::ShowAccepted,
		EOpenMobileAdsEventType::Shown,
		EOpenMobileAdsEventType::Clicked,
		EOpenMobileAdsEventType::Clicked,
		EOpenMobileAdsEventType::Dismissed
	};
	const int32 ExpectedEventCount = static_cast<int32>(UE_ARRAY_COUNT(ExpectedTypes));
	TestEqual(TEXT("Active clicks are preserved and post-dismissal clicks are ignored"), Events.Num(), ExpectedEventCount);
	if (Events.Num() == ExpectedEventCount)
	{
		for (int32 Index = 0; Index < Events.Num(); ++Index)
		{
			TestEqual(TEXT("The click lifecycle preserves provider order"), Events[Index].Type, ExpectedTypes[Index]);
			if (Index > 0)
			{
				TestTrue(TEXT("The click lifecycle sequence is increasing"), Events[Index - 1].Sequence < Events[Index].Sequence);
			}
		}
		for (int32 Index : {2, 3})
		{
			const FOpenMobileAdsEvent& ClickEvent = Events[Index];
			TestEqual(TEXT("The click has the normalized placement"), ClickEvent.Placement, FName(TEXT("ClickCallback")));
			TestEqual(TEXT("The click has the normalized format"), ClickEvent.Format, EOpenMobileAdFormat::Rewarded);
			TestEqual(TEXT("The click has the normalized provider"), ClickEvent.Provider, Provider.Name);
			TestEqual(TEXT("The click preserves the provider network"), ClickEvent.Network, FString(TEXT("MockNetwork")));
			TestEqual(TEXT("The click has the show request ID"), ClickEvent.RequestId, Show.RequestId);
			TestEqual(TEXT("The click has the shown cache ID"), ClickEvent.CachedAdId, CachedAdId);
			TestEqual(TEXT("The click reports showing state"), ClickEvent.PlacementState, EOpenMobileAdPlacementState::Showing);
		}
		TestEqual(TEXT("Dismissal restores idle state before its callback"), Events.Last().PlacementState, EOpenMobileAdPlacementState::Idle);
	}

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsDismissCallbackContractTest,
	"OpenMobile.Ads.ProviderContract.Dismiss.Callback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsDismissCallbackContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& FullScreen =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	FullScreen.Placement = TEXT("DismissFullScreen");
	FullScreen.Format = EOpenMobileAdFormat::Rewarded;
	FullScreen.Android.AdUnitId = TEXT("android-dismiss-full-screen");
	FullScreen.IOS.AdUnitId = TEXT("ios-dismiss-full-screen");
	FOpenMobileAdsPlacementSettings& Persistent =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Persistent.Placement = TEXT("DismissPersistent");
	Persistent.Format = EOpenMobileAdFormat::Banner;
	Persistent.Android.AdUnitId = TEXT("android-dismiss-persistent");
	Persistent.IOS.AdUnitId = TEXT("ios-dismiss-persistent");

	FMockProvider Provider(TEXT("MockAds"));
	FOpenMobileAdFormatCapabilities Banner;
	Banner.Format = EOpenMobileAdFormat::Banner;
	Banner.bCanLoad = true;
	Banner.bCanShow = true;
	Banner.bReportsDismiss = true;
	Provider.Capabilities.Formats.Add(Banner);
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before dismiss callback checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	TArray<FOpenMobileAdsEvent> Events;
	bool bIdleBeforeDismissBroadcast = false;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[Subsystem, &Events, &bIdleBeforeDismissBroadcast](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
			if (Event.Type == EOpenMobileAdsEventType::Dismissed)
			{
				const FOpenMobileAdsPlacementStatus Status =
					Subsystem->GetPlacementStatus(Event.Placement);
				bIdleBeforeDismissBroadcast =
					Status.State == EOpenMobileAdPlacementState::Idle
					&& !Status.CachedAdId.IsValid();
			}
		}
	);

	const FGuid FullScreenCachedAdId = FGuid::NewGuid();
	TestTrue(
		TEXT("The full-screen placement starts loading"),
		Subsystem->LoadAd(TEXT("DismissFullScreen")).bAccepted
	);
	FOpenMobileAdsEvent FullScreenLoaded;
	FullScreenLoaded.Type = EOpenMobileAdsEventType::Loaded;
	FullScreenLoaded.CachedAdId = FullScreenCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(FullScreenLoaded));
	DrainGameThreadTasks();
	const FOpenMobileAdsOperationResult FullScreenShow =
		Subsystem->ShowAd(TEXT("DismissFullScreen"));
	TestTrue(TEXT("The full-screen show is accepted"), FullScreenShow.bAccepted);
	FOpenMobileAdsEvent FullScreenShown;
	FullScreenShown.Type = EOpenMobileAdsEventType::Shown;
	Provider.ShowSink->Submit(MoveTemp(FullScreenShown));
	DrainGameThreadTasks();
	TestEqual(
		TEXT("A missing dismiss leaves the accepted show active"),
		Subsystem->GetPlacementStatus(TEXT("DismissFullScreen")).State,
		EOpenMobileAdPlacementState::Showing
	);
	TestTrue(
		TEXT("An accepted show can be cancelled when dismissal is missing"),
		Subsystem->CancelRequest(FullScreenShow.RequestId).bAccepted
	);
	const FOpenMobileAdsPlacementStatus CancelledStatus =
		Subsystem->GetPlacementStatus(TEXT("DismissFullScreen"));
	TestEqual(
		TEXT("Cancelling a show with no dismiss restores idle state"),
		CancelledStatus.State,
		EOpenMobileAdPlacementState::Idle
	);
	TestFalse(
		TEXT("Cancelling a show with no dismiss consumes its cache"),
		CancelledStatus.CachedAdId.IsValid()
	);
	TestEqual(
		TEXT("Cancelling a show with no dismiss releases its native cache"),
		Provider.ReleasedCachedAds.Num(),
		1
	);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(
			TEXT("Missing-dismiss recovery releases the shown cache identity"),
			Provider.ReleasedCachedAds[0],
			FullScreenCachedAdId
		);
	}

	Events.Reset();
	const FGuid PersistentCachedAdId = FGuid::NewGuid();
	TestTrue(
		TEXT("The persistent placement starts loading"),
		Subsystem->LoadAd(TEXT("DismissPersistent")).bAccepted
	);
	FOpenMobileAdsEvent PersistentLoaded;
	PersistentLoaded.Type = EOpenMobileAdsEventType::Loaded;
	PersistentLoaded.CachedAdId = PersistentCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(PersistentLoaded));
	DrainGameThreadTasks();
	Events.Reset();
	const FOpenMobileAdsOperationResult PersistentShow =
		Subsystem->ShowAd(TEXT("DismissPersistent"));
	TestTrue(TEXT("The persistent show is accepted"), PersistentShow.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> PersistentSink =
		Provider.ShowSink;
	FOpenMobileAdsEvent Dismissed;
	Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
	Dismissed.Placement = TEXT("WrongPlacement");
	Dismissed.Format = EOpenMobileAdFormat::Interstitial;
	Dismissed.Provider = TEXT("WrongProvider");
	Dismissed.Network = TEXT("MockNetwork");
	Dismissed.RequestId = FGuid::NewGuid();
	Dismissed.CachedAdId = FGuid::NewGuid();
	PersistentSink->Submit(Dismissed);
	PersistentSink->Submit(MoveTemp(Dismissed));
	FOpenMobileAdsEvent LateShown;
	LateShown.Type = EOpenMobileAdsEventType::Shown;
	PersistentSink->Submit(MoveTemp(LateShown));
	DrainGameThreadTasks();

	TestEqual(
		TEXT("An out-of-order dismiss is terminal and duplicate callbacks are ignored"),
		Events.Num(),
		2
	);
	if (Events.Num() == 2)
	{
		const FOpenMobileAdsEvent& DismissEvent = Events[1];
		TestEqual(TEXT("Dismiss follows service show acceptance"), Events[0].Type, EOpenMobileAdsEventType::ShowAccepted);
		TestEqual(TEXT("The provider close is normalized as dismissed"), DismissEvent.Type, EOpenMobileAdsEventType::Dismissed);
		TestEqual(TEXT("Dismiss has the normalized placement"), DismissEvent.Placement, FName(TEXT("DismissPersistent")));
		TestEqual(TEXT("Dismiss has the normalized format"), DismissEvent.Format, EOpenMobileAdFormat::Banner);
		TestEqual(TEXT("Dismiss has the normalized provider"), DismissEvent.Provider, Provider.Name);
		TestEqual(TEXT("Dismiss preserves the provider network"), DismissEvent.Network, FString(TEXT("MockNetwork")));
		TestEqual(TEXT("Dismiss has the show request ID"), DismissEvent.RequestId, PersistentShow.RequestId);
		TestEqual(TEXT("Dismiss has the shown cache ID"), DismissEvent.CachedAdId, PersistentCachedAdId);
		TestEqual(TEXT("Dismiss reports restored idle state"), DismissEvent.PlacementState, EOpenMobileAdPlacementState::Idle);
		TestTrue(TEXT("Dismiss ordering uses an increasing sequence"), Events[0].Sequence < DismissEvent.Sequence);
	}
	TestTrue(TEXT("Lifecycle state is restored before dismiss broadcasts"), bIdleBeforeDismissBroadcast);
	TestEqual(TEXT("Both consumed native caches are released"), Provider.ReleasedCachedAds.Num(), 2);

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsInitializationFailureCallbackContractTest,
	"OpenMobile.Ads.ProviderContract.Failure.Callback.Initialization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsInitializationFailureCallbackContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	const FOpenMobileAdsOperationResult Started = Subsystem->InitializeAds();
	TestTrue(TEXT("Initialization accepts the malformed native failure scenario"), Started.bAccepted);

	FOpenMobileAdsError Malformed;
	Malformed.bRetryable = true;
	Malformed.NativeDiagnostics.NativeCode = TEXT("future-init-code");
	Malformed.NativeDiagnostics.Provider = TEXT("WrongProvider");
	Malformed.NativeDiagnostics.Network = TEXT("MockNetwork");
	Provider.CompleteInitialization(MoveTemp(Malformed));
	DrainGameThreadTasks();

	const FOpenMobileAdsInitializationStatusSnapshot Status =
		Subsystem->GetInitializationStatus();
	TestEqual(TEXT("Native failure details cannot complete initialization as ready"), Status.ServiceState, EOpenMobileAdsServiceState::Failed);
	TestEqual(TEXT("Initialization failure retains its request ID"), Status.RequestId, Started.RequestId);
	TestEqual(TEXT("Malformed initialization failure becomes typed"), Status.Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	TestEqual(TEXT("Initialization failure receives its operation stage"), Status.Error.Stage, EOpenMobileAdsFailureStage::Initialization);
	TestEqual(TEXT("Initialization failure receives the owning provider"), Status.Error.Provider, Provider.Name);
	TestEqual(TEXT("Initialization native diagnostics receive the owning provider"), Status.Error.NativeDiagnostics.Provider, Provider.Name);
	TestEqual(TEXT("Initialization native code is preserved"), Status.Error.NativeDiagnostics.NativeCode, FString(TEXT("future-init-code")));
	TestEqual(TEXT("Initialization network is preserved"), Status.Error.NativeDiagnostics.Network, FString(TEXT("MockNetwork")));
	TestTrue(TEXT("Initialization retryability is preserved"), Status.Error.bRetryable);
	TestFalse(TEXT("Malformed initialization failure receives an explanation"), Status.Error.Explanation.IsEmpty());
	const FOpenMobileAdsInitializationComponentStatus* Component =
		FindInitializationComponent(
			Status,
			EOpenMobileAdsInitializationComponentType::Provider,
			Provider.Name
		);
	TestNotNull(TEXT("The failed provider remains in initialization status"), Component);
	if (Component)
	{
		TestEqual(TEXT("The provider component reports failure"), Component->State, EOpenMobileAdsInitializationState::Failed);
		TestEqual(TEXT("The provider component retains the typed error"), Component->Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	}

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsOperationFailureCallbackContractTest,
	"OpenMobile.Ads.ProviderContract.Failure.Callback.Operations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsOperationFailureCallbackContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("FailureCallback");
	Placement.Format = EOpenMobileAdFormat::Rewarded;
	Placement.Android.AdUnitId = TEXT("android-failure-callback");
	Placement.IOS.AdUnitId = TEXT("ios-failure-callback");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before failure callback checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult InitialLoad =
		Subsystem->LoadAd(TEXT("FailureCallback"));
	TestTrue(TEXT("The malformed native load failure is accepted"), InitialLoad.bAccepted);
	FOpenMobileAdsEvent NativeLoadFailure;
	NativeLoadFailure.Type = EOpenMobileAdsEventType::LoadFailed;
	NativeLoadFailure.Error.bRetryable = true;
	NativeLoadFailure.Error.NativeDiagnostics.NativeCode = TEXT("future-load-code");
	NativeLoadFailure.Error.NativeDiagnostics.Provider = TEXT("WrongProvider");
	NativeLoadFailure.Error.NativeDiagnostics.Network = TEXT("MockLoadNetwork");
	Provider.LoadSink->Submit(MoveTemp(NativeLoadFailure));
	DrainGameThreadTasks();

	TestEqual(TEXT("A malformed load failure follows load start"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		const FOpenMobileAdsEvent& Failure = Events[1];
		TestEqual(TEXT("Malformed load result remains load failed"), Failure.Type, EOpenMobileAdsEventType::LoadFailed);
		TestEqual(TEXT("Malformed load failure becomes typed"), Failure.Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
		TestEqual(TEXT("Load failure receives its operation stage"), Failure.Error.Stage, EOpenMobileAdsFailureStage::Load);
		TestEqual(TEXT("Load failure receives the normalized placement"), Failure.Error.Placement, FName(TEXT("FailureCallback")));
		TestEqual(TEXT("Load failure receives the normalized provider"), Failure.Error.Provider, Provider.Name);
		TestEqual(TEXT("Load failure diagnostics receive the owning provider"), Failure.Error.NativeDiagnostics.Provider, Provider.Name);
		TestEqual(TEXT("Load failure preserves native code"), Failure.Error.NativeDiagnostics.NativeCode, FString(TEXT("future-load-code")));
		TestEqual(TEXT("Load failure exposes its network on the event"), Failure.Network, FString(TEXT("MockLoadNetwork")));
		TestEqual(TEXT("Load failure exposes its diagnostic network"), Failure.Error.NativeDiagnostics.Network, FString(TEXT("MockLoadNetwork")));
		TestEqual(TEXT("Load failure has the accepted request ID"), Failure.RequestId, InitialLoad.RequestId);
		TestTrue(TEXT("Load failure preserves retryability"), Failure.Error.bRetryable);
		TestFalse(TEXT("Malformed load failure receives an explanation"), Failure.Error.Explanation.IsEmpty());
		TestEqual(TEXT("Fresh load failure reports failed state"), Failure.PlacementState, EOpenMobileAdPlacementState::Failed);
	}
	TestEqual(
		TEXT("Fresh load failure leaves the placement failed"),
		Subsystem->GetPlacementStatus(TEXT("FailureCallback")).State,
		EOpenMobileAdPlacementState::Failed
	);

	TestTrue(
		TEXT("The failed placement can load a replacement"),
		Subsystem->LoadAd(TEXT("FailureCallback")).bAccepted
	);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	Events.Reset();
	FOpenMobileAdsLoadOptions ReloadOptions;
	ReloadOptions.bForceReload = true;
	const FOpenMobileAdsOperationResult Replacement =
		Subsystem->LoadAd(TEXT("FailureCallback"), ReloadOptions);
	TestTrue(TEXT("A forced replacement load is accepted"), Replacement.bAccepted);
	AddExpectedError(
		TEXT("failed the load request without a typed error"),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	FOpenMobileAdsEvent EmptyLoadFailure;
	EmptyLoadFailure.Type = EOpenMobileAdsEventType::LoadFailed;
	Provider.LoadSink->Submit(MoveTemp(EmptyLoadFailure));
	DrainGameThreadTasks();

	TestEqual(TEXT("An empty replacement failure follows load start"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		const FOpenMobileAdsEvent& Failure = Events[1];
		TestEqual(TEXT("Missing load error receives a typed fallback"), Failure.Error.Code, EOpenMobileAdsErrorCode::ProviderFailure);
		TestEqual(TEXT("Missing load error receives the load stage"), Failure.Error.Stage, EOpenMobileAdsFailureStage::Load);
		TestEqual(TEXT("Replacement failure returns to ready before broadcast"), Failure.PlacementState, EOpenMobileAdPlacementState::Ready);
	}
	const FOpenMobileAdsPlacementStatus ReplacementStatus =
		Subsystem->GetPlacementStatus(TEXT("FailureCallback"));
	TestEqual(TEXT("Replacement failure preserves ready state"), ReplacementStatus.State, EOpenMobileAdPlacementState::Ready);
	TestEqual(TEXT("Replacement failure preserves the prior cache"), ReplacementStatus.CachedAdId, CachedAdId);

	Provider.bAcceptShow = false;
	Provider.ShowRejection = FOpenMobileAdsError();
	Provider.ShowRejection.bRetryable = true;
	Provider.ShowRejection.NativeDiagnostics.NativeCode = TEXT("NO_FILL");
	Provider.ShowRejection.NativeDiagnostics.Network = TEXT("MockRejectNetwork");
	const FOpenMobileAdsOperationResult RejectedShow =
		Subsystem->ShowAd(TEXT("FailureCallback"));
	TestFalse(TEXT("The provider can reject show immediately"), RejectedShow.bAccepted);
	TestEqual(TEXT("Immediate rejection is mapped from native code"), RejectedShow.Error.Code, EOpenMobileAdsErrorCode::NoFill);
	TestEqual(TEXT("Immediate rejection receives the show stage"), RejectedShow.Error.Stage, EOpenMobileAdsFailureStage::Show);
	TestEqual(TEXT("Immediate rejection receives the placement"), RejectedShow.Error.Placement, FName(TEXT("FailureCallback")));
	TestEqual(TEXT("Immediate rejection receives the provider"), RejectedShow.Error.Provider, Provider.Name);
	TestEqual(TEXT("Immediate rejection preserves native code"), RejectedShow.Error.NativeDiagnostics.NativeCode, FString(TEXT("NO_FILL")));
	TestEqual(TEXT("Immediate rejection preserves network"), RejectedShow.Error.NativeDiagnostics.Network, FString(TEXT("MockRejectNetwork")));
	TestTrue(TEXT("Immediate rejection preserves retryability"), RejectedShow.Error.bRetryable);
	TestTrue(TEXT("Immediate show rejection preserves ready state"), Subsystem->IsReady(TEXT("FailureCallback")));

	Provider.bAcceptShow = true;
	Events.Reset();
	const FOpenMobileAdsOperationResult AcceptedShow =
		Subsystem->ShowAd(TEXT("FailureCallback"));
	TestTrue(TEXT("The asynchronous show failure is accepted"), AcceptedShow.bAccepted);
	AddExpectedError(
		TEXT("failed the show request without a typed error"),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	FOpenMobileAdsEvent EmptyShowFailure;
	EmptyShowFailure.Type = EOpenMobileAdsEventType::Failed;
	EmptyShowFailure.Network = TEXT("MockShowNetwork");
	Provider.ShowSink->Submit(MoveTemp(EmptyShowFailure));
	DrainGameThreadTasks();

	TestEqual(TEXT("An empty show failure follows show acceptance"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		const FOpenMobileAdsEvent& Failure = Events[1];
		TestEqual(TEXT("Missing show error receives a typed fallback"), Failure.Error.Code, EOpenMobileAdsErrorCode::ProviderFailure);
		TestEqual(TEXT("Missing show error receives the show stage"), Failure.Error.Stage, EOpenMobileAdsFailureStage::Show);
		TestEqual(TEXT("Show failure preserves its event network"), Failure.Network, FString(TEXT("MockShowNetwork")));
		TestEqual(TEXT("Show failure copies network into diagnostics"), Failure.Error.NativeDiagnostics.Network, FString(TEXT("MockShowNetwork")));
		TestEqual(TEXT("Show failure has the accepted request ID"), Failure.RequestId, AcceptedShow.RequestId);
		TestEqual(TEXT("Show failure has the consumed cache ID"), Failure.CachedAdId, CachedAdId);
		TestEqual(TEXT("Accepted show failure reports failed state"), Failure.PlacementState, EOpenMobileAdPlacementState::Failed);
	}
	const FOpenMobileAdsPlacementStatus FailedStatus =
		Subsystem->GetPlacementStatus(TEXT("FailureCallback"));
	TestEqual(TEXT("Accepted show failure leaves failed state"), FailedStatus.State, EOpenMobileAdPlacementState::Failed);
	TestFalse(TEXT("Accepted show failure consumes the cache"), FailedStatus.CachedAdId.IsValid());
	TestEqual(TEXT("Accepted show failure releases the native cache"), Provider.ReleasedCachedAds.Num(), 1);

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRewardTypeContractTest,
	"OpenMobile.Ads.ProviderContract.Reward.Type",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRewardTypeContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	for (const FName PlacementName : {
		FName(TEXT("ProviderRewardType")),
		FName(TEXT("FallbackRewardType")),
		FName(TEXT("EmptyRewardType"))
	})
	{
		FOpenMobileAdsPlacementSettings& Placement =
			ScopedSettings.Settings->Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.Format = EOpenMobileAdFormat::Rewarded;
		Placement.Android.AdUnitId = FString::Printf(
			TEXT("android-%s"),
			*PlacementName.ToString()
		);
		Placement.IOS.AdUnitId = FString::Printf(
			TEXT("ios-%s"),
			*PlacementName.ToString()
		);
	}
	ScopedSettings.Settings->Placements[0].FallbackRewardType = TEXT("fallback-coin");
	ScopedSettings.Settings->Placements[1].FallbackRewardType = TEXT("星の欠片");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before reward type checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	TArray<FOpenMobileAdsEvent> Rewards;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Rewards](const FOpenMobileAdsEvent& Event)
		{
			if (Event.Type == EOpenMobileAdsEventType::RewardEarned)
			{
				Rewards.Add(Event);
			}
		}
	);

	auto SubmitReward = [this, Subsystem, &Provider](
		FName PlacementName,
		FString ProviderRewardType
	)
	{
		const FOpenMobileAdsOperationResult Load =
			Subsystem->LoadAd(PlacementName);
		TestTrue(TEXT("The reward type placement starts loading"), Load.bAccepted);
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = FGuid::NewGuid();
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
		const FOpenMobileAdsOperationResult Show =
			Subsystem->ShowAd(PlacementName);
		TestTrue(TEXT("The reward type placement starts showing"), Show.bAccepted);
		FOpenMobileAdsEvent Reward;
		Reward.Type = EOpenMobileAdsEventType::RewardEarned;
		Reward.bHasReward = true;
		Reward.Reward.Type = MoveTemp(ProviderRewardType);
		Reward.Reward.Amount = 1;
		Provider.ShowSink->Submit(MoveTemp(Reward));
		FOpenMobileAdsEvent Dismissed;
		Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
		Provider.ShowSink->Submit(MoveTemp(Dismissed));
		DrainGameThreadTasks();
	};

	const FString ProviderSpecificType = TEXT("特殊/coin.v2");
	SubmitReward(TEXT("ProviderRewardType"), ProviderSpecificType);
	SubmitReward(TEXT("FallbackRewardType"), FString());
	SubmitReward(TEXT("EmptyRewardType"), FString());

	TestEqual(TEXT("Each shown ad produces one reward type result"), Rewards.Num(), 3);
	if (Rewards.Num() == 3)
	{
		TestEqual(TEXT("Provider-specific reward type is preserved exactly"), Rewards[0].Reward.Type, ProviderSpecificType);
		TestEqual(TEXT("Empty provider type uses the Unicode placement fallback"), Rewards[1].Reward.Type, FString(TEXT("星の欠片")));
		TestTrue(TEXT("Reward type remains empty when no fallback is configured"), Rewards[2].Reward.Type.IsEmpty());
		for (int32 Index = 0; Index < Rewards.Num(); ++Index)
		{
			TestTrue(TEXT("Reward type event remains marked as a reward"), Rewards[Index].bHasReward);
			TestEqual(TEXT("Reward type event keeps its placement"), Rewards[Index].Placement, ScopedSettings.Settings->Placements[Index].Placement);
			TestEqual(TEXT("Reward type event keeps its provider"), Rewards[Index].Provider, Provider.Name);
			TestTrue(TEXT("Reward type event keeps its show request"), Rewards[Index].RequestId.IsValid());
		}
	}

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRewardAmountContractTest,
	"OpenMobile.Ads.ProviderContract.Reward.Amount",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRewardAmountContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	for (const FName PlacementName : {
		FName(TEXT("ProviderRewardAmount")),
		FName(TEXT("FallbackRewardAmount")),
		FName(TEXT("ZeroRewardAmount")),
		FName(TEXT("NegativeRewardAmount")),
		FName(TEXT("WideRewardAmount"))
	})
	{
		FOpenMobileAdsPlacementSettings& Placement =
			ScopedSettings.Settings->Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.Format = EOpenMobileAdFormat::Rewarded;
		Placement.Android.AdUnitId = FString::Printf(
			TEXT("android-%s"),
			*PlacementName.ToString()
		);
		Placement.IOS.AdUnitId = FString::Printf(
			TEXT("ios-%s"),
			*PlacementName.ToString()
		);
	}
	ScopedSettings.Settings->Placements[1].FallbackRewardAmount = 25;
	ScopedSettings.Settings->Placements[3].FallbackRewardAmount = 30;

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before reward amount checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	TArray<FOpenMobileAdsEvent> Rewards;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Rewards](const FOpenMobileAdsEvent& Event)
		{
			if (Event.Type == EOpenMobileAdsEventType::RewardEarned)
			{
				Rewards.Add(Event);
			}
		}
	);

	auto SubmitReward = [this, Subsystem, &Provider](
		FName PlacementName,
		int64 ProviderRewardAmount
	)
	{
		const FOpenMobileAdsOperationResult Load =
			Subsystem->LoadAd(PlacementName);
		TestTrue(TEXT("The reward amount placement starts loading"), Load.bAccepted);
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = FGuid::NewGuid();
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
		const FOpenMobileAdsOperationResult Show =
			Subsystem->ShowAd(PlacementName);
		TestTrue(TEXT("The reward amount placement starts showing"), Show.bAccepted);
		FOpenMobileAdsEvent Reward;
		Reward.Type = EOpenMobileAdsEventType::RewardEarned;
		Reward.bHasReward = true;
		Reward.Reward.Type = TEXT("coin");
		Reward.Reward.Amount = ProviderRewardAmount;
		Provider.ShowSink->Submit(MoveTemp(Reward));
		FOpenMobileAdsEvent Dismissed;
		Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
		Provider.ShowSink->Submit(MoveTemp(Dismissed));
		DrainGameThreadTasks();
	};

	SubmitReward(TEXT("ProviderRewardAmount"), 73);
	SubmitReward(TEXT("FallbackRewardAmount"), 0);
	SubmitReward(TEXT("ZeroRewardAmount"), 0);
	SubmitReward(TEXT("NegativeRewardAmount"), -7);
	SubmitReward(TEXT("WideRewardAmount"), MAX_int64);

	TestEqual(TEXT("Each shown ad produces one reward amount result"), Rewards.Num(), 5);
	if (Rewards.Num() == 5)
	{
		TestEqual(TEXT("Provider-specific reward amount is preserved"), Rewards[0].Reward.Amount, static_cast<int64>(73));
		TestEqual(TEXT("An omitted provider amount uses the placement fallback"), Rewards[1].Reward.Amount, static_cast<int64>(25));
		TestTrue(TEXT("A fallback amount remains grantable"), Rewards[1].bHasReward);
		TestEqual(TEXT("An omitted amount without a fallback normalizes to zero"), Rewards[2].Reward.Amount, static_cast<int64>(0));
		TestFalse(TEXT("An omitted amount without a fallback is not grantable"), Rewards[2].bHasReward);
		TestEqual(TEXT("A negative provider amount normalizes to zero"), Rewards[3].Reward.Amount, static_cast<int64>(0));
		TestFalse(TEXT("A negative provider amount does not use the fallback"), Rewards[3].bHasReward);
		TestEqual(TEXT("The signed 64-bit boundary does not overflow"), Rewards[4].Reward.Amount, static_cast<int64>(MAX_int64));
		TestTrue(TEXT("The signed 64-bit boundary remains grantable"), Rewards[4].bHasReward);
	}

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRewardCallbackContractTest,
	"OpenMobile.Ads.ProviderContract.Reward.Callback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRewardCallbackContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	for (const FName PlacementName : {
		FName(TEXT("RewardBeforeDismiss")),
		FName(TEXT("RewardAfterDismiss")),
		FName(TEXT("NoReward"))
	})
	{
		FOpenMobileAdsPlacementSettings& Placement =
			ScopedSettings.Settings->Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.Format = EOpenMobileAdFormat::Rewarded;
		Placement.Android.AdUnitId = FString::Printf(
			TEXT("android-%s"),
			*PlacementName.ToString()
		);
		Placement.IOS.AdUnitId = FString::Printf(
			TEXT("ios-%s"),
			*PlacementName.ToString()
		);
	}

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before reward callback checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			if (
				Event.Type == EOpenMobileAdsEventType::RewardEarned
				|| Event.Type == EOpenMobileAdsEventType::Dismissed
			)
			{
				Events.Add(Event);
			}
		}
	);

	auto StartShow = [this, Subsystem, &Provider](FName PlacementName)
	{
		const FOpenMobileAdsOperationResult Load = Subsystem->LoadAd(PlacementName);
		TestTrue(TEXT("The reward callback placement starts loading"), Load.bAccepted);
		const FGuid CachedAdId = FGuid::NewGuid();
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = CachedAdId;
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
		const FOpenMobileAdsOperationResult Show = Subsystem->ShowAd(PlacementName);
		TestTrue(TEXT("The reward callback placement starts showing"), Show.bAccepted);
		return TPair<FGuid, FGuid>(Show.RequestId, CachedAdId);
	};
	auto MakeReward = [](FString Network, FString VerificationId)
	{
		FOpenMobileAdsEvent Reward;
		Reward.Type = EOpenMobileAdsEventType::RewardEarned;
		Reward.Network = MoveTemp(Network);
		Reward.bHasReward = true;
		Reward.Reward.Type = TEXT("coin");
		Reward.Reward.Amount = 10;
		Reward.Reward.bServerVerified = true;
		Reward.Reward.VerificationId = MoveTemp(VerificationId);
		return Reward;
	};
	auto MakeDismissed = []()
	{
		FOpenMobileAdsEvent Dismissed;
		Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
		return Dismissed;
	};

	const TPair<FGuid, FGuid> Before = StartShow(TEXT("RewardBeforeDismiss"));
	const FOpenMobileAdsEvent BeforeReward = MakeReward(TEXT("network-before"), TEXT("verify-before"));
	Provider.ShowSink->Submit(BeforeReward);
	Provider.ShowSink->Submit(BeforeReward);
	Provider.ShowSink->Submit(MakeDismissed());
	DrainGameThreadTasks();

	const TPair<FGuid, FGuid> After = StartShow(TEXT("RewardAfterDismiss"));
	Provider.ShowSink->Submit(MakeDismissed());
	const FOpenMobileAdsEvent AfterReward = MakeReward(TEXT("network-after"), TEXT("確認-after"));
	Provider.ShowSink->Submit(AfterReward);
	Provider.ShowSink->Submit(AfterReward);
	DrainGameThreadTasks();

	const TPair<FGuid, FGuid> NoReward = StartShow(TEXT("NoReward"));
	Provider.ShowSink->Submit(MakeDismissed());
	DrainGameThreadTasks();

	TestEqual(TEXT("The three shows emit two rewards and three dismissals"), Events.Num(), 5);
	if (Events.Num() == 5)
	{
		TestEqual(TEXT("The first reward precedes dismissal"), Events[0].Type, EOpenMobileAdsEventType::RewardEarned);
		TestEqual(TEXT("The first show then dismisses"), Events[1].Type, EOpenMobileAdsEventType::Dismissed);
		TestEqual(TEXT("The second show dismisses first"), Events[2].Type, EOpenMobileAdsEventType::Dismissed);
		TestEqual(TEXT("The second reward follows dismissal"), Events[3].Type, EOpenMobileAdsEventType::RewardEarned);
		TestEqual(TEXT("The no-reward show only dismisses"), Events[4].Type, EOpenMobileAdsEventType::Dismissed);

		const FOpenMobileAdsEvent& BeforeResult = Events[0];
		TestEqual(TEXT("Reward keeps its placement"), BeforeResult.Placement, FName(TEXT("RewardBeforeDismiss")));
		TestEqual(TEXT("Reward keeps its request"), BeforeResult.RequestId, Before.Key);
		TestEqual(TEXT("Reward keeps its cached ad"), BeforeResult.CachedAdId, Before.Value);
		TestEqual(TEXT("Reward keeps its provider"), BeforeResult.Provider, Provider.Name);
		TestEqual(TEXT("Reward keeps its network"), BeforeResult.Network, FString(TEXT("network-before")));
		TestEqual(TEXT("Reward before dismissal reports showing state"), BeforeResult.PlacementState, EOpenMobileAdPlacementState::Showing);
		TestTrue(TEXT("Reward remains grantable"), BeforeResult.bHasReward);
		TestEqual(TEXT("Reward keeps its type"), BeforeResult.Reward.Type, FString(TEXT("coin")));
		TestEqual(TEXT("Reward keeps its amount"), BeforeResult.Reward.Amount, static_cast<int64>(10));
		TestTrue(TEXT("Reward keeps server verification state"), BeforeResult.Reward.bServerVerified);
		TestEqual(TEXT("Reward keeps verification ID"), BeforeResult.Reward.VerificationId, FString(TEXT("verify-before")));

		const FOpenMobileAdsEvent& AfterResult = Events[3];
		TestEqual(TEXT("Late reward keeps its placement"), AfterResult.Placement, FName(TEXT("RewardAfterDismiss")));
		TestEqual(TEXT("Late reward keeps its request"), AfterResult.RequestId, After.Key);
		TestEqual(TEXT("Late reward keeps its cached ad"), AfterResult.CachedAdId, After.Value);
		TestEqual(TEXT("Late reward keeps its provider"), AfterResult.Provider, Provider.Name);
		TestEqual(TEXT("Late reward keeps its network"), AfterResult.Network, FString(TEXT("network-after")));
		TestEqual(TEXT("Reward after dismissal reports idle state"), AfterResult.PlacementState, EOpenMobileAdPlacementState::Idle);
		TestTrue(TEXT("Late reward remains grantable"), AfterResult.bHasReward);
		TestTrue(TEXT("Late reward keeps server verification state"), AfterResult.Reward.bServerVerified);
		TestEqual(TEXT("Late reward keeps Unicode verification ID"), AfterResult.Reward.VerificationId, FString(TEXT("確認-after")));
		TestEqual(TEXT("No-reward dismissal keeps its request"), Events[4].RequestId, NoReward.Key);
	}

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsDestroyLifecycleContractTest,
	"OpenMobile.Ads.ProviderContract.Destroy.Lifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsDestroyLifecycleContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("DestroyReward");
	Placement.Android.AdUnitId = TEXT("android-destroy-unit");
	Placement.IOS.AdUnitId = TEXT("ios-destroy-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before destroy checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	auto LoadReady = [this, Subsystem, &Provider](FGuid CachedAdId)
	{
		const FOpenMobileAdsOperationResult Load =
			Subsystem->LoadAd(TEXT("DestroyReward"));
		TestTrue(TEXT("The placement loads before cached destroy"), Load.bAccepted);
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = CachedAdId;
		Provider.LoadSink->Submit(MoveTemp(Loaded));
		DrainGameThreadTasks();
		TestTrue(TEXT("The placement is ready before cached destroy"), Subsystem->IsReady(TEXT("DestroyReward")));
	};

	const FOpenMobileAdsOperationResult Load =
		Subsystem->LoadAd(TEXT("DestroyReward"));
	TestTrue(TEXT("The load starts before destroy"), Load.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> LoadSink =
		Provider.LoadSink;

	const FOpenMobileAdsOperationResult Destroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestTrue(TEXT("Destroy can replace an active load"), Destroy.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> FirstDestroySink =
		Provider.DestroySink;
	TestEqual(TEXT("Destroy reaches the provider once"), Provider.DestroyCalls, 1);
	TestEqual(TEXT("Destroy cancels the active native load"), Provider.CancelledRequests.Num(), 1);
	if (Provider.CancelledRequests.Num() == 1)
	{
		TestEqual(TEXT("The cancelled request is the replaced load"), Provider.CancelledRequests[0], Load.RequestId);
	}
	TestFalse(
		TEXT("The replaced load request is no longer active"),
		Subsystem->CancelRequest(Load.RequestId).bAccepted
	);
	const FOpenMobileAdsOperationResult DuplicateDestroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestFalse(TEXT("A placement cannot start two destroy operations"), DuplicateDestroy.bAccepted);
	TestEqual(TEXT("Duplicate destroy is typed as busy"), DuplicateDestroy.Error.Code, EOpenMobileAdsErrorCode::Busy);
	TestEqual(TEXT("Duplicate destroy does not reach the provider"), Provider.DestroyCalls, 1);
	if (LoadSink)
	{
		FOpenMobileAdsEvent LateLoaded;
		LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
		LateLoaded.CachedAdId = FGuid::NewGuid();
		LoadSink->Submit(MoveTemp(LateLoaded));
		DrainGameThreadTasks();
	}
	TestFalse(TEXT("A late load cannot survive destroy"), Subsystem->IsReady(TEXT("DestroyReward")));

	if (FirstDestroySink)
	{
		FOpenMobileAdsEvent Destroyed;
		Destroyed.Type = EOpenMobileAdsEventType::Destroyed;
		FirstDestroySink->Submit(MoveTemp(Destroyed));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("Destroy completion returns the placement to idle"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).State,
		EOpenMobileAdPlacementState::Idle
	);

	const FGuid RejectedCacheId = FGuid::NewGuid();
	LoadReady(RejectedCacheId);
	Provider.bAcceptDestroy = false;
	Provider.DestroyRejection = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Teardown,
		TEXT("DestroyReward"),
		TEXT("The mock provider rejected destroy."),
		Provider.Name
	);
	const FOpenMobileAdsOperationResult Rejected =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestFalse(TEXT("Provider destroy rejection is immediate"), Rejected.bAccepted);
	TestEqual(TEXT("Provider destroy rejection is typed"), Rejected.Error.Code, EOpenMobileAdsErrorCode::ProviderFailure);
	TestEqual(TEXT("Provider rejection has no request ID"), Rejected.RequestId.IsValid(), false);
	TestTrue(TEXT("Provider rejection preserves cached readiness"), Subsystem->IsReady(TEXT("DestroyReward")));
	TestEqual(
		TEXT("Provider rejection preserves the cache identity"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).CachedAdId,
		RejectedCacheId
	);
	Provider.bAcceptDestroy = true;

	const FOpenMobileAdsOperationResult CachedDestroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestTrue(TEXT("A ready cached ad can be destroyed"), CachedDestroy.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> CachedDestroySink =
		Provider.DestroySink;
	if (CachedDestroySink)
	{
		FOpenMobileAdsEvent Destroyed;
		Destroyed.Type = EOpenMobileAdsEventType::Destroyed;
		CachedDestroySink->Submit(MoveTemp(Destroyed));
		DrainGameThreadTasks();
	}
	TestEqual(TEXT("Completed cached destroy releases one native ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Cached destroy releases the matching identity"), Provider.ReleasedCachedAds[0], RejectedCacheId);
	}
	TestFalse(TEXT("Completed destroy invalidates cached readiness"), Subsystem->IsReady(TEXT("DestroyReward")));

	const FGuid ShowingCacheId = FGuid::NewGuid();
	LoadReady(ShowingCacheId);
	const FOpenMobileAdsOperationResult Show =
		Subsystem->ShowAd(TEXT("DestroyReward"));
	TestTrue(TEXT("The placement starts showing before destroy"), Show.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShowSink =
		Provider.ShowSink;
	const FOpenMobileAdsOperationResult ShowingDestroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestTrue(TEXT("Destroy can replace an active show"), ShowingDestroy.bAccepted);
	TestEqual(TEXT("Destroy cancels the active native show"), Provider.CancelledRequests.Num(), 2);
	if (Provider.CancelledRequests.Num() == 2)
	{
		TestEqual(TEXT("The cancelled request is the replaced show"), Provider.CancelledRequests[1], Show.RequestId);
	}
	if (ShowSink)
	{
		FOpenMobileAdsEvent LateDismissed;
		LateDismissed.Type = EOpenMobileAdsEventType::Dismissed;
		ShowSink->Submit(MoveTemp(LateDismissed));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("A late show callback cannot finish the destroy"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).State,
		EOpenMobileAdPlacementState::Destroying
	);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> CancelledDestroySink =
		Provider.DestroySink;
	TestTrue(
		TEXT("An accepted destroy can be cancelled safely"),
		Subsystem->CancelRequest(ShowingDestroy.RequestId).bAccepted
	);
	DrainGameThreadTasks();
	TestEqual(TEXT("Destroy cancellation reaches the provider"), Provider.CancelledRequests.Num(), 3);
	TestEqual(
		TEXT("Destroy cancellation leaves an idle placement"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).State,
		EOpenMobileAdPlacementState::Idle
	);
	TestFalse(TEXT("Destroy cancellation does not restore a stale cache"), Subsystem->IsReady(TEXT("DestroyReward")));
	TestEqual(TEXT("Destroy cancellation releases the shown cache"), Provider.ReleasedCachedAds.Num(), 2);
	if (CancelledDestroySink)
	{
		FOpenMobileAdsEvent LateDestroyed;
		LateDestroyed.Type = EOpenMobileAdsEventType::Destroyed;
		CancelledDestroySink->Submit(MoveTemp(LateDestroyed));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("A late destroy callback cannot change cancelled state"),
		Subsystem->GetPlacementStatus(TEXT("DestroyReward")).State,
		EOpenMobileAdPlacementState::Idle
	);

	const FGuid ShutdownCacheId = FGuid::NewGuid();
	LoadReady(ShutdownCacheId);
	const FOpenMobileAdsOperationResult ShutdownDestroy =
		Subsystem->DestroyAd(TEXT("DestroyReward"));
	TestTrue(TEXT("Destroy starts before subsystem shutdown"), ShutdownDestroy.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> ShutdownDestroySink =
		Provider.DestroySink;

	Subsystem->Deinitialize();
	TestEqual(TEXT("Shutdown cancels the active destroy"), Provider.CancelledRequests.Num(), 4);
	if (Provider.CancelledRequests.Num() == 4)
	{
		TestEqual(TEXT("Shutdown cancels the destroy request ID"), Provider.CancelledRequests[3], ShutdownDestroy.RequestId);
	}
	TestEqual(TEXT("Shutdown releases the remaining cached ad"), Provider.ReleasedCachedAds.Num(), 3);
	if (ShutdownDestroySink)
	{
		FOpenMobileAdsEvent LateDestroyed;
		LateDestroyed.Type = EOpenMobileAdsEventType::Destroyed;
		ShutdownDestroySink->Submit(MoveTemp(LateDestroyed));
		DrainGameThreadTasks();
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsDestroyAllLifecycleContractTest,
	"OpenMobile.Ads.ProviderContract.Destroy.AllLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsDestroyAllLifecycleContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	for (const FName PlacementName : {FName(TEXT("DestroyOne")), FName(TEXT("DestroyTwo"))})
	{
		FOpenMobileAdsPlacementSettings& Placement =
			ScopedSettings.Settings->Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.Android.AdUnitId = FString::Printf(TEXT("android-%s"), *PlacementName.ToString());
		Placement.IOS.AdUnitId = FString::Printf(TEXT("ios-%s"), *PlacementName.ToString());
	}

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before destroy-all checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	TestTrue(TEXT("The cached placement starts loading"), Subsystem->LoadAd(TEXT("DestroyTwo")).bAccepted);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	const FOpenMobileAdsOperationResult ActiveLoad =
		Subsystem->LoadAd(TEXT("DestroyOne"));
	TestTrue(TEXT("Another placement is loading before destroy all"), ActiveLoad.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> LoadSink =
		Provider.LoadSink;

	const FOpenMobileAdsOperationResult DestroyAll = Subsystem->DestroyAllAds();
	TestTrue(TEXT("Service-wide destroy is accepted"), DestroyAll.bAccepted);
	TestTrue(TEXT("Service-wide destroy has a request ID"), DestroyAll.RequestId.IsValid());
	TestTrue(TEXT("The provider receives an all-placements request"), Provider.LastDestroyRequest.bAllPlacements);
	TestEqual(TEXT("Service-wide destroy cancels active loading"), Provider.CancelledRequests.Num(), 1);
	if (Provider.CancelledRequests.Num() == 1)
	{
		TestEqual(TEXT("Destroy all cancels the loading request"), Provider.CancelledRequests[0], ActiveLoad.RequestId);
	}
	TestEqual(
		TEXT("The loading placement enters destroying state"),
		Subsystem->GetPlacementStatus(TEXT("DestroyOne")).State,
		EOpenMobileAdPlacementState::Destroying
	);
	TestEqual(
		TEXT("The cached placement enters destroying state"),
		Subsystem->GetPlacementStatus(TEXT("DestroyTwo")).State,
		EOpenMobileAdPlacementState::Destroying
	);
	const FOpenMobileAdsOperationResult Duplicate = Subsystem->DestroyAllAds();
	TestFalse(TEXT("Concurrent service-wide destroy is rejected"), Duplicate.bAccepted);
	TestEqual(TEXT("Concurrent service-wide destroy is busy"), Duplicate.Error.Code, EOpenMobileAdsErrorCode::Busy);
	TestEqual(TEXT("Concurrent service-wide destroy does not reach the provider"), Provider.DestroyCalls, 1);
	if (LoadSink)
	{
		FOpenMobileAdsEvent LateLoaded;
		LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
		LateLoaded.CachedAdId = FGuid::NewGuid();
		LoadSink->Submit(MoveTemp(LateLoaded));
		DrainGameThreadTasks();
	}
	TestFalse(TEXT("Late loading cannot escape destroy all"), Subsystem->IsReady(TEXT("DestroyOne")));

	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> DestroySink =
		Provider.DestroySink;
	if (DestroySink)
	{
		FOpenMobileAdsEvent Destroyed;
		Destroyed.Type = EOpenMobileAdsEventType::Destroyed;
		DestroySink->Submit(MoveTemp(Destroyed));
		DrainGameThreadTasks();
	}
	TestEqual(TEXT("Destroy all releases the cached native ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Destroy all releases the matching cache identity"), Provider.ReleasedCachedAds[0], CachedAdId);
	}
	TestEqual(
		TEXT("Destroy all clears the first placement"),
		Subsystem->GetPlacementStatus(TEXT("DestroyOne")).State,
		EOpenMobileAdPlacementState::Idle
	);
	TestEqual(
		TEXT("Destroy all clears the second placement"),
		Subsystem->GetPlacementStatus(TEXT("DestroyTwo")).State,
		EOpenMobileAdPlacementState::Idle
	);
	TestFalse(
		TEXT("Completed service-wide destroy is terminal"),
		Subsystem->CancelRequest(DestroyAll.RequestId).bAccepted
	);
	if (DestroySink)
	{
		FOpenMobileAdsEvent LateDestroyed;
		LateDestroyed.Type = EOpenMobileAdsEventType::Destroyed;
		DestroySink->Submit(MoveTemp(LateDestroyed));
		DrainGameThreadTasks();
	}

	TestTrue(TEXT("A placement can load after destroy all"), Subsystem->LoadAd(TEXT("DestroyTwo")).bAccepted);
	const FGuid CancelledCacheId = FGuid::NewGuid();
	FOpenMobileAdsEvent Reloaded;
	Reloaded.Type = EOpenMobileAdsEventType::Loaded;
	Reloaded.CachedAdId = CancelledCacheId;
	Provider.LoadSink->Submit(MoveTemp(Reloaded));
	DrainGameThreadTasks();
	const FOpenMobileAdsOperationResult CancelledDestroyAll = Subsystem->DestroyAllAds();
	TestTrue(TEXT("A second destroy all can start after completion"), CancelledDestroyAll.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> CancelledSink =
		Provider.DestroySink;
	TestTrue(
		TEXT("Service-wide destroy cancellation is accepted"),
		Subsystem->CancelRequest(CancelledDestroyAll.RequestId).bAccepted
	);
	DrainGameThreadTasks();
	TestFalse(TEXT("Cancelled destroy all does not restore cached readiness"), Subsystem->IsReady(TEXT("DestroyTwo")));
	TestEqual(
		TEXT("Cancelled destroy all leaves the placement idle"),
		Subsystem->GetPlacementStatus(TEXT("DestroyTwo")).State,
		EOpenMobileAdPlacementState::Idle
	);
	TestEqual(TEXT("Cancelled destroy all releases its cached ad"), Provider.ReleasedCachedAds.Num(), 2);
	if (CancelledSink)
	{
		FOpenMobileAdsEvent LateDestroyed;
		LateDestroyed.Type = EOpenMobileAdsEventType::Destroyed;
		CancelledSink->Submit(MoveTemp(LateDestroyed));
		DrainGameThreadTasks();
	}

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCacheContractTest,
	"OpenMobile.Ads.ProviderContract.Cache.Ownership",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCacheContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	for (const FName PlacementName : {FName(TEXT("RewardOne")), FName(TEXT("RewardTwo"))})
	{
		FOpenMobileAdsPlacementSettings& Placement =
			ScopedSettings.Settings->Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.Android.AdUnitId = FString::Printf(TEXT("android-%s"), *PlacementName.ToString());
		Placement.IOS.AdUnitId = FString::Printf(TEXT("ios-%s"), *PlacementName.ToString());
	}

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].MaxCachedAdsPerPlacement = 2;
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before cache operations"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	const FOpenMobileAdFormatCapabilities* Rewarded =
		Provider.Capabilities.FindFormat(EOpenMobileAdFormat::Rewarded);
	TestNotNull(TEXT("The mock provider advertises rewarded cache policy"), Rewarded);
	if (Rewarded)
	{
		TestEqual(TEXT("Provider cache capacity remains inspectable"), Rewarded->MaxCachedAdsPerPlacement, 2);
	}

	const FGuid FirstCachedAdId = FGuid::NewGuid();
	TestTrue(TEXT("The first placement load starts"), Subsystem->LoadAd(TEXT("RewardOne")).bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> FirstLoadSink =
		Provider.LoadSink;
	FOpenMobileAdsEvent FirstLoaded;
	FirstLoaded.Type = EOpenMobileAdsEventType::Loaded;
	FirstLoaded.CachedAdId = FirstCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(FirstLoaded));
	DrainGameThreadTasks();

	const FGuid SecondCachedAdId = FGuid::NewGuid();
	TestTrue(TEXT("The second placement load starts independently"), Subsystem->LoadAd(TEXT("RewardTwo")).bAccepted);
	FOpenMobileAdsEvent SecondLoaded;
	SecondLoaded.Type = EOpenMobileAdsEventType::Loaded;
	SecondLoaded.CachedAdId = SecondCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(SecondLoaded));
	DrainGameThreadTasks();
	TestEqual(
		TEXT("The first placement retains its cache identity"),
		Subsystem->GetPlacementStatus(TEXT("RewardOne")).CachedAdId,
		FirstCachedAdId
	);
	TestEqual(
		TEXT("The second placement has an independent cache identity"),
		Subsystem->GetPlacementStatus(TEXT("RewardTwo")).CachedAdId,
		SecondCachedAdId
	);

	FOpenMobileAdsLoadOptions ForceReload;
	ForceReload.bForceReload = true;
	const FOpenMobileAdsOperationResult CancelledReplacement =
		Subsystem->LoadAd(TEXT("RewardOne"), ForceReload);
	TestTrue(TEXT("A replacement load can be cancelled"), CancelledReplacement.bAccepted);
	TestTrue(
		TEXT("Cancelling a replacement load succeeds"),
		Subsystem->CancelRequest(CancelledReplacement.RequestId).bAccepted
	);
	DrainGameThreadTasks();
	TestEqual(
		TEXT("Cancellation restores the prior cache identity"),
		Subsystem->GetPlacementStatus(TEXT("RewardOne")).CachedAdId,
		FirstCachedAdId
	);
	TestTrue(TEXT("Cancellation restores ready state"), Subsystem->IsReady(TEXT("RewardOne")));
	TestTrue(TEXT("Cancellation does not release the prior cache"), Provider.ReleasedCachedAds.IsEmpty());

	TestTrue(
		TEXT("The first placement starts a replacement load"),
		Subsystem->LoadAd(TEXT("RewardOne"), ForceReload).bAccepted
	);
	const FGuid ReplacementCachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent ReplacementLoaded;
	ReplacementLoaded.Type = EOpenMobileAdsEventType::Loaded;
	ReplacementLoaded.CachedAdId = ReplacementCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(ReplacementLoaded));
	DrainGameThreadTasks();
	TestEqual(TEXT("Replacement releases one cached ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Replacement releases the prior cache identity"), Provider.ReleasedCachedAds[0], FirstCachedAdId);
	}
	TestEqual(
		TEXT("Replacement keeps the second placement unchanged"),
		Subsystem->GetPlacementStatus(TEXT("RewardTwo")).CachedAdId,
		SecondCachedAdId
	);

	if (FirstLoadSink)
	{
		FOpenMobileAdsEvent StaleLoaded;
		StaleLoaded.Type = EOpenMobileAdsEventType::Loaded;
		StaleLoaded.CachedAdId = FGuid::NewGuid();
		FirstLoadSink->Submit(MoveTemp(StaleLoaded));
		DrainGameThreadTasks();
	}
	TestEqual(
		TEXT("A stale load callback cannot replace the current cache"),
		Subsystem->GetPlacementStatus(TEXT("RewardOne")).CachedAdId,
		ReplacementCachedAdId
	);

	TestTrue(TEXT("The cached first placement can start showing"), Subsystem->ShowAd(TEXT("RewardOne")).bAccepted);
	FOpenMobileAdsEvent ShowFailed;
	ShowFailed.Type = EOpenMobileAdsEventType::Failed;
	ShowFailed.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Show,
		TEXT("RewardOne"),
		TEXT("Cached ad became unavailable.")
	);
	AddExpectedError(
		TEXT("Cached ad became unavailable."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.ShowSink->Submit(MoveTemp(ShowFailed));
	DrainGameThreadTasks();
	TestFalse(
		TEXT("Provider failure invalidates the affected cache"),
		Subsystem->GetPlacementStatus(TEXT("RewardOne")).CachedAdId.IsValid()
	);
	TestEqual(TEXT("Provider failure releases the affected native cache"), Provider.ReleasedCachedAds.Num(), 2);
	TestTrue(TEXT("Provider failure does not clear another placement"), Subsystem->IsReady(TEXT("RewardTwo")));

	AddExpectedError(
		TEXT("The ads provider was unregistered during an active placement operation."),
		EAutomationExpectedErrorFlags::Contains,
		2
	);
	Registration.Unregister();
	DrainGameThreadTasks();
	TestFalse(
		TEXT("Provider unregistration invalidates remaining cached state"),
		Subsystem->GetPlacementStatus(TEXT("RewardTwo")).CachedAdId.IsValid()
	);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCacheIdentityContractTest,
	"OpenMobile.Ads.ProviderContract.Cache.Identity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCacheIdentityContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("IdentityReward");
	Placement.Android.AdUnitId = TEXT("android-identity");
	Placement.IOS.AdUnitId = TEXT("ios-identity");

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].MaxCachedAdsPerPlacement = 0;
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before cache identity checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);
	const FOpenMobileAdsOperationResult NoCapacity =
		Subsystem->LoadAd(TEXT("IdentityReward"));
	TestFalse(TEXT("A provider without cache capacity rejects loading"), NoCapacity.bAccepted);
	TestEqual(
		TEXT("Invalid provider cache capacity is typed"),
		NoCapacity.Error.Code,
		EOpenMobileAdsErrorCode::ProviderFailure
	);
	Provider.Capabilities.Formats[0].MaxCachedAdsPerPlacement = 1;
	Provider.Capabilities.Formats[0].CacheLifetimeSeconds = -1.0;
	const FOpenMobileAdsOperationResult InvalidLifetime =
		Subsystem->LoadAd(TEXT("IdentityReward"));
	TestFalse(TEXT("A provider with a negative cache lifetime rejects loading"), InvalidLifetime.bAccepted);
	TestEqual(
		TEXT("Invalid provider cache lifetime is typed"),
		InvalidLifetime.Error.Code,
		EOpenMobileAdsErrorCode::ProviderFailure
	);
	Provider.Capabilities.Formats[0].CacheLifetimeSeconds = 0.0;

	TestTrue(TEXT("The first load starts"), Subsystem->LoadAd(TEXT("IdentityReward")).bAccepted);
	FOpenMobileAdsEvent MissingIdentity;
	MissingIdentity.Type = EOpenMobileAdsEventType::Loaded;
	AddExpectedError(
		TEXT("The ads provider reported a loaded ad without a cache identity."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.LoadSink->Submit(MoveTemp(MissingIdentity));
	DrainGameThreadTasks();
	const FOpenMobileAdsPlacementStatus FailedStatus =
		Subsystem->GetPlacementStatus(TEXT("IdentityReward"));
	TestEqual(TEXT("A missing cache identity fails the load"), FailedStatus.State, EOpenMobileAdPlacementState::Failed);
	TestEqual(
		TEXT("A missing cache identity is a provider failure"),
		FailedStatus.LastError.Code,
		EOpenMobileAdsErrorCode::ProviderFailure
	);
	TestFalse(TEXT("The service does not invent a cache identity"), FailedStatus.CachedAdId.IsValid());

	TestTrue(TEXT("A load can retry after the malformed callback"), Subsystem->LoadAd(TEXT("IdentityReward")).bAccepted);
	const FGuid StableCachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = StableCachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();

	FOpenMobileAdsLoadOptions ForceReload;
	ForceReload.bForceReload = true;
	TestTrue(TEXT("A forced replacement starts"), Subsystem->LoadAd(TEXT("IdentityReward"), ForceReload).bAccepted);
	FOpenMobileAdsEvent MalformedReplacement;
	MalformedReplacement.Type = EOpenMobileAdsEventType::Loaded;
	AddExpectedError(
		TEXT("The ads provider reported a loaded ad without a cache identity."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Provider.LoadSink->Submit(MoveTemp(MalformedReplacement));
	DrainGameThreadTasks();
	TestEqual(
		TEXT("A malformed replacement preserves the prior cache"),
		Subsystem->GetPlacementStatus(TEXT("IdentityReward")).CachedAdId,
		StableCachedAdId
	);
	TestTrue(TEXT("A malformed replacement keeps the prior ad ready"), Subsystem->IsReady(TEXT("IdentityReward")));
	TestTrue(TEXT("A malformed replacement does not release the prior ad"), Provider.ReleasedCachedAds.IsEmpty());

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCacheExpirationContractTest,
	"OpenMobile.Ads.ProviderContract.Cache.Expiration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCacheExpirationContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ExpiringReward");
	Placement.Android.AdUnitId = TEXT("android-expiring");
	Placement.IOS.AdUnitId = TEXT("ios-expiring");
	FOpenMobileAdsPlacementSettings& PreloadedPlacement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	PreloadedPlacement.Placement = TEXT("PreloadedExpiringReward");
	PreloadedPlacement.bPreload = true;
	PreloadedPlacement.Android.AdUnitId = TEXT("android-preloaded-expiring");
	PreloadedPlacement.IOS.AdUnitId = TEXT("ios-preloaded-expiring");

	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.Formats[0].CacheLifetimeSeconds = 600.0;
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	const TSharedRef<FControlledAdsClock> Clock =
		MakeShared<FControlledAdsClock>();
	FOpenMobileAdsClockTestAccess::SetClock(*Subsystem, Clock);
	TestTrue(
		TEXT("The provider initializes before expiration checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);
	TestTrue(TEXT("The expiring placement starts loading"), Subsystem->LoadAd(TEXT("ExpiringReward")).bAccepted);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Loaded.Timestamp = Clock->UtcNow();
	Loaded.CacheExpiresAt = Clock->UtcNow() + FTimespan::FromSeconds(10.0);
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();

	const FOpenMobileAdsPlacementStatus ReadyStatus =
		Subsystem->GetPlacementStatus(TEXT("ExpiringReward"));
	TestEqual(TEXT("The cache records its load timestamp"), ReadyStatus.CachedAt, Events.Last().Timestamp);
	TestEqual(
		TEXT("The cache records provider-reported expiration"),
		ReadyStatus.ExpiresAt,
		Clock->UtcNow() + FTimespan::FromSeconds(10.0)
	);
	TestEqual(
		TEXT("The loaded event exposes the resolved expiration"),
		Events.Last().CacheExpiresAt,
		ReadyStatus.ExpiresAt
	);
	Clock->Advance(9.999);
	TestTrue(
		TEXT("The cache is ready immediately before expiration"),
		Subsystem->IsReady(TEXT("ExpiringReward"))
	);
	Clock->Advance(0.001);
	TestFalse(
		TEXT("The cache is not ready at its exact expiration"),
		Subsystem->IsReady(TEXT("ExpiringReward"))
	);
	TestEqual(
		TEXT("CanShow reports the exact boundary as expired"),
		Subsystem->CanShow(TEXT("ExpiringReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::Expired
	);

	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	DrainGameThreadTasks();
	const FOpenMobileAdsPlacementStatus ExpiredStatus =
		Subsystem->GetPlacementStatus(TEXT("ExpiringReward"));
	TestEqual(TEXT("Expiration returns the placement to idle"), ExpiredStatus.State, EOpenMobileAdPlacementState::Idle);
	TestFalse(TEXT("Expiration invalidates the public cache ID"), ExpiredStatus.CachedAdId.IsValid());
	TestEqual(TEXT("Expiration clears the load timestamp"), ExpiredStatus.CachedAt, FDateTime());
	TestEqual(TEXT("Expiration clears the deadline"), ExpiredStatus.ExpiresAt, FDateTime());
	TestEqual(TEXT("Expiration releases the native cached ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Expiration releases the matching cache identity"), Provider.ReleasedCachedAds[0], CachedAdId);
	}
	TestTrue(
		TEXT("Expiration broadcasts a normalized event"),
		Events.ContainsByPredicate([](const FOpenMobileAdsEvent& Event)
		{
			return Event.Type == EOpenMobileAdsEventType::Expired;
		})
	);

	TestTrue(
		TEXT("The preload placement starts loading"),
		Subsystem->LoadAd(TEXT("PreloadedExpiringReward")).bAccepted
	);
	const FGuid PreloadedCachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Preloaded;
	Preloaded.Type = EOpenMobileAdsEventType::Loaded;
	Preloaded.CachedAdId = PreloadedCachedAdId;
	Preloaded.Timestamp = Clock->UtcNow();
	Preloaded.CacheExpiresAt = Clock->UtcNow() + FTimespan::FromSeconds(60.0);
	Provider.LoadSink->Submit(MoveTemp(Preloaded));
	DrainGameThreadTasks();
	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	Clock->ShiftWallClock(-3600.0);
	Clock->CurrentMonotonicSeconds += 60.0;
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	DrainGameThreadTasks();
	TestEqual(TEXT("Both expired caches release native state"), Provider.ReleasedCachedAds.Num(), 2);
	if (Provider.ReleasedCachedAds.Num() == 2)
	{
		TestEqual(
			TEXT("Monotonic time expires a cache after a backward wall-clock change"),
			Provider.ReleasedCachedAds[1],
			PreloadedCachedAdId
		);
	}
	TestEqual(
		TEXT("Expiration preloads a replacement when configured"),
		Provider.LoadCalls,
		3
	);
	TestEqual(
		TEXT("The replacement enters loading state"),
		Subsystem->GetPlacementStatus(TEXT("PreloadedExpiringReward")).State,
		EOpenMobileAdPlacementState::Loading
	);

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCoppaContractTest,
	"OpenMobile.Ads.Privacy.Coppa.ConfigurationLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCoppaContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent = false;

	const EOpenMobileAdsAgeTreatment Treatments[] = {
		EOpenMobileAdsAgeTreatment::Unspecified,
		EOpenMobileAdsAgeTreatment::Yes,
		EOpenMobileAdsAgeTreatment::No
	};
	for (const EOpenMobileAdsAgeTreatment Treatment : Treatments)
	{
		ScopedSettings.Settings->Privacy.ChildDirectedTreatment = Treatment;
		FMockProvider Provider(TEXT("MockAds"));
		FScopedProviderRegistration Registration(Provider);
		UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
			NewObject<UGameInstance>()
		);

		const FOpenMobileAdsOperationResult Started = Subsystem->InitializeAds();
		TestTrue(TEXT("COPPA configuration is accepted"), Started.bAccepted);
		TestEqual(
			TEXT("COPPA configuration reaches the provider unchanged"),
			Provider.LastInitializationRequest.Privacy.ChildDirectedTreatment,
			Treatment
		);
		Subsystem->Deinitialize();
	}

	ScopedSettings.Settings->Privacy.ChildDirectedTreatment =
		EOpenMobileAdsAgeTreatment::No;
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	FOpenMobileAdsPrivacySnapshot Privacy = Subsystem->GetPrivacySnapshot();
	Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::Yes;
	TestTrue(
		TEXT("A pre-start COPPA update is accepted"),
		Subsystem->UpdatePrivacySnapshot(Privacy).bAccepted
	);
	TestTrue(
		TEXT("Initialization accepts a pre-start COPPA update"),
		Subsystem->InitializeAds().bAccepted
	);
	TestEqual(
		TEXT("The pre-start COPPA value reaches provider initialization"),
		Provider.LastInitializationRequest.Privacy.ChildDirectedTreatment,
		EOpenMobileAdsAgeTreatment::Yes
	);

	int32 PrivacyChanges = 0;
	const FDelegateHandle PrivacyHandle =
		Subsystem->OnNativeConsentStatusChanged().AddLambda(
			[&PrivacyChanges](const FOpenMobileAdsPrivacySnapshot& Snapshot)
			{
				++PrivacyChanges;
			}
		);
	Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::No;
	const FOpenMobileAdsOperationResult RejectedUpdate =
		Subsystem->UpdatePrivacySnapshot(Privacy);
	TestFalse(
		TEXT("A post-start COPPA update is rejected"),
		RejectedUpdate.bAccepted
	);
	TestEqual(
		TEXT("A post-start COPPA update returns an invalid-state error"),
		RejectedUpdate.Error.Code,
		EOpenMobileAdsErrorCode::InvalidState
	);
	TestEqual(
		TEXT("A post-start COPPA update identifies the consent stage"),
		RejectedUpdate.Error.Stage,
		EOpenMobileAdsFailureStage::Consent
	);
	TestEqual(
		TEXT("COPPA configuration stays fixed after provider initialization starts"),
		Subsystem->GetPrivacySnapshot().ChildDirectedTreatment,
		EOpenMobileAdsAgeTreatment::Yes
	);
	TestEqual(
		TEXT("A rejected COPPA update does not broadcast a privacy change"),
		PrivacyChanges,
		0
	);

	Subsystem->OnNativeConsentStatusChanged().Remove(PrivacyHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsUnderAgeContractTest,
	"OpenMobile.Ads.Privacy.UnderAge.ConfigurationLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsUnderAgeContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->bDevelopmentTestMode = true;
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent = false;
	ScopedSettings.Settings->Privacy.ChildDirectedTreatment =
		EOpenMobileAdsAgeTreatment::No;

	const EOpenMobileAdsAgeTreatment Treatments[] = {
		EOpenMobileAdsAgeTreatment::Unspecified,
		EOpenMobileAdsAgeTreatment::No,
		EOpenMobileAdsAgeTreatment::Yes
	};
	for (const EOpenMobileAdsAgeTreatment Treatment : Treatments)
	{
		ScopedSettings.Settings->Privacy.UnderAgeOfConsent = Treatment;
		FMockProvider Provider(TEXT("MockAds"));
		FScopedProviderRegistration Registration(Provider);
		UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
			NewObject<UGameInstance>()
		);

		const FOpenMobileAdsOperationResult Started = Subsystem->InitializeAds();
		TestTrue(TEXT("Under-age configuration is accepted"), Started.bAccepted);
		TestEqual(
			TEXT("Under-age configuration reaches the provider unchanged"),
			Provider.LastInitializationRequest.Privacy.UnderAgeOfConsent,
			Treatment
		);
		TestEqual(
			TEXT("Under-age configuration does not change COPPA treatment"),
			Provider.LastInitializationRequest.Privacy.ChildDirectedTreatment,
			EOpenMobileAdsAgeTreatment::No
		);
		TestEqual(
			TEXT("Under-age treatment restricts consent debug intent"),
			Provider.LastInitializationRequest.Development.bEnableConsentDebug,
			Treatment != EOpenMobileAdsAgeTreatment::Yes
		);
		Subsystem->Deinitialize();
	}

	ScopedSettings.Settings->bDevelopmentTestMode = false;
	ScopedSettings.Settings->Privacy.UnderAgeOfConsent =
		EOpenMobileAdsAgeTreatment::No;
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	FOpenMobileAdsPrivacySnapshot Privacy = Subsystem->GetPrivacySnapshot();
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::NotRequired;
	Privacy.bConsentStatusFresh = true;
	Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::No;
	Privacy.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::Yes;
	TestTrue(
		TEXT("A pre-start under-age update is accepted"),
		Subsystem->UpdatePrivacySnapshot(Privacy).bAccepted
	);
	TestTrue(
		TEXT("Initialization accepts a pre-start under-age update"),
		Subsystem->InitializeAds().bAccepted
	);
	TestEqual(
		TEXT("The pre-start under-age value reaches provider initialization"),
		Provider.LastInitializationRequest.Privacy.UnderAgeOfConsent,
		EOpenMobileAdsAgeTreatment::Yes
	);

	Provider.CompleteInitialization();
	DrainGameThreadTasks();
	Provider.bBlockUnderAgeRequests = true;
	TestEqual(
		TEXT("Under-age treatment reaches ad-request policy"),
		Subsystem->CanRequestAds().BlockReason,
		EOpenMobileAdsCanRequestAdsBlockReason::ProviderPolicy
	);

	int32 PrivacyChanges = 0;
	const FDelegateHandle PrivacyHandle =
		Subsystem->OnNativeConsentStatusChanged().AddLambda(
			[&PrivacyChanges](const FOpenMobileAdsPrivacySnapshot& Snapshot)
			{
				++PrivacyChanges;
			}
		);
	Privacy.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::No;
	const FOpenMobileAdsOperationResult RejectedUpdate =
		Subsystem->UpdatePrivacySnapshot(Privacy);
	TestFalse(
		TEXT("A post-start under-age update is rejected"),
		RejectedUpdate.bAccepted
	);
	TestEqual(
		TEXT("A post-start under-age update returns an invalid-state error"),
		RejectedUpdate.Error.Code,
		EOpenMobileAdsErrorCode::InvalidState
	);
	TestEqual(
		TEXT("A post-start under-age update identifies the consent stage"),
		RejectedUpdate.Error.Stage,
		EOpenMobileAdsFailureStage::Consent
	);
	TestEqual(
		TEXT("Under-age configuration stays fixed after initialization starts"),
		Subsystem->GetPrivacySnapshot().UnderAgeOfConsent,
		EOpenMobileAdsAgeTreatment::Yes
	);
	TestEqual(
		TEXT("A rejected under-age update does not broadcast a privacy change"),
		PrivacyChanges,
		0
	);

	Subsystem->OnNativeConsentStatusChanged().Remove(PrivacyHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsGdprRequestGateContractTest,
	"OpenMobile.Ads.Privacy.Gdpr.RequestGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsGdprRequestGateContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		true;
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before GDPR request gating"),
		InitializeSuccessfully(*Subsystem, Provider, false)
	);

	auto ApplyState = [Subsystem](
		EOpenMobileAdsConsentStatus Status,
		EOpenMobileAdsGdprApplicability Applicability,
		EOpenMobileAdsConsentRequirement Requirement,
		EOpenMobileAdsConsentRequestState RequestState,
		FDateTime ExpiresAt = FDateTime()
	)
	{
		Subsystem->ApplyConsentStatusUpdate(
			FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
				Status,
				Applicability,
				Requirement,
				RequestState,
				TEXT("MockGdpr"),
				{},
				true,
				ExpiresAt
			)
		);
	};

	ApplyState(
		EOpenMobileAdsConsentStatus::Obtained,
		EOpenMobileAdsGdprApplicability::Applicable,
		EOpenMobileAdsConsentRequirement::Required,
		EOpenMobileAdsConsentRequestState::Allowed,
		FDateTime::UtcNow() - FTimespan::FromSeconds(1.0)
	);
	TestEqual(
		TEXT("Expired GDPR state blocks as stale"),
		Subsystem->CanRequestAds().BlockReason,
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentStale
	);

	ApplyState(
		EOpenMobileAdsConsentStatus::Obtained,
		EOpenMobileAdsGdprApplicability::Applicable,
		EOpenMobileAdsConsentRequirement::Required,
		EOpenMobileAdsConsentRequestState::Allowed,
		FDateTime::UtcNow() + FTimespan::FromHours(1.0)
	);
	TestTrue(
		TEXT("Fresh provider-approved GDPR state allows requests"),
		Subsystem->CanRequestAds().bCanRequestAds
	);

	ApplyState(
		EOpenMobileAdsConsentStatus::Obtained,
		EOpenMobileAdsGdprApplicability::Applicable,
		EOpenMobileAdsConsentRequirement::Required,
		EOpenMobileAdsConsentRequestState::Blocked
	);
	TestEqual(
		TEXT("Provider-blocked GDPR state has a structured reason"),
		Subsystem->CanRequestAds().BlockReason,
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentProviderBlocked
	);

	ApplyState(
		EOpenMobileAdsConsentStatus::Required,
		EOpenMobileAdsGdprApplicability::Applicable,
		EOpenMobileAdsConsentRequirement::Required,
		EOpenMobileAdsConsentRequestState::Blocked
	);
	TestEqual(
		TEXT("A required GDPR decision remains a user-decision block"),
		Subsystem->CanRequestAds().BlockReason,
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentRequired
	);

	ApplyState(
		EOpenMobileAdsConsentStatus::NotRequired,
		EOpenMobileAdsGdprApplicability::NotApplicable,
		EOpenMobileAdsConsentRequirement::NotRequired,
		EOpenMobileAdsConsentRequestState::Allowed
	);
	TestTrue(
		TEXT("A fresh not-required result allows requests"),
		Subsystem->CanRequestAds().bCanRequestAds
	);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsUsPrivacyPropagationContractTest,
	"OpenMobile.Ads.Privacy.UsState.ProviderPropagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsUsPrivacyPropagationContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		true;
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("UsPrivacyReward");
	Placement.Android.AdUnitId = TEXT("android-us-privacy");
	Placement.IOS.AdUnitId = TEXT("ios-us-privacy");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	FOpenMobileAdsPrivacySnapshot Privacy;
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::NotRequired;
	Privacy.ConsentRequirement = EOpenMobileAdsConsentRequirement::NotRequired;
	Privacy.ConsentRequestState = EOpenMobileAdsConsentRequestState::Allowed;
	Privacy.bConsentStatusFresh = true;
	Privacy.UsPrivacy.Applicability =
		EOpenMobileAdsUsPrivacyApplicability::Applicable;
	Privacy.UsPrivacy.Choice = EOpenMobileAdsUsPrivacyChoice::OptedOut;
	Privacy.UsPrivacy.PrivacyOptionsRequirement =
		EOpenMobileAdsPrivacyOptionsRequirement::Required;
	Privacy.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Restricted;
	Privacy.Source = TEXT("MockUsPrivacy");
	TestTrue(
		TEXT("US-state privacy can be set before initialization"),
		Subsystem->UpdatePrivacySnapshot(Privacy).bAccepted
	);
	TestTrue(
		TEXT("The provider initializes after US-state privacy is set"),
		InitializeSuccessfully(*Subsystem, Provider, false)
	);
	TestEqual(
		TEXT("US-state applicability reaches provider initialization"),
		Provider.LastInitializationRequest.PrivacyContext.UsPrivacy.Applicability,
		EOpenMobileAdsUsPrivacyApplicability::Applicable
	);
	TestEqual(
		TEXT("The opt-out reaches provider initialization"),
		Provider.LastInitializationRequest.PrivacyContext.UsPrivacy.Choice,
		EOpenMobileAdsUsPrivacyChoice::OptedOut
	);
	TestEqual(
		TEXT("Restricted processing reaches provider initialization"),
		Provider.LastInitializationRequest.PrivacyContext.UsPrivacy.DataProcessingMode,
		EOpenMobileAdsDataProcessingMode::Restricted
	);

	Privacy.UsPrivacy.Choice = EOpenMobileAdsUsPrivacyChoice::OptedIn;
	Privacy.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Standard;
	TestTrue(
		TEXT("A changed US-state choice is accepted after initialization"),
		Subsystem->UpdatePrivacySnapshot(Privacy).bAccepted
	);
	const FOpenMobileAdsOperationResult LoadResult =
		Subsystem->LoadAd(TEXT("UsPrivacyReward"));
	TestTrue(
		TEXT("The changed choice allows an ad load"),
		LoadResult.bAccepted
	);
	TestEqual(
		TEXT("The changed opt-in reaches the provider load"),
		Provider.LastLoadRequest.PrivacyContext.UsPrivacy.Choice,
		EOpenMobileAdsUsPrivacyChoice::OptedIn
	);
	TestEqual(
		TEXT("Standard processing reaches the provider load"),
		Provider.LastLoadRequest.PrivacyContext.UsPrivacy.DataProcessingMode,
		EOpenMobileAdsDataProcessingMode::Standard
	);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsConsentProviderFlowContractTest,
	"OpenMobile.Ads.Privacy.ConsentProvider.RequiredFormFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsConsentProviderFlowContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->bDevelopmentTestMode = true;
	ScopedSettings.Settings->TestDeviceIdentifiers = {TEXT("UMP-TEST-DEVICE")};
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		true;
	ScopedSettings.Settings->Privacy.UnderAgeOfConsent =
		EOpenMobileAdsAgeTreatment::Yes;

	FMockProvider Provider(TEXT("MockAds"));
	Provider.ConsentProviderName = TEXT("MockConsent");
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestNotNull(
		TEXT("Blueprints can start a consent refresh"),
		UOpenMobileAdsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UOpenMobileAdsSubsystem, RefreshConsent)
		)
	);

	const FOpenMobileAdsOperationResult Started = Subsystem->RefreshConsent();
	TestTrue(TEXT("A supported consent refresh is accepted"), Started.bAccepted);
	TestTrue(TEXT("A consent refresh receives a request ID"), Started.RequestId.IsValid());
	TestEqual(TEXT("The provider refresh starts once"), Provider.ConsentRefreshCalls, 1);
	TestEqual(
		TEXT("Consent refresh enters the normalized refreshing state"),
		Subsystem->GetPrivacySnapshot().ConsentActivity,
		EOpenMobileAdsConsentActivity::Refreshing
	);
	TestEqual(
		TEXT("TFUA reaches the consent provider before its request"),
		Provider.LastConsentRequest.Privacy.UnderAgeOfConsent,
		EOpenMobileAdsAgeTreatment::Yes
	);
	TestFalse(
		TEXT("TFUA suppresses consent debug intent"),
		Provider.LastConsentRequest.Development.bEnableConsentDebug
	);

	const FOpenMobileAdsOperationResult Duplicate = Subsystem->RefreshConsent();
	TestTrue(TEXT("An overlapping refresh is idempotent"), Duplicate.bAccepted);
	TestEqual(TEXT("An overlapping refresh keeps its request ID"), Duplicate.RequestId, Started.RequestId);
	TestEqual(TEXT("An overlapping refresh does not reach the provider"), Provider.ConsentRefreshCalls, 1);

	FOpenMobileAdsConsentStatusUpdate Required =
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
			EOpenMobileAdsConsentStatus::Required,
			EOpenMobileAdsGdprApplicability::Applicable,
			EOpenMobileAdsConsentRequirement::Required,
			EOpenMobileAdsConsentRequestState::Blocked,
			TEXT("MockConsent")
		);
	Provider.CompleteConsentRefresh(MoveTemp(Required));
	DrainGameThreadTasks();
	TestEqual(TEXT("A required result starts one form"), Provider.ConsentFormCalls, 1);
	TestEqual(
		TEXT("The required form enters presentation state"),
		Subsystem->GetPrivacySnapshot().ConsentActivity,
		EOpenMobileAdsConsentActivity::PresentingForm
	);
	TestEqual(TEXT("The form keeps the refresh request ID"), Provider.LastConsentFormRequest.RequestId, Started.RequestId);

	const FOpenMobileAdsOperationResult FormDuplicate = Subsystem->RefreshConsent();
	TestTrue(TEXT("A refresh during form presentation is idempotent"), FormDuplicate.bAccepted);
	TestEqual(TEXT("Form presentation keeps its request ID"), FormDuplicate.RequestId, Started.RequestId);
	TestEqual(TEXT("A duplicate does not present another form"), Provider.ConsentFormCalls, 1);

	FOpenMobileAdsConsentStatusUpdate Obtained =
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
			EOpenMobileAdsConsentStatus::Obtained,
			EOpenMobileAdsGdprApplicability::Applicable,
			EOpenMobileAdsConsentRequirement::Required,
			EOpenMobileAdsConsentRequestState::Allowed,
			TEXT("MockConsent")
		);
	Provider.CompleteConsentForm(MoveTemp(Obtained));
	DrainGameThreadTasks();
	const FOpenMobileAdsPrivacySnapshot Completed =
		Subsystem->GetPrivacySnapshot();
	TestEqual(TEXT("Form dismissal returns consent to idle"), Completed.ConsentActivity, EOpenMobileAdsConsentActivity::Idle);
	TestEqual(TEXT("The final UMP-style state remains obtained"), Completed.ConsentStatus, EOpenMobileAdsConsentStatus::Obtained);
	TestEqual(TEXT("Provider eligibility is preserved"), Completed.ConsentRequestState, EOpenMobileAdsConsentRequestState::Allowed);

	const FOpenMobileAdsOperationResult Next = Subsystem->RefreshConsent();
	TestTrue(TEXT("A later session refresh can start"), Next.bAccepted);
	TestNotEqual(TEXT("A later refresh gets a new request ID"), Next.RequestId, Started.RequestId);
	TestEqual(TEXT("A later refresh reaches the provider once"), Provider.ConsentRefreshCalls, 2);
	FOpenMobileAdsConsentStatusUpdate NotRequired =
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
			EOpenMobileAdsConsentStatus::NotRequired,
			EOpenMobileAdsGdprApplicability::NotApplicable,
			EOpenMobileAdsConsentRequirement::NotRequired,
			EOpenMobileAdsConsentRequestState::Allowed,
			TEXT("MockConsent")
		);
	Provider.CompleteConsentRefresh(MoveTemp(NotRequired));
	DrainGameThreadTasks();
	TestEqual(TEXT("A not-required refresh does not present a form"), Provider.ConsentFormCalls, 1);
	TestEqual(TEXT("A not-required refresh ends idle"), Subsystem->GetPrivacySnapshot().ConsentActivity, EOpenMobileAdsConsentActivity::Idle);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsConsentProviderFailureContractTest,
	"OpenMobile.Ads.Privacy.ConsentProvider.FailuresAndTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsConsentProviderFailureContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		true;
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);

	FOpenMobileAdsOperationResult Result = Subsystem->RefreshConsent();
	TestFalse(TEXT("A provider without consent support rejects refresh"), Result.bAccepted);
	TestEqual(TEXT("Missing consent support is typed"), Result.Error.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);

	Provider.ConsentProviderName = TEXT("MockConsent");
	Provider.bAcceptConsentRefresh = false;
	Provider.ConsentRejection.NativeDiagnostics.NativeCode = TEXT("consent_error");
	Provider.ConsentRejection.NativeDiagnostics.NativeMessage =
		TEXT("mock refresh failure");
	Result = Subsystem->RefreshConsent();
	TestFalse(TEXT("Immediate provider refresh rejection is returned"), Result.bAccepted);
	TestEqual(TEXT("Refresh rejection uses consent error mapping"), Result.Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	TestEqual(TEXT("Rejected refresh returns to idle"), Subsystem->GetPrivacySnapshot().ConsentActivity, EOpenMobileAdsConsentActivity::Idle);
	TestEqual(TEXT("Rejected refresh is visible in the snapshot"), Subsystem->GetPrivacySnapshot().Error.Code, EOpenMobileAdsErrorCode::NativeFailure);

	Provider.bAcceptConsentRefresh = true;
	Provider.bAcceptConsentForm = false;
	Provider.ConsentRejection = FOpenMobileAdsError();
	Provider.ConsentRejection.NativeDiagnostics.NativeCode =
		TEXT("form_unavailable");
	Provider.ConsentRejection.NativeDiagnostics.NativeMessage =
		TEXT("mock form unavailable");
	const FOpenMobileAdsOperationResult RequiredRefresh =
		Subsystem->RefreshConsent();
	TestTrue(TEXT("A retry can refresh after rejection"), RequiredRefresh.bAccepted);
	FOpenMobileAdsConsentStatusUpdate Required =
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
			EOpenMobileAdsConsentStatus::Required,
			EOpenMobileAdsGdprApplicability::Applicable,
			EOpenMobileAdsConsentRequirement::Required,
			EOpenMobileAdsConsentRequestState::Blocked,
			TEXT("MockConsent")
		);
	Provider.CompleteConsentRefresh(MoveTemp(Required));
	DrainGameThreadTasks();
	TestEqual(TEXT("An unavailable form preserves required consent"), Subsystem->GetPrivacySnapshot().ConsentStatus, EOpenMobileAdsConsentStatus::Required);
	TestEqual(TEXT("An unavailable form returns to idle"), Subsystem->GetPrivacySnapshot().ConsentActivity, EOpenMobileAdsConsentActivity::Idle);
	TestEqual(TEXT("An unavailable form is mapped"), Subsystem->GetPrivacySnapshot().Error.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);

	Provider.bAcceptConsentForm = true;
	const FOpenMobileAdsOperationResult Pending = Subsystem->RefreshConsent();
	TestTrue(TEXT("A later refresh starts after form failure"), Pending.bAccepted);
	const TSharedPtr<IOpenMobileAdsConsentProviderSink, ESPMode::ThreadSafe>
		LateSink = Provider.ConsentRefreshSink;
	Subsystem->Deinitialize();
	TestEqual(TEXT("Teardown cancels the active consent request"), Provider.CancelledConsentRequests.Num(), 1);
	if (Provider.CancelledConsentRequests.Num() == 1)
	{
		TestEqual(TEXT("Teardown cancels the exact consent request"), Provider.CancelledConsentRequests[0], Pending.RequestId);
	}
	if (LateSink)
	{
		LateSink->Complete(FOpenMobileAdsConsentStatusUpdate::Complete(
			EOpenMobileAdsConsentStatus::NotRequired,
			TEXT("MockConsent")
		));
	}
	DrainGameThreadTasks();
	TestNotEqual(TEXT("A late callback cannot replace consent after teardown"), Subsystem->GetPrivacySnapshot().ConsentStatus, EOpenMobileAdsConsentStatus::NotRequired);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsPrivacyOptionsEntryPointContractTest,
	"OpenMobile.Ads.Privacy.PrivacyOptions.EntryPoint",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsPrivacyOptionsEntryPointContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		true;
	FMockProvider Provider(TEXT("MockAds"));
	Provider.ConsentProviderName = TEXT("MockConsent");
	Provider.bSupportsPrivacyOptionsForm = true;
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);

	auto MakeUpdate = [](EOpenMobileAdsUsPrivacyChoice Choice)
	{
		FOpenMobileAdsConsentStatusUpdate Update =
			FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
				EOpenMobileAdsConsentStatus::Obtained,
				EOpenMobileAdsGdprApplicability::Applicable,
				EOpenMobileAdsConsentRequirement::Required,
				EOpenMobileAdsConsentRequestState::Allowed,
				TEXT("MockConsent")
			);
		Update.UsPrivacy.Applicability =
			EOpenMobileAdsUsPrivacyApplicability::Applicable;
		Update.UsPrivacy.Choice = Choice;
		Update.UsPrivacy.PrivacyOptionsRequirement =
			EOpenMobileAdsPrivacyOptionsRequirement::Required;
		Update.UsPrivacy.bPrivacyOptionsFormAvailable = true;
		Update.UsPrivacy.DataProcessingMode =
			EOpenMobileAdsDataProcessingMode::ProviderManaged;
		return Update;
	};

	Subsystem->ApplyConsentStatusUpdate(
		MakeUpdate(EOpenMobileAdsUsPrivacyChoice::OptedIn)
	);
	TestTrue(
		TEXT("A required privacy-options path is queryable"),
		Subsystem->IsPrivacyOptionsFormRequired()
	);
	TestTrue(
		TEXT("An available privacy-options form is queryable"),
		Subsystem->IsPrivacyOptionsFormAvailable()
	);
	TestNotNull(
		TEXT("Blueprints can query privacy-options requirements"),
		UOpenMobileAdsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileAdsSubsystem,
				IsPrivacyOptionsFormRequired
			)
		)
	);
	TestNotNull(
		TEXT("Blueprints can query privacy-options availability"),
		UOpenMobileAdsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileAdsSubsystem,
				IsPrivacyOptionsFormAvailable
			)
		)
	);
	TestNotNull(
		TEXT("Blueprints can present privacy options"),
		UOpenMobileAdsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(
				UOpenMobileAdsSubsystem,
				PresentPrivacyOptionsForm
			)
		)
	);
	FOpenMobileAdsConsentStatusUpdate ExpiredUpdate =
		MakeUpdate(EOpenMobileAdsUsPrivacyChoice::OptedIn);
	ExpiredUpdate.ExpiresAt = FDateTime::UtcNow() - FTimespan::FromSeconds(1.0);
	Subsystem->ApplyConsentStatusUpdate(MoveTemp(ExpiredUpdate));
	TestFalse(
		TEXT("An expired privacy-options requirement is not current"),
		Subsystem->IsPrivacyOptionsFormRequired()
	);
	TestFalse(
		TEXT("An expired privacy-options form is not available"),
		Subsystem->IsPrivacyOptionsFormAvailable()
	);
	TestFalse(
		TEXT("An expired privacy-options form cannot open"),
		Subsystem->PresentPrivacyOptionsForm().bAccepted
	);
	Subsystem->ApplyConsentStatusUpdate(
		MakeUpdate(EOpenMobileAdsUsPrivacyChoice::OptedIn)
	);

	TFuture<FOpenMobileAdsOperationResult> OffThreadResult = Async(
		EAsyncExecution::ThreadPool,
		[Subsystem]()
		{
			return Subsystem->PresentPrivacyOptionsForm();
		}
	);
	OffThreadResult.Wait();
	TestFalse(
		TEXT("Privacy options reject off-thread presentation"),
		OffThreadResult.Get().bAccepted
	);

	const FOpenMobileAdsOperationResult Refresh = Subsystem->RefreshConsent();
	TestTrue(TEXT("Consent refresh starts before the overlap check"), Refresh.bAccepted);
	const FOpenMobileAdsOperationResult RefreshConflict =
		Subsystem->PresentPrivacyOptionsForm();
	TestFalse(
		TEXT("Privacy options cannot overlap consent refresh"),
		RefreshConflict.bAccepted
	);
	TestEqual(
		TEXT("Consent overlap is a busy failure"),
		RefreshConflict.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);
	Provider.CompleteConsentRefresh(
		MakeUpdate(EOpenMobileAdsUsPrivacyChoice::OptedIn)
	);
	DrainGameThreadTasks();

	const FOpenMobileAdsOperationResult Started =
		Subsystem->PresentPrivacyOptionsForm();
	TestTrue(TEXT("An available privacy-options form starts"), Started.bAccepted);
	TestTrue(TEXT("Privacy-options presentation gets a request ID"), Started.RequestId.IsValid());
	TestEqual(TEXT("The provider presents privacy options once"), Provider.PrivacyOptionsFormCalls, 1);
	TestEqual(
		TEXT("Privacy options enter form presentation state"),
		Subsystem->GetPrivacySnapshot().ConsentActivity,
		EOpenMobileAdsConsentActivity::PresentingForm
	);
	const FOpenMobileAdsOperationResult Duplicate =
		Subsystem->PresentPrivacyOptionsForm();
	TestTrue(TEXT("A duplicate privacy-options call is idempotent"), Duplicate.bAccepted);
	TestEqual(TEXT("A duplicate keeps the request ID"), Duplicate.RequestId, Started.RequestId);
	TestEqual(TEXT("A duplicate does not present another form"), Provider.PrivacyOptionsFormCalls, 1);
	const FOpenMobileAdsOperationResult PrivacyConflict =
		Subsystem->RefreshConsent();
	TestFalse(
		TEXT("Consent refresh cannot replace active privacy options"),
		PrivacyConflict.bAccepted
	);
	TestEqual(
		TEXT("Privacy-options overlap is a busy failure"),
		PrivacyConflict.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);

	Provider.CompletePrivacyOptionsForm(
		MakeUpdate(EOpenMobileAdsUsPrivacyChoice::OptedOut)
	);
	DrainGameThreadTasks();
	const FOpenMobileAdsPrivacySnapshot Changed =
		Subsystem->GetPrivacySnapshot();
	TestEqual(TEXT("Privacy-options dismissal returns idle"), Changed.ConsentActivity, EOpenMobileAdsConsentActivity::Idle);
	TestEqual(TEXT("A changed privacy choice is applied"), Changed.UsPrivacy.Choice, EOpenMobileAdsUsPrivacyChoice::OptedOut);
	TestTrue(TEXT("The privacy-options form remains reopenable"), Changed.UsPrivacy.bPrivacyOptionsFormAvailable);

	const FOpenMobileAdsOperationResult Reopened =
		Subsystem->PresentPrivacyOptionsForm();
	TestTrue(TEXT("Privacy options can reopen after dismissal"), Reopened.bAccepted);
	TestNotEqual(TEXT("A reopened form gets a new request ID"), Reopened.RequestId, Started.RequestId);
	TestEqual(TEXT("Reopening reaches the provider again"), Provider.PrivacyOptionsFormCalls, 2);
	FOpenMobileAdsError NativeFailure;
	NativeFailure.NativeDiagnostics.NativeCode = TEXT("form_unavailable");
	NativeFailure.NativeDiagnostics.NativeMessage =
		TEXT("mock privacy-options form unavailable");
	Provider.FailPrivacyOptionsForm(MoveTemp(NativeFailure));
	DrainGameThreadTasks();
	TestEqual(
		TEXT("An unavailable privacy-options form returns idle"),
		Subsystem->GetPrivacySnapshot().ConsentActivity,
		EOpenMobileAdsConsentActivity::Idle
	);
	TestEqual(
		TEXT("Privacy-options failure is normalized"),
		Subsystem->GetPrivacySnapshot().Error.Code,
		EOpenMobileAdsErrorCode::ProviderUnavailable
	);

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsTrackingAuthorizationRequestContractTest,
	"OpenMobile.Ads.Privacy.TrackingAuthorization.Request",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsTrackingAuthorizationRequestContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent = false;
	ScopedSettings.Settings->bEnableTrackingAuthorization = false;
	ScopedSettings.Settings->bDelayAdsInitializationUntilTrackingAuthorization = true;
	FMockProvider Provider(TEXT("MockAds"));
	Provider.ConsentProviderName = TEXT("MockConsent");
	FScopedProviderRegistration ProviderRegistration(Provider);
	FMockTrackingAuthorizationBackend TrackingBackend;
	FScopedTrackingAuthorizationBackendRegistration TrackingRegistration(
		TrackingBackend
	);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	int32 StatusChangeCalls = 0;
	Subsystem->OnNativeTrackingAuthorizationStatusChanged().AddLambda(
		[&StatusChangeCalls](EOpenMobileAdsTrackingAuthorizationStatus Status)
		{
			++StatusChangeCalls;
		}
	);

	const FOpenMobileAdsOperationResult Disabled =
		Subsystem->RequestTrackingAuthorization();
	TestFalse(TEXT("Disabled ATT rejects the request"), Disabled.bAccepted);
	TestEqual(
		TEXT("Disabled ATT reports missing configuration"),
		Disabled.Error.Code,
		EOpenMobileAdsErrorCode::NotConfigured
	);
	TestEqual(TEXT("Disabled ATT does not reach iOS"), TrackingBackend.RequestCalls, 0);

	ScopedSettings.Settings->bEnableTrackingAuthorization = true;
	ScopedSettings.Settings->TrackingUsageDescription.Reset();
	const FOpenMobileAdsOperationResult MissingDescription =
		Subsystem->RequestTrackingAuthorization();
	TestFalse(
		TEXT("ATT without a usage description rejects the request"),
		MissingDescription.bAccepted
	);
	TestEqual(
		TEXT("Missing ATT text reports missing configuration"),
		MissingDescription.Error.Code,
		EOpenMobileAdsErrorCode::NotConfigured
	);

	ScopedSettings.Settings->TrackingUsageDescription =
		TEXT("We use this permission to measure advertising performance.");
	TestTrue(
		TEXT("Consent refresh starts before the ATT conflict check"),
		Subsystem->RefreshConsent().bAccepted
	);
	const FOpenMobileAdsOperationResult ConsentConflict =
		Subsystem->RequestTrackingAuthorization();
	TestFalse(TEXT("Active consent work blocks ATT"), ConsentConflict.bAccepted);
	TestEqual(
		TEXT("Active consent work reports a busy ATT request"),
		ConsentConflict.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);
	Provider.CompleteConsentRefresh(
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
			EOpenMobileAdsConsentStatus::NotRequired,
			EOpenMobileAdsGdprApplicability::NotApplicable,
			EOpenMobileAdsConsentRequirement::NotRequired,
			EOpenMobileAdsConsentRequestState::Allowed,
			TEXT("MockConsent")
		)
	);
	DrainGameThreadTasks();

	FCoreDelegates::ApplicationWillEnterBackgroundDelegate.Broadcast();
	const FOpenMobileAdsOperationResult Background =
		Subsystem->RequestTrackingAuthorization();
	TestFalse(TEXT("Background applications cannot request ATT"), Background.bAccepted);
	TestEqual(
		TEXT("Background ATT requests report busy"),
		Background.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);
	FCoreDelegates::ApplicationHasEnteredForegroundDelegate.Broadcast();
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();

	TrackingBackend.bAcceptRequest = false;
	const FOpenMobileAdsOperationResult NativeRejection =
		Subsystem->RequestTrackingAuthorization();
	TestFalse(TEXT("A native ATT rejection is returned immediately"), NativeRejection.bAccepted);
	TestEqual(
		TEXT("A native ATT rejection uses the normalized native failure"),
		NativeRejection.Error.Code,
		EOpenMobileAdsErrorCode::NativeFailure
	);
	TrackingBackend.bAcceptRequest = true;
	const FOpenMobileAdsOperationResult First =
		Subsystem->RequestTrackingAuthorization();
	TestTrue(TEXT("The first ATT prompt is accepted"), First.bAccepted);
	TestTrue(TEXT("The first ATT request has an operation ID"), First.RequestId.IsValid());
	const FOpenMobileAdsOperationResult Repeated =
		Subsystem->RequestTrackingAuthorization();
	TestTrue(TEXT("A repeated active ATT request is accepted"), Repeated.bAccepted);
	TestEqual(
		TEXT("A repeated active ATT request reuses the operation ID"),
		Repeated.RequestId,
		First.RequestId
	);
	TestEqual(TEXT("Repeated ATT requests show one accepted native prompt"), TrackingBackend.RequestCalls, 2);

	const FOpenMobileAdsOperationResult PendingInitialization =
		Subsystem->InitializeAds();
	TestFalse(
		TEXT("Pending ATT blocks configured ads initialization"),
		PendingInitialization.bAccepted
	);
	TestEqual(
		TEXT("Pending ATT uses a privacy initialization block"),
		PendingInitialization.Error.Code,
		EOpenMobileAdsErrorCode::PrivacyBlocked
	);
	TestEqual(TEXT("Pending ATT does not reach the provider"), Provider.InitializationCalls, 0);

	TrackingBackend.Complete(EOpenMobileAdsTrackingAuthorizationStatus::Denied);
	DrainGameThreadTasks();
	TestEqual(
		TEXT("A denied ATT result updates public status"),
		Subsystem->GetTrackingAuthorizationStatus(),
		EOpenMobileAdsTrackingAuthorizationStatus::Denied
	);
	TestEqual(TEXT("Initial and denied ATT states broadcast"), StatusChangeCalls, 2);
	const FOpenMobileAdsOperationResult AfterDecision =
		Subsystem->RequestTrackingAuthorization();
	TestTrue(TEXT("A resolved ATT request completes without another prompt"), AfterDecision.bAccepted);
	TestTrue(
		TEXT("A resolved ATT request receives a fresh operation ID"),
		AfterDecision.RequestId.IsValid()
			&& AfterDecision.RequestId != First.RequestId
	);
	TestEqual(TEXT("A resolved ATT status is not prompted again"), TrackingBackend.RequestCalls, 2);

	TestTrue(
		TEXT("Denied ATT permits ads initialization after the decision"),
		Subsystem->InitializeAds().bAccepted
	);
	TestEqual(TEXT("Resolved ATT reaches the ads provider"), Provider.InitializationCalls, 1);
	Subsystem->Deinitialize();

	TrackingBackend.Status =
		EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined;
	UOpenMobileAdsSubsystem* ShutdownSubsystem =
		NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	int32 ShutdownStatusChangeCalls = 0;
	ShutdownSubsystem->OnNativeTrackingAuthorizationStatusChanged().AddLambda(
		[&ShutdownStatusChangeCalls](
			EOpenMobileAdsTrackingAuthorizationStatus Status
		)
		{
			++ShutdownStatusChangeCalls;
		}
	);
	TestTrue(
		TEXT("An ATT request can start before subsystem shutdown"),
		ShutdownSubsystem->RequestTrackingAuthorization().bAccepted
	);
	ShutdownSubsystem->Deinitialize();
	TrackingBackend.Complete(EOpenMobileAdsTrackingAuthorizationStatus::Denied);
	DrainGameThreadTasks();
	TestEqual(
		TEXT("A late ATT completion is ignored after subsystem shutdown"),
		ShutdownStatusChangeCalls,
		1
	);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAdvertisingIdentifierAvailabilityContractTest,
	"OpenMobile.Ads.Privacy.AdvertisingIdentifier.Availability",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAdvertisingIdentifierAvailabilityContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	TestFalse(
		TEXT("A platform without an advertising identifier backend is unavailable"),
		FOpenMobileAdsTrackingAuthorizationPlatform::
			IsAdvertisingIdentifierAvailable()
	);

	FMockTrackingAuthorizationBackend TrackingBackend;
	FScopedTrackingAuthorizationBackendRegistration TrackingRegistration(
		TrackingBackend
	);
	const EOpenMobileAdsTrackingAuthorizationStatus BlockedStatuses[] = {
		EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined,
		EOpenMobileAdsTrackingAuthorizationStatus::Restricted,
		EOpenMobileAdsTrackingAuthorizationStatus::Denied,
		EOpenMobileAdsTrackingAuthorizationStatus::Unsupported,
		static_cast<EOpenMobileAdsTrackingAuthorizationStatus>(255)
	};
	for (const EOpenMobileAdsTrackingAuthorizationStatus Status : BlockedStatuses)
	{
		TrackingBackend.Status = Status;
		TestFalse(
			TEXT("A non-authorized ATT state has no advertising identifier"),
			FOpenMobileAdsTrackingAuthorizationPlatform::
				IsAdvertisingIdentifierAvailable()
		);
	}
	TestEqual(
		TEXT("Blocked ATT states never read the advertising identifier"),
		TrackingBackend.AdvertisingIdentifierReads,
		0
	);

	TrackingBackend.Status =
		EOpenMobileAdsTrackingAuthorizationStatus::Authorized;
	TestFalse(
		TEXT("An authorized zero advertising identifier is unavailable"),
		FOpenMobileAdsTrackingAuthorizationPlatform::
			IsAdvertisingIdentifierAvailable()
	);
	TrackingBackend.bHasNonZeroAdvertisingIdentifier = true;
	TestTrue(
		TEXT("An authorized nonzero advertising identifier is available"),
		FOpenMobileAdsTrackingAuthorizationPlatform::
			IsAdvertisingIdentifierAvailable()
	);
	TestEqual(
		TEXT("Authorized checks read the advertising identifier on demand"),
		TrackingBackend.AdvertisingIdentifierReads,
		2
	);

	const uint8 ZeroIdentifier[16] = {};
	uint8 NonZeroIdentifier[16] = {};
	NonZeroIdentifier[15] = 1;
	TestFalse(
		TEXT("An all-zero Apple advertising identifier is rejected"),
		OpenMobileAdsHasNonZeroAppleAdvertisingIdentifier(
			ZeroIdentifier,
			UE_ARRAY_COUNT(ZeroIdentifier)
		)
	);
	TestTrue(
		TEXT("A nonzero Apple advertising identifier is accepted"),
		OpenMobileAdsHasNonZeroAppleAdvertisingIdentifier(
			NonZeroIdentifier,
			UE_ARRAY_COUNT(NonZeroIdentifier)
		)
	);
	TestFalse(
		TEXT("A malformed Apple advertising identifier is rejected"),
		OpenMobileAdsHasNonZeroAppleAdvertisingIdentifier(
			NonZeroIdentifier,
			UE_ARRAY_COUNT(NonZeroIdentifier) - 1
		)
	);

	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The subsystem exposes only advertising identifier availability"),
		Subsystem->IsAdvertisingIdentifierAvailable()
	);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsTrackingAuthorizationStatusContractTest,
	"OpenMobile.Ads.Privacy.TrackingAuthorization.Status",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsTrackingAuthorizationStatusContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	TestEqual(
		TEXT("Apple not-determined status is normalized"),
		OpenMobileAdsMapAppleTrackingAuthorizationStatus(0),
		EOpenMobileAdsTrackingAuthorizationStatus::NotDetermined
	);
	TestEqual(
		TEXT("Apple restricted status is normalized"),
		OpenMobileAdsMapAppleTrackingAuthorizationStatus(1),
		EOpenMobileAdsTrackingAuthorizationStatus::Restricted
	);
	TestEqual(
		TEXT("Apple denied status is normalized"),
		OpenMobileAdsMapAppleTrackingAuthorizationStatus(2),
		EOpenMobileAdsTrackingAuthorizationStatus::Denied
	);
	TestEqual(
		TEXT("Apple authorized status is normalized"),
		OpenMobileAdsMapAppleTrackingAuthorizationStatus(3),
		EOpenMobileAdsTrackingAuthorizationStatus::Authorized
	);
	TestEqual(
		TEXT("Unknown future Apple status is unsupported"),
		OpenMobileAdsMapAppleTrackingAuthorizationStatus(99),
		EOpenMobileAdsTrackingAuthorizationStatus::Unsupported
	);

	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	FMockProvider Provider(TEXT("MockAds"));
	Provider.ConsentProviderName = TEXT("MockConsent");
	FScopedProviderRegistration ProviderRegistration(Provider);
	FMockTrackingAuthorizationBackend TrackingBackend;
	TrackingBackend.Status =
		EOpenMobileAdsTrackingAuthorizationStatus::Authorized;
	FScopedTrackingAuthorizationBackendRegistration TrackingRegistration(
		TrackingBackend
	);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	int32 ChangeCalls = 0;
	EOpenMobileAdsTrackingAuthorizationStatus LastStatus =
		EOpenMobileAdsTrackingAuthorizationStatus::Unsupported;
	Subsystem->OnNativeTrackingAuthorizationStatusChanged().AddLambda(
		[&ChangeCalls, &LastStatus](
			EOpenMobileAdsTrackingAuthorizationStatus Status
		)
		{
			++ChangeCalls;
			LastStatus = Status;
		}
	);

	TestTrue(
		TEXT("Consent refresh initializes platform status"),
		Subsystem->RefreshConsent().bAccepted
	);
	TestEqual(
		TEXT("Initial ATT status is published"),
		Subsystem->GetTrackingAuthorizationStatus(),
		EOpenMobileAdsTrackingAuthorizationStatus::Authorized
	);
	TestEqual(TEXT("Initial ATT status broadcasts once"), ChangeCalls, 1);
	Provider.CompleteConsentRefresh(
		FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
			EOpenMobileAdsConsentStatus::NotRequired,
			EOpenMobileAdsGdprApplicability::NotApplicable,
			EOpenMobileAdsConsentRequirement::NotRequired,
			EOpenMobileAdsConsentRequestState::Allowed,
			TEXT("MockConsent")
		)
	);
	DrainGameThreadTasks();

	TrackingBackend.Status = EOpenMobileAdsTrackingAuthorizationStatus::Denied;
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	TestEqual(TEXT("Foreground ATT change broadcasts"), ChangeCalls, 2);
	TestEqual(
		TEXT("Foreground ATT change updates the public status"),
		LastStatus,
		EOpenMobileAdsTrackingAuthorizationStatus::Denied
	);
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	TestEqual(TEXT("Unchanged foreground ATT status is deduplicated"), ChangeCalls, 2);

	TrackingBackend.Status =
		static_cast<EOpenMobileAdsTrackingAuthorizationStatus>(255);
	FCoreDelegates::ApplicationHasReactivatedDelegate.Broadcast();
	TestEqual(
		TEXT("Invalid backend ATT status falls back safely"),
		Subsystem->GetTrackingAuthorizationStatus(),
		EOpenMobileAdsTrackingAuthorizationStatus::Unsupported
	);
	TestEqual(TEXT("Safe ATT fallback broadcasts once"), ChangeCalls, 3);
	TestEqual(
		TEXT("Safe ATT fallback reaches native listeners"),
		LastStatus,
		EOpenMobileAdsTrackingAuthorizationStatus::Unsupported
	);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsDebugGeographyContractTest,
	"OpenMobile.Ads.Privacy.DebugGeography.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsDebugGeographyContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->bDevelopmentTestMode = true;
	ScopedSettings.Settings->TestDeviceIdentifiers = {TEXT("UMP-TEST-DEVICE")};
	ScopedSettings.Settings->DebugGeography = EOpenMobileAdsDebugGeography::Eea;

	FMockProvider Provider(TEXT("MockAds"));
	Provider.ConsentProviderName = TEXT("MockConsent");
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	auto CompleteRefresh = [&Provider]()
	{
		Provider.CompleteConsentRefresh(
			FOpenMobileAdsConsentStatusUpdate::CompleteProviderState(
				EOpenMobileAdsConsentStatus::NotRequired,
				EOpenMobileAdsGdprApplicability::NotApplicable,
				EOpenMobileAdsConsentRequirement::NotRequired,
				EOpenMobileAdsConsentRequestState::Allowed,
				TEXT("MockConsent")
			)
		);
		DrainGameThreadTasks();
	};

	TestTrue(
		TEXT("EEA geography refresh is accepted"),
		Subsystem->RefreshConsent().bAccepted
	);
	TestEqual(
		TEXT("EEA geography reaches the consent provider"),
		Provider.LastConsentRequest.Development.GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::Eea
	);
	CompleteRefresh();

	ScopedSettings.Settings->DebugGeography =
		EOpenMobileAdsDebugGeography::RegulatedUsState;
	ScopedSettings.Settings->TestDeviceIdentifiers.Reset();
	TestTrue(
		TEXT("A geography refresh without test devices is accepted"),
		Subsystem->RefreshConsent().bAccepted
	);
	TestEqual(
		TEXT("Missing test devices disable regulated-US geography"),
		Provider.LastConsentRequest.Development.GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::Disabled
	);
	CompleteRefresh();

	ScopedSettings.Settings->bDevelopmentTestMode = false;
	ScopedSettings.Settings->TestDeviceIdentifiers = {TEXT("UMP-TEST-DEVICE")};
	ScopedSettings.Settings->DebugGeography = EOpenMobileAdsDebugGeography::Other;
	TestTrue(
		TEXT("A production-mode consent refresh is accepted"),
		Subsystem->RefreshConsent().bAccepted
	);
	TestEqual(
		TEXT("Production mode disables other-region geography"),
		Provider.LastConsentRequest.Development.GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::Disabled
	);
	CompleteRefresh();

	ScopedSettings.Settings->bDevelopmentTestMode = true;
	ScopedSettings.Settings->DebugGeography = EOpenMobileAdsDebugGeography::Eea;
	FOpenMobileAdsPrivacySnapshot UnderAgePrivacy =
		Subsystem->GetPrivacySnapshot();
	UnderAgePrivacy.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::Yes;
	TestTrue(
		TEXT("Under-age treatment updates before consent refresh"),
		Subsystem->UpdatePrivacySnapshot(MoveTemp(UnderAgePrivacy)).bAccepted
	);
	TestTrue(
		TEXT("An under-age consent refresh is accepted"),
		Subsystem->RefreshConsent().bAccepted
	);
	TestEqual(
		TEXT("Under-age treatment disables debug geography"),
		Provider.LastConsentRequest.Development.GetEffectiveDebugGeography(),
		EOpenMobileAdsDebugGeography::Disabled
	);
	CompleteRefresh();

	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsConsentResetContractTest,
	"OpenMobile.Ads.Privacy.ConsentReset.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsConsentResetContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->bDevelopmentTestMode = true;
	FMockProvider Provider(TEXT("MockAds"));
	Provider.ConsentProviderName = TEXT("MockConsent");
	Provider.bSupportsConsentResetForTesting = true;
	FScopedProviderRegistration ProviderRegistration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	FOpenMobileAdsOperationResult ReentrantReset;
	bool bAttemptedReentrantReset = false;
	const FDelegateHandle ConsentHandle =
		Subsystem->OnNativeConsentStatusChanged().AddLambda(
			[Subsystem, &ReentrantReset, &bAttemptedReentrantReset](
				const FOpenMobileAdsPrivacySnapshot& Snapshot
			)
			{
				if (
					!bAttemptedReentrantReset
					&& Snapshot.ConsentActivity
						== EOpenMobileAdsConsentActivity::Resetting
				)
				{
					bAttemptedReentrantReset = true;
					ReentrantReset = Subsystem->ResetConsentForTesting();
				}
			}
		);

	const EOpenMobileAdsConsentStatus States[] = {
		EOpenMobileAdsConsentStatus::Unknown,
		EOpenMobileAdsConsentStatus::Required,
		EOpenMobileAdsConsentStatus::Granted,
		EOpenMobileAdsConsentStatus::Denied,
		EOpenMobileAdsConsentStatus::NotRequired,
		EOpenMobileAdsConsentStatus::Obtained
	};
	int32 ExpectedResetCalls = 0;
	for (const EOpenMobileAdsConsentStatus State : States)
	{
		FOpenMobileAdsConsentProviderDetails Details;
		Details.bIsAvailable = true;
		Details.RawStatus = TEXT("CACHED");
		Subsystem->ApplyConsentStatusUpdate(
			FOpenMobileAdsConsentStatusUpdate::Complete(
				State,
				TEXT("MockConsent"),
				MoveTemp(Details)
			)
		);

		const FOpenMobileAdsOperationResult Result =
			Subsystem->ResetConsentForTesting();
		++ExpectedResetCalls;
		TestTrue(TEXT("Development consent reset is accepted"), Result.bAccepted);
		TestTrue(TEXT("A reset returns an operation ID"), Result.RequestId.IsValid());
		TestEqual(
			TEXT("Every consent state reaches the provider reset API"),
			Provider.ConsentResetCalls,
			ExpectedResetCalls
		);
		const FOpenMobileAdsPrivacySnapshot& Snapshot =
			Subsystem->GetPrivacySnapshot();
		TestEqual(
			TEXT("Reset clears the local consent status"),
			Snapshot.ConsentStatus,
			EOpenMobileAdsConsentStatus::Unknown
		);
		TestEqual(
			TEXT("Successful reset returns consent activity to idle"),
			Snapshot.ConsentActivity,
			EOpenMobileAdsConsentActivity::Idle
		);
		TestFalse(
			TEXT("Reset consent state is no longer fresh"),
			Snapshot.bConsentStatusFresh
		);
		TestFalse(
			TEXT("Reset clears cached provider details"),
			Snapshot.ProviderDetails.bIsAvailable
		);
		TestFalse(TEXT("Successful reset has no error"), Snapshot.Error.IsSet());
	}
	TestTrue(
		TEXT("A reset event attempts the reentrant contract"),
		bAttemptedReentrantReset
	);
	TestFalse(TEXT("A reentrant reset is rejected"), ReentrantReset.bAccepted);
	TestEqual(
		TEXT("A reentrant reset reports a busy operation"),
		ReentrantReset.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);

	Subsystem->ApplyConsentStatusUpdate(
		FOpenMobileAdsConsentStatusUpdate::Complete(
			EOpenMobileAdsConsentStatus::Granted,
			TEXT("MockConsent")
		)
	);
	ScopedSettings.Settings->bDevelopmentTestMode = false;
	const FOpenMobileAdsOperationResult DisabledReset =
		Subsystem->ResetConsentForTesting();
	TestFalse(TEXT("Reset requires Development/Test Mode"), DisabledReset.bAccepted);
	TestEqual(
		TEXT("Disabled reset reports invalid state"),
		DisabledReset.Error.Code,
		EOpenMobileAdsErrorCode::InvalidState
	);
	TestEqual(
		TEXT("Disabled reset preserves local consent"),
		Subsystem->GetPrivacySnapshot().ConsentStatus,
		EOpenMobileAdsConsentStatus::Granted
	);
	TestEqual(
		TEXT("Disabled reset does not call the provider"),
		Provider.ConsentResetCalls,
		ExpectedResetCalls
	);

	ScopedSettings.Settings->bDevelopmentTestMode = true;
	Provider.bSupportsConsentResetForTesting = false;
	const FOpenMobileAdsOperationResult UnsupportedReset =
		Subsystem->ResetConsentForTesting();
	TestFalse(TEXT("Unsupported provider reset is rejected"), UnsupportedReset.bAccepted);
	TestEqual(
		TEXT("Unsupported reset reports provider availability"),
		UnsupportedReset.Error.Code,
		EOpenMobileAdsErrorCode::ProviderUnavailable
	);
	TestEqual(
		TEXT("Unsupported reset still clears service-owned state"),
		Subsystem->GetPrivacySnapshot().ConsentStatus,
		EOpenMobileAdsConsentStatus::Unknown
	);
	TestTrue(
		TEXT("Unsupported reset records its failure"),
		Subsystem->GetPrivacySnapshot().Error.IsSet()
	);

	Subsystem->ApplyConsentStatusUpdate(
		FOpenMobileAdsConsentStatusUpdate::Complete(
			EOpenMobileAdsConsentStatus::Denied,
			TEXT("MockConsent")
		)
	);
	Provider.bSupportsConsentResetForTesting = true;
	Provider.bAcceptConsentReset = false;
	Provider.ConsentResetError = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Consent,
		NAME_None,
		TEXT("The mock provider could not clear persistent consent."),
		TEXT("MockConsent")
	);
	const FOpenMobileAdsOperationResult FailedReset =
		Subsystem->ResetConsentForTesting();
	++ExpectedResetCalls;
	TestFalse(TEXT("Provider reset failure is rejected"), FailedReset.bAccepted);
	TestEqual(
		TEXT("Provider reset keeps its native failure"),
		FailedReset.Error.Code,
		EOpenMobileAdsErrorCode::NativeFailure
	);
	TestEqual(
		TEXT("Failed reset clears service-owned consent"),
		Subsystem->GetPrivacySnapshot().ConsentStatus,
		EOpenMobileAdsConsentStatus::Unknown
	);
	TestEqual(
		TEXT("Failed reset reaches the provider once"),
		Provider.ConsentResetCalls,
		ExpectedResetCalls
	);

	Provider.bAcceptConsentReset = true;
	const FOpenMobileAdsOperationResult Refresh = Subsystem->RefreshConsent();
	TestTrue(TEXT("Consent refresh starts before the busy reset check"), Refresh.bAccepted);
	const FOpenMobileAdsOperationResult BusyReset =
		Subsystem->ResetConsentForTesting();
	TestFalse(TEXT("Reset is blocked during consent refresh"), BusyReset.bAccepted);
	TestEqual(
		TEXT("An active refresh makes reset busy"),
		BusyReset.Error.Code,
		EOpenMobileAdsErrorCode::Busy
	);
	TestEqual(
		TEXT("Busy reset does not reach the provider"),
		Provider.ConsentResetCalls,
		ExpectedResetCalls
	);
	Provider.FailConsentRefresh(FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Consent,
		NAME_None,
		TEXT("The mock refresh was cancelled."),
		TEXT("MockConsent")
	));
	DrainGameThreadTasks();

	Subsystem->OnNativeConsentStatusChanged().Remove(ConsentHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsConsentSignalPropagationContractTest,
	"OpenMobile.Ads.Privacy.ConsentSignals.Propagation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsConsentSignalPropagationContractTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	const int32 GdprSignal = static_cast<int32>(
		EOpenMobileAdsConsentSignal::Gdpr
	);
	const int32 UsPrivacySignal = static_cast<int32>(
		EOpenMobileAdsConsentSignal::UsPrivacy
	);
	const int32 AllSignals = FOpenMobileAdsConsentSignals::AllSignalMask;

	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent =
		true;
	int32 Sequence = 0;
	FMockProvider Provider(TEXT("MockAds"));
	Provider.SupportedConsentSignalMask = AllSignals;
	Provider.ConfirmableConsentSignalMask = AllSignals;
	Provider.RuntimeConsentSignalMask = GdprSignal | UsPrivacySignal;
	Provider.Sequence = &Sequence;
	FScopedProviderRegistration ProviderRegistration(Provider);

	FMockConsentSignalConsumer AppliedAdapter(
		TEXT("MockAds"),
		EOpenMobileAdsConsentSignalConsumerType::Adapter,
		TEXT("AppliedAdapter"),
		TEXT("MockNetwork")
	);
	AppliedAdapter.SupportedSignalMask = AllSignals;
	AppliedAdapter.ConfirmableSignalMask = AllSignals;
	AppliedAdapter.RuntimeSignalMask = UsPrivacySignal;
	AppliedAdapter.Sequence = &Sequence;
	FScopedConsentSignalConsumerRegistration AppliedRegistration(AppliedAdapter);

	FMockConsentSignalConsumer UnconfirmedAdapter(
		TEXT("MockAds"),
		EOpenMobileAdsConsentSignalConsumerType::Adapter,
		TEXT("UnconfirmedAdapter"),
		TEXT("MockNetwork")
	);
	UnconfirmedAdapter.SupportedSignalMask = AllSignals;
	UnconfirmedAdapter.ConfirmableSignalMask = AllSignals & ~UsPrivacySignal;
	UnconfirmedAdapter.RuntimeSignalMask = UsPrivacySignal;
	UnconfirmedAdapter.Sequence = &Sequence;
	FScopedConsentSignalConsumerRegistration UnconfirmedRegistration(
		UnconfirmedAdapter
	);

	FMockConsentSignalConsumer UnsupportedNetwork(
		TEXT("MockAds"),
		EOpenMobileAdsConsentSignalConsumerType::Network,
		TEXT("UnsupportedNetwork")
	);
	UnsupportedNetwork.SupportedSignalMask = GdprSignal;
	UnsupportedNetwork.ConfirmableSignalMask = GdprSignal;
	UnsupportedNetwork.Sequence = &Sequence;
	FScopedConsentSignalConsumerRegistration UnsupportedRegistration(
		UnsupportedNetwork
	);

	FMockConsentSignalConsumer FailedAdapter(
		TEXT("MockAds"),
		EOpenMobileAdsConsentSignalConsumerType::Adapter,
		TEXT("FailedAdapter"),
		TEXT("FailedNetwork")
	);
	FailedAdapter.SupportedSignalMask = AllSignals;
	FailedAdapter.ConfirmableSignalMask = AllSignals;
	FailedAdapter.RuntimeSignalMask = UsPrivacySignal;
	FailedAdapter.ConsentSignalError = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Consent,
		NAME_None,
		TEXT("The adapter rejected the consent signal."),
		TEXT("MockAds")
	);
	FScopedConsentSignalConsumerRegistration FailedRegistration(FailedAdapter);

	FMockConsentSignalConsumer OtherProviderAdapter(
		TEXT("OtherAds"),
		EOpenMobileAdsConsentSignalConsumerType::Adapter,
		TEXT("OtherAdapter")
	);
	OtherProviderAdapter.SupportedSignalMask = AllSignals;
	OtherProviderAdapter.ConfirmableSignalMask = AllSignals;
	FScopedConsentSignalConsumerRegistration OtherRegistration(
		OtherProviderAdapter
	);

	FMockConsentSignalConsumer InvalidProviderConsumer(
		TEXT("MockAds"),
		EOpenMobileAdsConsentSignalConsumerType::Provider,
		TEXT("DuplicateProvider")
	);
	InvalidProviderConsumer.SupportedSignalMask = AllSignals;
	InvalidProviderConsumer.ConfirmableSignalMask = AllSignals;
	FScopedConsentSignalConsumerRegistration InvalidProviderRegistration(
		InvalidProviderConsumer
	);

	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	int32 DeliveryBroadcasts = 0;
	const FDelegateHandle DeliveryHandle =
		Subsystem->OnNativeConsentSignalDeliveryChanged().AddLambda(
			[&DeliveryBroadcasts](
				const FOpenMobileAdsConsentSignalDeliverySnapshot& Snapshot
			)
			{
				++DeliveryBroadcasts;
			}
		);

	FOpenMobileAdsPrivacySnapshot Privacy;
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Obtained;
	Privacy.GdprApplicability = EOpenMobileAdsGdprApplicability::Applicable;
	Privacy.ConsentRequirement = EOpenMobileAdsConsentRequirement::Required;
	Privacy.ConsentRequestState = EOpenMobileAdsConsentRequestState::Allowed;
	Privacy.UsPrivacy.Applicability =
		EOpenMobileAdsUsPrivacyApplicability::Applicable;
	Privacy.UsPrivacy.Choice = EOpenMobileAdsUsPrivacyChoice::OptedIn;
	Privacy.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Standard;
	Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::No;
	Privacy.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::No;
	Privacy.bConsentStatusFresh = true;
	Privacy.Source = TEXT("MockConsent");
	Privacy.LastUpdated = FDateTime::UtcNow();
	TestTrue(
		TEXT("A normalized privacy snapshot is accepted before initialization"),
		Subsystem->UpdatePrivacySnapshot(Privacy).bAccepted
	);

	TestTrue(
		TEXT("Initialization starts after signal propagation"),
		Subsystem->InitializeAds().bAccepted
	);
	TestEqual(
		TEXT("The direct provider receives signals once"),
		Provider.ConsentSignalCalls,
		1
	);
	TestTrue(
		TEXT("The direct provider receives every configured signal"),
		Provider.LastConsentSignalMask == AllSignals
	);
	TestTrue(
		TEXT("Direct-provider propagation precedes SDK initialization"),
		Provider.ConsentSignalSequence < Provider.InitializationSequence
	);
	TestEqual(TEXT("A matching adapter receives signals"), AppliedAdapter.Calls, 1);
	TestTrue(
		TEXT("Adapter propagation precedes SDK initialization"),
		AppliedAdapter.LastSequence < Provider.InitializationSequence
	);
	TestEqual(TEXT("Adapters for other providers are isolated"), OtherProviderAdapter.Calls, 0);
	TestEqual(
		TEXT("The adapter SPI cannot register another direct provider"),
		InvalidProviderConsumer.Calls,
		0
	);
	TestEqual(
		TEXT("The initialization request carries the normalized signals"),
		Provider.LastInitializationRequest.PrivacyContext.ConsentSignals,
		Provider.LastConsentSignals
	);

	const FOpenMobileAdsConsentSignalDeliverySnapshot InitialDelivery =
		Subsystem->GetConsentSignalDeliveryStatus();
	TestEqual(
		TEXT("The report lists the provider and matching consumers"),
		InitialDelivery.Consumers.Num(),
		5
	);
	const FOpenMobileAdsConsentSignalDeliveryStatus* ProviderDelivery =
		InitialDelivery.Find(
			EOpenMobileAdsConsentSignalConsumerType::Provider,
			TEXT("MockAds")
		);
	const FOpenMobileAdsConsentSignalDeliveryStatus* AppliedDelivery =
		InitialDelivery.Find(
			EOpenMobileAdsConsentSignalConsumerType::Adapter,
			TEXT("AppliedAdapter"),
			TEXT("MockNetwork")
		);
	const FOpenMobileAdsConsentSignalDeliveryStatus* UnconfirmedDelivery =
		InitialDelivery.Find(
			EOpenMobileAdsConsentSignalConsumerType::Adapter,
			TEXT("UnconfirmedAdapter"),
			TEXT("MockNetwork")
		);
	const FOpenMobileAdsConsentSignalDeliveryStatus* UnsupportedDelivery =
		InitialDelivery.Find(
			EOpenMobileAdsConsentSignalConsumerType::Network,
			TEXT("UnsupportedNetwork")
		);
	const FOpenMobileAdsConsentSignalDeliveryStatus* FailedDelivery =
		InitialDelivery.Find(
			EOpenMobileAdsConsentSignalConsumerType::Adapter,
			TEXT("FailedAdapter"),
			TEXT("FailedNetwork")
		);
	TestNotNull(TEXT("The direct-provider delivery is reported"), ProviderDelivery);
	TestNotNull(TEXT("The applied adapter delivery is reported"), AppliedDelivery);
	TestNotNull(TEXT("The unconfirmed adapter delivery is reported"), UnconfirmedDelivery);
	TestNotNull(TEXT("The unsupported network delivery is reported"), UnsupportedDelivery);
	TestNotNull(TEXT("The failed adapter delivery is reported"), FailedDelivery);
	if (ProviderDelivery)
	{
		TestEqual(
			TEXT("The provider confirms required signals"),
			ProviderDelivery->State,
			EOpenMobileAdsConsentSignalDeliveryState::Applied
		);
	}
	if (AppliedDelivery)
	{
		TestEqual(
			TEXT("A capable adapter confirms required signals"),
			AppliedDelivery->State,
			EOpenMobileAdsConsentSignalDeliveryState::Applied
		);
	}
	if (UnconfirmedDelivery)
	{
		TestEqual(
			TEXT("Missing confirmation is reported"),
			UnconfirmedDelivery->State,
			EOpenMobileAdsConsentSignalDeliveryState::Unconfirmed
		);
	}
	if (UnsupportedDelivery)
	{
		TestEqual(
			TEXT("Unsupported required signals are reported"),
			UnsupportedDelivery->State,
			EOpenMobileAdsConsentSignalDeliveryState::Unsupported
		);
	}
	if (FailedDelivery)
	{
		TestEqual(
			TEXT("A rejected signal is reported as failed"),
			FailedDelivery->State,
			EOpenMobileAdsConsentSignalDeliveryState::Failed
		);
		TestEqual(
			TEXT("A failed delivery keeps the typed error"),
			FailedDelivery->Error.Code,
			EOpenMobileAdsErrorCode::NativeFailure
		);
	}

	Provider.CompleteInitialization();
	DrainGameThreadTasks();
	Privacy.UsPrivacy.Choice = EOpenMobileAdsUsPrivacyChoice::OptedOut;
	Privacy.UsPrivacy.DataProcessingMode =
		EOpenMobileAdsDataProcessingMode::Restricted;
	Privacy.LastUpdated = FDateTime::UtcNow();
	TestTrue(
		TEXT("A changed choice is accepted after initialization"),
		Subsystem->UpdatePrivacySnapshot(Privacy).bAccepted
	);
	TestEqual(TEXT("The provider receives the changed choice"), Provider.ConsentSignalCalls, 2);
	TestEqual(TEXT("Only the changed provider signal is applied"), Provider.LastConsentSignalMask, UsPrivacySignal);
	TestEqual(
		TEXT("A runtime-capable adapter receives the changed choice"),
		AppliedAdapter.Calls,
		2
	);
	TestEqual(
		TEXT("An unconfirmed adapter still receives supported updates"),
		UnconfirmedAdapter.Calls,
		2
	);
	TestEqual(
		TEXT("A network without runtime support is not called again"),
		UnsupportedNetwork.Calls,
		1
	);
	TestEqual(TEXT("Initial and changed deliveries are broadcast"), DeliveryBroadcasts, 2);

	TestTrue(
		TEXT("An identical snapshot remains accepted"),
		Subsystem->UpdatePrivacySnapshot(Privacy).bAccepted
	);
	TestEqual(TEXT("Identical signals do not reapply to the provider"), Provider.ConsentSignalCalls, 2);
	TestEqual(TEXT("Identical signals do not rebroadcast delivery"), DeliveryBroadcasts, 2);

	Subsystem->OnNativeConsentSignalDeliveryChanged().Remove(DeliveryHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsInitializationIdempotencyContractTest,
	"OpenMobile.Ads.ProviderContract.Initialization.IdempotencyAndConfiguration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsInitializationIdempotencyContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->bDevelopmentTestMode = true;
	ScopedSettings.Settings->TestDeviceIdentifiers = {
		TEXT("GLOBAL-DEVICE-A"),
		TEXT("GLOBAL-DEVICE-B")
	};
	ScopedSettings.Settings->Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::Yes;
	ScopedSettings.Settings->Privacy.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::No;
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent = false;
	ScopedSettings.Settings->RequestConfiguration.MaxAdContentRating =
		EOpenMobileAdsMaxAdContentRating::ParentalGuidance;

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);

	const FOpenMobileAdsOperationResult First = Subsystem->InitializeAds();
	TestTrue(TEXT("The first initialization request is accepted"), First.bAccepted);
	TestTrue(TEXT("Initialization receives a stable request ID"), First.RequestId.IsValid());
	TestEqual(TEXT("The service enters initializing state"), Subsystem->GetServiceState(), EOpenMobileAdsServiceState::Initializing);
	TestEqual(TEXT("The provider starts once"), Provider.InitializationCalls, 1);
	TestTrue(TEXT("Test mode reaches the provider before initialization"), Provider.LastInitializationRequest.Development.bEnabled);
	TestTrue(TEXT("Test mode enables test devices"), Provider.LastInitializationRequest.Development.bUseTestDevices);
	TestTrue(TEXT("Test mode enables official test IDs"), Provider.LastInitializationRequest.Development.bUseTestAdUnitIds);
	TestTrue(TEXT("Test mode enables consent debug intent"), Provider.LastInitializationRequest.Development.bEnableConsentDebug);
	TestTrue(TEXT("Test mode enables verbose diagnostics"), Provider.LastInitializationRequest.Development.bEnableVerboseDiagnostics);
	TestEqual(
		TEXT("Global test-device identifiers reach the provider before initialization"),
		Provider.LastInitializationRequest.Development.TestDeviceIdentifiers,
		ScopedSettings.Settings->TestDeviceIdentifiers
	);
	TestEqual(
		TEXT("Child-directed treatment reaches the provider"),
		Provider.LastInitializationRequest.Privacy.ChildDirectedTreatment,
		EOpenMobileAdsAgeTreatment::Yes
	);
	TestEqual(
		TEXT("Under-age treatment reaches the provider"),
		Provider.LastInitializationRequest.Privacy.UnderAgeOfConsent,
		EOpenMobileAdsAgeTreatment::No
	);
	TestEqual(
		TEXT("Request configuration reaches the provider"),
		Provider.LastInitializationRequest.RequestConfiguration.MaxAdContentRating,
		EOpenMobileAdsMaxAdContentRating::ParentalGuidance
	);

	const FOpenMobileAdsOperationResult Overlapping = Subsystem->InitializeAds();
	TestTrue(TEXT("An overlapping initialization call is accepted"), Overlapping.bAccepted);
	TestEqual(TEXT("Overlapping calls share the request ID"), Overlapping.RequestId, First.RequestId);
	TestEqual(TEXT("An overlapping call does not restart the provider"), Provider.InitializationCalls, 1);

	TFuture<void> Completion = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		Provider.CompleteInitialization();
	});
	Completion.Wait();
	DrainGameThreadTasks();
	TestEqual(TEXT("Provider completion makes the service ready"), Subsystem->GetServiceState(), EOpenMobileAdsServiceState::Ready);

	const FOpenMobileAdsOperationResult Repeated = Subsystem->InitializeAds();
	TestTrue(TEXT("Initialization remains idempotent after success"), Repeated.bAccepted);
	TestEqual(TEXT("Successful repeated calls share the request ID"), Repeated.RequestId, First.RequestId);
	TestEqual(TEXT("A successful repeated call does not restart the provider"), Provider.InitializationCalls, 1);

	Subsystem->Deinitialize();
	TestEqual(TEXT("Teardown shuts down the initialized provider once"), Provider.ShutdownCalls, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsInitializationStatusContractTest,
	"OpenMobile.Ads.ProviderContract.Initialization.StatusReporting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsInitializationStatusContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	FMockProvider Provider(TEXT("MockAds"));
	Provider.Capabilities.ProviderVersion = TEXT("mock-2.4");
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	TArray<FOpenMobileAdsInitializationStatusSnapshot> Changes;
	const FDelegateHandle StatusHandle =
		Subsystem->OnNativeInitializationStatusChanged().AddLambda(
			[&Changes](const FOpenMobileAdsInitializationStatusSnapshot& Status)
			{
				Changes.Add(Status);
			}
		);

	const FOpenMobileAdsOperationResult Started = Subsystem->InitializeAds();
	TestTrue(TEXT("Initialization starts"), Started.bAccepted);
	const FOpenMobileAdsInitializationStatusSnapshot Initializing =
		Subsystem->GetInitializationStatus();
	TestEqual(TEXT("The snapshot is initializing"), Initializing.ServiceState, EOpenMobileAdsServiceState::Initializing);
	TestEqual(TEXT("The snapshot keeps the request ID"), Initializing.RequestId, Started.RequestId);
	TestTrue(TEXT("The snapshot records its start time"), Initializing.StartedAt != FDateTime());
	const FOpenMobileAdsInitializationComponentStatus* InitialProvider =
		FindInitializationComponent(
			Initializing,
			EOpenMobileAdsInitializationComponentType::Provider,
			TEXT("MockAds")
		);
	TestNotNull(TEXT("The provider has a normalized status"), InitialProvider);
	if (InitialProvider)
	{
		TestEqual(TEXT("The provider starts as initializing"), InitialProvider->State, EOpenMobileAdsInitializationState::Initializing);
		TestEqual(TEXT("The provider version is exposed"), InitialProvider->Version, FString(TEXT("mock-2.4")));
		TestTrue(TEXT("Provider capabilities are exposed"), InitialProvider->bHasCapabilities);
		TestTrue(TEXT("Rewarded capability is retained"), InitialProvider->Capabilities.SupportsFormat(EOpenMobileAdFormat::Rewarded));
	}

	TFuture<void> ComponentUpdates = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		FOpenMobileAdsInitializationComponentStatus Network;
		Network.Type = EOpenMobileAdsInitializationComponentType::Network;
		Network.Name = TEXT("MockNetwork");
		Network.Parent = TEXT("MockAds");
		Network.State = EOpenMobileAdsInitializationState::Ready;
		Network.Version = TEXT("1.2.0");
		Network.LatencyMilliseconds = 12.5;
		Provider.ReportInitializationStatus(MoveTemp(Network));

		FOpenMobileAdsInitializationComponentStatus Adapter;
		Adapter.Type = EOpenMobileAdsInitializationComponentType::Adapter;
		Adapter.Name = TEXT("MockAdapter");
		Adapter.Parent = TEXT("MockNetwork");
		Adapter.State = EOpenMobileAdsInitializationState::Initializing;
		Provider.ReportInitializationStatus(MoveTemp(Adapter));
	});
	ComponentUpdates.Wait();
	DrainGameThreadTasks();
	const FOpenMobileAdsInitializationStatusSnapshot ComponentStatus =
		Subsystem->GetInitializationStatus();
	const FOpenMobileAdsInitializationComponentStatus* Network =
		FindInitializationComponent(
			ComponentStatus,
			EOpenMobileAdsInitializationComponentType::Network,
			TEXT("MockNetwork")
		);
	TestNotNull(TEXT("Network status is reported"), Network);
	if (Network)
	{
		TestEqual(TEXT("Network version is retained"), Network->Version, FString(TEXT("1.2.0")));
		TestEqual(TEXT("Network latency is retained"), Network->LatencyMilliseconds, 12.5);
	}

	Provider.CompleteInitialization();
	DrainGameThreadTasks();
	FOpenMobileAdsInitializationStatusSnapshot Ready = Subsystem->GetInitializationStatus();
	TestEqual(TEXT("The service reports ready"), Ready.ServiceState, EOpenMobileAdsServiceState::Ready);
	TestTrue(TEXT("Service latency is recorded"), Ready.LatencyMilliseconds >= 0.0);
	TestTrue(TEXT("A pending adapter reports partial success"), Ready.bPartialSuccess);

	FOpenMobileAdsInitializationComponentStatus FailedAdapter;
	FailedAdapter.Type = EOpenMobileAdsInitializationComponentType::Adapter;
	FailedAdapter.Name = TEXT("MockAdapter");
	FailedAdapter.Parent = TEXT("MockNetwork");
	FailedAdapter.State = EOpenMobileAdsInitializationState::Failed;
	FailedAdapter.LatencyMilliseconds = 25.0;
	FailedAdapter.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Initialization,
		NAME_None,
		TEXT("The mock adapter did not initialize."),
		TEXT("MockAds")
	);
	Provider.ReportInitializationStatus(FailedAdapter);
	DrainGameThreadTasks();
	Ready = Subsystem->GetInitializationStatus();
	TestEqual(TEXT("Adapter failure does not fail the ready provider"), Ready.ServiceState, EOpenMobileAdsServiceState::Ready);
	TestTrue(TEXT("Adapter failure reports partial success"), Ready.bPartialSuccess);
	const FOpenMobileAdsInitializationComponentStatus* Failed =
		FindInitializationComponent(
			Ready,
			EOpenMobileAdsInitializationComponentType::Adapter,
			TEXT("MockAdapter")
		);
	TestNotNull(TEXT("Failed adapter remains inspectable"), Failed);
	if (Failed)
	{
		TestEqual(TEXT("Adapter failure detail is retained"), Failed->Error.Code, EOpenMobileAdsErrorCode::ProviderFailure);
	}

	FailedAdapter.State = EOpenMobileAdsInitializationState::Ready;
	FailedAdapter.LatencyMilliseconds = 40.0;
	FailedAdapter.Error = FOpenMobileAdsError();
	Provider.ReportInitializationStatus(MoveTemp(FailedAdapter));
	DrainGameThreadTasks();
	Ready = Subsystem->GetInitializationStatus();
	TestFalse(TEXT("A late ready adapter clears partial success"), Ready.bPartialSuccess);
	TestTrue(TEXT("Each accepted transition broadcasts a snapshot"), Changes.Num() >= 6);

	Subsystem->OnNativeInitializationStatusChanged().Remove(StatusHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsInitializationFailureContractTest,
	"OpenMobile.Ads.ProviderContract.Initialization.Failure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsInitializationFailureContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);

	const FOpenMobileAdsOperationResult Started = Subsystem->InitializeAds();
	TestTrue(TEXT("Initialization starts before an asynchronous failure"), Started.bAccepted);
	Provider.CompleteInitialization(FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Initialization,
		NAME_None,
		TEXT("The mock SDK failed to initialize."),
		TEXT("MockAds")
	));
	DrainGameThreadTasks();
	TestEqual(TEXT("Provider failure makes the service failed"), Subsystem->GetServiceState(), EOpenMobileAdsServiceState::Failed);
	const FOpenMobileAdsInitializationStatusSnapshot FailedStatus =
		Subsystem->GetInitializationStatus();
	TestEqual(TEXT("The status snapshot reports failure"), FailedStatus.ServiceState, EOpenMobileAdsServiceState::Failed);
	TestEqual(TEXT("The status snapshot keeps the provider error"), FailedStatus.Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	const FOpenMobileAdsInitializationComponentStatus* FailedProvider =
		FindInitializationComponent(
			FailedStatus,
			EOpenMobileAdsInitializationComponentType::Provider,
			TEXT("MockAds")
		);
	TestNotNull(TEXT("The failed provider remains inspectable"), FailedProvider);
	if (FailedProvider)
	{
		TestEqual(TEXT("The provider status reports failure"), FailedProvider->State, EOpenMobileAdsInitializationState::Failed);
		TestEqual(TEXT("The provider status keeps the failure detail"), FailedProvider->Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	}

	const FOpenMobileAdsOperationResult Repeated = Subsystem->InitializeAds();
	TestFalse(TEXT("A failed initialization is not reported as accepted"), Repeated.bAccepted);
	TestEqual(TEXT("The provider error is retained"), Repeated.Error.Code, EOpenMobileAdsErrorCode::NativeFailure);
	TestEqual(TEXT("A repeated failed call does not restart the provider"), Provider.InitializationCalls, 1);

	Subsystem->Deinitialize();
	TestEqual(TEXT("A provider that accepted initialization is shut down"), Provider.ShutdownCalls, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsInitializationSelectionContractTest,
	"OpenMobile.Ads.ProviderContract.Initialization.SelectionFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsInitializationSelectionContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;

	ScopedSettings.Settings->PreferredProvider = TEXT("MissingAds");
	UOpenMobileAdsSubsystem* MissingSubsystem = NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	const FOpenMobileAdsOperationResult Missing = MissingSubsystem->InitializeAds();
	TestFalse(TEXT("A missing preferred provider is rejected"), Missing.bAccepted);
	TestEqual(TEXT("A missing provider has a typed error"), Missing.Error.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);
	MissingSubsystem->Deinitialize();

	FMockProvider UnsupportedProvider(TEXT("UnsupportedAds"), false);
	FScopedProviderRegistration UnsupportedRegistration(UnsupportedProvider);
	ScopedSettings.Settings->PreferredProvider = TEXT("UnsupportedAds");
	UOpenMobileAdsSubsystem* UnsupportedSubsystem = NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	const FOpenMobileAdsOperationResult Unsupported = UnsupportedSubsystem->InitializeAds();
	TestFalse(TEXT("An unsupported platform is rejected"), Unsupported.bAccepted);
	TestEqual(TEXT("Unsupported platform failure is distinct"), Unsupported.Error.Code, EOpenMobileAdsErrorCode::UnsupportedPlatform);
	UnsupportedSubsystem->Deinitialize();

	FMockProvider Alpha(TEXT("AlphaAds"));
	FMockProvider Beta(TEXT("BetaAds"));
	FScopedProviderRegistration AlphaRegistration(Alpha);
	FScopedProviderRegistration BetaRegistration(Beta);
	ScopedSettings.Settings->PreferredProvider = NAME_None;
	UOpenMobileAdsSubsystem* ConflictSubsystem = NewObject<UOpenMobileAdsSubsystem>(NewObject<UGameInstance>());
	const FOpenMobileAdsOperationResult Conflict = ConflictSubsystem->InitializeAds();
	TestFalse(TEXT("Ambiguous provider selection is rejected"), Conflict.bAccepted);
	TestEqual(TEXT("Ambiguous selection has a typed error"), Conflict.Error.Code, EOpenMobileAdsErrorCode::ProviderConflict);
	ConflictSubsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsProviderSelectionContractTest,
	"OpenMobile.Ads.ProviderContract.Selection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsProviderSelectionContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FMockProvider Alpha(TEXT("AlphaAds"));
	FMockProvider Beta(TEXT("BetaAds"));
	TArray<IOpenMobileAdsProvider*> Providers{&Beta, &Alpha};

	const FOpenMobileAdsProviderSelection Conflict =
		FOpenMobileAdsProviderResolver::Resolve(Providers, NAME_None);
	TestNull(TEXT("Conflicts do not pick by registration order"), Conflict.Provider);
	TestEqual(TEXT("Conflict is typed"), Conflict.Error.Code, EOpenMobileAdsErrorCode::ProviderConflict);

	const FOpenMobileAdsProviderSelection Preferred =
		FOpenMobileAdsProviderResolver::Resolve(Providers, TEXT("alphaads"));
	TestTrue(TEXT("Provider names match without case sensitivity"), Preferred.Provider == &Alpha);
	TestFalse(TEXT("Successful selection has no error"), Preferred.Error.IsSet());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsProviderErrorContractTest,
	"OpenMobile.Ads.ProviderContract.Errors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsProviderErrorContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("UnsupportedInterstitial");
	Placement.Format = EOpenMobileAdFormat::Interstitial;
	Placement.Android.AdUnitId = TEXT("android-interstitial");
	Placement.IOS.AdUnitId = TEXT("ios-interstitial");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));

	TestEqual(
		TEXT("Empty placement errors are typed"),
		Subsystem->LoadAd(NAME_None).Error.Code,
		EOpenMobileAdsErrorCode::InvalidPlacement
	);
	TestEqual(
		TEXT("Unknown placement errors are typed"),
		Subsystem->LoadAd(TEXT("Unknown")).Error.Code,
		EOpenMobileAdsErrorCode::UnknownPlacement
	);
	TestEqual(
		TEXT("Unsupported format errors are typed"),
		Subsystem->LoadAd(TEXT("UnsupportedInterstitial")).Error.Code,
		EOpenMobileAdsErrorCode::UnsupportedFormat
	);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsLoadCallbackContractTest,
	"OpenMobile.Ads.ProviderContract.Load.Callback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsLoadCallbackContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	for (const FName PlacementName : {FName(TEXT("CallbackSuccess")), FName(TEXT("CallbackFailure"))})
	{
		FOpenMobileAdsPlacementSettings& Placement =
			ScopedSettings.Settings->Placements.Emplace_GetRef();
		Placement.Placement = PlacementName;
		Placement.Format = EOpenMobileAdFormat::Rewarded;
		Placement.Android.AdUnitId = FString::Printf(TEXT("android-%s"), *PlacementName.ToString());
		Placement.IOS.AdUnitId = FString::Printf(TEXT("ios-%s"), *PlacementName.ToString());
	}

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before load callback checks"),
		InitializeSuccessfully(*Subsystem, Provider)
	);

	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult SuccessfulLoad =
		Subsystem->LoadAd(TEXT("CallbackSuccess"));
	TestTrue(TEXT("The successful callback request is accepted"), SuccessfulLoad.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> SuccessSink =
		Provider.LoadSink;
	FOpenMobileAdsEvent DuplicateStarted;
	DuplicateStarted.Type = EOpenMobileAdsEventType::LoadStarted;
	DuplicateStarted.Provider = TEXT("WrongProvider");
	DuplicateStarted.Placement = TEXT("WrongPlacement");
	DuplicateStarted.Format = EOpenMobileAdFormat::Banner;
	DuplicateStarted.RequestId = FGuid::NewGuid();
	SuccessSink->Submit(MoveTemp(DuplicateStarted));
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.Provider = TEXT("WrongProvider");
	Loaded.Placement = TEXT("WrongPlacement");
	Loaded.Format = EOpenMobileAdFormat::Banner;
	Loaded.RequestId = FGuid::NewGuid();
	Loaded.CachedAdId = CachedAdId;
	SuccessSink->Submit(MoveTemp(Loaded));
	FOpenMobileAdsEvent DuplicateFailure;
	DuplicateFailure.Type = EOpenMobileAdsEventType::LoadFailed;
	DuplicateFailure.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::ProviderFailure,
		EOpenMobileAdsFailureStage::Load,
		TEXT("WrongPlacement"),
		TEXT("This duplicate terminal result must be ignored."),
		TEXT("WrongProvider")
	);
	SuccessSink->Submit(MoveTemp(DuplicateFailure));
	DrainGameThreadTasks();

	TestEqual(TEXT("A successful load broadcasts one start and one terminal result"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		const FOpenMobileAdsEvent& Started = Events[0];
		const FOpenMobileAdsEvent& Completed = Events[1];
		TestEqual(TEXT("The first successful event is load started"), Started.Type, EOpenMobileAdsEventType::LoadStarted);
		TestEqual(TEXT("The successful terminal event is loaded"), Completed.Type, EOpenMobileAdsEventType::Loaded);
		TestEqual(TEXT("Load started has the normalized placement"), Started.Placement, FName(TEXT("CallbackSuccess")));
		TestEqual(TEXT("Loaded has the normalized placement"), Completed.Placement, FName(TEXT("CallbackSuccess")));
		TestEqual(TEXT("Load started has the normalized format"), Started.Format, EOpenMobileAdFormat::Rewarded);
		TestEqual(TEXT("Loaded has the normalized format"), Completed.Format, EOpenMobileAdFormat::Rewarded);
		TestEqual(TEXT("Load started has the normalized provider"), Started.Provider, Provider.Name);
		TestEqual(TEXT("Loaded has the normalized provider"), Completed.Provider, Provider.Name);
		TestEqual(TEXT("Load started has the accepted request ID"), Started.RequestId, SuccessfulLoad.RequestId);
		TestEqual(TEXT("Loaded has the accepted request ID"), Completed.RequestId, SuccessfulLoad.RequestId);
		TestEqual(TEXT("Loaded preserves the provider cache ID"), Completed.CachedAdId, CachedAdId);
		TestEqual(TEXT("Load started reports loading state"), Started.PlacementState, EOpenMobileAdPlacementState::Loading);
		TestEqual(TEXT("Loaded reports ready state"), Completed.PlacementState, EOpenMobileAdPlacementState::Ready);
		TestTrue(TEXT("Load callback sequence is increasing"), Started.Sequence < Completed.Sequence);
	}

	const FOpenMobileAdsOperationResult FailedLoad =
		Subsystem->LoadAd(TEXT("CallbackFailure"));
	TestTrue(TEXT("The failed callback request is accepted"), FailedLoad.bAccepted);
	const TSharedPtr<IOpenMobileAdsProviderEventSink, ESPMode::ThreadSafe> FailureSink =
		Provider.LoadSink;
	FOpenMobileAdsEvent Failed;
	Failed.Type = EOpenMobileAdsEventType::LoadFailed;
	Failed.Provider = TEXT("WrongProvider");
	Failed.Placement = TEXT("WrongPlacement");
	Failed.Format = EOpenMobileAdFormat::Banner;
	Failed.RequestId = FGuid::NewGuid();
	Failed.Error = FOpenMobileAdsError::Make(
		EOpenMobileAdsErrorCode::NativeFailure,
		EOpenMobileAdsFailureStage::Load,
		TEXT("WrongPlacement"),
		TEXT("The native load failed."),
		TEXT("WrongProvider"),
		FString(),
		true
	);
	FailureSink->Submit(MoveTemp(Failed));
	DrainGameThreadTasks();

	TestEqual(TEXT("A failed load adds one start and one terminal result"), Events.Num(), 4);
	if (Events.Num() == 4)
	{
		const FOpenMobileAdsEvent& Started = Events[2];
		const FOpenMobileAdsEvent& Completed = Events[3];
		TestEqual(TEXT("The first failed event is load started"), Started.Type, EOpenMobileAdsEventType::LoadStarted);
		TestEqual(TEXT("The failed terminal event is load failed"), Completed.Type, EOpenMobileAdsEventType::LoadFailed);
		TestEqual(TEXT("Failed load has the normalized placement"), Completed.Placement, FName(TEXT("CallbackFailure")));
		TestEqual(TEXT("Failed load has the normalized format"), Completed.Format, EOpenMobileAdFormat::Rewarded);
		TestEqual(TEXT("Failed load has the normalized provider"), Completed.Provider, Provider.Name);
		TestEqual(TEXT("Failed load has the accepted request ID"), Completed.RequestId, FailedLoad.RequestId);
		TestEqual(TEXT("Failed load normalizes the error placement"), Completed.Error.Placement, FName(TEXT("CallbackFailure")));
		TestEqual(TEXT("Failed load normalizes the error provider"), Completed.Error.Provider, Provider.Name);
		TestEqual(TEXT("Failed load reports failed state"), Completed.PlacementState, EOpenMobileAdPlacementState::Failed);
	}

	FOpenMobileAdsEvent LateLoaded;
	LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
	LateLoaded.CachedAdId = FGuid::NewGuid();
	FailureSink->Submit(MoveTemp(LateLoaded));
	FOpenMobileAdsEvent LateFailure;
	LateFailure.Type = EOpenMobileAdsEventType::LoadFailed;
	FailureSink->Submit(MoveTemp(LateFailure));
	DrainGameThreadTasks();
	TestEqual(TEXT("Late terminal callbacks are ignored"), Events.Num(), 4);

	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsProviderEventContractTest,
	"OpenMobile.Ads.ProviderContract.EventsAndTeardown",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsProviderEventContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ContinueReward");
	Placement.Format = EOpenMobileAdFormat::Rewarded;
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TestNotNull(TEXT("Subsystem can be created"), Subsystem);
	if (!Subsystem)
	{
		return false;
	}
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));

	TArray<FOpenMobileAdsEvent> Events;
	bool bEveryCallbackWasOnGameThread = true;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events, &bEveryCallbackWasOnGameThread](const FOpenMobileAdsEvent& Event)
		{
			bEveryCallbackWasOnGameThread &= IsInGameThread();
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult LoadResult =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestTrue(TEXT("Configured load is accepted"), LoadResult.bAccepted);
	TestTrue(TEXT("Provider receives a request ID"), Provider.LastLoadRequest.RequestId.IsValid());
	TestEqual(TEXT("Provider receives the placement"), Provider.LastLoadRequest.Placement.Placement, FName(TEXT("ContinueReward")));
	TestTrue(TEXT("Provider receives an event sink"), Provider.LoadSink.IsValid());
	if (!Provider.LoadSink.IsValid())
	{
		AddError(FString::Printf(
			TEXT("Load was rejected: %s"),
			*LoadResult.Error.Explanation
		));
		Subsystem->Deinitialize();
		Subsystem->OnNativeAdsEvent().Remove(EventHandle);
		return false;
	}

	TFuture<void> Callback = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		FOpenMobileAdsEvent PrematureDismiss;
		PrematureDismiss.Type = EOpenMobileAdsEventType::Dismissed;
		Provider.LoadSink->Submit(PrematureDismiss);
		FOpenMobileAdsEvent Loaded;
		Loaded.Type = EOpenMobileAdsEventType::Loaded;
		Loaded.CachedAdId = FGuid::NewGuid();
		Provider.LoadSink->Submit(Loaded);
	});
	Callback.Wait();
	DrainGameThreadTasks();

	TestTrue(TEXT("Off-thread callbacks are marshaled to the game thread"), bEveryCallbackWasOnGameThread);
	TestTrue(TEXT("Placement becomes ready"), Subsystem->IsReady(TEXT("ContinueReward")));
	TestEqual(TEXT("Load emits started then loaded"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		TestEqual(TEXT("First event is load started"), Events[0].Type, EOpenMobileAdsEventType::LoadStarted);
		TestEqual(TEXT("Second event is loaded"), Events[1].Type, EOpenMobileAdsEventType::Loaded);
		TestTrue(TEXT("Event sequence is increasing"), Events[0].Sequence < Events[1].Sequence);
	}

	FOpenMobileAdsEvent DuplicateLoaded;
	DuplicateLoaded.Type = EOpenMobileAdsEventType::Loaded;
	Provider.LoadSink->Submit(DuplicateLoaded);
	DrainGameThreadTasks();
	TestEqual(TEXT("Duplicate terminal callbacks are ignored"), Events.Num(), 2);

	const FOpenMobileAdsOperationResult ShowResult =
		Subsystem->ShowAd(TEXT("ContinueReward"));
	TestTrue(TEXT("Ready placements can be shown"), ShowResult.bAccepted);
	TestTrue(TEXT("Provider receives a show sink"), Provider.ShowSink.IsValid());
	if (!Provider.ShowSink.IsValid())
	{
		Subsystem->Deinitialize();
		Subsystem->OnNativeAdsEvent().Remove(EventHandle);
		return false;
	}

	FOpenMobileAdsEvent Shown;
	Shown.Type = EOpenMobileAdsEventType::Shown;
	Provider.ShowSink->Submit(Shown);
	TFuture<void> ClickCallback = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		FOpenMobileAdsEvent Clicked;
		Clicked.Type = EOpenMobileAdsEventType::Clicked;
		Provider.ShowSink->Submit(Clicked);
	});
	TFuture<void> RevenueCallback = Async(EAsyncExecution::ThreadPool, [&Provider]()
	{
		FOpenMobileAdsEvent Revenue;
		Revenue.Type = EOpenMobileAdsEventType::RevenuePaid;
		Provider.ShowSink->Submit(Revenue);
	});
	ClickCallback.Wait();
	RevenueCallback.Wait();
	FOpenMobileAdsEvent Reward;
	Reward.Type = EOpenMobileAdsEventType::RewardEarned;
	Reward.bHasReward = true;
	Reward.Reward.Amount = 10;
	Provider.ShowSink->Submit(Reward);
	Provider.ShowSink->Submit(Reward);
	FOpenMobileAdsEvent Dismissed;
	Dismissed.Type = EOpenMobileAdsEventType::Dismissed;
	Provider.ShowSink->Submit(Dismissed);
	FOpenMobileAdsEvent LateClick;
	LateClick.Type = EOpenMobileAdsEventType::Clicked;
	Provider.ShowSink->Submit(LateClick);
	FOpenMobileAdsEvent LateRevenue;
	LateRevenue.Type = EOpenMobileAdsEventType::RevenuePaid;
	Provider.ShowSink->Submit(LateRevenue);
	DrainGameThreadTasks();

	TestEqual(TEXT("Duplicate rewards are ignored"), Events.Num(), 8);
	if (Events.Num() == 8)
	{
		TestEqual(TEXT("Show acceptance precedes provider events"), Events[2].Type, EOpenMobileAdsEventType::ShowAccepted);
		TestEqual(TEXT("Shown precedes reward"), Events[3].Type, EOpenMobileAdsEventType::Shown);
		TestEqual(TEXT("Reward precedes dismissal"), Events[6].Type, EOpenMobileAdsEventType::RewardEarned);
		TestEqual(TEXT("Dismissal is terminal"), Events[7].Type, EOpenMobileAdsEventType::Dismissed);
		for (int32 Index = 1; Index < Events.Num(); ++Index)
		{
			TestTrue(
				TEXT("Concurrent callbacks retain one total event order"),
				Events[Index - 1].Sequence < Events[Index].Sequence
			);
		}
	}
	TestEqual(
		TEXT("Dismissal restores idle state"),
		Subsystem->GetPlacementStatus(TEXT("ContinueReward")).State,
		EOpenMobileAdPlacementState::Idle
	);

	Subsystem->Deinitialize();
	FOpenMobileAdsEvent LateFailure;
	LateFailure.Type = EOpenMobileAdsEventType::LoadFailed;
	Provider.LoadSink->Submit(LateFailure);
	DrainGameThreadTasks();
	TestEqual(TEXT("Post-destroy callbacks are ignored"), Events.Num(), 8);
	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsProviderUnregistrationContractTest,
	"OpenMobile.Ads.ProviderContract.Unregistration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsProviderUnregistrationContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("ContinueReward");
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));
	const FOpenMobileAdsOperationResult LoadResult =
		Subsystem->LoadAd(TEXT("ContinueReward"));
	TestTrue(TEXT("Load begins before unregistration"), LoadResult.bAccepted);
	DrainGameThreadTasks();

	AddExpectedError(
		TEXT("The ads provider was unregistered during an active placement operation."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Registration.Unregister();
	DrainGameThreadTasks();
	const FOpenMobileAdsPlacementStatus Status =
		Subsystem->GetPlacementStatus(TEXT("ContinueReward"));
	TestEqual(TEXT("Unregistration fails active state"), Status.State, EOpenMobileAdPlacementState::Failed);
	TestEqual(TEXT("Unregistration has a typed error"), Status.LastError.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);
	const FOpenMobileAdsInitializationStatusSnapshot InitializationStatus =
		Subsystem->GetInitializationStatus();
	TestEqual(TEXT("Unregistration fails initialization status"), InitializationStatus.ServiceState, EOpenMobileAdsServiceState::Failed);
	TestEqual(TEXT("Initialization status keeps unregistration detail"), InitializationStatus.Error.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);
	const FOpenMobileAdsInitializationComponentStatus* ProviderStatus =
		FindInitializationComponent(
			InitializationStatus,
			EOpenMobileAdsInitializationComponentType::Provider,
			TEXT("MockAds")
		);
	TestNotNull(TEXT("The unregistered provider remains inspectable"), ProviderStatus);
	if (ProviderStatus)
	{
		TestEqual(TEXT("The unregistered provider reports failure"), ProviderStatus->State, EOpenMobileAdsInitializationState::Failed);
	}

	if (Provider.LoadSink)
	{
		FOpenMobileAdsEvent LateLoaded;
		LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
		Provider.LoadSink->Submit(LateLoaded);
		DrainGameThreadTasks();
	}
	TestFalse(TEXT("Late load does not recover an unregistered provider"), Subsystem->IsReady(TEXT("ContinueReward")));
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsRequestCancellationContractTest,
	"OpenMobile.Ads.ProviderContract.Cancellation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsRequestCancellationContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("CancelableReward");
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));
	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult LoadResult =
		Subsystem->LoadAd(TEXT("CancelableReward"));
	TestTrue(TEXT("Load begins before cancellation"), LoadResult.bAccepted);
	DrainGameThreadTasks();

	const FOpenMobileAdsOperationResult CancelResult =
		Subsystem->CancelRequest(LoadResult.RequestId);
	TestTrue(TEXT("Active request cancellation is accepted"), CancelResult.bAccepted);
	DrainGameThreadTasks();
	TestEqual(TEXT("Provider cancellation runs once"), Provider.CancelledRequests.Num(), 1);
	if (Provider.CancelledRequests.Num() == 1)
	{
		TestEqual(TEXT("Provider receives the active request ID"), Provider.CancelledRequests[0], LoadResult.RequestId);
	}
	TestEqual(TEXT("Cancellation emits one terminal event"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		TestEqual(TEXT("Cancellation uses the failed terminal event"), Events[1].Type, EOpenMobileAdsEventType::Failed);
		TestEqual(TEXT("Cancellation has a typed error"), Events[1].Error.Code, EOpenMobileAdsErrorCode::Cancelled);
	}
	TestEqual(
		TEXT("Cancelled load returns placement to idle"),
		Subsystem->GetPlacementStatus(TEXT("CancelableReward")).State,
		EOpenMobileAdPlacementState::Idle
	);

	FOpenMobileAdsEvent LateLoaded;
	LateLoaded.Type = EOpenMobileAdsEventType::Loaded;
	Provider.LoadSink->Submit(LateLoaded);
	DrainGameThreadTasks();
	TestEqual(TEXT("Late callback after cancellation is ignored"), Events.Num(), 2);
	TestFalse(TEXT("Late callback cannot restore readiness"), Subsystem->IsReady(TEXT("CancelableReward")));

	TestFalse(
		TEXT("Completed request cannot be cancelled again"),
		Subsystem->CancelRequest(LoadResult.RequestId).bAccepted
	);
	const FOpenMobileAdsOperationResult TeardownLoad =
		Subsystem->LoadAd(TEXT("CancelableReward"));
	TestTrue(TEXT("A new load can begin after cancellation"), TeardownLoad.bAccepted);
	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	TestEqual(TEXT("Subsystem teardown cancels active provider work"), Provider.CancelledRequests.Num(), 2);
	if (Provider.CancelledRequests.Num() == 2)
	{
		TestEqual(TEXT("Teardown cancels the current request"), Provider.CancelledRequests[1], TeardownLoad.RequestId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsDestroyAllFailureContractTest,
	"OpenMobile.Ads.ProviderContract.DestroyAllFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsDestroyAllFailureContractTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("DestroyFailureReward");
	Placement.Android.AdUnitId = TEXT("android-destroy-failure");
	Placement.IOS.AdUnitId = TEXT("ios-destroy-failure");
	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));
	TestTrue(
		TEXT("The placement loads before destroy-all failure"),
		Subsystem->LoadAd(TEXT("DestroyFailureReward")).bAccepted
	);
	const FGuid CachedAdId = FGuid::NewGuid();
	FOpenMobileAdsEvent Loaded;
	Loaded.Type = EOpenMobileAdsEventType::Loaded;
	Loaded.CachedAdId = CachedAdId;
	Provider.LoadSink->Submit(MoveTemp(Loaded));
	DrainGameThreadTasks();
	TestTrue(TEXT("The placement is ready before destroy-all failure"), Subsystem->IsReady(TEXT("DestroyFailureReward")));
	TArray<FOpenMobileAdsEvent> Events;
	const FDelegateHandle EventHandle = Subsystem->OnNativeAdsEvent().AddLambda(
		[&Events](const FOpenMobileAdsEvent& Event)
		{
			Events.Add(Event);
		}
	);

	const FOpenMobileAdsOperationResult DestroyResult = Subsystem->DestroyAllAds();
	TestTrue(TEXT("Service-wide destroy starts"), DestroyResult.bAccepted);
	TestTrue(TEXT("Provider receives a service-wide destroy sink"), Provider.DestroySink.IsValid());
	if (Provider.DestroySink)
	{
		FOpenMobileAdsEvent Failed;
		Failed.Type = EOpenMobileAdsEventType::Failed;
		Failed.Error = FOpenMobileAdsError::Make(
			EOpenMobileAdsErrorCode::ProviderFailure,
			EOpenMobileAdsFailureStage::Teardown,
			NAME_None,
			TEXT("The provider could not destroy all ads."),
			TEXT("MockAds")
		);
		AddExpectedError(
			TEXT("The provider could not destroy all ads."),
			EAutomationExpectedErrorFlags::Contains,
			1
		);
		Provider.DestroySink->Submit(Failed);
		DrainGameThreadTasks();
	}

	TestEqual(TEXT("Destroy-all failure emits one terminal event"), Events.Num(), 1);
	if (Events.Num() == 1)
	{
		TestEqual(TEXT("Destroy-all terminal event is failed"), Events[0].Type, EOpenMobileAdsEventType::Failed);
		TestEqual(TEXT("Destroy-all failure keeps the request ID"), Events[0].RequestId, DestroyResult.RequestId);
	}
	const FOpenMobileAdsPlacementStatus FailedStatus =
		Subsystem->GetPlacementStatus(TEXT("DestroyFailureReward"));
	TestEqual(TEXT("Destroy-all failure leaves a typed failed state"), FailedStatus.State, EOpenMobileAdPlacementState::Failed);
	TestEqual(TEXT("Destroy-all failure keeps its typed error"), FailedStatus.LastError.Code, EOpenMobileAdsErrorCode::ProviderFailure);
	TestFalse(TEXT("Destroy-all failure cannot revive cached readiness"), Subsystem->IsReady(TEXT("DestroyFailureReward")));
	TestEqual(TEXT("Destroy-all failure releases the cached native ad"), Provider.ReleasedCachedAds.Num(), 1);
	if (Provider.ReleasedCachedAds.Num() == 1)
	{
		TestEqual(TEXT("Destroy-all failure releases the matching cache"), Provider.ReleasedCachedAds[0], CachedAdId);
	}
	TestFalse(
		TEXT("Terminal destroy-all request is no longer cancellable"),
		Subsystem->CancelRequest(DestroyResult.RequestId).bAccepted
	);

	const FOpenMobileAdsOperationResult UnregisteredDestroy = Subsystem->DestroyAllAds();
	TestTrue(TEXT("A second service-wide destroy starts"), UnregisteredDestroy.bAccepted);
	AddExpectedError(
		TEXT("The ads provider was unregistered during a service-wide operation."),
		EAutomationExpectedErrorFlags::Contains,
		1
	);
	Registration.Unregister();
	DrainGameThreadTasks();
	TestEqual(TEXT("Unregistration emits one service-wide terminal event"), Events.Num(), 2);
	if (Events.Num() == 2)
	{
		TestEqual(TEXT("Unregistration keeps the service-wide request ID"), Events[1].RequestId, UnregisteredDestroy.RequestId);
		TestEqual(TEXT("Unregistration is typed"), Events[1].Error.Code, EOpenMobileAdsErrorCode::ProviderUnavailable);
	}
	TestFalse(
		TEXT("Unregistered service-wide request is terminal"),
		Subsystem->CancelRequest(UnregisteredDestroy.RequestId).bAccepted
	);
	Subsystem->OnNativeAdsEvent().Remove(EventHandle);
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsAsyncWorldCleanupTest,
	"OpenMobile.Ads.Blueprint.AsyncWorldCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsAsyncWorldCleanupTest::RunTest(const FString& Parameters)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("WorldCleanupReward");
	Placement.Android.AdUnitId = TEXT("android-mock-unit");
	Placement.IOS.AdUnitId = TEXT("ios-mock-unit");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(GameInstance);
	TestTrue(TEXT("The provider initializes before placement operations"), InitializeSuccessfully(*Subsystem, Provider));
	const FOpenMobileAdsOperationResult LoadResult =
		Subsystem->LoadAd(TEXT("WorldCleanupReward"));
	TestTrue(TEXT("Load begins before world cleanup"), LoadResult.bAccepted);

	UWorld* World = NewObject<UWorld>();
	UOpenMobileAdsAsyncAction* Action = NewObject<UOpenMobileAdsAsyncAction>();
	Action->Subsystem = Subsystem;
	Action->TargetWorld = World;
	Action->RequestId = LoadResult.RequestId;
	Action->HandleWorldCleanup(World, true, true);
	DrainGameThreadTasks();

	TestTrue(TEXT("World cleanup finishes the async proxy"), Action->bFinished);
	TestEqual(TEXT("World cleanup cancels provider work once"), Provider.CancelledRequests.Num(), 1);
	if (Provider.CancelledRequests.Num() == 1)
	{
		TestEqual(TEXT("World cleanup cancels the active request"), Provider.CancelledRequests[0], LoadResult.RequestId);
	}
	TestFalse(TEXT("World cleanup leaves no ready ad"), Subsystem->IsReady(TEXT("WorldCleanupReward")));
	Subsystem->Deinitialize();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FOpenMobileAdsCanRequestAdsSubsystemTest,
	"OpenMobile.Ads.Privacy.CanRequestAds.Subsystem",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FOpenMobileAdsCanRequestAdsSubsystemTest::RunTest(
	const FString& Parameters
)
{
	using namespace OpenMobileAdsProviderContractTests;
	FScopedSettings ScopedSettings;
	ScopedSettings.Settings->PreferredProvider = TEXT("MockAds");
	ScopedSettings.Settings->Privacy.bDelayProviderInitializationUntilConsent = true;
	ScopedSettings.Settings->Privacy.ChildDirectedTreatment =
		EOpenMobileAdsAgeTreatment::Yes;
	ScopedSettings.Settings->Privacy.UnderAgeOfConsent =
		EOpenMobileAdsAgeTreatment::No;
	ScopedSettings.Settings->Placements.Reset();
	FOpenMobileAdsPlacementSettings& Placement =
		ScopedSettings.Settings->Placements.Emplace_GetRef();
	Placement.Placement = TEXT("CanRequestReward");
	Placement.Android.AdUnitId = TEXT("android-can-request");
	Placement.IOS.AdUnitId = TEXT("ios-can-request");

	FMockProvider Provider(TEXT("MockAds"));
	FScopedProviderRegistration Registration(Provider);
	UOpenMobileAdsSubsystem* Subsystem = NewObject<UOpenMobileAdsSubsystem>(
		NewObject<UGameInstance>()
	);
	TestTrue(
		TEXT("The provider initializes before policy evaluation"),
		InitializeSuccessfully(*Subsystem, Provider, false)
	);
	TestNotNull(
		TEXT("Blueprints can evaluate ad-request policy"),
		UOpenMobileAdsSubsystem::StaticClass()->FindFunctionByName(
			GET_FUNCTION_NAME_CHECKED(UOpenMobileAdsSubsystem, CanRequestAds)
		)
	);
	TestNotNull(
		TEXT("Blueprints can bind ad-request policy changes"),
		UOpenMobileAdsSubsystem::StaticClass()->FindPropertyByName(
			GET_MEMBER_NAME_CHECKED(UOpenMobileAdsSubsystem, OnCanRequestAdsChanged)
		)
	);
	TestEqual(
		TEXT("Ready service still waits for a consent result"),
		Subsystem->CanRequestAds().BlockReason,
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentUnknown
	);

	TArray<FOpenMobileAdsCanRequestAdsResult> Events;
	const FDelegateHandle EventHandle =
		Subsystem->OnNativeCanRequestAdsChanged().AddLambda(
			[&Events](const FOpenMobileAdsCanRequestAdsResult& Result)
			{
				Events.Add(Result);
			}
		);

	FOpenMobileAdsPrivacySnapshot Privacy = Subsystem->GetPrivacySnapshot();
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Privacy.bConsentStatusFresh = true;
	Privacy.ChildDirectedTreatment = EOpenMobileAdsAgeTreatment::Yes;
	Privacy.UnderAgeOfConsent = EOpenMobileAdsAgeTreatment::No;
	Privacy.Source = TEXT("MockConsent");
	Provider.bBlockChildDirectedRequests = true;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	FOpenMobileAdsCanRequestAdsResult Decision = Subsystem->CanRequestAds();
	TestEqual(
		TEXT("Age treatment reaches provider request policy"),
		Decision.BlockReason,
		EOpenMobileAdsCanRequestAdsBlockReason::ProviderPolicy
	);
	TestEqual(
		TEXT("The provider classifies its age policy"),
		Decision.BlockType,
		EOpenMobileAdsCanRequestAdsBlockType::Configuration
	);
	TestFalse(
		TEXT("The privacy snapshot mirrors the full request decision"),
		Subsystem->GetPrivacySnapshot().bCanRequestAds
	);

	Provider.bBlockChildDirectedRequests = false;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	TestTrue(
		TEXT("Supported age treatment allows the request"),
		Subsystem->CanRequestAds().bCanRequestAds
	);
	TestTrue(
		TEXT("The privacy snapshot mirrors an allowed decision"),
		Subsystem->GetPrivacySnapshot().bCanRequestAds
	);

	Privacy.ProviderDetails.bIsAvailable = true;
	Privacy.ProviderDetails.RawStatus = TEXT("UNCHANGED_POLICY");
	Subsystem->UpdatePrivacySnapshot(Privacy);
	TestEqual(
		TEXT("A privacy update reevaluates without duplicate decision events"),
		Events.Num(),
		2
	);

	Privacy.bConsentStatusFresh = false;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	Decision = Subsystem->CanRequestAds();
	TestEqual(
		TEXT("Stale consent blocks with a temporary reason"),
		Decision.BlockReason,
		EOpenMobileAdsCanRequestAdsBlockReason::ConsentStale
	);
	TestEqual(
		TEXT("Stale consent is temporary"),
		Decision.BlockType,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary
	);

	Privacy.bConsentStatusFresh = true;
	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Required;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	Decision = Subsystem->CanRequestAds();
	TestEqual(
		TEXT("Required consent blocks for a user decision"),
		Decision.BlockType,
		EOpenMobileAdsCanRequestAdsBlockType::UserDecision
	);

	Privacy.ConsentStatus = EOpenMobileAdsConsentStatus::Granted;
	Provider.RequestPolicy.State =
		EOpenMobileAdsProviderRequestPolicyState::TemporarilyBlocked;
	Subsystem->UpdatePrivacySnapshot(Privacy);
	Decision = Subsystem->CanRequestAds();
	TestEqual(
		TEXT("Provider request policy is reevaluated"),
		Decision.BlockReason,
		EOpenMobileAdsCanRequestAdsBlockReason::ProviderPolicy
	);
	TestEqual(
		TEXT("Provider temporary policy remains temporary"),
		Decision.BlockType,
		EOpenMobileAdsCanRequestAdsBlockType::Temporary
	);
	const FOpenMobileAdsOperationResult RejectedLoad =
		Subsystem->LoadAd(TEXT("CanRequestReward"));
	TestFalse(TEXT("Provider policy blocks loading"), RejectedLoad.bAccepted);
	TestEqual(
		TEXT("A blocked load returns a privacy error"),
		RejectedLoad.Error.Code,
		EOpenMobileAdsErrorCode::PrivacyBlocked
	);
	TestEqual(TEXT("A blocked load does not reach the provider"), Provider.LoadCalls, 0);
	TestEqual(
		TEXT("Provider policy also blocks showing"),
		Subsystem->CanShow(TEXT("CanRequestReward")).BlockReason,
		EOpenMobileAdsCanShowBlockReason::PrivacyBlocked
	);
	TestEqual(TEXT("Each changed decision broadcasts once"), Events.Num(), 5);
	Subsystem->OnNativeCanRequestAdsChanged().Remove(EventHandle);
	return true;
}

#endif
