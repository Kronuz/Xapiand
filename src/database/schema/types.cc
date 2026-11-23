/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
 * Copyright (c) 2015-2019 Dubalu LLC
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

// A field's type from its first value: complete_specification, validate_required_data,
// validate_required_namespace_data and guess_concrete_type.

#include "database/schema.h"

#include <algorithm>                              // for std::move
#include <cassert>                                // for assert
#include <cmath>                                  // for std::pow
#include <cstdint>                                // for uint64_t
#include <cstring>                                // for size_t, strlen
#include <cctype>                                 // for tolower
#include <functional>                             // for ref, reference_wrapper
#include <limits>                                 // for std::numeric_limits
#include <mutex>                                  // for std::mutex
#include <ostream>                                // for operator<<, basic_ostream
#include <memory>                                 // for std::weak_ptr
#include <set>                                    // for std::set
#include <stdexcept>                              // for std::out_of_range
#include <type_traits>                            // for remove_reference<>::type
#include <unordered_map>                          // for std::unordered_map
#include <utility>

#include "base_x.hh"                              // for Base64
#include "cast.h"                                 // for Cast
#include "cuuid.hh"                                // for UUIDGenerator, cuuid::generate/serialize
#include "database/handler.h"                     // for DatabaseHandler
#include "database/lock.h"                        // for lock_shard
#include "database/shard.h"                       // for Shard
#include "datetime.h"                             // for isDate, isDatetime, tm_t
#include "datetime_msgpack.h"          // for the Datetime MsgPack overloads
#include "exception_engine.h"                     // for ClientError, SerializationError
#include "geospatial/geospatial.h"                // for GeoSpatial
#include "manager.h"                              // for XapiandManager, XapiandMan...
#include "multivalue/generate_terms.h"            // for integer, geo, datetime, positive
#include "opts.h"                                 // for opts::*
#include "random.hh"                              // for random_int
#include "reserved/schema.h"                      // for RESERVED_
#include "script.h"                               // for Script
#include "serialize_list.h"                       // for StringList
#include "split.h"                                // for Split
#include "stopper.h"                              // for stopWordsFingerprint, checkStopWords
#include "static_string.hh"                       // for static_string
#include "xapiand_strings.hh"                             // for strings::format, strings::inplace_lower
#include "database/schema/reserved.h"
#include "database/schema/walk.h"


using namespace schema_detail;


/*  _____ _____ _____ _____ _____ _____ _____ _____
 * |_____|_____|_____|_____|_____|_____|_____|_____|
 */

void
Schema::complete_specification(const MsgPack& item_value)
{
	L_CALL("Schema::complete_specification({})", item_value.to_string());

	if (!specification.flags.concrete) {
		if (!specification.endpoint.empty()) {
			if (specification.flags.strict) {
				THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
			}
			specification.sep_types[SPC_FOREIGN_TYPE] = FieldType::foreign;
		}
		if (specification.sep_types[SPC_FOREIGN_TYPE] != FieldType::foreign) {
			if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
				if (specification.flags.strict) {
					THROW(MissingTypeError, "Type of field {} is missing", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
				specification.sep_types[SPC_CONCRETE_TYPE] = guess_concrete_type(item_value);
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					THROW(MissingTypeError, "Type of field {} cannot be guessed", specification.full_meta_name.empty() ? "<root>" : repr(specification.full_meta_name));
				}
			}

			if (specification.flags.inside_namespace) {
				validate_required_namespace_data();
			} else {
				validate_required_data(get_mutable_properties(specification.full_meta_name));
			}
		}
	}

	auto paths = get_partial_paths();
	specification.partial_index_spcs.reserve(paths.size());

	for (auto& path : paths) {
		index_spc_t spc(
			specification.sep_types[SPC_CONCRETE_TYPE],
			std::move(path.first),
			path.second ? get_slot(path.first, specification.get_ctype()) : specification.slot,
			specification.accuracy,
			specification.acc_prefix);

		for (auto& acc_prefix : spc.acc_prefix) {
			acc_prefix.insert(0, acc_term_prefix(spc.prefix));
		}

		specification.partial_index_spcs.push_back(std::move(spc));
	}

	specification.flags.complete = true;
}


void
Schema::validate_required_namespace_data()
{
	L_CALL("Schema::validate_required_namespace_data() {{type:{}}}", _get_str_type(specification.sep_types));

	// This should be the exact same as validate_required_data(),
	// except this doesn't write to the mut_properties.

	std::set<uint64_t> set_acc;

	auto type = specification.sep_types[SPC_CONCRETE_TYPE];
	switch (type) {
		case FieldType::object:
			specification.flags.concrete = true;
			break;

		case FieldType::geo:
			// Set partials and error.
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_number()) {
								const auto val_acc = _accuracy.u64();
								if (val_acc <= HTM_MAX_LEVEL) {
									set_acc.insert(val_acc);
								} else {
									THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {} ({} not supported)", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL, val_acc);
								}
							} else {
								THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
					}
				} else {
					set_acc.insert(def_accuracy_geo.begin(), def_accuracy_geo.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::date:
		case FieldType::datetime:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							uint64_t accuracy;
							if (_accuracy.is_string()) {
								auto accuracy_date = _get_accuracy_datetime(_accuracy.str_view());
								if (accuracy_date != UnitTime::INVALID) {
									accuracy = toUType(accuracy_date);
								} else {
									THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date), repr(_accuracy.str_view()));
								}
							} else if (_accuracy.is_number()) {
								accuracy = _accuracy.u64();
								if (!validate_acc_date(static_cast<UnitTime>(accuracy))) {
									THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date));
								}
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date), repr(_accuracy.str_view()));
							}
							set_acc.insert(accuracy);
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date));
					}
				} else {
					set_acc.insert(def_accuracy_datetime.begin(), def_accuracy_datetime.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::time:
		case FieldType::timedelta:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_string()) {
								auto accuracy_time = _get_accuracy_time(_accuracy.str_view());
								if (accuracy_time != UnitTime::INVALID) {
									set_acc.insert(toUType(accuracy_time));
								} else {
									THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time), repr(_accuracy.str_view()));
								}
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time), repr(_accuracy.str_view()));
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time));
					}
				} else {
					set_acc.insert(def_accuracy_time.begin(), def_accuracy_time.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::integer:
		case FieldType::positive:
		case FieldType::floating:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_number()) {
								set_acc.insert(_accuracy.u64());
							} else {
								THROW(ClientError, "Data inconsistency, '{}' in '{}' must be an array of positive numbers", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be an array of positive numbers", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
					}
				} else {
					set_acc.insert(def_accuracy_num.begin(), def_accuracy_num.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::text:
			// Language could be needed, for soundex.
			if (specification.aux_language.empty() && !specification.aux_stem_language.empty()) {
				specification.language = specification.aux_stem_language;
			}
			if (specification.aux_stem_language.empty() && !specification.aux_language.empty()) {
				specification.stem_language = specification.aux_language;
			}

			specification.flags.concrete = true;
			break;

		case FieldType::keyword:
			// Process RESERVED_BOOL_TERM
			if (!specification.flags.has_bool_term) {
				// By default, if normalized name has upper characters then it is consider bool term.
				specification.flags.bool_term = strings::hasupper(specification.meta_name);
				specification.flags.has_bool_term = true;
			}

			specification.flags.concrete = true;
			break;

		case FieldType::script:
			if (!specification.flags.has_index) {
				specification.index = TypeIndex::NONE;  // Fallback to index anything.
				specification.flags.has_index = true;
			}
			specification.flags.concrete = true;
			break;

		case FieldType::boolean:
		case FieldType::uuid:
			specification.flags.concrete = true;
			break;

		case FieldType::empty:
			specification.flags.concrete = false;
			break;

		default:
			THROW(ClientError, "{}: '{}' is not supported", RESERVED_TYPE, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
	}

	if (specification.flags.concrete) {
		if (toUType(specification.index & TypeIndex::VALUES) != 0u) {
			// Write RESERVED_SLOT in properties (if it has values).
			if (!specification.flags.is_namespace) {
				if (specification.slot == engine::BAD_VALUENO) {
					specification.slot = get_slot(specification.prefix.field, specification.get_ctype());
				}
			}
		}
		if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
			// Write RESERVED_ACCURACY and RESERVED_ACC_PREFIX in properties.
			if (!set_acc.empty()) {
				specification.acc_prefix.clear();
				for (const auto& acc : set_acc) {
					specification.acc_prefix.push_back(get_prefix(acc));
				}
				specification.accuracy.assign(set_acc.begin(), set_acc.end());
			}
		}
	}
}


void
Schema::validate_required_data(MsgPack& mut_properties)
{
	L_CALL("Schema::validate_required_data({}) {{type:{}}}", mut_properties.to_string(), _get_str_type(specification.sep_types));

	dispatch_set_default_spc(mut_properties);

	std::set<uint64_t> set_acc;

	auto type = specification.sep_types[SPC_CONCRETE_TYPE];
	switch (type) {
		case FieldType::object:
			specification.flags.concrete = true;
			break;

		case FieldType::geo:
			// Set partials and error.
			mut_properties[RESERVED_PARTIALS] = static_cast<bool>(specification.flags.partials);
			mut_properties[RESERVED_ERROR] = specification.error;
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_number()) {
								const auto val_acc = _accuracy.u64();
								if (val_acc <= HTM_MAX_LEVEL) {
									set_acc.insert(val_acc);
								} else {
									THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {} ({} not supported)", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL, val_acc);
								}
							} else {
								THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, level value in '{}': '{}' must be a positive number between 0 and {}", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
					}
				} else {
					set_acc.insert(def_accuracy_geo.begin(), def_accuracy_geo.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::date:
		case FieldType::datetime:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							uint64_t accuracy;
							if (_accuracy.is_string()) {
								auto accuracy_date = _get_accuracy_datetime(_accuracy.str_view());
								if (accuracy_date != UnitTime::INVALID) {
									accuracy = toUType(accuracy_date);
								} else {
									THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date), repr(_accuracy.str_view()));
								}
							} else if (_accuracy.is_number()) {
								accuracy = _accuracy.u64();
								if (!validate_acc_date(static_cast<UnitTime>(accuracy))) {
									THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date));
								}
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date), repr(_accuracy.str_view()));
							}
							set_acc.insert(accuracy);
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, type == FieldType::datetime ? DATETIME_STR : DATE_STR, repr(str_set_acc_date));
					}
				} else {
					set_acc.insert(def_accuracy_datetime.begin(), def_accuracy_datetime.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::time:
		case FieldType::timedelta:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_string()) {
								auto accuracy_time = _get_accuracy_time(_accuracy.str_view());
								if (accuracy_time != UnitTime::INVALID) {
									set_acc.insert(toUType(accuracy_time));
								} else {
									THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time), repr(_accuracy.str_view()));
								}
							} else {
								THROW(ClientError, "Data inconsistency, '{}': '{}' must be a subset of {} ({} not supported)", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time), repr(_accuracy.str_view()));
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be a subset of {}", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), repr(str_set_acc_time));
					}
				} else {
					set_acc.insert(def_accuracy_time.begin(), def_accuracy_time.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::integer:
		case FieldType::positive:
		case FieldType::floating:
			if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
				if (specification.doc_acc) {
					if (specification.doc_acc->is_array()) {
						for (const auto& _accuracy : *specification.doc_acc) {
							if (_accuracy.is_number()) {
								set_acc.insert(_accuracy.u64());
							} else {
								THROW(ClientError, "Data inconsistency, '{}' in '{}' must be an array of positive numbers", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
							}
						}
					} else {
						THROW(ClientError, "Data inconsistency, '{}' in '{}' must be an array of positive numbers", RESERVED_ACCURACY, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
					}
				} else {
					set_acc.insert(def_accuracy_num.begin(), def_accuracy_num.end());
				}
			}
			specification.flags.concrete = true;
			break;

		case FieldType::text:
			mut_properties[RESERVED_NGRAM] = static_cast<bool>(specification.flags.ngram);
			mut_properties[RESERVED_CJK_NGRAM] = static_cast<bool>(specification.flags.cjk_ngram);
			mut_properties[RESERVED_CJK_WORDS] = static_cast<bool>(specification.flags.cjk_words);

			// Language could be needed, for soundex.
			if (specification.aux_language.empty() && !specification.aux_stem_language.empty()) {
				specification.language = specification.aux_stem_language;
			}
			if (!specification.language.empty()) {
				mut_properties[RESERVED_LANGUAGE] = specification.language;
				try {
					mut_properties[RESERVED_STOP_FINGERPRINT] = stopWordsFingerprint(specification.language);
				} catch (const UnknownStopWordsError& exc) {
					THROW(ClientError, "{}: {}", repr(RESERVED_LANGUAGE), exc.what());
				}
				mut_properties[RESERVED_STOP_STRATEGY] = enum_name(specification.stop_strategy);
			}
			if (specification.aux_stem_language.empty() && !specification.aux_language.empty()) {
				specification.stem_language = specification.aux_language;
			}
			if (!specification.stem_language.empty()) {
				mut_properties[RESERVED_STEM_LANGUAGE] = specification.stem_language;
				mut_properties[RESERVED_STEM_STRATEGY] = enum_name(specification.stem_strategy);
			}

			specification.flags.concrete = true;
			break;

		case FieldType::keyword:
			// Process RESERVED_BOOL_TERM
			if (!specification.flags.has_bool_term) {
				// By default, if normalized name has upper characters then it is consider bool term.
				const auto bool_term = strings::hasupper(specification.meta_name);
				if (specification.flags.bool_term != bool_term) {
					specification.flags.bool_term = bool_term;
					mut_properties[RESERVED_BOOL_TERM] = static_cast<bool>(specification.flags.bool_term);
				}
				specification.flags.has_bool_term = true;
			}

			specification.flags.concrete = true;
			break;

		case FieldType::script:
			if (!specification.flags.has_index) {
				const auto index = TypeIndex::NONE; // Fallback to index anything.
				if (specification.index != index) {
					specification.index = index;
					mut_properties[RESERVED_INDEX] = _get_str_index(index);
				}
				specification.flags.has_index = true;
			}
			specification.flags.concrete = true;
			break;

		case FieldType::boolean:
		case FieldType::uuid:
			specification.flags.concrete = true;
			break;

		case FieldType::empty:
			specification.flags.concrete = false;
			break;

		default:
			THROW(ClientError, "{}: '{}' is not supported", RESERVED_TYPE, enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
	}

	if (specification.flags.concrete) {
		if (toUType(specification.index & TypeIndex::VALUES) != 0u) {
			// Write RESERVED_SLOT in properties (if it has values).
			if (!specification.flags.is_namespace) {
				if (specification.slot == engine::BAD_VALUENO) {
					specification.slot = get_slot(specification.prefix.field, specification.get_ctype());
				}
				mut_properties[RESERVED_SLOT] = specification.slot;
			}
		}
		if (toUType(specification.index & TypeIndex::TERMS) != 0u) {
			// Write RESERVED_ACCURACY and RESERVED_ACC_PREFIX in properties.
			if (!set_acc.empty()) {
				specification.acc_prefix.clear();
				for (const auto& acc : set_acc) {
					specification.acc_prefix.push_back(get_prefix(acc));
				}
				specification.accuracy.assign(set_acc.begin(), set_acc.end());
				switch (specification.sep_types[SPC_CONCRETE_TYPE]) {
					case FieldType::date:
					case FieldType::datetime:
					case FieldType::time:
					case FieldType::timedelta:
						mut_properties[RESERVED_ACCURACY] = MsgPack::ARRAY();
						for (auto& acc : specification.accuracy) {
							mut_properties[RESERVED_ACCURACY].push_back(_get_str_acc_date(static_cast<UnitTime>(acc)));
						}
						break;
					default:
						mut_properties[RESERVED_ACCURACY] = specification.accuracy;
						break;
				}
				mut_properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
			}
		}
	}

	// Process RESERVED_TYPE
	mut_properties[RESERVED_TYPE] = _get_str_type(specification.sep_types);

	// L_DEBUG("\nspecification = {}\nmut_properties = {}", specification.to_string(4), mut_properties.to_string(true));
}


// Whether the integer `value` is in the range of the integer type `type`, whatever its encoding.
static bool
fits_integer_type(FieldType type, const MsgPack& value)
{
	switch (value.get_type()) {
		case MsgPack::Type::NEGATIVE_INTEGER:
			return type == FieldType::integer || (type == FieldType::positive && value.i64() >= 0);
		case MsgPack::Type::POSITIVE_INTEGER:
			return type == FieldType::positive || (type == FieldType::integer && value.u64() <= static_cast<uint64_t>(std::numeric_limits<int64_t>::max()));
		default:
			return false;
	}
}


FieldType
Schema::guess_concrete_type(const MsgPack& item_doc)
{
	L_CALL("Schema::guess_concrete_type({}) {{complete:{}, concrete:{}}}", item_doc.to_string(), !!specification.flags.complete, !!specification.flags.concrete);

	if (specification.flags.complete || specification.flags.concrete) {
		return specification.sep_types[SPC_CONCRETE_TYPE];
	}

	switch (item_doc.get_type()) {
		case MsgPack::Type::POSITIVE_INTEGER:
		case MsgPack::Type::NEGATIVE_INTEGER:
			if (specification.flags.numeric_detection) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return Serialize::guess_type(item_doc);
				} else if (!fits_integer_type(specification.sep_types[SPC_CONCRETE_TYPE], item_doc)) {
					THROW(ClientError, "Type mismatch '{}' -> '{}'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), enum_name(Serialize::guess_type(item_doc)));
				}
				return specification.sep_types[SPC_CONCRETE_TYPE];
			}
			break;

		case MsgPack::Type::FLOAT:
			if (specification.flags.numeric_detection) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::floating;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::floating) {
					THROW(ClientError, "Type mismatch '{}' -> 'floating'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::floating;
			}
			break;

		case MsgPack::Type::BOOLEAN:
			if (specification.flags.bool_detection) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::boolean;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::boolean) {
					THROW(ClientError, "Type mismatch '{}' -> 'boolean'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::boolean;
			}
			break;

		case MsgPack::Type::STR: {
			const auto str_value = item_doc.str_view();
			if (specification.flags.uuid_detection && Serialize::isUUID(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::uuid;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::uuid) {
					THROW(ClientError, "Type mismatch '{}' -> 'uuid'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::uuid;
			}
			if (specification.flags.date_detection && Datetime::isDate(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::date;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::date) {
					THROW(ClientError, "Type mismatch '{}' -> 'date'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::date;
			}
			if (specification.flags.datetime_detection && Datetime::isDatetime(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::datetime;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::datetime) {
					THROW(ClientError, "Type mismatch '{}' -> 'datetime'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::datetime;
			}
			if (specification.flags.time_detection && Datetime::isTime(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::time;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::time) {
					THROW(ClientError, "Type mismatch '{}' -> 'time'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::time;
			}
			if (specification.flags.timedelta_detection && Datetime::isTimedelta(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::timedelta;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::timedelta) {
					THROW(ClientError, "Type mismatch '{}' -> 'timedelta'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::timedelta;
			}
			if (specification.flags.geo_detection && EWKT::isEWKT(str_value)) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::geo;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::geo) {
					THROW(ClientError, "Type mismatch '{}' -> 'geo'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::geo;
			}
			if (specification.flags.bool_detection) {
				if (str_value == "true" || str_value == "false") {
					if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
						return FieldType::boolean;
					} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::boolean) {
						THROW(ClientError, "Type mismatch '{}' -> 'boolean'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
					}
					return FieldType::boolean;
				}
			}
			if (specification.flags.text_detection && !specification.flags.bool_term) {
				if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
					return FieldType::text;
				} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::text) {
					THROW(ClientError, "Type mismatch '{}' -> 'text'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
				}
				return FieldType::text;
			}
			if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
				return FieldType::keyword;
			} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::keyword) {
				THROW(ClientError, "Type mismatch '{}' -> 'keyword'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
			}
			return FieldType::keyword;
		}

		case MsgPack::Type::MAP:
			for (auto it = item_doc.begin(); it != item_doc.end(); ++it) {
				auto str_key = it->str_view();
				if (is_reserved(str_key)) {
					auto field_type = Cast::get_field_type(str_key);
					if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
						return field_type;
					} else if (specification.sep_types[SPC_CONCRETE_TYPE] != field_type) {
						THROW(ClientError, "Type mismatch '{}' -> '{}'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), enum_name(field_type));
					}
					return field_type;
				} else if (!is_comment(str_key)) {
					break;
				}
			}
			if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
				return FieldType::object;
			} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::object) {
				THROW(ClientError, "Type mismatch '{}' -> 'object'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
			}
			return FieldType::object;

		case MsgPack::Type::ARRAY:
			if (specification.flags.geo_detection) {
				auto items = item_doc.size();
				if ((items == 2 && item_doc[0].is_number() && item_doc[1].is_number()) ||
					(items == 3 && item_doc[0].is_number() && item_doc[1].is_number() && item_doc[2].is_number())) {
					// GeoJSON requires longitude first, latutude second
					auto longitude = item_doc[0].f64();
					auto latitude = item_doc[1].f64();
					if (longitude >= -180.0 && longitude <= 180.0 && latitude >= -90.0 && latitude <= 90.0) {
						if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::empty) {
							return FieldType::geo;
						} else if (specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::geo) {
							THROW(ClientError, "Type mismatch '{}' -> 'geo'", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));
						}
						return FieldType::geo;
					}
				}
			}
			break;

		default:
			break;
	}

	return FieldType::empty;
}
