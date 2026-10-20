/*
 * Debouncing a call whose loss would lose data (a commit, a WAL fsync): if the
 * debouncer refuses it, the caller runs it now instead.
 */

#pragma once

#include <utility>               // for std::forward

#include "scheduler.h"           // for schedule_result


// Debounces `args` under `key`. On stopping (shutdown), past_horizon or a
// second no_memory, runs `run_now(args...)` on this thread instead.
template <typename Debouncer, typename RunNow, typename Key, typename... Args>
inline void
debounce_or_run(Debouncer& debouncer, RunNow&& run_now, const Key& key, Args&&... args)
{
	auto result = debouncer.debounce(key, args...);
	if (result == schedule_result::no_memory) {
		result = debouncer.debounce(key, args...);
	}
	if (result != schedule_result::scheduled) {
		std::forward<RunNow>(run_now)(std::forward<Args>(args)...);
	}
}
