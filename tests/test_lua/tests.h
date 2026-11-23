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

#pragma once

// The Lua tests, one function each, run in order by tests/test_lua.cc.

namespace detail {

// results.cc
void test_a_script_transforms_the_document();
void test_params_and_old_doc_are_in_scope();
void test_numbers_keep_their_kind();
void test_a_script_error_is_a_client_error_and_leaves_the_document_as_it_was();
void test_a_syntax_error_names_its_line();
void test_a_result_past_its_byte_budget_is_refused();
void test_a_result_past_its_node_budget_is_refused();
void test_print_is_bounded_per_call();

// environment.cc
void test_interleaved_calls_keep_their_own_doc_and_globals();
void test_the_calls_really_interleaved();
void test_collection_runs_while_calls_are_suspended();
void test_concurrent_calls_of_one_script_each_get_the_whole_byte_cap();
void test_a_call_leaves_no_garbage_in_its_state();
void test_a_call_of_its_own_state_leaves_the_cached_one_as_it_was();
void test_a_cached_script_keeps_no_call_params();
void test_a_call_leaves_its_string_table_as_it_was();
void test_calls_keep_a_threads_cached_scripts_within_the_budget();
void test_overlapping_calls_of_a_large_script_take_turns();
void test_the_after_call_collection_frees_in_slices();
void test_a_threads_cached_scripts_hold_at_most_one_calls_cap();
void test_the_script_cache_default_is_one_and_fits_the_budget();
void test_report_the_cost_of_a_vararg_chain();
void test_report_the_cost_of_a_trivial_call();
void test_writes_do_not_outlive_their_call();
void test_the_environment_is_closed();
void test_a_script_cannot_set_a_finalizer();
void test_a_changed_script_replaces_its_cached_processor();

// bounds.cc
void test_an_infinite_loop_ends_at_the_instruction_cap();
void test_pcall_cannot_swallow_a_bound();
void test_a_runaway_callback_ends_at_the_non_yieldable_bound();
void test_a_runaway_sort_comparator_ends();
void test_table_sort_with_a_comparator_is_a_merge_sort_that_completes();
void test_an_exponential_pattern_ends_at_the_matcher_step_budget();
void test_ordinary_patterns_stay_inside_the_budget();
void test_an_allocation_past_the_cap_ends();
void test_the_state_serves_the_next_call_after_every_bound();
void test_a_document_with_a_cycle_or_a_shared_blowup_is_refused();
void test_conversion_runs_no_script_code();

// bounds_hooks.cc
void test_a_script_cannot_declare_a_to_be_closed_variable();
void test_a_close_handler_cannot_outlive_a_bound();
void test_library_calls_are_charged_by_what_they_touch();
void test_a_slow_search_ends_at_the_work_cap();
void test_the_matcher_is_charged_by_work_not_by_entries();
void test_a_vararg_chain_ends_at_the_work_cap();
void test_one_call_cannot_hold_the_loop_past_the_call_cap();
void test_weak_tables_are_refused_and_never_weak();
void test_the_uncharged_functions_are_retired();
void test_table_sort_without_a_comparator_is_the_merge_sort();
void test_a_charged_loop_yields_before_its_slice_of_instructions_is_spent();
void test_patched_library_functions_still_compute();
void test_plain_find_agrees_with_std_string_find();

// script_names.cc
void test_a_local_script_name_cannot_evict_or_stand_in_for_a_foreign_reference();
void test_a_cached_foreign_script_is_trusted_for_a_second_and_then_read_again();
void test_a_foreign_link_that_leaves_the_data_directory_is_refused();
void test_a_name_only_request_for_a_script_that_is_not_cached_is_refused();
void test_a_schema_stores_the_body_of_a_name_only_script_or_refuses_it();

// slices.cc
void test_a_long_script_lets_other_work_run();
void test_without_a_budget_a_script_runs_to_its_end();
void test_a_cancelled_call_stops_at_its_next_slice_and_frees_its_thread();
void test_a_cancelled_call_leaves_no_garbage();
void test_a_cancel_inside_the_collection_lets_it_finish();

// heavy_lane.cc
void test_a_call_runs_every_slice_in_the_heavy_lane_and_the_request_is_light_after();
void test_a_heavy_request_stays_heavy_after_its_call();
void test_a_script_error_leaves_the_request_light();

// method.cc
void test_a_script_reads_the_method_of_its_write();

}  // namespace detail
