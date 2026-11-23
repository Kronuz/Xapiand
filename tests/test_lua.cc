/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// Lua scripts (phase 7c D10): a call's own environment, its slices, and its bounds. Each bound ends
// the call with a ScriptLimitError (a 400), and the state serves the next call.

#include "test_harness.h"

#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <signal.h>
#include <unistd.h>

#include "test_lua/tests.h"

namespace {

// A bound that fails to end its call would hang the test: stop it instead.
[[maybe_unused]] const auto watchdog = [] {
	std::signal(SIGALRM, [](int) { std::fprintf(stderr, "HANG: a script ran past the watchdog\n"); ::_exit(3); });
	return ::alarm(600);
}();

}  // namespace

using namespace detail;

TEST_MAIN(
	test_a_name_only_request_for_a_script_that_is_not_cached_is_refused,
	test_a_schema_stores_the_body_of_a_name_only_script_or_refuses_it,
	test_a_script_transforms_the_document,
	test_params_and_old_doc_are_in_scope,
	test_numbers_keep_their_kind,
	test_a_script_error_is_a_client_error_and_leaves_the_document_as_it_was,
	test_a_syntax_error_names_its_line,
	test_interleaved_calls_keep_their_own_doc_and_globals,
	test_the_calls_really_interleaved,
	test_collection_runs_while_calls_are_suspended,
	test_concurrent_calls_of_one_script_each_get_the_whole_byte_cap,
	test_a_call_leaves_no_garbage_in_its_state,
	test_a_call_of_its_own_state_leaves_the_cached_one_as_it_was,
	test_a_cached_script_keeps_no_call_params,
	test_a_call_leaves_its_string_table_as_it_was,
	test_calls_keep_a_threads_cached_scripts_within_the_budget,
	test_overlapping_calls_of_a_large_script_take_turns,
	test_the_after_call_collection_frees_in_slices,
	test_a_threads_cached_scripts_hold_at_most_one_calls_cap,
	test_the_script_cache_default_is_one_and_fits_the_budget,
	test_report_the_cost_of_a_vararg_chain,
	test_report_the_cost_of_a_trivial_call,
	test_writes_do_not_outlive_their_call,
	test_the_environment_is_closed,
	test_a_script_cannot_set_a_finalizer,
	test_a_script_cannot_declare_a_to_be_closed_variable,
	test_a_changed_script_replaces_its_cached_processor,
	test_an_infinite_loop_ends_at_the_instruction_cap,
	test_pcall_cannot_swallow_a_bound,
	test_a_runaway_callback_ends_at_the_non_yieldable_bound,
	test_a_runaway_sort_comparator_ends,
	test_table_sort_with_a_comparator_is_a_merge_sort_that_completes,
	test_an_exponential_pattern_ends_at_the_matcher_step_budget,
	test_ordinary_patterns_stay_inside_the_budget,
	test_an_allocation_past_the_cap_ends,
	test_the_state_serves_the_next_call_after_every_bound,
	test_a_document_with_a_cycle_or_a_shared_blowup_is_refused,
	test_conversion_runs_no_script_code,
	test_a_close_handler_cannot_outlive_a_bound,
	test_library_calls_are_charged_by_what_they_touch,
	test_a_slow_search_ends_at_the_work_cap,
	test_the_matcher_is_charged_by_work_not_by_entries,
	test_a_vararg_chain_ends_at_the_work_cap,
	test_weak_tables_are_refused_and_never_weak,
	test_the_uncharged_functions_are_retired,
	test_table_sort_without_a_comparator_is_the_merge_sort,
	test_patched_library_functions_still_compute,
	test_plain_find_agrees_with_std_string_find,
	test_a_result_past_its_byte_budget_is_refused,
	test_a_result_past_its_node_budget_is_refused,
	test_a_local_script_name_cannot_evict_or_stand_in_for_a_foreign_reference,
	test_print_is_bounded_per_call,
	test_a_cached_foreign_script_is_trusted_for_a_second_and_then_read_again,
	test_one_call_cannot_hold_the_loop_past_the_call_cap,
	test_a_long_script_lets_other_work_run,
	test_without_a_budget_a_script_runs_to_its_end,
	test_a_cancelled_call_stops_at_its_next_slice_and_frees_its_thread,
	test_a_cancelled_call_leaves_no_garbage,
	test_a_cancel_inside_the_collection_lets_it_finish,
	test_a_charged_loop_yields_before_its_slice_of_instructions_is_spent,
	test_a_foreign_link_that_leaves_the_data_directory_is_refused,
	test_a_call_runs_every_slice_in_the_heavy_lane_and_the_request_is_light_after,
	test_a_heavy_request_stays_heavy_after_its_call,
	test_a_script_error_leaves_the_request_light,
	test_a_script_reads_the_method_of_its_write   // last: it starts a node, which sets the options
)
