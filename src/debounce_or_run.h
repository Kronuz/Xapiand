/*
 * Debouncing a call whose loss would lose data (a commit, a WAL fsync): if the
 * debouncer refuses it while stopping or out of memory, the caller runs it now.
 */

#pragma once

#include <utility>               // for std::forward

#ifndef DEBOUNCE_OR_RUN_PAST_HORIZON
#include <cassert>               // for assert
#include "logger.h"              // for L_ERR
// A delay past the scheduler's 24 h horizon is the caller's error, never a reason
// to run the operation a day early: log it, and stop debug builds.
#define DEBOUNCE_OR_RUN_PAST_HORIZON() do { \
	L_ERR("Debounced operation past the scheduler's 24 h horizon: not run"); \
	assert(false && "debounced past the scheduler's 24 h horizon"); \
} while (0)
#endif


// Debounces `args` under `key` and returns the debouncer's schedule_result. On
// stopping (shutdown) or a second no_memory, runs `run_now(args...)` on this
// thread instead; on past_horizon runs nothing and reports the error.
template <typename Debouncer, typename RunNow, typename Key, typename... Args>
inline auto
debounce_or_run(Debouncer& debouncer, RunNow&& run_now, const Key& key, Args&&... args)
{
	auto result = debouncer.debounce(key, args...);
	using Result = decltype(result);
	if (result == Result::no_memory) {
		result = debouncer.debounce(key, args...);
	}
	if (result == Result::stopping || result == Result::no_memory) {
		std::forward<RunNow>(run_now)(std::forward<Args>(args)...);
	} else if (result == Result::past_horizon) {
		DEBOUNCE_OR_RUN_PAST_HORIZON();
	}
	return result;
}
