#include "OpenMobileSensorPermissions.h"

#include "OpenMobileSensorsPermissionPolicy.h"

FName FOpenMobileSensorPermissions::GetPermissionName(
	EOpenMobileSensorPermission Permission
)
{
	switch (Permission)
	{
	case EOpenMobileSensorPermission::MotionActivity:
		return FOpenMobileSensorsPermissionPolicy::MotionActivity();
	case EOpenMobileSensorPermission::ActivityRecognition:
		return FOpenMobileSensorsPermissionPolicy::ActivityRecognition();
	case EOpenMobileSensorPermission::TrueHeadingLocation:
		return FOpenMobileSensorsPermissionPolicy::TrueHeadingLocation();
	}
	return NAME_None;
}

FString FOpenMobileSensorPermissions::GetExplanation(
	EOpenMobileSensorPermission Permission
)
{
	return FOpenMobileSensorsPermissionPolicy::GetExplanation(
		GetPermissionName(Permission)
	);
}

FOpenMobileSensorPermissionDescriptor FOpenMobileSensorPermissions::Describe(
	EOpenMobileSensorPermission Permission,
	EOpenMobilePermissionStatus Status
)
{
	FOpenMobileSensorPermissionDescriptor Descriptor;
	Descriptor.Permission = Permission;
	Descriptor.PermissionName = GetPermissionName(Permission);
	Descriptor.Explanation = GetExplanation(Permission);
	Descriptor.Status = Status;
	return Descriptor;
}
