#pragma once

#include "CoreMinimal.h"
#include "IOpenMobileSensorsBackend.h"

struct FOpenMobileSensorsBackendToken;

class FOpenMobileSensorsAndroidBackend final : public IOpenMobileSensorsBackend
{
public:
	virtual ~FOpenMobileSensorsAndroidBackend() override;
	virtual FName GetBackendName() const override;
	virtual FOpenMobileCapability GetBackendCapability() const override;
	virtual bool RequiresHighSamplingRateDeclaration() const override;
	virtual bool HasHighSamplingRateDeclaration() const override;
	virtual void BeginShutdown() override;
	static double ConvertSensorEventTimestampNanoseconds(
		int64 TimestampNanoseconds
	);

	bool PublishVectorBatchFromHandler(
		const FOpenMobileSensorsBackendToken& Token,
		const FOpenMobileSensorBackendStreamHandle& Handle,
		const FOpenMobileVectorSensorBatch& Batch
	);

private:
	bool EnsureSensorHandlerThread();
	void StopSensorHandlerThread();

	FCriticalSection HandlerMutex;
	void* SensorHandlerThread = nullptr;
	void* SensorHandler = nullptr;
	TAtomic<bool> bShuttingDown = false;
};
