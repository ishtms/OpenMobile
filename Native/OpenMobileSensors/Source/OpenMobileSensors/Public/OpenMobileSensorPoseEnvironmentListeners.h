#pragma once

#include "CoreMinimal.h"
#include "OpenMobileSensorListener.h"
#include "OpenMobileSensorPoseEnvironmentListeners.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOpenMobileAttitudeSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Rotation") FQuat, Rotation,
	bool, bHasEulerRotation,
	UPARAM(DisplayName = "Euler Rotation (degrees)") FRotator, EulerRotationDegrees,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileAttitudeListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Sample", ToolTip = "Broadcast device attitude as a normalized rotation with optional Euler angles."))
	FOpenMobileAttitudeSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Attitude", Keywords = "OpenMobile sensors attitude pose rotation orientation quaternion", ToolTip = "Starts an owner-scoped attitude listener with automatic cleanup."))
	static UOpenMobileAttitudeListener* ListenForAttitude(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::Game,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Get Latest Attitude Sample", ToolTip = "Copies one coherent snapshot of the last attitude sample delivered to this listener."))
	bool GetLatestAttitude(FQuat& OutRotation, bool& bOutHasEulerRotation,
		FRotator& OutEulerRotationDegrees,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleAttitudeSample(const FOpenMobileAttitudeSensorSample& InSample) override;

private:
	FOpenMobileAttitudeSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileMagneticHeadingSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Heading (degrees)") double, HeadingDegrees,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileMagneticHeadingListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Sample", ToolTip = "Broadcast clockwise magnetic heading in degrees from north."))
	FOpenMobileMagneticHeadingSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Magnetic Heading", Keywords = "OpenMobile sensors magnetic heading compass north bearing", ToolTip = "Starts an owner-scoped magnetic-heading listener with automatic cleanup."))
	static UOpenMobileMagneticHeadingListener* ListenForMagneticHeading(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Get Latest Magnetic Heading", ToolTip = "Copies one coherent snapshot of the last magnetic-heading sample."))
	bool GetLatestHeading(double& OutHeadingDegrees,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleHeadingSample(const FOpenMobileHeadingSensorSample& InSample) override;

private:
	FOpenMobileHeadingSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileTrueHeadingSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Heading (degrees)") double, HeadingDegrees,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileTrueHeadingListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Sample", ToolTip = "Broadcast clockwise true heading in degrees from geographic north."))
	FOpenMobileTrueHeadingSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for True Heading", Keywords = "OpenMobile sensors true heading compass geographic north bearing location", ToolTip = "Starts an owner-scoped true-heading listener. Supply location through the owning location provider when required."))
	static UOpenMobileTrueHeadingListener* ListenForTrueHeading(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		EOpenMobileSensorCoordinateSpace CoordinateSpace = EOpenMobileSensorCoordinateSpace::DeviceFixed,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Get Latest True Heading", ToolTip = "Copies one coherent snapshot of the last true-heading sample."))
	bool GetLatestHeading(double& OutHeadingDegrees,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleHeadingSample(const FOpenMobileHeadingSensorSample& InSample) override;

private:
	FOpenMobileHeadingSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobilePressureSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Pressure (hPa)") double, PressureHectopascals,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobilePressureListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Sample", ToolTip = "Broadcast atmospheric pressure in hectopascals."))
	FOpenMobilePressureSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Pressure", Keywords = "OpenMobile sensors pressure barometer atmospheric hectopascal", ToolTip = "Starts an owner-scoped pressure listener with automatic cleanup."))
	static UOpenMobilePressureListener* ListenForPressure(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Get Latest Pressure Sample", ToolTip = "Copies one coherent snapshot of the last pressure sample."))
	bool GetLatestPressure(double& OutPressureHectopascals,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleScalarSample(const FOpenMobileScalarSensorSample& InSample) override;

private:
	FOpenMobileScalarSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileRelativeAltitudeSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Altitude (m)") double, AltitudeMetres,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileRelativeAltitudeListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Sample", ToolTip = "Broadcast altitude change from the session baseline in metres."))
	FOpenMobileRelativeAltitudeSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Relative Altitude", Keywords = "OpenMobile sensors relative altitude elevation height stairs barometer", ToolTip = "Starts an owner-scoped relative-altitude listener with automatic cleanup."))
	static UOpenMobileRelativeAltitudeListener* ListenForRelativeAltitude(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Get Latest Relative Altitude", ToolTip = "Copies one coherent snapshot of the last relative-altitude sample."))
	bool GetLatestAltitude(double& OutAltitudeMetres,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleScalarSample(const FOpenMobileScalarSensorSample& InSample) override;

private:
	FOpenMobileScalarSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileAbsoluteAltitudeSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Altitude (m)") double, AltitudeMetres,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileAbsoluteAltitudeListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Sample", ToolTip = "Broadcast absolute altitude in metres when the platform provides it."))
	FOpenMobileAbsoluteAltitudeSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Absolute Altitude", Keywords = "OpenMobile sensors absolute altitude elevation height metres", ToolTip = "Starts an owner-scoped absolute-altitude listener with automatic cleanup."))
	static UOpenMobileAbsoluteAltitudeListener* ListenForAbsoluteAltitude(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Get Latest Absolute Altitude", ToolTip = "Copies one coherent snapshot of the last absolute-altitude sample."))
	bool GetLatestAltitude(double& OutAltitudeMetres,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleScalarSample(const FOpenMobileScalarSensorSample& InSample) override;

private:
	FOpenMobileScalarSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(
	FOpenMobileAmbientLightSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Illuminance (lux)") double, IlluminanceLux,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileAmbientLightListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Sample", ToolTip = "Broadcast ambient illuminance in lux."))
	FOpenMobileAmbientLightSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Ambient Light", Keywords = "OpenMobile sensors ambient light illuminance lux brightness", ToolTip = "Starts an owner-scoped ambient-light listener with automatic cleanup."))
	static UOpenMobileAmbientLightListener* ListenForAmbientLight(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Get Latest Ambient Light", ToolTip = "Copies one coherent snapshot of the last ambient-light sample."))
	bool GetLatestIlluminance(double& OutIlluminanceLux,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleScalarSample(const FOpenMobileScalarSensorSample& InSample) override;

private:
	FOpenMobileScalarSensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FiveParams(
	FOpenMobileProximitySampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Is Near") bool, bIsNear,
	bool, bHasDistance,
	UPARAM(DisplayName = "Distance (m)") double, DistanceMetres,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobileProximityListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Sample", ToolTip = "Broadcast near state and optional measured distance."))
	FOpenMobileProximitySampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Proximity", Keywords = "OpenMobile sensors proximity near distance face pocket", ToolTip = "Starts an owner-scoped proximity listener with automatic cleanup."))
	static UOpenMobileProximityListener* ListenForProximity(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Environment", meta = (DisplayName = "Get Latest Proximity Sample", ToolTip = "Copies one coherent snapshot of the last proximity sample."))
	bool GetLatestProximity(bool& bOutIsNear, bool& bOutHasDistance,
		double& OutDistanceMetres,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleProximitySample(const FOpenMobileProximitySensorSample& InSample) override;

private:
	FOpenMobileProximitySensorSample LatestSample;
	bool bHasSample = false;
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE_FourParams(
	FOpenMobilePhysicalOrientationSampleDynamic,
	UOpenMobileSensorListener*, Listener,
	UPARAM(DisplayName = "Orientation") EOpenMobilePhysicalOrientation, Orientation,
	UPARAM(DisplayName = "Confidence") double, Confidence,
	FOpenMobileSensorSampleInfo, SampleInfo
);

UCLASS(meta = (ExposedAsyncProxy = "Listener"))
class OPENMOBILESENSORS_API UOpenMobilePhysicalOrientationListener final
	: public UOpenMobileSensorListener
{
	GENERATED_BODY()

public:
	UPROPERTY(BlueprintAssignable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Sample", ToolTip = "Broadcast the device's classified physical orientation and confidence."))
	FOpenMobilePhysicalOrientationSampleDynamic Sample;

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DefaultToSelf = "ListenerOwner", AutoCreateRefTerm = "AdvancedOptions", AdvancedDisplay = "AdvancedOptions,bUseAdvancedOptions,ListenerOwner", DisplayName = "Listen for Physical Orientation", Keywords = "OpenMobile sensors physical orientation portrait landscape face up face down", ToolTip = "Starts an owner-scoped physical-orientation listener with automatic cleanup."))
	static UOpenMobilePhysicalOrientationListener* ListenForPhysicalOrientation(
		const UObject* WorldContextObject,
		const FOpenMobileSensorStreamOptions& AdvancedOptions,
		EOpenMobileSensorRatePreset RatePreset = EOpenMobileSensorRatePreset::UI,
		bool bUseAdvancedOptions = false,
		UObject* ListenerOwner = nullptr
	);

	UFUNCTION(BlueprintCallable, Category = "OpenMobile|Sensors|Pose and Heading", meta = (DisplayName = "Get Latest Physical Orientation", ToolTip = "Copies one coherent snapshot of the last physical-orientation sample."))
	bool GetLatestOrientation(EOpenMobilePhysicalOrientation& OutOrientation,
		double& OutConfidence,
		FOpenMobileSensorSampleInfo& OutSampleInfo) const;

protected:
	virtual void HandleOrientationSample(const FOpenMobileOrientationSensorSample& InSample) override;

private:
	FOpenMobileOrientationSensorSample LatestSample;
	bool bHasSample = false;
};
