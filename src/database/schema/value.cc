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

// Indexing one value of a field: its terms, values, partial paths and stored data.

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


void
Schema::index_item_value(engine::Document& doc, MsgPack*& data, const MsgPack& item_value, size_t pos)
{
	L_CALL("Schema::index_item_value(<doc>, {}, {}, {})", data->to_string(), item_value.to_string(), pos);

	if (!specification.flags.complete) {
		complete_specification(item_value);
	}

	if (specification.partial_index_spcs.empty()) {
		index_item(doc, item_value, pos);
		if (specification.flags.store) {
			store_item(item_value, *data);
		}
	} else {
		index_spc_t start_index_spc(specification.sep_types[SPC_CONCRETE_TYPE], std::move(specification.prefix.field), specification.slot, std::move(specification.accuracy), std::move(specification.acc_prefix));
		for (const auto& index_spc : specification.partial_index_spcs) {
			specification.update(index_spc);
			index_item(doc, item_value, pos);
		}
		if (specification.flags.store) {
			store_item(item_value, *data);
		}
		specification.update(std::move(start_index_spc));
	}

	if (specification.sep_types[SPC_FOREIGN_TYPE] == FieldType::foreign) {
		if (!specification.flags.static_foreign) {
			data->get(RESERVED_FOREIGN) = specification.endpoint;
		}
	}
}


std::unordered_set<std::pair<std::string, bool>>
Schema::get_partial_paths()
{
	L_CALL("Schema::get_partial_paths()");

	if (specification.partial_prefixes.size() > LIMIT_PARTIAL_PATHS_DEPTH) {
		THROW(ClientError, "Partial paths limit depth is {}, and partial paths provided has a depth of {}", LIMIT_PARTIAL_PATHS_DEPTH, specification.partial_prefixes.size());
	}

	std::vector<std::pair<std::string, bool>> paths;
	bool do_get_uuid_slot = toUType(specification.index & TypeIndex::VALUES) != 0u;
	bool do_get_slot = do_get_uuid_slot && (specification.flags.has_uuid_prefix || specification.flags.inside_namespace);

	if (specification.partial_prefixes.size() >= 2) {
		auto it = specification.partial_prefixes.begin();

		if (!it->uuid.empty() && it->field != it->uuid) {
			paths.emplace_back(it->uuid, do_get_uuid_slot);
		}
		if (!it->field.empty()) {
			paths.emplace_back(it->field, do_get_slot);
		}

		const auto it_last = specification.partial_prefixes.end() - 1;
		for (++it; it != it_last; ++it) {
			const auto size = paths.size();
			for (size_t i = 0; i < size; ++i) {
				if (!it->uuid.empty() && it->field != it->uuid) {
					paths.emplace_back(paths[i].first + it->uuid, paths[i].second || do_get_uuid_slot);
				}
				if (!it->field.empty()) {
					paths.emplace_back(paths[i].first + it->field, paths[i].second || do_get_slot);
				}
			}
		}

		const auto size = paths.size();
		for (size_t i = 0; i < size; ++i) {
			if (!it->uuid.empty() && it->field != it->uuid) {
				paths.emplace_back(paths[i].first + it->uuid, paths[i].second || do_get_uuid_slot);
			}
			if (!it->field.empty()) {
				paths[i].first.append(it->field);
			}
		}
	} else {
		if (!specification.prefix.uuid.empty() && specification.prefix.field != specification.prefix.uuid) {
			paths.emplace_back(specification.prefix.uuid, do_get_uuid_slot);
		}
		if (!specification.prefix.field.empty()) {
			paths.emplace_back(specification.prefix.field, do_get_slot);
		}
	}

	return std::unordered_set<std::pair<std::string, bool>>(std::make_move_iterator(paths.begin()), std::make_move_iterator(paths.end()));
}


void
Schema::index_partial_paths(engine::Document& doc)
{
	L_CALL("Schema::index_partial_paths(<engine::Document>)");

	if (specification.flags.partial_paths) {
		if (toUType(specification.index & TypeIndex::FIELD_TERMS) != 0u) {
			const auto paths = get_partial_paths();
			for (const auto& path : paths) {
				doc.add_boolean_term(doc.field(path.first, engine::FieldKind::boolean), std::string_view());
			}
		}
	}
}


inline void
Schema::index_term(engine::Document& doc, std::string_view field, std::string_view term, const specification_t& field_spc, size_t pos)
{
	L_CALL("Schema::index_term(<doc>, {}, {}, <field_spc>, {})", repr(field), repr(term), pos);

	const bool is_key = field_spc.prefix.field == DOCUMENT_ID_TERM_PREFIX && field == field_name(field_spc.prefix.field, field_spc.get_ctype());

	// The limit is on the whole term, the field's name included.
	if (field.size() + term.size() > 245) {
		if (field_spc.sep_types[SPC_CONCRETE_TYPE] == FieldType::keyword) {
			THROW(ClientError, "Keyword too long");
		}
		return;
	}

	// The id's terms are the key field's: the id's type character and its bytes.
	std::string key;
	if (is_key) {
		key = document_key(term, field_spc.get_ctype());
		field = engine::KEY_FIELD;
		term = key;
	}

	auto weight = field_spc.flags.bool_term ? 0 : field_spc.weight[getPos(pos, field_spc.weight.size())];
	auto position = field_spc.position[getPos(pos, field_spc.position.size())];
	const auto kind = position != 0u ? engine::FieldKind::text : field_spc.flags.bool_term ? engine::FieldKind::boolean : engine::FieldKind::terms;
	const auto doc_field = doc.field(field, kind);
	if (position != 0u) {
		doc.add_posting(doc_field, term, position, weight);
	} else {
		doc.add_term(doc_field, term, weight);
	}
	L_INDEX("Field Term [{}] -> {} {}  Bool: {}  Posting: {}", pos, repr(field), repr(term), field_spc.flags.bool_term, position);
}


inline void
Schema::index_item(engine::Document& doc, const MsgPack& item, size_t pos)
{
	L_CALL("Schema::index_item(<doc>, <item>, {})", pos);

	assert(specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::empty);
	assert(specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::object);

	if (item.is_undefined()) {
		return;
	}

	bool field_terms = toUType(specification.index & TypeIndex::FIELD_TERMS) != 0u;

	if (item.is_null()) {
		if (field_terms) {
			index_term(doc, specification.prefix.field, std::string_view(), specification, pos);
		}
		return;
	}

	bool field_values = toUType(specification.index & TypeIndex::FIELD_VALUES) != 0u;

	bool global_terms = toUType(specification.index & TypeIndex::GLOBAL_TERMS) != 0u;
	bool global_values = toUType(specification.index & TypeIndex::GLOBAL_VALUES) != 0u;

	const auto& g_specification = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);

	// A number meets its field by the one rule (D92, Serialize::exact); a cast object converts any other value.
	const auto concrete_type = specification.sep_types[SPC_CONCRETE_TYPE];
	const bool numeric = concrete_type == FieldType::integer || concrete_type == FieldType::positive || concrete_type == FieldType::floating;
	MsgPack value_holder;
	const MsgPack& value = numeric ? value_holder = Serialize::exact(specification.full_meta_name, concrete_type, item)
		: item.is_map() ? value_holder = Cast::cast(item) : item;

	switch (concrete_type) {
		case FieldType::floating: {
			// An integer keeps its exact bytes and terms; a float's terms are its truncation (design D6).
			std::string ser_value;
			int64_t term_val;
			switch (value.get_type()) {
				case MsgPack::Type::NEGATIVE_INTEGER:
					term_val = value.i64();
					ser_value = Serialize::integer(term_val);
					break;
				case MsgPack::Type::POSITIVE_INTEGER:
					term_val = static_cast<int64_t>(std::min(value.u64(), static_cast<uint64_t>(std::numeric_limits<int64_t>::max())));
					ser_value = Serialize::positive(value.u64());
					break;
				default:
					term_val = GenerateTerms::truncate<int64_t>(value.f64());
					ser_value = Serialize::floating(FieldType::floating, value.f64());
					break;
			}
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, term_val);
				} else {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, term_val);
					GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, term_val);
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, term_val);
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, term_val);
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::integer: {
			const auto i_val = value.i64();
			auto ser_value = Serialize::integer(i_val);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, static_cast<int64_t>(i_val));
				} else {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(i_val));
					GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(i_val));
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(i_val));
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(i_val));
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::positive: {
			const auto u_val = value.u64();
			auto ser_value = Serialize::positive(u_val);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::positive(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, static_cast<int64_t>(u_val));
				} else {
					GenerateTerms::positive(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(u_val));
					GenerateTerms::positive(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(u_val));
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::positive(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(u_val));
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::positive(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(u_val));
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::date:
		case FieldType::datetime: {
			Datetime::tm_t tm;
			auto ser_value = Serialize::datetime(value, tm);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::datetime(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, tm);
				} else {
					GenerateTerms::datetime(doc, specification.accuracy, specification.acc_prefix, tm);
					GenerateTerms::datetime(doc, g_specification.accuracy, g_specification.acc_prefix, tm);
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::datetime(doc, g_specification.accuracy, g_specification.acc_prefix, tm);
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::datetime(doc, specification.accuracy, specification.acc_prefix, tm);
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::time: {
			double t_val = 0.0;
			auto ser_value = Serialize::time(value, t_val);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, static_cast<int64_t>(t_val));
				} else {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(t_val));
					GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(t_val));
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(t_val));
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(t_val));
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::timedelta: {
			double t_val = 0.0;
			auto ser_value = Serialize::timedelta(value, t_val);
			if (global_terms && field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				if (specification.accuracy == g_specification.accuracy) {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, static_cast<int64_t>(t_val));
				} else {
					GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(t_val));
					GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(t_val));
				}
			} else if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				GenerateTerms::integer(doc, g_specification.accuracy, g_specification.acc_prefix, static_cast<int64_t>(t_val));
			} else if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				GenerateTerms::integer(doc, specification.accuracy, specification.acc_prefix, static_cast<int64_t>(t_val));
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::geo: {
			GeoSpatial geo(value);
			const auto& geometry = geo.getGeometry();
			if ((global_terms || global_values) && (field_terms || field_values)) {
				if (specification.flags.partials == g_specification.flags.partials && specification.error == g_specification.error) {
					auto ranges = geometry->getRanges(specification.flags.partials, specification.error);
					if (!ranges.empty()) {
						if (global_terms && field_terms) {
							auto ser_value = Serialize::ranges_hash(ranges);
							index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
							index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
							if (specification.accuracy == g_specification.accuracy) {
								GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, g_specification.acc_prefix, ranges);
							} else {
								GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, ranges);
								GenerateTerms::geo(doc, g_specification.accuracy, g_specification.acc_prefix, ranges);
							}
						} else if (global_terms) {
							auto ser_value = Serialize::ranges_hash(ranges);
							index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
							GenerateTerms::geo(doc, g_specification.accuracy, g_specification.acc_prefix, ranges);
						} else if (field_terms) {
							auto ser_value = Serialize::ranges_hash(ranges);
							index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
							GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, ranges);
						}
						if (global_values) {
							merge_geospatial_values(map_values[g_specification.slot], ranges, geometry->getCentroids());
						}
						if (field_values) {
							merge_geospatial_values(map_values[specification.slot], std::move(ranges), geometry->getCentroids());
						}
					}
				} else {
					auto global_ranges = geometry->getRanges(g_specification.flags.partials, g_specification.error);
					if (!global_ranges.empty()) {
						if (global_terms) {
							auto ser_value = Serialize::ranges_hash(global_ranges);
							index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
							GenerateTerms::geo(doc, g_specification.accuracy, g_specification.acc_prefix, global_ranges);
						}
						if (global_values) {
							merge_geospatial_values(map_values[g_specification.slot], std::move(global_ranges), geometry->getCentroids());
						}
					}
					auto field_ranges = geometry->getRanges(specification.flags.partials, specification.error);
					if (!field_ranges.empty()) {
						if (field_terms) {
							auto ser_value = Serialize::ranges_hash(field_ranges);
							index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
							GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, field_ranges);
						}
						if (field_values) {
							merge_geospatial_values(map_values[specification.slot], std::move(field_ranges), geometry->getCentroids());
						}
					}
				}
			} else if (global_terms || global_values) {
				auto ranges = geometry->getRanges(g_specification.flags.partials, g_specification.error);
				if (!ranges.empty()) {
					if (global_terms) {
						auto ser_value = Serialize::ranges_hash(ranges);
						index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
						GenerateTerms::geo(doc, g_specification.accuracy, g_specification.acc_prefix, ranges);
					}
					if (global_values) {
						merge_geospatial_values(map_values[g_specification.slot], std::move(ranges), geometry->getCentroids());
					}
				}
			} else if (field_terms || field_values) {
				auto ranges = geometry->getRanges(specification.flags.partials, specification.error);
				if (!ranges.empty()) {
					if (field_terms) {
						auto ser_value = Serialize::ranges_hash(ranges);
						index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
						GenerateTerms::geo(doc, specification.accuracy, specification.acc_prefix, ranges);
					}
					if (field_values) {
						merge_geospatial_values(map_values[specification.slot], std::move(ranges), geometry->getCentroids());
					}
				}
			}
			return;
		}

		case FieldType::keyword: {
			if (value.is_string()) {
				auto ser_value = value.str();
				if (global_terms) {
					GenerateTerms::text(doc,
						g_specification.prefix.field + g_specification.get_ctype(), ser_value,
						g_specification.positions[getPos(pos, g_specification.positions.size())],
						g_specification.flags.bool_term ? 0 : g_specification.weight[getPos(pos, g_specification.weight.size())],
						g_specification.flags.cjk_ngram,
						g_specification.language, g_specification.stem_language,
						getGeneratorStopStrategy(g_specification.stop_strategy),
						getGeneratorStemStrategy(g_specification.stem_strategy),
						g_specification.flags.cjk_words);
				}
				if (field_terms) {
					if (specification.flags.bool_term) {
						index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
					} else {
						index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), strings::lower(ser_value), specification, pos);
					}
				}
				if (global_values) {
					map_values[g_specification.slot].insert(ser_value);
				}
				if (field_values) {
					map_values[specification.slot].insert(std::move(ser_value));
				}
				return;
			} else {
				THROW(ClientError, "Format invalid for {} type: {}", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), value.to_string());
			}
		}

		case FieldType::text: {
			if (value.is_string()) {
				auto ser_value = value.str();
				if (global_terms) {
					GenerateTerms::text(doc,
						g_specification.prefix.field + g_specification.get_ctype(), ser_value,
						g_specification.positions[getPos(pos, g_specification.positions.size())],
						g_specification.flags.bool_term ? 0 : g_specification.weight[getPos(pos, g_specification.weight.size())],
						g_specification.flags.cjk_ngram,
						g_specification.language, g_specification.stem_language,
						getGeneratorStopStrategy(g_specification.stop_strategy),
						getGeneratorStemStrategy(g_specification.stem_strategy),
						g_specification.flags.cjk_words);
				}
				if (field_terms) {
					GenerateTerms::text(doc,
						specification.prefix.field + specification.get_ctype(), ser_value,
						specification.positions[getPos(pos, specification.positions.size())],
						specification.flags.bool_term ? 0 : specification.weight[getPos(pos, specification.weight.size())],
						specification.flags.cjk_ngram,
						specification.language, specification.stem_language,
						getGeneratorStopStrategy(specification.stop_strategy),
						getGeneratorStemStrategy(specification.stem_strategy),
						specification.flags.cjk_words);
				}
				if (ser_value.size() <= 100) {
					// For text and string, only add relatively short values
					if (global_values) {
						map_values[g_specification.slot].insert(ser_value);
					}
					if (field_values) {
						map_values[specification.slot].insert(std::move(ser_value));
					}
				}
				return;
			} else {
				THROW(ClientError, "Format invalid for {} type: {}", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), value.to_string());
			}
		}

		case FieldType::boolean: {
			auto ser_value = Serialize::MsgPack(specification, value);
			if (global_terms) {
				index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
			}
			if (field_terms) {
				index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
			}
			if (global_values) {
				map_values[g_specification.slot].insert(ser_value);
			}
			if (field_values) {
				map_values[specification.slot].insert(std::move(ser_value));
			}
			return;
		}

		case FieldType::uuid: {
			if (value.is_string()) {
				auto ser_value = Serialize::uuid(value.str_view());
				if (global_terms) {
					index_term(doc, field_name(g_specification.prefix.field, g_specification.get_ctype()), ser_value, g_specification, pos);
				}
				if (field_terms) {
					index_term(doc, field_name(specification.prefix.field, specification.get_ctype()), ser_value, specification, pos);
				}
				if (global_values) {
					map_values[g_specification.slot].insert(ser_value);
				}
				if (field_values) {
					map_values[specification.slot].insert(std::move(ser_value));
				}
				return;
			} else {
				THROW(ClientError, "Format invalid for uuid type: {}", value.to_string());
			}
		}

		case FieldType::script:
			if (value.is_string()) {
				return;
			}
			THROW(ClientError, "Format invalid for {} type: {}", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]), value.to_string());

		case FieldType::object:
			THROW(ClientError, "Type: '{}' is an invalid value type", enum_name(specification.sep_types[SPC_CONCRETE_TYPE]));

		default:
			THROW(ClientError, "Type: {:#04x} is an unknown value type", toUType(specification.sep_types[SPC_CONCRETE_TYPE]));
	}
}


void
Schema::store_item(const MsgPack& value, MsgPack& data)
{
	L_CALL("Schema::store_item({}, {})", value.to_string(), data.to_string());

	// Add value to data.
	auto& data_value = data[RESERVED_VALUE];

	switch (specification.sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::uuid:
			switch (data_value.get_type()) {
				case MsgPack::Type::UNDEFINED:
					data_value = normalize_uuid(value);
					break;
				case MsgPack::Type::ARRAY:
					data_value.push_back(normalize_uuid(value));
					break;
				default:
					data_value = MsgPack({ data_value, normalize_uuid(value) });
					break;
			}
			break;

		case FieldType::date:
		case FieldType::datetime: {
			// A cast object (`{"_date": ...}`) is read through its cast, as index_item reads it.
			const auto iso = Datetime::iso8601(Datetime::DatetimeParser(value.is_map() ? Cast::cast(value) : value));
			switch (data_value.get_type()) {
				case MsgPack::Type::UNDEFINED:
					data_value = iso;
					break;
				case MsgPack::Type::ARRAY:
					data_value.push_back(iso);
					break;
				default:
					data_value = MsgPack({ data_value, iso });
					break;
			}
			break;
		}

		default:
			switch (data_value.get_type()) {
				case MsgPack::Type::UNDEFINED:
					data_value = value;
					break;
				case MsgPack::Type::ARRAY:
					data_value.push_back(value);
					break;
				default:
					data_value = MsgPack({ data_value, value });
					break;
			}
			break;
	}
}


void
Schema::merge_geospatial_values(std::set<std::string>& s, std::vector<range_t> ranges, std::vector<Cartesian> centroids)
{
	L_CALL("Schema::merge_geospatial_values(...)");

	if (s.empty()) {
		s.insert(Serialize::ranges_centroids(ranges, centroids));
	} else {
		auto prev_value = Unserialize::ranges_centroids(*s.begin());
		s.clear();
		ranges = HTM::range_union(std::move(ranges), std::vector<range_t>(std::make_move_iterator(prev_value.first.begin()), std::make_move_iterator(prev_value.first.end())));
		const auto& prev_centroids = prev_value.second;
		if (!prev_centroids.empty()) {
			std::vector<Cartesian> missing;
			auto centroids_begin = centroids.begin();
			auto centroids_end = centroids.end();
			for (const auto& _centroid : prev_centroids) {
				if (std::find(centroids_begin, centroids_end, _centroid) == centroids_end) {
					missing.push_back(_centroid);
				}
			}
			centroids.insert(centroids.end(), missing.begin(), missing.end());
		}
		s.insert(Serialize::ranges_centroids(ranges, centroids));
	}
}
