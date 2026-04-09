#pragma once

#include "Async/Async.h"
#include "CoreMinimal.h"

namespace OpenMobile
{
	/** Executes immediately on the game thread or safely schedules work onto it. */
	template <typename CallbackType>
	void DispatchToGameThread(CallbackType&& Callback)
	{
		if (IsInGameThread())
		{
			Callback();
			return;
		}

		AsyncTask(ENamedThreads::GameThread, Forward<CallbackType>(Callback));
	}
}
