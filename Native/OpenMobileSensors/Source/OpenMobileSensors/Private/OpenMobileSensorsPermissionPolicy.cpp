#include "OpenMobileSensorsPermissionPolicy.h"

FName FOpenMobileSensorsPermissionPolicy::MotionActivity()
{
	static const FName Name(TEXT("OpenMobile.Sensors.Permission.MotionActivity"));
	return Name;
}

FName FOpenMobileSensorsPermissionPolicy::ActivityRecognition()
{
	static const FName Name(
		TEXT("OpenMobile.Sensors.Permission.ActivityRecognition")
	);
	return Name;
}

FName FOpenMobileSensorsPermissionPolicy::TrueHeadingLocation()
{
	static const FName Name(
		TEXT("OpenMobile.Sensors.Prerequisite.TrueHeadingLocation")
	);
	return Name;
}

bool FOpenMobileSensorsPermissionPolicy::IsSensorPermission(FName Permission)
{
	return Permission == MotionActivity()
		|| Permission == ActivityRecognition()
		|| Permission == TrueHeadingLocation();
}

FString FOpenMobileSensorsPermissionPolicy::GetExplanation(FName Permission)
{
	if (Permission == MotionActivity())
	{
		return TEXT("Motion activity access is required for activity and pedometer data.");
	}
	if (Permission == ActivityRecognition())
	{
		return TEXT("Activity recognition access is required for steps and activity data.");
	}
	if (Permission == TrueHeadingLocation())
	{
		return TEXT("True heading requires suitable authorized location input.");
	}
	return {};
}
