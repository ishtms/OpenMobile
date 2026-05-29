#pragma once

#include "OpenMobileDeviceOrientationControl.h"

class FOpenMobileDeviceOrientationControlPolicy final
{
public:
	static bool Validate(
		const FOpenMobileOrientationPolicyRequest& Request,
		FOpenMobileError& OutError
	);
};

class FOpenMobileDeviceOrientationRequestStack final
{
public:
	void Add(
		uint64 RequestId,
		const FOpenMobileOrientationPolicyRequest& Request
	);
	void Remove(uint64 RequestId);
	TOptional<FOpenMobileOrientationPolicyRequest> GetEffectiveRequest() const;
	bool IsEmpty() const { return Requests.IsEmpty(); }
	void Reset() { Requests.Reset(); }

private:
	struct FEntry
	{
		uint64 RequestId = 0;
		FOpenMobileOrientationPolicyRequest Request;
	};

	TArray<FEntry> Requests;
};
