#include "Features/IModularFeatures.h"
#include "IOpenMobileMotionActivityProvider.h"
#include "Modules/ModuleManager.h"

class FOpenMobileSensorsActivityProviderFixtureModule final
	: public IModuleInterface
	, public IOpenMobileMotionActivityProvider
{
public:
	virtual void StartupModule() override
	{
		IModularFeatures::Get().RegisterModularFeature(
			GetModularFeatureName(),
			this
		);
	}

	virtual void ShutdownModule() override
	{
		if (IModularFeatures::Get().IsModularFeatureAvailable(
			GetModularFeatureName()))
		{
			IModularFeatures::Get().UnregisterModularFeature(
				GetModularFeatureName(),
				this
			);
		}
	}

	virtual uint32 GetInterfaceVersion() const override
	{
		return InterfaceVersion;
	}

	virtual FName GetProviderName() const override
	{
		return TEXT("ArchitectureFixture");
	}

	virtual FOpenMobileSensorCapability GetCapability() const override
	{
		FOpenMobileSensorCapability Capability;
		Capability.Sensor.Type = EOpenMobileSensorType::MotionActivity;
		Capability.Availability.Name = TEXT("OpenMobile.Sensors.MotionActivity");
		Capability.Availability.State = EOpenMobileCapabilityState::NotSupported;
		return Capability;
	}

	virtual FOpenMobileSensorOperationResult StartStream(
		const FOpenMobileMotionActivityProviderStreamHandle& Handle,
		const FOpenMobileMotionActivityProviderRequest& Request,
		FOpenMobileMotionActivityProviderCallbacks&& Callbacks
	) override
	{
		static_cast<void>(Handle);
		static_cast<void>(Request);
		static_cast<void>(Callbacks);
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::NotSupported;
		return Result;
	}

	virtual FOpenMobileSensorOperationResult ReconfigureStream(
		const FOpenMobileMotionActivityProviderStreamHandle& Handle,
		const FOpenMobileMotionActivityProviderRequest& Request
	) override
	{
		static_cast<void>(Handle);
		static_cast<void>(Request);
		FOpenMobileSensorOperationResult Result;
		Result.Code = EOpenMobileSensorResultCode::NotSupported;
		return Result;
	}

	virtual void StopStream(
		const FOpenMobileMotionActivityProviderStreamHandle& Handle
	) override
	{
		static_cast<void>(Handle);
	}
};

IMPLEMENT_MODULE(
	FOpenMobileSensorsActivityProviderFixtureModule,
	OpenMobileSensorsActivityProviderFixture
);
