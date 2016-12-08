/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#include "schema.h"

#include <ctype.h>                         // for tolower
#include <algorithm>                       // for move
#include <cmath>                           // for pow
#include <cstdint>                         // for uint64_t
#include <cstring>                         // for size_t, strlen
#include <functional>                      // for ref, reference_wrapper
#include <ostream>                         // for operator<<, basic_ostream
#include <set>                             // for __tree_const_iterator, set
#include <stdexcept>                       // for out_of_range
#include <type_traits>                     // for remove_reference<>::type
#include <unordered_set>                   // for unordered_set

#include "datetime.h"                      // for isDate, tm_t
#include "exception.h"                     // for ClientError, MSG_ClientError
#include "geo/wkt_parser.h"                // for EWKT_Parser
#include "log.h"                           // for L_CALL
#include "manager.h"                       // for XapiandManager, XapiandMan...
#include "multivalue/generate_terms.h"     // for integer, geo, date, positive
#include "serialise.h"                     // for type


#ifndef L_SCHEMA
#define L_SCHEMA_DEFINED
#define L_SCHEMA L_TEST
#endif


/*
 * 1. Try reading schema from the metadata.
 * 2. Feed specification_t with the read schema using update_*;
 *    sets field_found for all found fields.
 * 3. Feed specification_t with the object sent by the user using process_*,
 *    except those that are already fixed because are reserved to be and
 *    they already exist in the metadata.
 * 4. If the field in the schema still has no RESERVED_TYPE (field_with_type)
 *    and a value is received for the field, call validate_required_data() to
 *    initialize the specification with validated data sent by the user.
 * 5. If there are values sent by user, fills the document to be indexed by
 *    using index_object() and index_array().
 */


/*
 * Unordered Maps used for reading user data specification.
 */

const std::unordered_map<std::string, UnitTime> map_acc_date({
	{ "second",     UnitTime::SECOND     }, { "minute",  UnitTime::MINUTE  },
	{ "hour",       UnitTime::HOUR       }, { "day",     UnitTime::DAY     },
	{ "month",      UnitTime::MONTH      }, { "year",    UnitTime::YEAR    },
	{ "decade",     UnitTime::DECADE     }, { "century", UnitTime::CENTURY },
	{ "millennium", UnitTime::MILLENNIUM },
});


const std::unordered_map<std::string, StopStrategy> map_stop_strategy({
	{ "stop_none",    StopStrategy::STOP_NONE    }, { "none",    StopStrategy::STOP_NONE    },
	{ "stop_all",     StopStrategy::STOP_ALL     }, { "all",     StopStrategy::STOP_ALL     },
	{ "stop_stemmed", StopStrategy::STOP_STEMMED }, { "stemmed", StopStrategy::STOP_STEMMED },
});


const std::unordered_map<std::string, StemStrategy> map_stem_strategy({
	{ "stem_none",  StemStrategy::STEM_NONE   }, { "none",  StemStrategy::STEM_NONE   },
	{ "stem_some",  StemStrategy::STEM_SOME   }, { "some",  StemStrategy::STEM_SOME   },
	{ "stem_all",   StemStrategy::STEM_ALL    }, { "all",   StemStrategy::STEM_ALL    },
	{ "stem_all_z", StemStrategy::STEM_ALL_Z  }, { "all_z", StemStrategy::STEM_ALL_Z  },
});


const std::unordered_map<std::string, TypeIndex> map_index({
	{ "none",                      TypeIndex::NONE                      },
	{ "field_terms",               TypeIndex::FIELD_TERMS               },
	{ "field_values",              TypeIndex::FIELD_VALUES              },
	{ "fields",                    TypeIndex::FIELD_ALL                 },
	{ "field_all",                 TypeIndex::FIELD_ALL                 },
	{ "global_terms",              TypeIndex::GLOBAL_TERMS              },
	{ "terms",                     TypeIndex::TERMS                     },
	{ "global_terms,field_values", TypeIndex::GLOBAL_TERMS_FIELD_VALUES },
	{ "field_values,global_terms", TypeIndex::GLOBAL_TERMS_FIELD_VALUES },
	{ "global_terms,fields",       TypeIndex::GLOBAL_TERMS_FIELD_ALL    },
	{ "global_terms,field_all",    TypeIndex::GLOBAL_TERMS_FIELD_ALL    },
	{ "fields,global_terms",       TypeIndex::GLOBAL_TERMS_FIELD_ALL    },
	{ "field_all,global_terms",    TypeIndex::GLOBAL_TERMS_FIELD_ALL    },
	{ "global_values",             TypeIndex::GLOBAL_VALUES             },
	{ "global_values,field_terms", TypeIndex::GLOBAL_VALUES_FIELD_TERMS },
	{ "field_terms,global_values", TypeIndex::GLOBAL_VALUES_FIELD_TERMS },
	{ "values",                    TypeIndex::VALUES                    },
	{ "global_values,fields",      TypeIndex::GLOBAL_VALUES_FIELD_ALL   },
	{ "global_values,field_all",   TypeIndex::GLOBAL_VALUES_FIELD_ALL   },
	{ "fields,global_values",      TypeIndex::GLOBAL_VALUES_FIELD_ALL   },
	{ "field_all,global_values",   TypeIndex::GLOBAL_VALUES_FIELD_ALL   },
	{ "globals",                   TypeIndex::GLOBAL_ALL                },
	{ "global_all",                TypeIndex::GLOBAL_ALL                },
	{ "globals,field_terms",       TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "globals,field_terms",       TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "global_all,field_terms",    TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "field_terms,globals",       TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "field_terms,global_all",    TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "global_all,field_values",   TypeIndex::GLOBAL_ALL_FIELD_VALUES   },
	{ "globals,field_values",      TypeIndex::GLOBAL_ALL_FIELD_VALUES   },
	{ "field_values,globals",      TypeIndex::GLOBAL_ALL_FIELD_VALUES   },
	{ "field_values,global_all",   TypeIndex::GLOBAL_ALL_FIELD_VALUES   },
	{ "all",                       TypeIndex::ALL                       },
});


const std::unordered_map<std::string, FieldType> map_type({
	{ FLOAT_STR,       FieldType::FLOAT        }, { INTEGER_STR,     FieldType::INTEGER      },
	{ POSITIVE_STR,    FieldType::POSITIVE     }, { TERM_STR,        FieldType::TERM         },
	{ TEXT_STR,        FieldType::TEXT         }, { STRING_STR,      FieldType::STRING       },
	{ DATE_STR,        FieldType::DATE         }, { GEO_STR,         FieldType::GEO          },
	{ BOOLEAN_STR,     FieldType::BOOLEAN      }, { UUID_STR,        FieldType::UUID         },
});


/*
 * Default accuracies.
 */

static const std::vector<uint64_t> def_accuracy_num({ 100, 1000, 10000, 100000, 1000000, 10000000 });
static const std::vector<uint64_t> def_accuracy_date({ toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY) });
static const std::vector<uint64_t> def_accuracy_geo({ 0, 5, 10, 15, 20, 25 });


/*
 * Helper functions to print readable form of enums
 */

inline static std::string readable_acc_date(UnitTime unit) noexcept {
	switch (unit) {
		case UnitTime::SECOND:     return "second";
		case UnitTime::MINUTE:     return "minute";
		case UnitTime::HOUR:       return "hour";
		case UnitTime::DAY:        return "day";
		case UnitTime::MONTH:      return "month";
		case UnitTime::YEAR:       return "year";
		case UnitTime::DECADE:     return "decade";
		case UnitTime::CENTURY:    return "century";
		case UnitTime::MILLENNIUM: return "millennium";
		default:                   return "UnitTime::UNKNOWN";
	}
}


inline static std::string readable_stop_strategy(StopStrategy stop_strategy) noexcept {
	switch (stop_strategy) {
		case StopStrategy::STOP_NONE:    return "stop_none";
		case StopStrategy::STOP_ALL:     return "stop_all";
		case StopStrategy::STOP_STEMMED: return "stop_stemmed";
		default:                         return "StopStrategy::UNKNOWN";
	}
}


inline static std::string readable_stem_strategy(StemStrategy stem_strategy) noexcept {
	switch (stem_strategy) {
		case StemStrategy::STEM_NONE:   return "stem_none";
		case StemStrategy::STEM_SOME:   return "stem_some";
		case StemStrategy::STEM_ALL:    return "stem_all";
		case StemStrategy::STEM_ALL_Z:  return "stem_all_z";
		default:                        return "StemStrategy::UNKNOWN";
	}
}


inline static std::string readable_index(TypeIndex index) noexcept {
	switch (index) {
		case TypeIndex::NONE:                       return "none";
		case TypeIndex::FIELD_TERMS:                return "field_terms";
		case TypeIndex::FIELD_VALUES:               return "field_values";
		case TypeIndex::FIELD_ALL:                  return "fields";
		case TypeIndex::GLOBAL_TERMS:               return "global_terms";
		case TypeIndex::TERMS:                      return "terms";
		case TypeIndex::GLOBAL_TERMS_FIELD_VALUES:  return "global_terms,field_values";
		case TypeIndex::GLOBAL_TERMS_FIELD_ALL:     return "global_terms,fields";
		case TypeIndex::GLOBAL_VALUES:              return "global_values";
		case TypeIndex::GLOBAL_VALUES_FIELD_TERMS:  return "global_values,field_terms";
		case TypeIndex::VALUES:                     return "values";
		case TypeIndex::GLOBAL_VALUES_FIELD_ALL:    return "global_values,fields";
		case TypeIndex::GLOBAL_ALL:                 return "globals";
		case TypeIndex::GLOBAL_ALL_FIELD_TERMS:     return "globals,field_terms";
		case TypeIndex::GLOBAL_ALL_FIELD_VALUES:    return "globals,field_values";
		case TypeIndex::ALL:                        return "all";
		default:                                    return "TypeIndex::UNKNOWN";
	}
}


inline static std::string readable_type(const std::array<FieldType, 3>& sep_types) {
	std::string result;
	if (sep_types[0] != FieldType::EMPTY) {
		result += Serialise::type(sep_types[0]);
	}
	if (sep_types[1] != FieldType::EMPTY) {
		if (!result.empty()) result += "/";
		result += Serialise::type(sep_types[1]);
	}
	if (sep_types[2] != FieldType::EMPTY) {
		if (!result.empty()) result += "/";
		result += Serialise::type(sep_types[2]);
	}
	return result;
}


/*
 *  Functions for generating the name given an accuracy.
 */

static std::string acc_name_num(uint64_t acc) {
	return "_" + std::to_string(acc);
}

static std::string acc_name_date(uint64_t acc) {
	return "_" + readable_acc_date((UnitTime)(acc));
}

static std::string acc_name_geo(uint64_t acc) {
	return "_geo" + std::to_string(acc);
}


/*
 * Default acc_prefixes for global values.
 */

static const std::vector<std::string> global_acc_prefix_num = []() {
	std::vector<std::string> res;
	res.reserve(def_accuracy_num.size());
	for (const auto& acc : def_accuracy_num) {
		res.push_back(DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::INTEGER)) + Serialise::namespace_field(acc_name_num(acc)));
	}
	return res;
}();

static const std::vector<std::string> global_acc_prefix_date = []() {
	std::vector<std::string> res;
	res.reserve(def_accuracy_date.size());
	for (const auto& acc : def_accuracy_date) {
		res.push_back(DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::DATE)) + Serialise::namespace_field(acc_name_date(acc)));
	}
	return res;
}();

static const std::vector<std::string> global_acc_prefix_geo = []() {
	std::vector<std::string> res;
	res.reserve(def_accuracy_geo.size());
	for (const auto& acc : def_accuracy_geo) {
		res.push_back(DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::GEO)) + Serialise::namespace_field(acc_name_geo(acc)));
	}
	return res;
}();


/*
 * Acceptable values string used when there is a data inconsistency.
 */

static const std::string str_set_acc_date = []() {
	std::string res("{ ");
	for (const auto& p : map_acc_date) {
		res.append(p.first).append(", ");
	}
	res.push_back('}');
	return res;
}();

static const std::string str_set_stop_strategy = []() {
	std::string res("{ ");
	for (const auto& p : map_stop_strategy) {
		res.append(p.first).append(", ");
	}
	res.push_back('}');
	return res;
}();

static const std::string str_set_stem_strategy = []() {
	std::string res("{ ");
	for (const auto& p : map_stem_strategy) {
		res.append(p.first).append(", ");
	}
	res.push_back('}');
	return res;
}();

static const std::string str_set_index = []() {
	std::string res("{ ");
	for (const auto& p : map_index) {
		res.append(p.first).append(", ");
	}
	res.push_back('}');
	return res;
}();

static const std::string str_set_type = []() {
	std::string res("{ ");
	for (const auto& p : map_type) {
		res.append(p.first).append(", ");
	}
	res.push_back('}');
	return res;
}();


const specification_t default_spc;


const std::unordered_map<std::string, Schema::dispatch_set_default_spc> Schema::map_dispatch_set_default_spc({
	{ ID_FIELD_NAME,  &Schema::set_default_spc_id },
	{ CT_FIELD_NAME,  &Schema::set_default_spc_ct },
});


const std::unordered_map<std::string, Schema::dispatch_process_reserved> Schema::map_dispatch_document({
	{ RESERVED_WEIGHT,             &Schema::process_weight          },
	{ RESERVED_POSITION,           &Schema::process_position        },
	{ RESERVED_SPELLING,           &Schema::process_spelling        },
	{ RESERVED_POSITIONS,          &Schema::process_positions       },
	{ RESERVED_TYPE,               &Schema::process_type            },
	{ RESERVED_PREFIX,             &Schema::process_prefix          },
	{ RESERVED_SLOT,               &Schema::process_slot            },
	{ RESERVED_INDEX,              &Schema::process_index           },
	{ RESERVED_STORE,              &Schema::process_store           },
	{ RESERVED_RECURSIVE,          &Schema::process_recursive       },
	{ RESERVED_DYNAMIC,            &Schema::process_dynamic         },
	{ RESERVED_STRICT,             &Schema::process_strict          },
	{ RESERVED_D_DETECTION,        &Schema::process_d_detection     },
	{ RESERVED_N_DETECTION,        &Schema::process_n_detection     },
	{ RESERVED_G_DETECTION,        &Schema::process_g_detection     },
	{ RESERVED_B_DETECTION,        &Schema::process_b_detection     },
	{ RESERVED_S_DETECTION,        &Schema::process_s_detection     },
	{ RESERVED_T_DETECTION,        &Schema::process_t_detection     },
	{ RESERVED_U_DETECTION,        &Schema::process_u_detection     },
	{ RESERVED_BOOL_TERM,          &Schema::process_bool_term       },
	{ RESERVED_VALUE,              &Schema::process_value           },
	{ RESERVED_NAME,               &Schema::process_name            },
	{ RESERVED_ACCURACY,           &Schema::process_accuracy        },
	{ RESERVED_ACC_PREFIX,         &Schema::process_acc_prefix      },
	{ RESERVED_LANGUAGE,           &Schema::process_language        },
	{ RESERVED_STOP_STRATEGY,      &Schema::process_stop_strategy   },
	{ RESERVED_STEM_STRATEGY,      &Schema::process_stem_strategy   },
	{ RESERVED_STEM_LANGUAGE,      &Schema::process_stem_language   },
	{ RESERVED_PARTIALS,           &Schema::process_partials        },
	{ RESERVED_ERROR,              &Schema::process_error           },
	{ RESERVED_NAMESPACE,          &Schema::process_namespace       },
	{ RESERVED_SCRIPT,             &Schema::process_script          },
	{ RESERVED_FLOAT,              &Schema::process_cast_object     },
	{ RESERVED_POSITIVE,           &Schema::process_cast_object     },
	{ RESERVED_INTEGER,            &Schema::process_cast_object     },
	{ RESERVED_BOOLEAN,            &Schema::process_cast_object     },
	{ RESERVED_TERM,               &Schema::process_cast_object     },
	{ RESERVED_TEXT,               &Schema::process_cast_object     },
	{ RESERVED_DATE,               &Schema::process_cast_object     },
	{ RESERVED_UUID,               &Schema::process_cast_object     },
	{ RESERVED_EWKT,               &Schema::process_cast_object     },
	{ RESERVED_POINT,              &Schema::process_cast_object     },
	{ RESERVED_POLYGON,            &Schema::process_cast_object     },
	{ RESERVED_CIRCLE,             &Schema::process_cast_object     },
	{ RESERVED_CHULL,              &Schema::process_cast_object     },
	{ RESERVED_MULTIPOINT,         &Schema::process_cast_object     },
	{ RESERVED_MULTIPOLYGON,       &Schema::process_cast_object     },
	{ RESERVED_MULTICIRCLE,        &Schema::process_cast_object     },
	{ RESERVED_MULTICHULL,         &Schema::process_cast_object     },
	{ RESERVED_GEO_COLLECTION,     &Schema::process_cast_object     },
	{ RESERVED_GEO_INTERSECTION,   &Schema::process_cast_object     },
});


const std::unordered_map<std::string, Schema::dispatch_update_reserved> Schema::map_dispatch_properties({
	{ RESERVED_WEIGHT,          &Schema::update_weight           },
	{ RESERVED_POSITION,        &Schema::update_position         },
	{ RESERVED_SPELLING,        &Schema::update_spelling         },
	{ RESERVED_POSITIONS,       &Schema::update_positions        },
	{ RESERVED_TYPE,            &Schema::update_type             },
	{ RESERVED_PREFIX,          &Schema::update_prefix           },
	{ RESERVED_SLOT,            &Schema::update_slot             },
	{ RESERVED_INDEX,           &Schema::update_index            },
	{ RESERVED_STORE,           &Schema::update_store            },
	{ RESERVED_RECURSIVE,       &Schema::update_recursive        },
	{ RESERVED_DYNAMIC,         &Schema::update_dynamic          },
	{ RESERVED_STRICT,          &Schema::update_strict           },
	{ RESERVED_D_DETECTION,     &Schema::update_d_detection      },
	{ RESERVED_N_DETECTION,     &Schema::update_n_detection      },
	{ RESERVED_G_DETECTION,     &Schema::update_g_detection      },
	{ RESERVED_B_DETECTION,     &Schema::update_b_detection      },
	{ RESERVED_S_DETECTION,     &Schema::update_s_detection      },
	{ RESERVED_T_DETECTION,     &Schema::update_t_detection      },
	{ RESERVED_U_DETECTION,     &Schema::update_u_detection      },
	{ RESERVED_BOOL_TERM,       &Schema::update_bool_term        },
	{ RESERVED_ACCURACY,        &Schema::update_accuracy         },
	{ RESERVED_ACC_PREFIX,      &Schema::update_acc_prefix       },
	{ RESERVED_LANGUAGE,        &Schema::update_language         },
	{ RESERVED_STOP_STRATEGY,   &Schema::update_stop_strategy    },
	{ RESERVED_STEM_STRATEGY,   &Schema::update_stem_strategy    },
	{ RESERVED_STEM_LANGUAGE,   &Schema::update_stem_language    },
	{ RESERVED_PARTIALS,        &Schema::update_partials         },
	{ RESERVED_ERROR,           &Schema::update_error            },
	{ RESERVED_NAMESPACE,       &Schema::update_namespace        },
});


const std::unordered_map<std::string, Schema::dispatch_readable> Schema::map_dispatch_readable({
	{ RESERVED_TYPE,            &Schema::readable_type           },
	{ RESERVED_PREFIX,          &Schema::readable_prefix         },
	{ RESERVED_STOP_STRATEGY,   &Schema::readable_stop_strategy  },
	{ RESERVED_STEM_STRATEGY,   &Schema::readable_stem_strategy  },
	{ RESERVED_STEM_LANGUAGE,   &Schema::readable_stem_language  },
	{ RESERVED_INDEX,           &Schema::readable_index          },
	{ RESERVED_ACC_PREFIX,      &Schema::readable_acc_prefix     },
});


const std::unordered_map<std::string, std::pair<bool, std::string>> map_stem_language({
	{ "armenian",    { true,  "hy" } },  { "hy",               { true,  "hy" } },  { "basque",          { true,  "ue" } },
	{ "eu",          { true,  "eu" } },  { "catalan",          { true,  "ca" } },  { "ca",              { true,  "ca" } },
	{ "danish",      { true,  "da" } },  { "da",               { true,  "da" } },  { "dutch",           { true,  "nl" } },
	{ "nl",          { true,  "nl" } },  { "kraaij_pohlmann",  { false, "nl" } },  { "english",         { true,  "en" } },
	{ "en",          { true,  "en" } },  { "earlyenglish",     { false, "en" } },  { "english_lovins",  { false, "en" } },
	{ "lovins",      { false, "en" } },  { "english_porter",   { false, "en" } },  { "porter",          { false, "en" } },
	{ "finnish",     { true,  "fi" } },  { "fi",               { true,  "fi" } },  { "french",          { true,  "fr" } },
	{ "fr",          { true,  "fr" } },  { "german",           { true,  "de" } },  { "de",              { true,  "de" } },
	{ "german2",     { false, "de" } },  { "hungarian",        { true,  "hu" } },  { "hu",              { true,  "hu" } },
	{ "italian",     { true,  "it" } },  { "it",               { true,  "it" } },  { "norwegian",       { true,  "no" } },
	{ "nb",          { false, "no" } },  { "nn",               { false, "no" } },  { "no",              { true,  "no" } },
	{ "portuguese",  { true,  "pt" } },  { "pt",               { true,  "pt" } },  { "romanian",        { true,  "ro" } },
	{ "ro",          { true,  "ro" } },  { "russian",          { true,  "ru" } },  { "ru",              { true,  "ru" } },
	{ "spanish",     { true,  "es" } },  { "es",               { true,  "es" } },  { "swedish",         { true,  "sv" } },
	{ "sv",          { true,  "sv" } },  { "turkish",          { true,  "tr" } },  { "tr",              { true,  "tr" } },
	{ "none",        { false, DEFAULT_LANGUAGE } },
});


const std::unique_ptr<Xapian::SimpleStopper>& getStopper(const std::string& language) {
	static std::unordered_map<std::string, std::unique_ptr<Xapian::SimpleStopper>> stoppers;
	auto it = stoppers.find(language);
	if (it == stoppers.end()) {
		return it->second;
	} else {
		std::ifstream words(PATH_STOP_WORDS + std::string("/") + language + ".txt");
		if (words.is_open()) {
			auto& stopper = stoppers[language];
			stopper = std::make_unique<Xapian::SimpleStopper>(std::istream_iterator<std::string>(words), std::istream_iterator<std::string>());
			return stopper;
		} else {
			static std::unique_ptr<Xapian::SimpleStopper> stopper;
			return stopper;
		}
	}
}


required_spc_t::flags_t::flags_t()
	: bool_term(DEFAULT_BOOL_TERM),
	  partials(DEFAULT_GEO_PARTIALS),
	  store(true),
	  parent_store(true),
	  is_recursive(true),
	  dynamic(true),
	  strict(false),
	  date_detection(true),
	  numeric_detection(true),
	  geo_detection(true),
	  bool_detection(true),
	  string_detection(true),
	  text_detection(true),
	  uuid_detection(true),
	  field_found(true),
	  field_with_type(false),
	  reserved_slot(false),
	  has_bool_term(false),
	  has_index(false),
	  has_namespace(false),
	  inside_namespace(false),
	  dynamic_type(false) { }


required_spc_t::required_spc_t()
	: sep_types({{ FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY }}),
	  slot(Xapian::BAD_VALUENO),
	  language(DEFAULT_LANGUAGE),
	  stop_strategy(DEFAULT_STOP_STRATEGY),
	  stem_strategy(DEFAULT_STEM_STRATEGY),
	  stem_language(DEFAULT_LANGUAGE),
	  error(DEFAULT_GEO_ERROR) { }


required_spc_t::required_spc_t(Xapian::valueno _slot, FieldType type, const std::vector<uint64_t>& acc,
	const std::vector<std::string>& _acc_prefix)
	: sep_types({{ FieldType::EMPTY, FieldType::EMPTY, type }}),
	  prefix("G" + std::string(1, toUType(type))),
	  slot(_slot),
	  accuracy(acc),
	  acc_prefix(_acc_prefix),
	  language(DEFAULT_LANGUAGE),
	  stop_strategy(DEFAULT_STOP_STRATEGY),
	  stem_strategy(DEFAULT_STEM_STRATEGY),
	  stem_language(DEFAULT_LANGUAGE),
	  error(DEFAULT_GEO_ERROR) { }


required_spc_t::required_spc_t(const required_spc_t& o)
	: sep_types(o.sep_types),
	  prefix(o.prefix),
	  slot(o.slot),
	  flags(o.flags),
	  accuracy(o.accuracy),
	  acc_prefix(o.acc_prefix),
	  language(o.language),
	  stop_strategy(o.stop_strategy),
	  stem_strategy(o.stem_strategy),
	  stem_language(o.stem_language),
	  error(o.error),
	  paths_namespace(o.paths_namespace) { }


required_spc_t::required_spc_t(required_spc_t&& o) noexcept
	: sep_types(std::move(o.sep_types)),
	  prefix(std::move(o.prefix)),
	  slot(std::move(o.slot)),
	  flags(std::move(o.flags)),
	  accuracy(std::move(o.accuracy)),
	  acc_prefix(std::move(o.acc_prefix)),
	  language(std::move(o.language)),
	  stop_strategy(std::move(o.stop_strategy)),
	  stem_strategy(std::move(o.stem_strategy)),
	  stem_language(std::move(o.stem_language)),
	  error(std::move(o.error)),
	  paths_namespace(std::move(o.paths_namespace)) { }


required_spc_t&
required_spc_t::operator=(const required_spc_t& o)
{
	sep_types = o.sep_types;
	prefix = o.prefix;
	slot = o.slot;
	flags = o.flags;
	accuracy = o.accuracy;
	acc_prefix = o.acc_prefix;
	language = o.language;
	stop_strategy = o.stop_strategy;
	stem_strategy = o.stem_strategy;
	stem_language = o.stem_language;
	error = o.error;
	paths_namespace = o.paths_namespace;
	return *this;
}


required_spc_t&
required_spc_t::operator=(required_spc_t&& o) noexcept
{
	sep_types = std::move(o.sep_types);
	prefix = std::move(o.prefix);
	slot = std::move(o.slot);
	flags = std::move(o.flags);
	accuracy = std::move(o.accuracy);
	acc_prefix = std::move(o.acc_prefix);
	language = std::move(o.language);
	stop_strategy = std::move(o.stop_strategy);
	stem_strategy = std::move(o.stem_strategy);
	stem_language = std::move(o.stem_language);
	error = std::move(o.error);
	paths_namespace = std::move(o.paths_namespace);
	return *this;
}


specification_t::specification_t()
	: position({ 0 }),
	  weight({ 1 }),
	  spelling({ DEFAULT_SPELLING }),
	  positions({ DEFAULT_POSITIONS }),
	  index(DEFAULT_INDEX) { }


specification_t::specification_t(Xapian::valueno _slot, FieldType type, const std::vector<uint64_t>& acc,
	const std::vector<std::string>& _acc_prefix)
	: required_spc_t(_slot, type, acc, _acc_prefix),
	  position({ 0 }),
	  weight({ 1 }),
	  spelling({ DEFAULT_SPELLING }),
	  positions({ DEFAULT_POSITIONS }),
	  index(DEFAULT_INDEX) { }


specification_t::specification_t(const specification_t& o)
	: required_spc_t(o),
	  position(o.position),
	  weight(o.weight),
	  spelling(o.spelling),
	  positions(o.positions),
	  index(o.index),
	  script(o.script),
	  name(o.name),
	  meta_name(o.meta_name),
	  full_meta_name(o.full_meta_name),
	  normalized_name(o.normalized_name),
	  full_normalized_name(o.full_normalized_name),
	  aux_stem_lan(o.aux_stem_lan),
	  aux_lan(o.aux_lan) { }


specification_t::specification_t(specification_t&& o) noexcept
	: required_spc_t(std::move(o)),
	  position(std::move(o.position)),
	  weight(std::move(o.weight)),
	  spelling(std::move(o.spelling)),
	  positions(std::move(o.positions)),
	  index(std::move(o.index)),
	  script(std::move(o.script)),
	  name(std::move(o.name)),
	  meta_name(std::move(o.meta_name)),
	  full_meta_name(std::move(o.full_meta_name)),
	  normalized_name(std::move(o.normalized_name)),
	  full_normalized_name(std::move(o.full_normalized_name)),
	  aux_stem_lan(std::move(o.aux_stem_lan)),
	  aux_lan(std::move(o.aux_lan)) { }


specification_t&
specification_t::operator=(const specification_t& o)
{
	position = o.position;
	weight = o.weight;
	spelling = o.spelling;
	positions = o.positions;
	index = o.index;
	value.reset();
	value_rec.reset();
	doc_acc.reset();
	script = o.script;
	name = o.name;
	meta_name = o.meta_name;
	full_meta_name = o.full_meta_name;
	normalized_name = o.normalized_name;
	full_normalized_name = o.full_normalized_name;
	aux_stem_lan = o.aux_stem_lan;
	aux_lan = o.aux_lan;
	required_spc_t::operator=(o);
	return *this;
}


specification_t&
specification_t::operator=(specification_t&& o) noexcept
{
	position = std::move(o.position);
	weight = std::move(o.weight);
	spelling = std::move(o.spelling);
	positions = std::move(o.positions);
	index = std::move(o.index);
	value.reset();
	value_rec.reset();
	doc_acc.reset();
	script = std::move(o.script);
	name = std::move(o.name);
	meta_name = std::move(o.meta_name);
	full_meta_name = std::move(o.full_meta_name);
	normalized_name = std::move(o.normalized_name);
	full_normalized_name = std::move(o.full_normalized_name);
	aux_stem_lan = std::move(o.aux_stem_lan);
	aux_lan = std::move(o.aux_lan);
	required_spc_t::operator=(std::move(o));
	return *this;
}


const specification_t&
specification_t::get_global(FieldType field_type)
{
	switch (field_type) {
		case FieldType::FLOAT: {
			static const specification_t spc(DB_SLOT_NUMERIC, FieldType::FLOAT, def_accuracy_num, global_acc_prefix_num);
			return spc;
		}
		case FieldType::INTEGER: {
			static const specification_t spc(DB_SLOT_NUMERIC, FieldType::INTEGER, def_accuracy_num, global_acc_prefix_num);
			return spc;
		}
		case FieldType::POSITIVE: {
			static const specification_t spc(DB_SLOT_NUMERIC, FieldType::POSITIVE, def_accuracy_num, global_acc_prefix_num);
			return spc;
		}
		case FieldType::TERM: {
			static const specification_t spc(DB_SLOT_STRING, FieldType::TEXT, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		case FieldType::TEXT: {
			static const specification_t spc(DB_SLOT_STRING, FieldType::TEXT, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		case FieldType::STRING: {
			static const specification_t spc(DB_SLOT_STRING, FieldType::TEXT, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		case FieldType::BOOLEAN: {
			static const specification_t spc(DB_SLOT_BOOLEAN, FieldType::BOOLEAN, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		case FieldType::DATE: {
			static const specification_t spc(DB_SLOT_DATE, FieldType::DATE, def_accuracy_date, global_acc_prefix_date);
			return spc;
		}
		case FieldType::GEO: {
			static const specification_t spc(DB_SLOT_GEO, FieldType::GEO, def_accuracy_geo, global_acc_prefix_geo);
			return spc;
		}
		case FieldType::UUID: {
			static const specification_t spc(DB_SLOT_UUID, FieldType::UUID, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		default:
			THROW(ClientError, "Type: '%u' is an unknown type", field_type);
	}
}


std::string
specification_t::to_string() const
{
	std::stringstream str;
	str << "\n{\n";
	str << "\t" << RESERVED_POSITION << ": [ ";
	for (const auto& _position : position) {
		str << _position << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_WEIGHT   << ": [ ";
	for (const auto& _w : weight) {
		str << _w << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_SPELLING << ": [ ";
	for (const auto& spel : spelling) {
		str << (spel ? "true" : "false") << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_POSITIONS<< ": [ ";
	for (const auto& _positions : positions) {
		str << (_positions ? "true" : "false") << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_LANGUAGE         << ": " << language          << "\n";
	str << "\t" << RESERVED_STOP_STRATEGY    << ": " << readable_stop_strategy(stop_strategy) << "\n";
	str << "\t" << RESERVED_STEM_STRATEGY    << ": " << readable_stem_strategy(stem_strategy) << "\n";
	str << "\t" << RESERVED_STEM_LANGUAGE    << ": " << stem_language     << "\n";

	str << "\t" << RESERVED_ACCURACY << ": [ ";
	for (const auto& acc : accuracy) {
		str << acc << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_ACC_PREFIX  << ": [ ";
	for (const auto& acc_p : acc_prefix) {
		str << acc_p << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_NAMESPACE  << ": [ ";
	for (const auto& name : paths_namespace) {
		str << repr(name) << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_VALUE       << ": " << (value ? value->to_string() : std::string())                  << "\n";
	str << "\t" << "value_rec"          << ": " << (value_rec ? value_rec->to_string().c_str() : std::string())  << "\n";

	str << "\t" << RESERVED_SLOT        << ": " << slot                           << "\n";
	str << "\t" << RESERVED_TYPE        << ": " << readable_type(sep_types)       << "\n";
	str << "\t" << RESERVED_PREFIX      << ": " << prefix                         << "\n";
	str << "\t" << RESERVED_INDEX       << ": " << readable_index(index)          << "\n";
	str << "\t" << RESERVED_STORE       << ": " << (flags.store             ? "true" : "false") << "\n";
	str << "\t" << RESERVED_RECURSIVE   << ": " << (flags.is_recursive      ? "true" : "false") << "\n";
	str << "\t" << RESERVED_DYNAMIC     << ": " << (flags.dynamic           ? "true" : "false") << "\n";
	str << "\t" << RESERVED_STRICT      << ": " << (flags.strict            ? "true" : "false") << "\n";
	str << "\t" << RESERVED_D_DETECTION << ": " << (flags.date_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_N_DETECTION << ": " << (flags.numeric_detection ? "true" : "false") << "\n";
	str << "\t" << RESERVED_G_DETECTION << ": " << (flags.geo_detection     ? "true" : "false") << "\n";
	str << "\t" << RESERVED_B_DETECTION << ": " << (flags.bool_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_S_DETECTION << ": " << (flags.string_detection  ? "true" : "false") << "\n";
	str << "\t" << RESERVED_T_DETECTION << ": " << (flags.text_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_U_DETECTION << ": " << (flags.uuid_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_BOOL_TERM   << ": " << (flags.bool_term         ? "true" : "false") << "\n";
	str << "\t" << "field_found"        << ": " << (flags.field_found       ? "true" : "false") << "\n";
	str << "\t" << "dynamic_type"       << ": " << (flags.dynamic_type      ? "true" : "false") << "\n";
	str << "\t" << "inside_namespace"   << ": " << (flags.inside_namespace  ? "true" : "false") << "\n";

	str << "\t" << RESERVED_NAME           << ": " << name                 << "\n";
	str << "\t" << "meta_name"             << ": " << meta_name            << "\n";
	str << "\t" << "full_meta_name"        << ": " << full_meta_name       << "\n";
	str << "\t" << "normalized_name"       << ": " << normalized_name      << "\n";
	str << "\t" << "full_normalized_name"  << ": " << full_normalized_name << "\n";

	str << "}\n";

	return str.str();
}


Schema::Schema(const std::shared_ptr<const MsgPack>& other)
	: schema(other)
{
	if (schema->is_undefined()) {
		MsgPack new_schema = {
			{ RESERVED_VERSION, DB_VERSION_SCHEMA },
			{ RESERVED_SCHEMA, MsgPack() },
		};
		new_schema.lock();
		schema = std::make_shared<const MsgPack>(std::move(new_schema));
	} else {
		try {
			const auto& version = schema->at(RESERVED_VERSION);
			if (version.as_f64() != DB_VERSION_SCHEMA) {
				THROW(Error, "Different database's version schemas, the current version is %1.1f", DB_VERSION_SCHEMA);
			}
		} catch (const std::out_of_range&) {
			THROW(Error, "Schema is corrupt, you need provide a new one");
		} catch (const msgpack::type_error&) {
			THROW(Error, "Schema is corrupt, you need provide a new one");
		}
	}
}


MsgPack&
Schema::get_mutable()
{
	L_CALL(this, "Schema::get_mutable()");

	if (!mut_schema) {
		mut_schema = std::make_unique<MsgPack>(*schema);
	}

	MsgPack* prop = &mut_schema->at(RESERVED_SCHEMA);
	std::vector<std::string> field_names;
	stringTokenizer(specification.full_meta_name, DB_OFFSPRING_UNION, field_names);
	for (const auto& field_name : field_names) {
		prop = &(*prop)[field_name];
	}
	return *prop;
}


MsgPack&
Schema::clear() noexcept
{
	L_CALL(this, "Schema::clear()");

	if (!mut_schema) {
		mut_schema = std::make_unique<MsgPack>(*schema);
	}

	auto& prop = mut_schema->at(RESERVED_SCHEMA);
	prop.clear();
	return prop;
}


void
Schema::restart_specification()
{
	L_CALL(this, "Schema::restart_specification()");

	specification.flags.bool_term        = default_spc.flags.bool_term;
	specification.flags.partials         = default_spc.flags.partials;
	specification.flags.has_index        = default_spc.flags.has_index;
	specification.flags.has_namespace    = default_spc.flags.has_namespace;
	specification.flags.field_with_type  = default_spc.flags.field_with_type;

	specification.sep_types              = default_spc.sep_types;
	specification.prefix                 = default_spc.prefix;
	specification.slot                   = default_spc.slot;
	specification.accuracy               = default_spc.accuracy;
	specification.acc_prefix             = default_spc.acc_prefix;
	specification.name                   = default_spc.name;
	specification.language               = default_spc.language;
	specification.stop_strategy          = default_spc.stop_strategy;
	specification.stem_strategy          = default_spc.stem_strategy;
	specification.stem_language          = default_spc.stem_language;
	specification.error                  = default_spc.error;
	specification.aux_stem_lan           = default_spc.aux_stem_lan;
	specification.aux_lan                = default_spc.aux_lan;
}


void
Schema::index_object(const MsgPack*& parent_properties, const MsgPack& object, MsgPack*& parent_data, Xapian::Document& doc, const std::string& name)
{
	L_CALL(this, "Schema::index_object(%s, %s)", repr(parent_properties->to_string()).c_str(), repr(name).c_str());

	if (!specification.flags.is_recursive) {
		if (specification.flags.store) {
			(*parent_data)[name] = object;
		}
		return;
	}

	const auto spc_start = specification;
	const MsgPack* properties = nullptr;
	MsgPack* data = nullptr;
	if (name.empty()) {
		properties = parent_properties;
		data = parent_data;
		specification.flags.field_found = true;
	} else {
		data = specification.flags.store ? &(*parent_data)[name] : parent_data;
		specification.name.assign(name);
		properties = &get_subproperties(*parent_properties);
	}

	switch (object.getType()) {
		case MsgPack::Type::MAP: {
			bool offsprings = false;
			TaskVector tasks;
			tasks.reserve(object.size());
			for (const auto& item_key : object) {
				const auto str_key = item_key.as_string();
				try {
					auto func = map_dispatch_document.at(str_key);
					(this->*func)(str_key, object.at(str_key));
				} catch (const std::out_of_range&) {
					if (is_valid(str_key)) {
						tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(data), std::ref(doc), std::move(str_key)));
						offsprings = true;
					}
				}
			}

			if (specification.name.empty()) {
				if (data != parent_data && !specification.flags.store) {
					parent_data->erase(name);
					data = parent_data;
				}
			} else {
				properties = &get_subproperties(*properties);
				if (specification.flags.store) {
					data = &(*data)[specification.name];
				}
			}

			process_item_value(doc, data, offsprings);

			const auto spc_object = std::move(specification);
			for (auto& task : tasks) {
				specification = spc_object;
				task.get();
			}
			break;
		}

		case MsgPack::Type::ARRAY:
			set_type_to_array();
			index_array(*properties, object, *data, doc);
			break;

		default:
			process_item_value(doc, data, object);
			break;
	}

	if (data->is_undefined()) {
		parent_data->erase(name);
	}

	specification = std::move(spc_start);
}


void
Schema::index_array(const MsgPack& properties, const MsgPack& array, MsgPack& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index_array()");

	const auto spc_start = specification;
	size_t pos = 0;
	for (const auto& item : array) {
		switch (item.getType()) {
			case MsgPack::Type::MAP: {
				TaskVector tasks;
				tasks.reserve(item.size());
				specification.value = nullptr;
				specification.value_rec = nullptr;
				auto sub_properties = &properties;
				MsgPack* data_pos = nullptr;
				bool offsprings = false;

				for (const auto& property : item) {
					auto str_prop = property.as_string();
					try {
						auto func = map_dispatch_document.at(str_prop);
						(this->*func)(str_prop, item.at(str_prop));
					} catch (const std::out_of_range&) {
						if (is_valid(str_prop)) {
							tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(sub_properties), std::ref(item.at(str_prop)), std::ref(data_pos), std::ref(doc), std::move(str_prop)));
							offsprings = true;
						}
					}
				}
				data_pos = specification.flags.store ? &data[pos] : &data;

				if (!specification.name.empty()) {
					sub_properties = &get_subproperties(*sub_properties);
					if (specification.flags.store) {
						data_pos = &(*data_pos)[specification.name];
					}
				}

				process_item_value(doc, data_pos, offsprings);

				const auto spc_item = std::move(specification);
				for (auto& task : tasks) {
					specification = spc_item;
					task.get();
				}

				specification = spc_start;
				break;
			}
			case MsgPack::Type::ARRAY: {
				auto data_pos = specification.flags.store ? &data[pos] : &data;
				process_item_value(doc, data_pos, item);
				break;
			}
			default: {
				process_item_value(doc, specification.flags.store ? data[pos] : data, item, pos);
				break;
			}
		}
		++pos;
	}
}


void
Schema::process_item_value(Xapian::Document& doc, MsgPack& data, const MsgPack& item_value, size_t pos)
{
	L_CALL(this, "Schema::process_item_value(<doc>, %s, %s, %zu)", data.to_string().c_str(), item_value.to_string().c_str(), pos);

	L_SCHEMA(this, "Specification: %s", specification.to_string().c_str());  // Final specification, as indexed

	if (item_value.is_null()) {
		index_paths_namespace(doc);
		if (specification.flags.store) {
			data = item_value;
		}
		return;
	}

	if (specification.flags.inside_namespace) {
		if (!specification.flags.field_with_type) {
			validate_required_namespace_data(item_value);
		}

		auto namespace_spc = get_namespace_specifications();
		bool add_value = true;
		for (auto& spc : namespace_spc) {
			specification.sep_types[2] = std::move(spc.sep_types[2]);
			specification.prefix       = std::move(spc.prefix);
			specification.slot         = std::move(spc.slot);
			specification.accuracy     = std::move(spc.accuracy);
			specification.acc_prefix   = std::move(spc.acc_prefix);
			index_item(doc, item_value, data, pos, add_value);
			add_value = false;
		}
	} else {
		if (!specification.flags.field_found && !specification.flags.dynamic) {
			THROW(ClientError, "%s is not dynamic", specification.full_normalized_name.c_str());
		}

		if (!specification.flags.field_with_type) {
			validate_required_data(item_value);
		} else if (specification.flags.dynamic_type) {
			update_dynamic_specification();
		}

		index_item(doc, item_value, data, pos);
	}

	if (specification.flags.store) {
		data = data[RESERVED_VALUE];
	}
}


void
Schema::process_item_value(Xapian::Document& doc, MsgPack*& data, const MsgPack& item_value)
{
	L_CALL(this, "Schema::process_item_value(<doc>, %s, %s)", data->to_string().c_str(), item_value.to_string().c_str());

	L_SCHEMA(this, "Specification: %s", specification.to_string().c_str());  // Final specification, as indexed

	if (item_value.is_null()) {
		index_paths_namespace(doc);
		if (specification.flags.store) {
			*data = item_value;
		}
		return;
	}

	if (specification.flags.inside_namespace) {
		if (!specification.flags.field_with_type) {
			validate_required_namespace_data(item_value);
		}

		auto namespace_spc = get_namespace_specifications();
		bool add_values = true;
		for (auto& spc : namespace_spc) {
			specification.sep_types[2] = std::move(spc.sep_types[2]);
			specification.prefix       = std::move(spc.prefix);
			specification.slot         = std::move(spc.slot);
			specification.accuracy     = std::move(spc.accuracy);
			specification.acc_prefix   = std::move(spc.acc_prefix);
			index_item(doc, item_value, *data, add_values);
			add_values = false;
		}
	} else {
		if (!specification.flags.field_found && !specification.flags.dynamic) {
			THROW(ClientError, "%s is not dynamic", specification.full_normalized_name.c_str());
		}

		if (!specification.flags.field_with_type) {
			validate_required_data(item_value);
		} else if (specification.flags.dynamic_type) {
			update_dynamic_specification();
		}

		index_item(doc, item_value, *data);
	}

	if (specification.flags.store && data->size() == 1) {
		*data = (*data)[RESERVED_VALUE];
	}
}


void
Schema::process_item_value(Xapian::Document& doc, MsgPack*& data, bool offsprings)
{
	L_CALL(this, "Schema::process_item_value(<doc>, %s, %s)", repr(data->to_string()).c_str(), offsprings ? "true" : "false");

	L_SCHEMA(this, "Specification: %s", specification.to_string().c_str());  // Final specification, as indexed

	set_type_to_object(offsprings);

	auto val = specification.value ? std::move(specification.value) : std::move(specification.value_rec);
	if (val) {
		if (val->is_null()) {
			index_paths_namespace(doc, offsprings);
			if (specification.flags.store) {
				offsprings ? (*data)[RESERVED_VALUE] = *val : *data = *val;
			}
			return;
		}

		if (specification.flags.inside_namespace) {
			if (!specification.flags.field_with_type) {
				validate_required_namespace_data(*val);
			}

			auto namespace_spc = get_namespace_specifications();
			bool add_values = true;
			if (offsprings) {
				for (auto& spc : namespace_spc) {
					specification.sep_types[2] = std::move(spc.sep_types[2]);
					specification.prefix       = std::move(spc.prefix);
					specification.slot         = std::move(spc.slot);
					specification.accuracy     = std::move(spc.accuracy);
					specification.acc_prefix   = std::move(spc.acc_prefix);
					index_item(doc, *val, *data, add_values);
					add_values = false;
				}
			} else {
				for (auto& spc : namespace_spc) {
					specification.sep_types[2] = std::move(spc.sep_types[2]);
					specification.prefix       = std::move(spc.prefix);
					specification.slot         = std::move(spc.slot);
					specification.accuracy     = std::move(spc.accuracy);
					specification.acc_prefix   = std::move(spc.acc_prefix);
					index_item(doc, *val, *data, add_values);
					specification.prefix.pop_back();
					doc.add_term(specification.prefix);
					add_values = false;
				}
			}
		} else {
			if (!specification.flags.field_found && !specification.flags.dynamic) {
				THROW(ClientError, "%s is not dynamic", specification.full_normalized_name.c_str());
			}

			if (!specification.flags.field_with_type) {
				validate_required_data(*val);
			} else if (specification.flags.dynamic_type) {
				update_dynamic_specification();
			}
			index_item(doc, *val, *data);
		}

		if (specification.flags.store && !offsprings) {
			*data = (*data)[RESERVED_VALUE];
		}
	} else {
		index_paths_namespace(doc, offsprings);
	}
}


std::vector<std::string>
Schema::get_prefixes_namespace(const std::vector<std::string>& paths_namespace)
{
	L_CALL(nullptr, "Schema::get_prefixes_namespace(%zu)", paths_namespace.size());

	if (paths_namespace.size() > NAMESPACE_LIMIT_DEPTH) {
		THROW(ClientError, "Namespace limit depth is %d, and the namespace provided has a depth of %zu", NAMESPACE_LIMIT_DEPTH, paths_namespace.size());
	}

	std::vector<std::string> prefixes;
	prefixes.reserve(std::pow(2, paths_namespace.size() - 2));
	auto it = paths_namespace.begin();
	prefixes.push_back(DOCUMENT_NAMESPACE_TERM_PREFIX + *it);
	auto it_e = paths_namespace.end() - 1;
	for (++it; it != it_e; ++it) {
		const auto size = prefixes.size();
		for (size_t i = 0; i < size; ++i) {
			std::string prefix;
			prefix.reserve(prefixes[i].length() + it->length());
			prefix.assign(prefixes[i]).append(*it);
			prefixes.push_back(std::move(prefix));
		}
	}

	for (auto& prefix : prefixes) {
		prefix.append(*it_e);
	}

	return prefixes;
}


std::vector<required_spc_t>
Schema::get_namespace_specifications() const
{
	L_CALL(this, "Schema::get_namespace_specifications()");

	auto prefixes_namespace = get_prefixes_namespace(specification.paths_namespace);

	std::vector<required_spc_t> data;
	data.reserve(prefixes_namespace.size());
	if (toUType(specification.index & TypeIndex::VALUES)) {
		for (auto& prefix_namespace : prefixes_namespace) {
			data.push_back(get_namespace_specification(specification.sep_types[2], prefix_namespace));
		}
	} else {
		for (auto& prefix_namespace : prefixes_namespace) {
			required_spc_t spc = specification_t::get_global(specification.sep_types[2]);
			spc.prefix.assign(prefix_namespace).push_back(spc.get_prefix());
			data.push_back(std::move(spc));
		}
	}

	return data;
}


required_spc_t
Schema::get_namespace_specification(FieldType namespace_type, std::string& prefix_namespace)
{
	L_CALL(nullptr, "Schema::get_namespace_specification('%c', %s)", toUType(namespace_type), repr(prefix_namespace).c_str());

	required_spc_t spc = specification_t::get_global(namespace_type);
	spc.prefix.assign(prefix_namespace).push_back(spc.get_prefix());
	spc.slot = get_slot(spc.prefix);

	switch (spc.sep_types[2]) {
		case FieldType::INTEGER:
		case FieldType::POSITIVE:
		case FieldType::FLOAT: {
			static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::INTEGER));
			spc.acc_prefix.clear();
			for (const auto& acc : spc.accuracy) {
				prefix_namespace.append(DB_OFFSPRING_UNION).append(acc_name_num(acc));
				spc.acc_prefix.push_back(prefix_type + Serialise::dynamic_namespace_field(prefix_namespace));
			}
			break;
		}
		case FieldType::DATE: {
			static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::DATE));
			spc.acc_prefix.clear();
			for (const auto& acc : spc.accuracy) {
				prefix_namespace.append(DB_OFFSPRING_UNION).append(acc_name_date(acc));
				spc.acc_prefix.push_back(Serialise::dynamic_namespace_field(prefix_namespace));
			}
			break;
		}
		case FieldType::GEO: {
			static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::GEO));
			spc.acc_prefix.clear();
			for (const auto& acc : spc.accuracy) {
				prefix_namespace.append(DB_OFFSPRING_UNION).append(acc_name_geo(acc));
				spc.acc_prefix.push_back(prefix_type + Serialise::dynamic_namespace_field(prefix_namespace));
			}
			break;
		}
		default:
			break;
	}

	return spc;
}


void
Schema::update_dynamic_specification()
{
	L_CALL(this, "Schema::update_dynamic_specification()");

	if (toUType(specification.index & TypeIndex::FIELD_TERMS)) {
		specification.prefix.assign(get_dynamic_prefix(specification.full_normalized_name, specification.get_prefix()));
	}

	if (toUType(specification.index & TypeIndex::FIELD_VALUES)) {
		specification.slot = get_slot(specification.full_normalized_name);
		switch (specification.sep_types[2]) {
			case FieldType::INTEGER:
			case FieldType::POSITIVE:
			case FieldType::FLOAT: {
				static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::INTEGER));
				for (const auto& acc : specification.accuracy) {
					auto acc_full_name = specification.full_normalized_name;
					acc_full_name.append(DB_OFFSPRING_UNION).append(acc_name_num(acc));
					specification.acc_prefix.push_back(prefix_type + Serialise::dynamic_namespace_field(acc_full_name));
				}
				break;
			}
			case FieldType::DATE: {
				static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::DATE));
				for (const auto& acc : specification.accuracy) {
					auto acc_full_name = specification.full_normalized_name;
					acc_full_name.append(DB_OFFSPRING_UNION).append(acc_name_date(acc));
					specification.acc_prefix.push_back(Serialise::dynamic_namespace_field(acc_full_name));
				}
				break;
			}
			case FieldType::GEO: {
				static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::GEO));
				for (const auto& acc : specification.accuracy) {
					auto acc_full_name = specification.full_normalized_name;
					acc_full_name.append(DB_OFFSPRING_UNION).append(acc_name_geo(acc));
					specification.acc_prefix.push_back(prefix_type + Serialise::dynamic_namespace_field(acc_full_name));
				}
				break;
			}
			default:
				break;
		}
	}
}


void
Schema::set_type_to_object(bool offsprings)
{
	L_CALL(this, "Schema::set_type_to_object(%d)", offsprings);

	if unlikely(offsprings && specification.sep_types[0] == FieldType::EMPTY && !specification.flags.inside_namespace) {
		auto& _types = get_mutable()[RESERVED_TYPE];
		if (_types.is_undefined()) {
			_types = MsgPack({ FieldType::OBJECT, FieldType::EMPTY, FieldType::EMPTY });
			specification.sep_types[0] = FieldType::OBJECT;
		} else {
			_types[0] = FieldType::OBJECT;
			specification.sep_types[0] = FieldType::OBJECT;
		}
	}
}


void
Schema::set_type_to_array()
{
	L_CALL(this, "Schema::set_type_to_array()");

	if unlikely(specification.sep_types[1] == FieldType::EMPTY && !specification.flags.inside_namespace) {
		auto& _types = get_mutable()[RESERVED_TYPE];
		if (_types.is_undefined()) {
			_types = MsgPack({ FieldType::EMPTY, FieldType::ARRAY, FieldType::EMPTY });
			specification.sep_types[1] = FieldType::ARRAY;
		} else {
			_types[1] = FieldType::ARRAY;
			specification.sep_types[1] = FieldType::ARRAY;
		}
	}
}


void
Schema::validate_required_data()
{
	L_CALL(this, "Schema::validate_required_data()");

	if (!specification.full_meta_name.empty()) {
		auto& properties = get_mutable();

		try {
			auto func = map_dispatch_set_default_spc.at(specification.full_normalized_name);
			(this->*func)(properties);
		} catch (const std::out_of_range&) { }

		// Process RESERVED_ACCURACY, RESERVED_ACC_PREFIX.
		std::set<uint64_t> set_acc;
		switch (specification.sep_types[2]) {
			case FieldType::GEO: {
				// Set partials and error.
				properties[RESERVED_PARTIALS] = static_cast<bool>(specification.flags.partials);
				properties[RESERVED_ERROR] = specification.error;

				if (specification.doc_acc) {
					try {
						for (const auto& _accuracy : *specification.doc_acc) {
							auto val_acc = _accuracy.as_u64();
							if (val_acc <= HTM_MAX_LEVEL) {
								set_acc.insert(val_acc);
							} else {
								THROW(ClientError, "Data inconsistency, level value in '%s': '%s' must be a positive number between 0 and %d (%llu not supported)", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL, val_acc);
							}
						}
					} catch (const msgpack::type_error&) {
						THROW(ClientError, "Data inconsistency, level value in '%s': '%s' must be a positive number between 0 and %d", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
					}
				} else {
					set_acc.insert(def_accuracy_geo.begin(), def_accuracy_geo.end());
				}
				// Process RESERVED_ACCURACY and RESERVED_ACC_PREFIX
				if (!specification.flags.dynamic_type && set_acc.size()) {
					if (specification.acc_prefix.empty()) {
						static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::GEO));
						for (const auto& acc : set_acc) {
							auto acc_full_name = specification.full_normalized_name;
							acc_full_name.append(DB_OFFSPRING_UNION).append(acc_name_geo(acc));
							specification.acc_prefix.push_back(prefix_type + Serialise::namespace_field(acc_full_name));
						}
					} else if (specification.acc_prefix.size() != set_acc.size()) {
						THROW(ClientError, "Data inconsistency, there must be a prefix for each unique value in %s", RESERVED_ACCURACY);
					}
					specification.accuracy.insert(specification.accuracy.end(), set_acc.begin(), set_acc.end());
					properties[RESERVED_ACCURACY]   = specification.accuracy;
					properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				}
				break;
			}
			case FieldType::DATE: {
				if (specification.doc_acc) {
					try {
						for (const auto& _accuracy : *specification.doc_acc) {
							auto str_accuracy(lower_string(_accuracy.as_string()));
							try {
								set_acc.insert(toUType(map_acc_date.at(str_accuracy)));
							} catch (const std::out_of_range&) {
								THROW(ClientError, "Data inconsistency, '%s': '%s' must be a subset of %s (%s not supported)", RESERVED_ACCURACY, DATE_STR, repr(str_set_acc_date).c_str(), repr(str_accuracy).c_str());
							}
						}
					} catch (const msgpack::type_error&) {
						THROW(ClientError, "Data inconsistency, '%s' in '%s' must be a subset of %s", RESERVED_ACCURACY, DATE_STR, repr(str_set_acc_date).c_str());
					}
				} else {
					set_acc.insert(def_accuracy_date.begin(), def_accuracy_date.end());
				}
				// Process RESERVED_ACCURACY and RESERVED_ACC_PREFIX
				if (!specification.flags.dynamic_type && set_acc.size()) {
					if (specification.acc_prefix.empty()) {
						static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::DATE));
						for (const auto& acc : set_acc) {
							auto acc_full_name = specification.full_normalized_name;
							acc_full_name.append(DB_OFFSPRING_UNION).append(acc_name_date(acc));
							specification.acc_prefix.push_back(prefix_type + Serialise::namespace_field(acc_full_name));
						}
					} else if (specification.acc_prefix.size() != set_acc.size()) {
						THROW(ClientError, "Data inconsistency, there must be a prefix for each unique value in %s", RESERVED_ACCURACY);
					}
					specification.accuracy.insert(specification.accuracy.end(), set_acc.begin(), set_acc.end());
					properties[RESERVED_ACCURACY]   = specification.accuracy;
					properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				}
				break;
			}
			case FieldType::INTEGER:
			case FieldType::POSITIVE:
			case FieldType::FLOAT: {
				if (specification.doc_acc) {
					try {
						for (const auto& _accuracy : *specification.doc_acc) {
							set_acc.insert(_accuracy.as_u64());
						}
					} catch (const msgpack::type_error&) {
						THROW(ClientError, "Data inconsistency, %s in %s must be an array of positive numbers", RESERVED_ACCURACY, Serialise::type(specification.sep_types[2]).c_str());
					}
				} else {
					set_acc.insert(def_accuracy_num.begin(), def_accuracy_num.end());
				}
				// Process RESERVED_ACCURACY and RESERVED_ACC_PREFIX
				if (!specification.flags.dynamic_type && set_acc.size()) {
					if (specification.acc_prefix.empty()) {
						static std::string prefix_type = DOCUMENT_ACCURACY_TERM_PREFIX + std::string(1, toUType(FieldType::INTEGER));
						for (const auto& acc : set_acc) {
							auto acc_full_name = specification.full_normalized_name;
							acc_full_name.append(DB_OFFSPRING_UNION).append(acc_name_num(acc));
							specification.acc_prefix.push_back(prefix_type + Serialise::namespace_field(acc_full_name));
						}
					} else if (specification.acc_prefix.size() != set_acc.size()) {
						THROW(ClientError, "Data inconsistency, there must be a prefix for each unique value in %s", RESERVED_ACCURACY);
					}
					specification.accuracy.insert(specification.accuracy.end(), set_acc.begin(), set_acc.end());
					properties[RESERVED_ACCURACY]   = specification.accuracy;
					properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
				}
				break;
			}
			case FieldType::TEXT: {
				if (!specification.flags.has_index) {
					auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
					if (specification.index != index) {
						specification.index = index;
						properties[RESERVED_INDEX] = specification.index;
					}
				}

				properties[RESERVED_LANGUAGE] = specification.language;

				properties[RESERVED_STOP_STRATEGY] = specification.stop_strategy;

				properties[RESERVED_STEM_STRATEGY] = specification.stem_strategy;
				if (specification.aux_stem_lan.empty() && !specification.aux_lan.empty()) {
					specification.stem_language = specification.aux_lan;
				}
				properties[RESERVED_STEM_LANGUAGE] = specification.stem_language;

				if (specification.aux_lan.empty() && !specification.aux_stem_lan.empty()) {
					specification.language = specification.aux_stem_lan;
				}
				break;
			}
			case FieldType::STRING: {
				if (!specification.flags.has_index) {
					auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
					if (specification.index != index) {
						specification.index = index;
						properties[RESERVED_INDEX] = specification.index;
					}
				}

				properties[RESERVED_LANGUAGE] = specification.language;
				break;
			}
			case FieldType::TERM: {
				if (!specification.flags.has_index) {
					auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
					if (specification.index != index) {
						specification.index = index;
						properties[RESERVED_INDEX] = specification.index;
					}
				}

				// Process RESERVED_BOOL_TERM
				if (!specification.flags.has_bool_term) {
					// By default, if normalized name has upper characters then it is consider bool term.
					specification.flags.bool_term = strhasupper(specification.normalized_name);
				}
				properties[RESERVED_BOOL_TERM] = static_cast<bool>(specification.flags.bool_term);
				break;
			}
			case FieldType::BOOLEAN:
			case FieldType::UUID:
				break;
			default:
				THROW(ClientError, "%s '%c' is not supported", RESERVED_TYPE, specification.sep_types[2]);
		}

		if (!specification.flags.has_index && !specification.paths_namespace.empty()) {
			auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
			if (specification.index != index) {
				specification.index = index;
				properties[RESERVED_INDEX] = specification.index;
			}
		}

		if (specification.flags.dynamic_type) {
			if (!specification.flags.has_index) {
				auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
				if (specification.index != index) {
					specification.index = index;
					properties[RESERVED_INDEX] = specification.index;
				}
			}

			if (set_acc.size()) {
				specification.accuracy.insert(specification.accuracy.end(), set_acc.begin(), set_acc.end());
				properties[RESERVED_ACCURACY] = specification.accuracy;
			}
			update_dynamic_specification();
		} else {
			// Process RESERVED_PREFIX
			if (specification.prefix.empty()) {
				specification.prefix = get_prefix(specification.full_normalized_name, specification.get_prefix());
			}
			properties[RESERVED_PREFIX] = specification.prefix;

			// Process RESERVED_SLOT
			if (specification.slot == Xapian::BAD_VALUENO) {
				specification.slot = get_slot(specification.full_normalized_name);
			} else if (specification.slot < DB_SLOT_RESERVED && !specification.flags.reserved_slot) {
				specification.slot += DB_SLOT_RESERVED;
			}
			properties[RESERVED_SLOT] = specification.slot;
		}

		// Process RESERVED_TYPE
		properties[RESERVED_TYPE] = specification.sep_types;

		specification.flags.field_with_type = true;

		// L_DEBUG(this, "\nspecification = %s\nproperties = %s", specification.to_string().c_str(), properties.to_string(true).c_str());
	}
}


void
Schema::validate_required_namespace_data(const MsgPack& value)
{
	L_CALL(this, "Schema::validate_required_namespace_data(%s)", repr(value.to_string()).c_str());

	if (specification.sep_types[2] == FieldType::EMPTY) {
		guess_field_type(value);
	}

	if (!specification.full_meta_name.empty()) {
		switch (specification.sep_types[2]) {
			case FieldType::GEO:
				// Set partials and error.
				specification.flags.partials = default_spc.flags.partials;
				specification.error = default_spc.error;
				break;

			case FieldType::TEXT:
				if (!specification.flags.has_index) {
					specification.index &= ~TypeIndex::VALUES; // Fallback to index anything but values
				}

				specification.language = default_spc.language;

				specification.stop_strategy = default_spc.stop_strategy;

				specification.stem_strategy = default_spc.stem_strategy;
				specification.stem_language = default_spc.stem_language;
				break;

			case FieldType::STRING:
				if (!specification.flags.has_index) {
					specification.index &= ~TypeIndex::VALUES; // Fallback to index anything but values
				}

				specification.language = default_spc.language;
				break;

			case FieldType::TERM:
				if (!specification.flags.has_index) {
					specification.index &= ~TypeIndex::VALUES; // Fallback to index anything but values
				}

				specification.flags.bool_term = strhasupper(specification.normalized_name);
				break;

			case FieldType::DATE:
			case FieldType::INTEGER:
			case FieldType::POSITIVE:
			case FieldType::FLOAT:
			case FieldType::BOOLEAN:
			case FieldType::UUID:
				break;

			default:
				THROW(ClientError, "%s '%c' is not supported", RESERVED_TYPE, specification.sep_types[2]);
		}

		specification.flags.field_with_type = true;
	}
}


void
Schema::validate_required_data(const MsgPack& value)
{
	L_CALL(this, "Schema::validate_required_data(%s)", repr(value.to_string()).c_str());

	L_SCHEMA(this, "Specification: %s", specification.to_string().c_str());  // Print specification as sent by user

	if (specification.sep_types[2] == FieldType::EMPTY) {
		if (XapiandManager::manager->strict || specification.flags.strict) {
			THROW(MissingTypeError, "Type of field %s is missing", repr(specification.full_normalized_name).c_str());
		}
		guess_field_type(value);
	}

	validate_required_data();
}


void
Schema::guess_field_type(const MsgPack& item_doc)
{
	L_CALL(this, "Schema::guess_field_type(%s)", repr(item_doc.to_string()).c_str());

	const auto& field = item_doc.is_array() ? item_doc.at(0) : item_doc;
	switch (field.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			if (specification.flags.numeric_detection) {
				specification.sep_types[2] = FieldType::POSITIVE;
				return;
			}
			break;
		case MsgPack::Type::NEGATIVE_INTEGER:
			if (specification.flags.numeric_detection) {
				specification.sep_types[2] = FieldType::INTEGER;
				return;
			}
			break;
		case MsgPack::Type::FLOAT:
			if (specification.flags.numeric_detection) {
				specification.sep_types[2] = FieldType::FLOAT;
				return;
			}
			break;
		case MsgPack::Type::BOOLEAN:
			if (specification.flags.bool_detection) {
				specification.sep_types[2] = FieldType::BOOLEAN;
				return;
			}
			break;
		case MsgPack::Type::STR: {
			auto str_value = field.as_string();
			if (specification.flags.date_detection && Datetime::isDate(str_value)) {
				specification.sep_types[2] = FieldType::DATE;
				return;
			}
			if (specification.flags.geo_detection && EWKT_Parser::isEWKT(str_value)) {
				specification.sep_types[2] = FieldType::GEO;
				return;
			}
			if (specification.flags.uuid_detection && Serialise::isUUID(str_value)) {
				specification.sep_types[2] = FieldType::UUID;
				return;
			}
			if (specification.flags.text_detection && (!specification.flags.string_detection || Serialise::isText(str_value, specification.flags.bool_term))) {
				specification.sep_types[2] = FieldType::TEXT;
				return;
			}
			if (specification.flags.string_detection) {
				specification.sep_types[2] = FieldType::STRING;
				return;
			}
			if (specification.flags.bool_detection) {
				try {
					Serialise::boolean(str_value);
					specification.sep_types[2] = FieldType::BOOLEAN;
					return;
				} catch (const SerialisationError&) { }
			}
			break;
		}
		case MsgPack::Type::ARRAY:
			THROW(ClientError, "'%s' can not be array of arrays", RESERVED_VALUE);
		case MsgPack::Type::MAP:
			if (item_doc.size() == 1) {
				auto cast_word = item_doc.begin()->as_string();
				try {
					specification.sep_types[2] = Cast::getType(cast_word);
					return;
				} catch (const std::out_of_range&) {
					THROW(ClientError, "Unknown cast type %s", cast_word.c_str());
				}
			}
			THROW(ClientError, "Expected map with one element");
		default:
			break;
	}

	THROW(ClientError, "'%s': %s is ambiguous", RESERVED_VALUE, repr(item_doc.to_string()).c_str());
}


void
Schema::index_item(Xapian::Document& doc, const MsgPack& value, MsgPack& data, size_t pos, bool add_value)
{
	L_CALL(this, "Schema::index_item(<doc>, %s, %s, %zu, %s)", repr(value.to_string()).c_str(), repr(data.to_string()).c_str(), pos, add_value ? "true" : "false");

	_index_item(doc, std::array<std::reference_wrapper<const MsgPack>, 1>({{ value }}), pos);

	if (specification.flags.store && add_value) {
		// Add value to data.
		auto& data_value = data[RESERVED_VALUE];
		switch (data_value.getType()) {
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
	}
}


void
Schema::index_item(Xapian::Document& doc, const MsgPack& values, MsgPack& data, bool add_values)
{
	L_CALL(this, "Schema::index_item(<doc>, %s, %s, %s)", repr(values.to_string()).c_str(), repr(data.to_string()).c_str(), add_values ? "true" : "false");

	if (values.is_array()) {
		set_type_to_array();

		_index_item(doc, values, 0);

		if (specification.flags.store && add_values) {
			// Add value to data.
			auto& data_value = data[RESERVED_VALUE];
			switch (data_value.getType()) {
				case MsgPack::Type::UNDEFINED:
					data_value = values;
					break;
				case MsgPack::Type::ARRAY:
					for (const auto& value : values) {
						data_value.push_back(value);
					}
					break;
				default:
					data_value = MsgPack({ data_value });
					for (const auto& value : values) {
						data_value.push_back(value);
					}
					break;
			}
		}
	} else {
		index_item(doc, values, data, 0, add_values);
	}
}


void
Schema::index_paths_namespace(Xapian::Document& doc, bool offsprings)
{
	if (specification.flags.inside_namespace && !offsprings) {
		auto prefixes_namespace = get_prefixes_namespace(specification.paths_namespace);
		for (const auto& prefix_namespace : prefixes_namespace) {
			doc.add_term(prefix_namespace);
		}
	} else if (!specification.flags.has_index && !specification.paths_namespace.empty()) {
		auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
		if (specification.index != index) {
			specification.index = index;
			get_mutable()[RESERVED_INDEX] = specification.index;
		}
	}
}


template <typename T>
inline void
Schema::_index_item(Xapian::Document& doc, T&& values, size_t pos)
{
	L_CALL(this, "Schema::_index_item(<doc>, <values>, %zu)", pos);

	switch (specification.index) {
		case TypeIndex::NONE: {
			return;
		}
		case TypeIndex::FIELD_TERMS: {
			for (const MsgPack& value : values) {
				index_term(doc, Serialise::MsgPack(specification, value), specification, pos++);
			}
			break;
		}
		case TypeIndex::FIELD_VALUES: {
			StringSet& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				index_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, specification, pos++);
			}
			break;
		}
		case TypeIndex::FIELD_ALL: {
			StringSet& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				index_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, specification, pos++, &specification);
			}
			break;
		}
		case TypeIndex::GLOBAL_TERMS: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			for (const MsgPack& value : values) {
				index_term(doc, Serialise::MsgPack(global_spc, value), global_spc, pos++);
			}
			break;
		}
		case TypeIndex::TERMS: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			for (const MsgPack& value : values) {
				index_all_term(doc, value, specification, global_spc, pos++);
			}
			break;
		}
		case TypeIndex::GLOBAL_TERMS_FIELD_VALUES: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				index_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, specification, pos++, nullptr, &global_spc);
			}
			break;
		}
		case TypeIndex::GLOBAL_TERMS_FIELD_ALL: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				index_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, specification, pos++, &specification, &global_spc);
			}
			break;
		}
		case TypeIndex::GLOBAL_VALUES: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				index_value(doc, value.is_map() ? Cast::cast(value) : value, s_g, global_spc, pos++);
			}
			break;
		}
		case TypeIndex::GLOBAL_VALUES_FIELD_TERMS: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				index_value(doc, value.is_map() ? Cast::cast(value) : value, s_g, global_spc, pos++, &specification);
			}
			break;
		}
		case TypeIndex::VALUES: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_g = map_values[global_spc.slot];
			StringSet& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				index_all_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, s_g, specification, global_spc, pos++);
			}
			break;
		}
		case TypeIndex::GLOBAL_VALUES_FIELD_ALL: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_g = map_values[global_spc.slot];
			StringSet& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				index_all_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, s_g, specification, global_spc, pos++);
			}
			break;
		}
		case TypeIndex::GLOBAL_ALL: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				index_value(doc, value.is_map() ? Cast::cast(value) : value, s_g, global_spc, pos++, nullptr, &global_spc);
			}
			break;
		}
		case TypeIndex::GLOBAL_ALL_FIELD_TERMS: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				index_value(doc, value.is_map() ? Cast::cast(value) : value, s_g, global_spc, pos++, &specification, &global_spc);
			}
			break;
		}
		case TypeIndex::GLOBAL_ALL_FIELD_VALUES: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_g = map_values[global_spc.slot];
			StringSet& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				index_all_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, s_g, specification, global_spc, pos++);
			}
			break;
		}
		case TypeIndex::ALL: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[2]);
			StringSet& s_f = map_values[specification.slot];
			StringSet& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				index_all_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, s_g, specification, global_spc, pos++);
			}
			break;
		}
	}
}


void
Schema::index_term(Xapian::Document& doc, std::string serialise_val, const specification_t& field_spc, size_t pos)
{
	L_CALL(nullptr, "Schema::index_term(%s)", repr(serialise_val).c_str());

	if (serialise_val.empty()) {
		return;
	}

	switch (field_spc.sep_types[2]) {
		case FieldType::TEXT: {
			Xapian::TermGenerator term_generator;
			term_generator.set_document(doc);
			const auto& stopper = getStopper(field_spc.language);
			term_generator.set_stopper(stopper.get());
			term_generator.set_stopper_strategy(getGeneratorStopStrategy(field_spc.stop_strategy));
			term_generator.set_stemmer(Xapian::Stem(field_spc.stem_language));
			term_generator.set_stemming_strategy(getGeneratorStemStrategy(field_spc.stem_strategy));
			// Xapian::WritableDatabase *wdb = nullptr;
			// bool spelling = field_spc.spelling[getPos(pos, field_spc.spelling.size())];
			// if (spelling) {
			// 	wdb = static_cast<Xapian::WritableDatabase *>(database->db.get());
			// 	term_generator.set_database(*wdb);
			// 	term_generator.set_flags(Xapian::TermGenerator::FLAG_SPELLING);
			// }
			bool positions = field_spc.positions[getPos(pos, field_spc.positions.size())];
			if (positions) {
				term_generator.index_text(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix);
			} else {
				term_generator.index_text_without_positions(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix);
			}
			L_INDEX(nullptr, "Field Text to Index [%d] => %s:%s [Positions: %s]", pos, field_spc.prefix.c_str(), serialise_val.c_str(), positions ? "true" : "false");
			break;
		}

		case FieldType::STRING: {
			Xapian::TermGenerator term_generator;
			term_generator.set_document(doc);
			auto position = field_spc.position[getPos(pos, field_spc.position.size())]; // String uses position (not positions) which is off by default
			if (position) {
				term_generator.index_text(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix);
				L_INDEX(nullptr, "Field String to Index [%d] => %s:%s [Positions: %s]", pos, field_spc.prefix.c_str(), serialise_val.c_str(), position ? "true" : "false");
			} else {
				term_generator.index_text_without_positions(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix);
				L_INDEX(nullptr, "Field String to Index [%d] => %s:%s", pos, field_spc.prefix.c_str(), serialise_val.c_str());
			}
			break;
		}

		case FieldType::TERM:
			if (!field_spc.flags.bool_term) {
				to_lower(serialise_val);
			}

		default: {
			serialise_val = prefixed(serialise_val, field_spc.prefix);
			auto weight = field_spc.flags.bool_term ? 0 : field_spc.weight[getPos(pos, field_spc.weight.size())];
			auto position = field_spc.position[getPos(pos, field_spc.position.size())];
			if (position) {
				doc.add_posting(serialise_val, position, weight);
			} else {
				doc.add_term(serialise_val, weight);
			}
			L_INDEX(nullptr, "Field Term [%d] -> %s  Bool: %d  Posting: %d", pos, repr(serialise_val).c_str(), field_spc.flags.bool_term, position);
			break;
		}
	}
}


void
Schema::index_all_term(Xapian::Document& doc, const MsgPack& value, const specification_t& field_spc, const specification_t& global_spc, size_t pos)
{
	L_CALL(nullptr, "Schema::index_all_term()");

	if (field_spc.sep_types[2] == FieldType::GEO && (field_spc.flags.partials != global_spc.flags.partials ||
		field_spc.error != global_spc.error)) {
		index_term(doc, Serialise::MsgPack(field_spc, value), field_spc, pos);
		index_term(doc, Serialise::MsgPack(global_spc, value), global_spc, pos);
	} else {
		auto serialise_val = Serialise::MsgPack(field_spc, value);
		index_term(doc, serialise_val, field_spc, pos);
		index_term(doc, serialise_val, global_spc, pos);
	}
}


void
Schema::index_value(Xapian::Document& doc, const MsgPack& value, StringSet& s, const specification_t& spc, size_t pos, const specification_t* field_spc, const specification_t* global_spc)
{
	L_CALL(nullptr, "Schema::index_value()");

	switch (spc.sep_types[2]) {
		case FieldType::FLOAT: {
			try {
				auto f_val = value.as_f64();
				auto ser_value = Serialise::_float(f_val);
				if (field_spc) {
					index_term(doc, ser_value, *field_spc, pos);
				}
				if (global_spc) {
					index_term(doc, ser_value, *global_spc, pos);
				}
				s.insert(std::move(ser_value));
				GenerateTerms::integer(doc, spc.accuracy, spc.acc_prefix, static_cast<int64_t>(f_val));
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for float type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::INTEGER: {
			try {
				auto i_val = value.as_i64();
				auto ser_value = Serialise::integer(i_val);
				if (field_spc) {
					index_term(doc, ser_value, *field_spc, pos);
				}
				if (global_spc) {
					index_term(doc, ser_value, *global_spc, pos);
				}
				s.insert(std::move(ser_value));
				GenerateTerms::integer(doc, spc.accuracy, spc.acc_prefix, i_val);
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for integer type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::POSITIVE: {
			try {
				auto u_val = value.as_u64();
				auto ser_value = Serialise::positive(u_val);
				if (field_spc) {
					index_term(doc, ser_value, *field_spc, pos);
				}
				if (global_spc) {
					index_term(doc, ser_value, *global_spc, pos);
				}
				s.insert(std::move(ser_value));
				GenerateTerms::positive(doc, spc.accuracy, spc.acc_prefix, u_val);
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for positive type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::DATE: {
			try {
				Datetime::tm_t tm;
				auto ser_value = Serialise::date(value, tm);
				if (field_spc) {
					index_term(doc, ser_value, *field_spc, pos);
				}
				if (global_spc) {
					index_term(doc, ser_value, *global_spc, pos);
				}
				s.insert(std::move(ser_value));
				GenerateTerms::date(doc, spc.accuracy, spc.acc_prefix, tm);
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for date type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::GEO: {
			try {
				EWKT_Parser ewkt(value.as_string(), spc.flags.partials, spc.error);
				auto ser_value = Serialise::trixels(ewkt.trixels);
				if (field_spc) {
					if (field_spc->flags.partials != spc.flags.partials || field_spc->error != spc.error) {
						EWKT_Parser f_ewkt(value.as_string(), field_spc->flags.partials, field_spc->error);
						index_term(doc, Serialise::trixels(f_ewkt.trixels), *field_spc, pos);
					} else {
						index_term(doc, ser_value, *field_spc, pos);
					}
				}
				if (global_spc) {
					if (global_spc->flags.partials != spc.flags.partials || global_spc->error != spc.error) {
						EWKT_Parser g_ewkt(value.as_string(), global_spc->flags.partials, global_spc->error);
						index_term(doc, Serialise::trixels(g_ewkt.trixels), *global_spc, pos);
					} else {
						index_term(doc, std::move(ser_value), *global_spc, pos);
					}
				}
				auto ranges = ewkt.getRanges();
				s.insert(Serialise::geo(ranges, ewkt.centroids));
				GenerateTerms::geo(doc, spc.accuracy, spc.acc_prefix, ranges);
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for geo type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING: {
			try {
				auto ser_value = value.as_string();
				if (field_spc) {
					index_term(doc, ser_value, *field_spc, pos);
				}
				if (global_spc) {
					index_term(doc, ser_value, *global_spc, pos);
				}
				s.insert(std::move(ser_value));
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for %s type: %s", Serialise::type(spc.sep_types[2]).c_str(), repr(value.to_string()).c_str());
			}
		}
		case FieldType::BOOLEAN: {
			try {
				auto ser_value = Serialise::MsgPack(spc, value);
				if (field_spc) {
					index_term(doc, ser_value, *field_spc, pos);
				}
				if (global_spc) {
					index_term(doc, ser_value, *global_spc, pos);
				}
				s.insert(std::move(ser_value));
				return;
			} catch (const SerialisationError&) {
				THROW(ClientError, "Format invalid for boolean type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::UUID: {
			try {
				auto ser_value = Serialise::uuid(value.as_string());
				if (field_spc) {
					index_term(doc, ser_value, *field_spc, pos);
				}
				if (global_spc) {
					index_term(doc, ser_value, *global_spc, pos);
				}
				s.insert(std::move(ser_value));
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for uuid type: %s", repr(value.to_string()).c_str());
			} catch (const SerialisationError&) {
				THROW(ClientError, "Format invalid for uuid type: %s", repr(value.to_string()).c_str());
			}
		}
		default:
			THROW(ClientError, "Type: '%c' is an unknown type", spc.sep_types[2]);
	}
}


void
Schema::index_all_value(Xapian::Document& doc, const MsgPack& value, StringSet& s_f, StringSet& s_g, const specification_t& field_spc, const specification_t& global_spc, size_t pos)
{
	L_CALL(nullptr, "Schema::index_all_value()");

	switch (field_spc.sep_types[2]) {
		case FieldType::FLOAT: {
			try {
				auto f_val = value.as_f64();
				auto ser_value = Serialise::_float(f_val);
				if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
					index_term(doc, ser_value, field_spc, pos);
				}
				if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
					index_term(doc, ser_value, global_spc, pos);
				}
				s_f.insert(ser_value);
				s_g.insert(std::move(ser_value));
				if (field_spc.accuracy == global_spc.accuracy) {
					GenerateTerms::integer(doc, field_spc.accuracy, field_spc.acc_prefix, global_spc.acc_prefix, static_cast<int64_t>(f_val));
				} else {
					GenerateTerms::integer(doc, field_spc.accuracy, field_spc.acc_prefix, static_cast<int64_t>(f_val));
					GenerateTerms::integer(doc, global_spc.accuracy, global_spc.acc_prefix, static_cast<int64_t>(f_val));
				}
				break;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for float type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::INTEGER: {
			try {
				auto i_val = value.as_i64();
				auto ser_value = Serialise::integer(i_val);
				if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
					index_term(doc, ser_value, field_spc, pos);
				}
				if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
					index_term(doc, ser_value, global_spc, pos);
				}
				s_f.insert(ser_value);
				s_g.insert(std::move(ser_value));
				if (field_spc.accuracy == global_spc.accuracy) {
					GenerateTerms::integer(doc, field_spc.accuracy, field_spc.acc_prefix, global_spc.acc_prefix, i_val);
				} else {
					GenerateTerms::integer(doc, field_spc.accuracy, field_spc.acc_prefix, i_val);
					GenerateTerms::integer(doc, global_spc.accuracy, global_spc.acc_prefix, i_val);
				}
				break;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for integer type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::POSITIVE: {
			try {
				auto u_val = value.as_u64();
				auto ser_value = Serialise::positive(u_val);
				if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
					index_term(doc, ser_value, field_spc, pos);
				}
				if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
					index_term(doc, ser_value, global_spc, pos);
				}
				s_f.insert(ser_value);
				s_g.insert(std::move(ser_value));
				if (field_spc.accuracy == global_spc.accuracy) {
					GenerateTerms::positive(doc, field_spc.accuracy, field_spc.acc_prefix, global_spc.acc_prefix, u_val);
				} else {
					GenerateTerms::positive(doc, field_spc.accuracy, field_spc.acc_prefix, u_val);
					GenerateTerms::positive(doc, global_spc.accuracy, global_spc.acc_prefix, u_val);
				}
				break;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for positive type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::DATE: {
			try {
				Datetime::tm_t tm;
				auto ser_value = Serialise::date(value, tm);
				if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
					index_term(doc, ser_value, field_spc, pos);
				}
				if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
					index_term(doc, ser_value, global_spc, pos);
				}
				s_f.insert(ser_value);
				s_g.insert(std::move(ser_value));
				if (field_spc.accuracy == global_spc.accuracy) {
					GenerateTerms::date(doc, field_spc.accuracy, field_spc.acc_prefix, global_spc.acc_prefix, tm);
				} else {
					GenerateTerms::date(doc, field_spc.accuracy, field_spc.acc_prefix, tm);
					GenerateTerms::date(doc, global_spc.accuracy, global_spc.acc_prefix, tm);
				}
				break;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for date type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::GEO: {
			try {
				auto str_ewkt = value.as_string();
				if (field_spc.flags.partials == global_spc.flags.partials && field_spc.error == global_spc.error) {
					EWKT_Parser ewkt(str_ewkt, field_spc.flags.partials, field_spc.error);
					if (toUType(field_spc.index & TypeIndex::TERMS)) {
						auto ser_value = Serialise::trixels(ewkt.trixels);
						if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
							index_term(doc, ser_value, field_spc, pos);
						}
						if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
							index_term(doc, std::move(ser_value), global_spc, pos);
						}
					}
					auto ranges = ewkt.getRanges();
					auto ser_ranges = Serialise::geo(ranges, ewkt.centroids);
					s_f.insert(ser_ranges);
					s_g.insert(std::move(ser_ranges));
					if (field_spc.accuracy == global_spc.accuracy) {
						GenerateTerms::geo(doc, field_spc.accuracy, field_spc.acc_prefix, global_spc.acc_prefix, ranges);
					} else {
						GenerateTerms::geo(doc, field_spc.accuracy, field_spc.acc_prefix, ranges);
						GenerateTerms::geo(doc, global_spc.accuracy, global_spc.acc_prefix, ranges);
					}
				} else {
					EWKT_Parser ewkt(str_ewkt, field_spc.flags.partials, field_spc.error);
					EWKT_Parser g_ewkt(str_ewkt, global_spc.flags.partials, global_spc.error);
					if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
						index_term(doc, Serialise::trixels(ewkt.trixels), field_spc, pos);
					}
					if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
						index_term(doc, Serialise::trixels(g_ewkt.trixels), global_spc, pos);
					}
					auto ranges = ewkt.getRanges();
					auto g_ranges = g_ewkt.getRanges();
					s_f.insert(Serialise::geo(ranges, ewkt.centroids));
					s_g.insert(Serialise::geo(g_ranges, g_ewkt.centroids));
					GenerateTerms::geo(doc, field_spc.accuracy, field_spc.acc_prefix, ranges);
					GenerateTerms::geo(doc, global_spc.accuracy, global_spc.acc_prefix, g_ranges);
				}
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for geo type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING: {
			try {
				auto ser_value = value.as_string();
				if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
					index_term(doc, ser_value, field_spc, pos);
				}
				if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
					index_term(doc, ser_value, global_spc, pos);
				}
				s_f.insert(ser_value);
				s_g.insert(std::move(ser_value));
				break;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for %s type: %s", Serialise::type(field_spc.sep_types[2]).c_str(), repr(value.to_string()).c_str());
			}
		}
		case FieldType::BOOLEAN: {
			try {
				auto ser_value = Serialise::MsgPack(field_spc, value);
				if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
					index_term(doc, ser_value, field_spc, pos);
				}
				if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
					index_term(doc, ser_value, global_spc, pos);
				}
				s_f.insert(ser_value);
				s_g.insert(std::move(ser_value));
				break;
			} catch (const SerialisationError&) {
				THROW(ClientError, "Format invalid for boolean type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::UUID: {
			try {
				auto ser_value = Serialise::uuid(value.as_string());
				if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
					index_term(doc, ser_value, field_spc, pos);
				}
				if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
					index_term(doc, ser_value, global_spc, pos);
				}
				s_f.insert(ser_value);
				s_g.insert(std::move(ser_value));
				break;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for uuid type: %s", repr(value.to_string()).c_str());
			}
		}
		default:
			THROW(ClientError, "Type: '%c' is an unknown type", field_spc.sep_types[2]);
	}
}


void
Schema::update_schema(const MsgPack*& parent_properties, const MsgPack& obj_schema, const std::string& name)
{
	L_CALL(this, "Schema::update_schema(%s)", repr(name).c_str());

	const auto spc_start = specification;
	if (obj_schema.is_map()) {
		specification.name.assign(name);
		const MsgPack* properties = &get_schema_subproperties(*parent_properties);
		bool offsprings = false;
		TaskVector tasks;
		tasks.reserve(obj_schema.size());
		for (const auto& item_key : obj_schema) {
			const auto str_key = item_key.as_string();
			try {
				auto func = map_dispatch_document.at(str_key);
				(this->*func)(str_key, obj_schema.at(str_key));
			} catch (const std::out_of_range&) {
				if (is_valid(str_key)) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::update_schema, this, std::ref(properties), std::ref(obj_schema.at(str_key)), std::move(str_key)));
					offsprings = true;
				}
			}
		}

		if (!specification.name.empty()) {
			properties = &get_schema_subproperties(*properties);
		}

		if (!specification.flags.field_with_type && specification.sep_types[2] != FieldType::EMPTY) {
			validate_required_data();
		}

		if (offsprings && specification.flags.inside_namespace) {
			THROW(ClientError, "An namespace object can not have children in Schema");
		}
		set_type_to_object(offsprings);

		const auto spc_object = std::move(specification);
		for (auto& task : tasks) {
			specification = spc_object;
			task.get();
		}
	} else {
		THROW(ClientError, "Schema must be an object of objects");
	}

	specification = std::move(spc_start);
}


const MsgPack&
Schema::get_schema_subproperties(const MsgPack& properties)
{
	L_CALL(this, "Schema::get_schema_subproperties(%s)", repr(properties.to_string()).c_str());

	std::vector<std::string> field_names;
	stringTokenizer(specification.name, DB_OFFSPRING_UNION, field_names);

	const MsgPack* subproperties = &properties;
	const auto it_e = field_names.end();

	static const auto dsit_e = map_dispatch_set_default_spc.end();
	for (auto it = field_names.begin(); it != it_e; ++it) {
		const auto& field_name = *it;
		if (!is_valid(field_name) && specification.full_normalized_name.empty() && map_dispatch_set_default_spc.find(field_name) == dsit_e) {
			THROW(ClientError, "The field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(field_name).c_str());
		}
		restart_specification();
		try {
			get_subproperties(subproperties, field_name, field_name);
		} catch (const std::out_of_range&) {
			MsgPack* mut_subprop = &get_mutable();
			for ( ; it != it_e; ++it) {
				specification.meta_name = *it;
				specification.normalized_name = specification.meta_name;
				specification.flags.dynamic_type = (specification.meta_name == UUID_FIELD_NAME);
				add_field(mut_subprop);
			}

			// Found field always false for adding inheritable specification.
			specification.flags.field_found = false;

			return *mut_subprop;
		}
	}

	// Found field always false for adding inheritable specification.
	specification.flags.field_found = false;

	return *subproperties;
}


void
Schema::get_subproperties(const MsgPack*& properties, const std::string& meta_name, const std::string& normalized_name)
{
	L_CALL(this, "Schema::get_subproperties(%s, %s, %s)", repr(properties->to_string()).c_str(), repr(meta_name).c_str(), repr(normalized_name).c_str());

	properties = &properties->at(meta_name);
	specification.flags.field_found = true;
	try {
		auto data_lan = map_stem_language.at(normalized_name);
		if (data_lan.first) {
			specification.language = data_lan.second;
			specification.aux_lan = data_lan.second;
		}
	} catch (const std::out_of_range&) { }

	if (specification.full_meta_name.empty()) {
		specification.full_meta_name.assign(meta_name);
		specification.full_normalized_name.assign(normalized_name);
	} else {
		specification.full_meta_name.append(DB_OFFSPRING_UNION).append(meta_name);
		specification.full_normalized_name.append(DB_OFFSPRING_UNION).append(normalized_name);
	}

	update_specification(*properties);
}


const MsgPack&
Schema::get_subproperties(const MsgPack& properties)
{
	L_CALL(this, "Schema::get_subproperties(%s)", repr(properties.to_string()).c_str());

	std::vector<std::string> field_names;
	stringTokenizer(specification.name, DB_OFFSPRING_UNION, field_names);

	const MsgPack* subproperties = &properties;
	const auto it_e = field_names.end();

	if (specification.paths_namespace.empty()) {
		static const auto dsit_e = map_dispatch_set_default_spc.end();
		for (auto it = field_names.begin(); it != it_e; ++it) {
			const auto& field_name = *it;
			if ((!is_valid(field_name) || field_name == UUID_FIELD_NAME) && specification.full_normalized_name.empty() && map_dispatch_set_default_spc.find(field_name) == dsit_e) {
				THROW(ClientError, "The field name: %s (%s) is not valid or reserved", repr(specification.name).c_str(), repr(field_name).c_str());
			}
			restart_specification();
			try {
				get_subproperties(subproperties, field_name, field_name);
			} catch (const std::out_of_range&) {
				detect_dynamic(field_name);
				if (specification.flags.dynamic_type) {
					try {
						get_subproperties(subproperties, specification.meta_name, specification.normalized_name);
						continue;
					} catch (const std::out_of_range&) { }
				}

				specification.flags.field_found = false;
				MsgPack* mut_subprop = &get_mutable();
				add_field(mut_subprop);
				for (++it; it != it_e; ++it) {
					detect_dynamic(*it);
					add_field(mut_subprop);
				}
				return *mut_subprop;
			}
		}
	} else {
		specification.flags.field_found = false;
		restart_specification();
		for (const auto& field_name : field_names) {
			detect_dynamic(field_name);
			specification.paths_namespace.push_back(Serialise::dynamic_namespace_field(specification.normalized_name));
		}
		specification.flags.inside_namespace = true;
	}

	return *subproperties;
}


void
Schema::detect_dynamic(const std::string& field_name)
{
	L_CALL(this, "Schema::detect_dynamic(%s)", repr(field_name).c_str());

	if (Serialise::isUUID(field_name)) {
		specification.normalized_name.assign(lower_string(field_name));
		specification.meta_name.assign(UUID_FIELD_NAME);
		specification.flags.dynamic_type = true;
	} else {
		specification.normalized_name.assign(field_name);
		specification.meta_name.assign(field_name);
	}
}


void
Schema::add_field(MsgPack*& properties)
{
	L_CALL(this, "Schema::add_field(%s)", repr(properties->to_string()).c_str());

	properties = &(*properties)[specification.meta_name];

	try {
		auto data_lan = map_stem_language.at(specification.normalized_name);
		if (data_lan.first) {
			specification.language = data_lan.second;
			specification.aux_lan = data_lan.second;
		}
	} catch (const std::out_of_range) { }

	if (specification.full_meta_name.empty()) {
		specification.full_meta_name.assign(specification.meta_name);
		specification.full_normalized_name.assign(specification.normalized_name);
	} else {
		specification.full_meta_name.append(DB_OFFSPRING_UNION).append(specification.meta_name);
		specification.full_normalized_name.append(DB_OFFSPRING_UNION).append(specification.normalized_name);
	}
}


void
Schema::update_specification(const MsgPack& properties)
{
	L_CALL(this, "Schema::update_specification(%s)", repr(properties.to_string()).c_str());

	for (const auto& property : properties) {
		auto str_prop = property.as_string();
		try {
			auto func = map_dispatch_properties.at(str_prop);
			(this->*func)(properties.at(str_prop));
		} catch (const std::out_of_range&) { }
	}
}


void
Schema::update_position(const MsgPack& prop_position)
{
	L_CALL(this, "Schema::update_position(%s)", repr(prop_position.to_string()).c_str());

	specification.position.clear();
	if (prop_position.is_array()) {
		for (const auto& _position : prop_position) {
			specification.position.push_back(static_cast<Xapian::termpos>(_position.as_u64()));
		}
	} else {
		specification.position.push_back(static_cast<Xapian::termpos>(prop_position.as_u64()));
	}
}


void
Schema::update_weight(const MsgPack& prop_weight)
{
	L_CALL(this, "Schema::update_weight(%s)", repr(prop_weight.to_string()).c_str());

	specification.weight.clear();
	if (prop_weight.is_array()) {
		for (const auto& _weight : prop_weight) {
			specification.weight.push_back(static_cast<Xapian::termpos>(_weight.as_u64()));
		}
	} else {
		specification.weight.push_back(static_cast<Xapian::termpos>(prop_weight.as_u64()));
	}
}


void
Schema::update_spelling(const MsgPack& prop_spelling)
{
	L_CALL(this, "Schema::update_spelling(%s)", repr(prop_spelling.to_string()).c_str());

	specification.spelling.clear();
	if (prop_spelling.is_array()) {
		for (const auto& _spelling : prop_spelling) {
			specification.spelling.push_back(_spelling.as_bool());
		}
	} else {
		specification.spelling.push_back(prop_spelling.as_bool());
	}
}


void
Schema::update_positions(const MsgPack& prop_positions)
{
	L_CALL(this, "Schema::update_positions(%s)", repr(prop_positions.to_string()).c_str());

	specification.positions.clear();
	if (prop_positions.is_array()) {
		for (const auto& _positions : prop_positions) {
			specification.positions.push_back(_positions.as_bool());
		}
	} else {
		specification.positions.push_back(prop_positions.as_bool());
	}
}


void
Schema::update_language(const MsgPack& prop_language)
{
	L_CALL(this, "Schema::update_language(%s)", repr(prop_language.to_string()).c_str());

	specification.language = prop_language.as_string();
}


void
Schema::update_stop_strategy(const MsgPack& prop_stop_strategy)
{
	L_CALL(this, "Schema::update_stop_strategy(%s)", repr(prop_stop_strategy.to_string()).c_str());

	specification.stop_strategy = static_cast<StopStrategy>(prop_stop_strategy.as_u64());
}


void
Schema::update_stem_strategy(const MsgPack& prop_stem_strategy)
{
	L_CALL(this, "Schema::update_stem_strategy(%s)", repr(prop_stem_strategy.to_string()).c_str());

	specification.stem_strategy = static_cast<StemStrategy>(prop_stem_strategy.as_u64());
}


void
Schema::update_stem_language(const MsgPack& prop_stem_language)
{
	L_CALL(this, "Schema::update_stem_language(%s)", repr(prop_stem_language.to_string()).c_str());

	specification.stem_language = prop_stem_language.as_string();
}


void
Schema::update_type(const MsgPack& prop_type)
{
	L_CALL(this, "Schema::update_type(%s)", repr(prop_type.to_string()).c_str());

	specification.sep_types[0] = (FieldType)prop_type.at(0).as_u64();
	specification.sep_types[1] = (FieldType)prop_type.at(1).as_u64();
	specification.sep_types[2] = (FieldType)prop_type.at(2).as_u64();
	specification.flags.field_with_type = specification.sep_types[2] != FieldType::EMPTY;
}


void
Schema::update_accuracy(const MsgPack& prop_accuracy)
{
	L_CALL(this, "Schema::update_accuracy(%s)", repr(prop_accuracy.to_string()).c_str());

	for (const auto& acc : prop_accuracy) {
		specification.accuracy.push_back(acc.as_f64());
	}
}


void
Schema::update_acc_prefix(const MsgPack& prop_acc_prefix)
{
	L_CALL(this, "Schema::update_acc_prefix(%s)", repr(prop_acc_prefix.to_string()).c_str());

	for (const auto& acc_p : prop_acc_prefix) {
		specification.acc_prefix.push_back(acc_p.as_string());
	}
}


void
Schema::update_prefix(const MsgPack& prop_prefix)
{
	L_CALL(this, "Schema::update_prefix(%s)", repr(prop_prefix.to_string()).c_str());

	specification.prefix = prop_prefix.as_string();
}


void
Schema::update_slot(const MsgPack& prop_slot)
{
	L_CALL(this, "Schema::update_slot(%s)", repr(prop_slot.to_string()).c_str());

	specification.slot = static_cast<Xapian::valueno>(prop_slot.as_u64());
}


void
Schema::update_index(const MsgPack& prop_index)
{
	L_CALL(this, "Schema::update_index(%s)", repr(prop_index.to_string()).c_str());

	specification.index = static_cast<TypeIndex>(prop_index.as_u64());
	specification.flags.has_index = true;
}


void
Schema::update_store(const MsgPack& prop_store)
{
	L_CALL(this, "Schema::update_store(%s)", repr(prop_store.to_string()).c_str());

	specification.flags.parent_store = specification.flags.store;
	specification.flags.store = prop_store.as_bool() && specification.flags.parent_store;
}


void
Schema::update_recursive(const MsgPack& prop_recursive)
{
	L_CALL(this, "Schema::update_recursive(%s)", repr(prop_recursive.to_string()).c_str());

	specification.flags.is_recursive = prop_recursive.as_bool();
}


void
Schema::update_dynamic(const MsgPack& prop_dynamic)
{
	L_CALL(this, "Schema::update_dynamic(%s)", repr(prop_dynamic.to_string()).c_str());

	specification.flags.dynamic = prop_dynamic.as_bool();
}


void
Schema::update_strict(const MsgPack& prop_strict)
{
	L_CALL(this, "Schema::update_strict(%s)", repr(prop_strict.to_string()).c_str());

	specification.flags.strict = prop_strict.as_bool();
}


void
Schema::update_d_detection(const MsgPack& prop_d_detection)
{
	L_CALL(this, "Schema::update_d_detection(%s)", repr(prop_d_detection.to_string()).c_str());

	specification.flags.date_detection = prop_d_detection.as_bool();
}


void
Schema::update_n_detection(const MsgPack& prop_n_detection)
{
	L_CALL(this, "Schema::update_n_detection(%s)", repr(prop_n_detection.to_string()).c_str());

	specification.flags.numeric_detection = prop_n_detection.as_bool();
}


void
Schema::update_g_detection(const MsgPack& prop_g_detection)
{
	L_CALL(this, "Schema::update_g_detection(%s)", repr(prop_g_detection.to_string()).c_str());

	specification.flags.geo_detection = prop_g_detection.as_bool();
}


void
Schema::update_b_detection(const MsgPack& prop_b_detection)
{
	L_CALL(this, "Schema::update_b_detection(%s)", repr(prop_b_detection.to_string()).c_str());

	specification.flags.bool_detection = prop_b_detection.as_bool();
}


void
Schema::update_s_detection(const MsgPack& prop_s_detection)
{
	L_CALL(this, "Schema::update_s_detection(%s)", repr(prop_s_detection.to_string()).c_str());

	specification.flags.string_detection = prop_s_detection.as_bool();
}


void
Schema::update_t_detection(const MsgPack& prop_t_detection)
{
	L_CALL(this, "Schema::update_t_detection(%s)", repr(prop_t_detection.to_string()).c_str());

	specification.flags.text_detection = prop_t_detection.as_bool();
}


void
Schema::update_u_detection(const MsgPack& prop_u_detection)
{
	L_CALL(this, "Schema::update_u_detection(%s)", repr(prop_u_detection.to_string()).c_str());

	specification.flags.uuid_detection = prop_u_detection.as_bool();
}


void
Schema::update_bool_term(const MsgPack& prop_bool_term)
{
	L_CALL(this, "Schema::update_bool_term(%s)", repr(prop_bool_term.to_string()).c_str());

	specification.flags.bool_term = prop_bool_term.as_bool();
}


void
Schema::update_partials(const MsgPack& prop_partials)
{
	L_CALL(this, "Schema::update_partials(%s)", repr(prop_partials.to_string()).c_str());

	specification.flags.partials = prop_partials.as_bool();
}


void
Schema::update_error(const MsgPack& prop_error)
{
	L_CALL(this, "Schema::update_error(%s)", repr(prop_error.to_string()).c_str());

	specification.error = prop_error.as_f64();
}


void
Schema::update_namespace(const MsgPack& prop_namespace)
{
	L_CALL(this, "Schema::update_namespace(%s)", repr(prop_namespace.to_string()).c_str());

	if (prop_namespace.as_bool()) {
		if (specification.flags.dynamic_type) {
			specification.paths_namespace.push_back(Serialise::dynamic_namespace_field(specification.full_normalized_name));
		} else {
			specification.paths_namespace.push_back(Serialise::namespace_field(specification.full_normalized_name));
		}
	}
	specification.flags.has_namespace = true;
}


void
Schema::process_position(const std::string& prop_name, const MsgPack& doc_position)
{
	// RESERVED_POSITION is heritable and can change between documents.
	L_CALL(this, "Schema::process_position(%s)", repr(doc_position.to_string()).c_str());

	try {
		specification.position.clear();
		if (doc_position.is_array()) {
			if (doc_position.empty()) {
				THROW(ClientError, "Data inconsistency, %s must be a positive integer or a not-empty array of positive integers", prop_name.c_str());
			}
			for (const auto& _position : doc_position) {
				specification.position.push_back(static_cast<unsigned>(_position.as_u64()));
			}
		} else {
			specification.position.push_back(static_cast<unsigned>(doc_position.as_u64()));
		}

		if unlikely(!specification.flags.field_found && !specification.flags.inside_namespace) {
			if (specification.position.size() == 1) {
				get_mutable()[prop_name] = static_cast<uint64_t>(specification.position.front());
			} else {
				get_mutable()[prop_name] = specification.position;
			}
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a positive integer or a not-empty array of positive integers", prop_name.c_str());
	}
}


void
Schema::process_weight(const std::string& prop_name, const MsgPack& doc_weight)
{
	// RESERVED_WEIGHT property is heritable and can change between documents.
	L_CALL(this, "Schema::process_weight(%s)", repr(doc_weight.to_string()).c_str());

	try {
		specification.weight.clear();
		if (doc_weight.is_array()) {
			if (doc_weight.empty()) {
				THROW(ClientError, "Data inconsistency, %s must be a positive integer or a not-empty array of positive integers", prop_name.c_str());
			}
			for (const auto& _weight : doc_weight) {
				specification.weight.push_back(static_cast<unsigned>(_weight.as_u64()));
			}
		} else {
			specification.weight.push_back(static_cast<unsigned>(doc_weight.as_u64()));
		}

		if unlikely(!specification.flags.field_found && !specification.flags.inside_namespace) {
			if (specification.weight.size() == 1) {
				get_mutable()[prop_name] = static_cast<uint64_t>(specification.weight.front());
			} else {
				get_mutable()[prop_name] = specification.weight;
			}
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a positive integer or a not-empty array of positive integers", prop_name.c_str());
	}
}


void
Schema::process_spelling(const std::string& prop_name, const MsgPack& doc_spelling)
{
	// RESERVED_SPELLING is heritable and can change between documents.
	L_CALL(this, "Schema::process_spelling(%s)", repr(doc_spelling.to_string()).c_str());

	try {
		specification.spelling.clear();
		if (doc_spelling.is_array()) {
			if (doc_spelling.empty()) {
				THROW(ClientError, "Data inconsistency, %s must be a boolean or a not-empty array of booleans", prop_name.c_str());
			}
			for (const auto& _spelling : doc_spelling) {
				specification.spelling.push_back(_spelling.as_bool());
			}
		} else {
			specification.spelling.push_back(doc_spelling.as_bool());
		}

		if unlikely(!specification.flags.field_found && !specification.flags.inside_namespace) {
			if (specification.spelling.size() == 1) {
				get_mutable()[prop_name] = static_cast<bool>(specification.spelling.front());
			} else {
				get_mutable()[prop_name] = specification.spelling;
			}
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a boolean or a not-empty array of booleans", prop_name.c_str());
	}
}


void
Schema::process_positions(const std::string& prop_name, const MsgPack& doc_positions)
{
	// RESERVED_POSITIONS is heritable and can change between documents.
	L_CALL(this, "Schema::process_positions(%s)", repr(doc_positions.to_string()).c_str());

	try {
		specification.positions.clear();
		if (doc_positions.is_array()) {
			if (doc_positions.empty()) {
				THROW(ClientError, "Data inconsistency, %s must be a boolean or a not-empty array of booleans", prop_name.c_str());
			}
			for (const auto& _positions : doc_positions) {
				specification.positions.push_back(_positions.as_bool());
			}
		} else {
			specification.positions.push_back(doc_positions.as_bool());
		}

		if unlikely(!specification.flags.field_found && !specification.flags.inside_namespace) {
			if (specification.positions.size() == 1) {
				get_mutable()[prop_name] = static_cast<bool>(specification.positions.front());
			} else {
				get_mutable()[prop_name] = specification.positions;
			}
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a boolean or a not-empty array of booleans", prop_name.c_str());
	}
}


void
Schema::process_language(const std::string& prop_name, const MsgPack& doc_language)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_language(%s)", repr(doc_language.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		auto _str_language = lower_string(doc_language.as_string());
		try {
			auto data_lan = map_stem_language.at(_str_language);
			if (data_lan.first) {
				specification.language = data_lan.second;
				specification.aux_lan = data_lan.second;
			} else {
				THROW(ClientError, "%s: %s is not supported", repr(prop_name).c_str(), repr(_str_language).c_str());
			}
		} catch (const std::out_of_range&) {
			THROW(ClientError, "%s: %s is not supported", repr(prop_name).c_str(), repr(_str_language).c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", repr(prop_name).c_str());
	}
}


void
Schema::process_stop_strategy(const std::string& prop_name, const MsgPack& doc_stop_strategy)
{
	// RESERVED_STOP_STRATEGY isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_stop_strategy(%s)", repr(doc_stop_strategy.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		auto _stop_strategy = lower_string(doc_stop_strategy.as_string());
		try {
			specification.stop_strategy = map_stop_strategy.at(_stop_strategy);
		} catch (const std::out_of_range&) {
			THROW(ClientError, "%s can be in %s (%s not supported)", prop_name.c_str(), str_set_stop_strategy.c_str(), _stop_strategy.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::process_stem_strategy(const std::string& prop_name, const MsgPack& doc_stem_strategy)
{
	// RESERVED_STEM_STRATEGY isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_stem_strategy(%s)", repr(doc_stem_strategy.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		auto _stem_strategy = lower_string(doc_stem_strategy.as_string());
		try {
			specification.stem_strategy = map_stem_strategy.at(_stem_strategy);
		} catch (const std::out_of_range&) {
			THROW(ClientError, "%s can be in %s (%s not supported)", prop_name.c_str(), str_set_stem_strategy.c_str(), _stem_strategy.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::process_stem_language(const std::string& prop_name, const MsgPack& doc_stem_language)
{
	// RESERVED_STEM_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_stem_language(%s)", repr(doc_stem_language.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		auto _stem_language = lower_string(doc_stem_language.as_string());
		try {
			auto data_lan = map_stem_language.at(_stem_language);
			specification.stem_language = _stem_language;
			specification.aux_stem_lan = data_lan.second;
		} catch (const std::out_of_range&) {
			THROW(ClientError, "%s: %s is not supported", repr(prop_name).c_str(), repr(_stem_language).c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", repr(prop_name).c_str());
	}
}


void
Schema::process_type(const std::string& prop_name, const MsgPack& doc_type)
{
	// RESERVED_TYPE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_type(%s)", repr(doc_type.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		auto str_type = lower_string(doc_type.as_string());
		if (str_type.empty()) {
			THROW(ClientError, "%s must be in { object, array, [object/][array/]< %s > }", prop_name.c_str(), str_set_type.c_str());
		}

		try {
			specification.sep_types[2] = map_type.at(str_type);
		} catch (const std::out_of_range&) {
			std::vector<std::string> tokens;
			stringTokenizer(str_type, "/", tokens);
			try {
				switch (tokens.size()) {
					case 1:
						if (tokens[0] == OBJECT_STR) {
							specification.sep_types[0] = FieldType::OBJECT;
							return;
						} else if (tokens[0] == ARRAY_STR) {
							specification.sep_types[1] = FieldType::ARRAY;
							return;
						}
						break;
					case 2:
						if (tokens[0] == OBJECT_STR) {
							specification.sep_types[0] = FieldType::OBJECT;
							if (tokens[1] == ARRAY_STR) {
								specification.sep_types[1] = FieldType::ARRAY;
								return;
							} else {
								specification.sep_types[2] = map_type.at(tokens[1]);
								return;
							}
						} else if (tokens[0] == ARRAY_STR) {
							specification.sep_types[1] = FieldType::ARRAY;
							specification.sep_types[2] = map_type.at(tokens[1]);
							return;
						}
						break;
					case 3:
						if (tokens[0] == OBJECT_STR && tokens[1] == ARRAY_STR) {
							specification.sep_types[0] = FieldType::OBJECT;
							specification.sep_types[1] = FieldType::ARRAY;
							specification.sep_types[2] = map_type.at(tokens[2]);
						}
						break;
				}
				THROW(ClientError, "%s must be in { object, array, [object/][array/]< %s > }", prop_name.c_str(), str_set_type.c_str());
			} catch (const std::out_of_range&) {
				THROW(ClientError, "%s must be in { object, array, [object/][array/]< %s > }", prop_name.c_str(), str_set_type.c_str());
			}
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::process_accuracy(const std::string& prop_name, const MsgPack& doc_accuracy)
{
	// RESERVED_ACCURACY isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_accuracy(%s)", repr(doc_accuracy.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	if (doc_accuracy.is_array()) {
		try {
			specification.doc_acc = std::make_unique<const MsgPack>(doc_accuracy);
		} catch (const msgpack::type_error&) {
			THROW(ClientError, "Data inconsistency, %s must be array", prop_name.c_str());
		}
	} else {
		THROW(ClientError, "Data inconsistency, %s must be array", prop_name.c_str());
	}
}


void
Schema::process_acc_prefix(const std::string& prop_name, const MsgPack& doc_acc_prefix)
{
	// RESERVED_ACC_PREFIX isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_acc_prefix(%s)", repr(doc_acc_prefix.to_string()).c_str());

	// It is taken into account only if RESERVED_ACCURACY is defined.
	if likely(specification.flags.field_with_type) {
		return;
	}

	if (doc_acc_prefix.is_array()) {
		std::unordered_set<std::string> uset_acc_prefix;
		uset_acc_prefix.reserve(doc_acc_prefix.size());
		specification.acc_prefix.reserve(doc_acc_prefix.size());
		try {
			for (const auto& _acc_prefix : doc_acc_prefix) {
				auto prefix = _acc_prefix.as_string();
				if (uset_acc_prefix.insert(prefix).second) {
					specification.acc_prefix.push_back(std::move(prefix));
				}
			}
		} catch (const msgpack::type_error&) {
			THROW(ClientError, "Data inconsistency, %s must be an array of strings", prop_name.c_str());
		}
	} else {
		THROW(ClientError, "Data inconsistency, %s must be an array of strings", prop_name.c_str());
	}
}


void
Schema::process_prefix(const std::string& prop_name, const MsgPack& doc_prefix)
{
	// RESERVED_PREFIX isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_prefix(%s)", repr(doc_prefix.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		specification.prefix = doc_prefix.as_string();
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::process_slot(const std::string& prop_name, const MsgPack& doc_slot)
{
	// RESERVED_SLOT isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_slot(%s)", repr(doc_slot.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		specification.slot = static_cast<unsigned>(doc_slot.as_u64());
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a positive integer", prop_name.c_str());
	}
}


void
Schema::process_index(const std::string& prop_name, const MsgPack& doc_index)
{
	// RESERVED_INDEX is heritable and can change.
	L_CALL(this, "Schema::process_index(%s)", repr(doc_index.to_string()).c_str());

	try {
		auto str_index = lower_string(doc_index.as_string());
		specification.flags.has_index = true;
		try {
			specification.index = map_index.at(str_index);

			if unlikely(!specification.flags.field_found && !specification.flags.inside_namespace) {
				get_mutable()[prop_name] = specification.index;
			}
		} catch (const std::out_of_range&) {
			THROW(ClientError, "%s must be in %s (%s not supported)", prop_name.c_str(), str_set_index.c_str(), str_index.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::process_store(const std::string& prop_name, const MsgPack& doc_store)
{
	L_CALL(this, "Schema::process_store(%s)", repr(doc_store.to_string()).c_str());
	/*
	 * RESERVED_STORE is heritable and can change, but once fixed in false
	 * it cannot change in its offsprings.
	 */
	try {
		auto val_store = doc_store.as_bool();
		specification.flags.store = val_store && specification.flags.parent_store;
		specification.flags.parent_store = specification.flags.store;

		if unlikely(!specification.flags.field_found && !specification.flags.inside_namespace) {
			get_mutable()[prop_name] = val_store;
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_recursive(const std::string& prop_name, const MsgPack& doc_recursive)
{
	L_CALL(this, "Schema::process_recursive(%s)", repr(doc_recursive.to_string()).c_str());
	/*
	 * RESERVED_RECURSIVE is heritable and can change, but once fixed in false
	 * it does not process its children.
	 */
	try {
		specification.flags.is_recursive = doc_recursive.as_bool();
		if unlikely(!specification.flags.field_found && !specification.flags.inside_namespace) {
			get_mutable()[prop_name.c_str()] = static_cast<bool>(specification.flags.is_recursive);
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_dynamic(const std::string& prop_name, const MsgPack& doc_dynamic)
{
	// RESERVED_DYNAMIC is heritable but can't change.
	L_CALL(this, "Schema::process_dynamic(%s)", repr(doc_dynamic.to_string()).c_str());

	if likely(specification.flags.field_found || specification.flags.inside_namespace) {
		return;
	}

	try {
		specification.flags.dynamic = doc_dynamic.as_bool();
		get_mutable()[prop_name] = static_cast<bool>(specification.flags.dynamic);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_strict(const std::string& prop_name, const MsgPack& doc_strict)
{
	// RESERVED_STRICT is heritable but can't change.
	L_CALL(this, "Schema::process_strict(%s)", repr(doc_strict.to_string()).c_str());

	if likely(specification.flags.field_found || specification.flags.inside_namespace) {
		return;
	}

	try {
		specification.flags.strict = doc_strict.as_bool();
		get_mutable()[prop_name] = static_cast<bool>(specification.flags.strict);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_d_detection(const std::string& prop_name, const MsgPack& doc_d_detection)
{
	// RESERVED_D_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::process_d_detection(%s)", repr(doc_d_detection.to_string()).c_str());

	if likely(specification.flags.field_found || specification.flags.inside_namespace) {
		return;
	}

	try {
		specification.flags.date_detection = doc_d_detection.as_bool();
		get_mutable()[prop_name] = static_cast<bool>(specification.flags.date_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_n_detection(const std::string& prop_name, const MsgPack& doc_n_detection)
{
	// RESERVED_N_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::process_n_detection(%s)", repr(doc_n_detection.to_string()).c_str());

	if likely(specification.flags.field_found || specification.flags.inside_namespace) {
		return;
	}

	try {
		specification.flags.numeric_detection = doc_n_detection.as_bool();
		get_mutable()[prop_name] = static_cast<bool>(specification.flags.numeric_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_g_detection(const std::string& prop_name, const MsgPack& doc_g_detection)
{
	// RESERVED_G_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::process_g_detection(%s)", repr(doc_g_detection.to_string()).c_str());

	if likely(specification.flags.field_found || specification.flags.inside_namespace) {
		return;
	}

	try {
		specification.flags.geo_detection = doc_g_detection.as_bool();
		get_mutable()[prop_name] = static_cast<bool>(specification.flags.geo_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_b_detection(const std::string& prop_name, const MsgPack& doc_b_detection)
{
	// RESERVED_B_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::process_b_detection(%s)", repr(doc_b_detection.to_string()).c_str());

	if likely(specification.flags.field_found || specification.flags.inside_namespace) {
		return;
	}

	try {
		specification.flags.bool_detection = doc_b_detection.as_bool();
		get_mutable()[prop_name] = static_cast<bool>(specification.flags.bool_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_s_detection(const std::string& prop_name, const MsgPack& doc_s_detection)
{
	// RESERVED_S_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::process_s_detection(%s)", repr(doc_s_detection.to_string()).c_str());

	if likely(specification.flags.field_found || specification.flags.inside_namespace) {
		return;
	}

	try {
		specification.flags.string_detection = doc_s_detection.as_bool();
		get_mutable()[prop_name] = static_cast<bool>(specification.flags.string_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_t_detection(const std::string& prop_name, const MsgPack& doc_t_detection)
{
	// RESERVED_T_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::process_t_detection(%s)", repr(doc_t_detection.to_string()).c_str());

	if likely(specification.flags.field_found || specification.flags.inside_namespace) {
		return;
	}

	try {
		specification.flags.text_detection = doc_t_detection.as_bool();
		get_mutable()[prop_name] = static_cast<bool>(specification.flags.text_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_u_detection(const std::string& prop_name, const MsgPack& doc_u_detection)
{
	// RESERVED_U_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::process_u_detection(%s)", repr(doc_u_detection.to_string()).c_str());

	if likely(specification.flags.field_found || specification.flags.inside_namespace) {
		return;
	}

	try {
		specification.flags.uuid_detection = doc_u_detection.as_bool();
		get_mutable()[prop_name] = static_cast<bool>(specification.flags.uuid_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_bool_term(const std::string& prop_name, const MsgPack& doc_bool_term)
{
	// RESERVED_BOOL_TERM isn't heritable and can't change.
	L_CALL(this, "Schema::process_bool_term(%s)", repr(doc_bool_term.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		specification.flags.bool_term = doc_bool_term.as_bool();
		specification.flags.has_bool_term = true;
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a boolean", prop_name.c_str());
	}
}


void
Schema::process_partials(const std::string& prop_name, const MsgPack& doc_partials)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_partials(%s)", repr(doc_partials.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		specification.flags.partials = doc_partials.as_bool();
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_error(const std::string& prop_name, const MsgPack& doc_error)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_error(%s)", repr(doc_error.to_string()).c_str());

	if likely(specification.flags.field_with_type) {
		return;
	}

	try {
		specification.error = doc_error.as_f64();
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a double", prop_name.c_str());
	}
}


void
Schema::process_namespace(const std::string& prop_name, const MsgPack& doc_namespace)
{
	// RESERVED_NAMESPACE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_namespace(%s)", repr(doc_namespace.to_string()).c_str());

	if likely(specification.flags.field_found || !specification.paths_namespace.empty()) {
		return;
	}

	try {
		// Only save in Schema if RESERVED_NAMESPACE is true.
		if (doc_namespace.as_bool()) {
			if (specification.flags.dynamic_type) {
				specification.paths_namespace.push_back(Serialise::dynamic_namespace_field(specification.full_normalized_name));
			} else {
				specification.paths_namespace.push_back(Serialise::namespace_field(specification.full_normalized_name));
			}
			get_mutable()[prop_name] = true;
		} else {
			get_mutable()[prop_name] = false;
		}
		specification.flags.has_namespace = true;
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_value(const std::string&, const MsgPack& doc_value)
{
	// RESERVED_VALUE isn't heritable and is not saved in schema.
	L_CALL(this, "Schema::process_value(%s)", repr(doc_value.to_string()).c_str());

	specification.value = std::make_unique<const MsgPack>(doc_value);
}


void
Schema::process_name(const std::string& prop_name, const MsgPack& doc_name)
{
	// RESERVED_NAME isn't heritable and is not saved in schema.
	L_CALL(this, "Schema::process_name(%s)", repr(doc_name.to_string()).c_str());

	try {
		specification.name.assign(doc_name.as_string());
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::process_script(const std::string&, const MsgPack& doc_script)
{
	// RESERVED_SCRIPT isn't heritable and is not saved in schema.
	L_CALL(this, "Schema::process_script(%s)", repr(doc_script.to_string()).c_str());

	specification.script = std::make_unique<const MsgPack>(doc_script);
}


void
Schema::process_cast_object(const std::string& prop_name, const MsgPack& doc_cast_object)
{
	// This property isn't heritable and is not saved in schema.
	L_CALL(this, "Schema::process_cast_object(%s)", repr(doc_cast_object.to_string()).c_str());

	if (!specification.value_rec) {
		specification.value_rec = std::make_unique<MsgPack>();
		(*specification.value_rec)[prop_name] = doc_cast_object;
	} else {
		THROW(ClientError, "Only one cast object can be defined");
	}
}


void
Schema::set_default_spc_id(MsgPack& properties)
{
	L_CALL(this, "Schema::set_default_spc_id(%s)", repr(properties.to_string()).c_str());

	specification.flags.has_bool_term = true;
	specification.flags.bool_term = true;
	properties[RESERVED_BOOL_TERM] = true;  // force bool term

	if (!specification.flags.has_index) {
		specification.flags.has_index = true;
		auto index = specification.index | TypeIndex::FIELD_ALL;  // force field_all
		if (specification.index != index) {
			specification.index = index;
			properties[RESERVED_INDEX] = specification.index;
		}
	}

	// ID_FIELD_NAME can not be TEXT nor STRING.
	if (specification.sep_types[2] == FieldType::TEXT || specification.sep_types[2] == FieldType::STRING) {
		specification.sep_types[2] = FieldType::TERM;
	}

	// Process RESERVED_PREFIX
	if (specification.prefix.empty()) {
		specification.prefix = DOCUMENT_ID_TERM_PREFIX;
	}

	// Process RESERVED_SLOT
	if (specification.slot == Xapian::BAD_VALUENO) {
		specification.slot = DB_SLOT_ID;
	}
	specification.flags.reserved_slot = true;
}


void
Schema::set_default_spc_ct(MsgPack& properties)
{
	L_CALL(this, "Schema::set_default_spc_ct(%s)", repr(properties.to_string()).c_str());

	if (!specification.flags.has_index) {
		specification.flags.has_index = true;
		auto index = (specification.index | TypeIndex::FIELD_VALUES) & ~TypeIndex::FIELD_TERMS; // Fallback to index anything but values
		if (specification.index != index) {
			specification.index = index;
			properties[RESERVED_INDEX] = specification.index;
		}
	}

	// RESERVED_TYPE by default is STRING
	if (specification.sep_types[2] == FieldType::EMPTY) {
		specification.sep_types[2] = FieldType::TERM;
	}

	// Process RESERVED_PREFIX
	if (specification.prefix.empty()) {
		specification.prefix = DOCUMENT_CONTENT_TYPE_TERM_PREFIX;
	}

	// Process RESERVED_SLOT
	if (specification.slot == Xapian::BAD_VALUENO) {
		specification.slot = DB_SLOT_CONTENT_TYPE;
	}
	specification.flags.reserved_slot = true;

	if (!specification.flags.has_namespace) {
		specification.flags.has_namespace = true;
		specification.paths_namespace.push_back(Serialise::namespace_field(specification.full_normalized_name));
		properties[RESERVED_NAMESPACE] = true;
	}
}


const MsgPack
Schema::get_readable() const
{
	L_CALL(this, "Schema::get_readable()");

	auto schema_readable = mut_schema ? *mut_schema : *schema;
	auto& properties = schema_readable.at(RESERVED_SCHEMA);
	if unlikely(properties.is_undefined()) {
		schema_readable.erase(RESERVED_SCHEMA);
	} else {
		readable(properties, true);
	}

	return schema_readable;
}


void
Schema::readable(MsgPack& item_schema, bool is_root)
{
	L_CALL(nullptr, "Schema::readable(%s, %d)", repr(item_schema.to_string()).c_str(), is_root);

	// Change this item of schema in readable form.
	static const auto dsit_e = map_dispatch_set_default_spc.end();
	for (auto it = item_schema.begin(); it != item_schema.end(); ) {
		auto str_key = it->as_string();
		try {
			auto func = map_dispatch_readable.at(str_key);
			if (!(*func)(item_schema.at(str_key), item_schema)) {
				it = item_schema.erase(it);
				continue;
			}
		} catch (const std::out_of_range&) {
			if (is_valid(str_key) || (is_root && map_dispatch_set_default_spc.find(str_key) != dsit_e)) {
				auto& sub_item = item_schema.at(str_key);
				readable(sub_item, false);
			}
		}
		++it;
	}
}


bool
Schema::readable_type(MsgPack& prop_type, MsgPack& properties)
{
	L_CALL(nullptr, "Schema::readable_type(%s, %s)", repr(prop_type.to_string()).c_str(), repr(properties.to_string()).c_str());

	std::array<FieldType, 3> sep_types({{
		(FieldType)prop_type.at(0).as_u64(),
		(FieldType)prop_type.at(1).as_u64(),
		(FieldType)prop_type.at(2).as_u64()
	}});
	prop_type = ::readable_type(sep_types);

	// Readable accuracy.
	if (sep_types[2] == FieldType::DATE) {
		for (auto& _accuracy : properties.at(RESERVED_ACCURACY)) {
			_accuracy = readable_acc_date((UnitTime)_accuracy.as_u64());
		}
	}

	return true;
}


bool
Schema::readable_prefix(MsgPack& prop_prefix, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_prefix(%s)", repr(prop_prefix.to_string()).c_str());

	prop_prefix = prop_prefix.as_string();

	return true;
}


bool
Schema::readable_stop_strategy(MsgPack& prop_stop_strategy, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_stop_strategy(%s)", repr(prop_stop_strategy.to_string()).c_str());

	prop_stop_strategy = ::readable_stop_strategy((StopStrategy)prop_stop_strategy.as_u64());

	return true;
}


bool
Schema::readable_stem_strategy(MsgPack& prop_stem_strategy, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_stem_strategy(%s)", repr(prop_stem_strategy.to_string()).c_str());

	prop_stem_strategy = ::readable_stem_strategy((StemStrategy)prop_stem_strategy.as_u64());

	return true;
}


bool
Schema::readable_stem_language(MsgPack& prop_stem_language, MsgPack& prop)
{
	L_CALL(nullptr, "Schema::readable_stem_language(%s)", repr(prop_stem_language.to_string()).c_str());

	auto language = prop[RESERVED_LANGUAGE].as_string();
	auto stem_language = prop_stem_language.as_string();

	return (language != stem_language);
}


bool
Schema::readable_index(MsgPack& prop_index, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_index(%s)", repr(prop_index.to_string()).c_str());

	prop_index = ::readable_index((TypeIndex)prop_index.as_u64());

	return true;
}


bool
Schema::readable_acc_prefix(MsgPack& prop_acc_prefix, MsgPack& properties)
{
	L_CALL(nullptr, "Schema::readable_acc_prefix(%s)", repr(prop_acc_prefix.to_string()).c_str());

	for (auto& prop_prefix : prop_acc_prefix) {
		readable_prefix(prop_prefix, properties);
	}

	return true;
}


std::shared_ptr<const MsgPack>
Schema::get_modified_schema()
{
	L_CALL(this, "Schema::get_modified_schema()");

	if (mut_schema) {
		auto schema = std::shared_ptr<const MsgPack>(mut_schema.release());
		schema->lock();
		return schema;
	} else {
		return std::shared_ptr<const MsgPack>();
	}
}


std::shared_ptr<const MsgPack>
Schema::get_const_schema() const
{
	L_CALL(this, "Schema::get_const_schema()");

	return schema;
}


std::string
Schema::to_string(bool prettify) const
{
	L_CALL(this, "Schema::to_string(%d)", prettify);

	return get_readable().to_string(prettify);
}


MsgPack
Schema::index(const MsgPack& object, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index(%s, <doc>)", repr(object.to_string()).c_str());

	try {
		MsgPack data;
		TaskVector tasks;
		tasks.reserve(object.size());
		auto prop_ptr = mut_schema ? &mut_schema->at(RESERVED_SCHEMA) : &schema->at(RESERVED_SCHEMA);
		auto data_ptr = &data;

		specification = default_spc;
		if (*prop_ptr) {
			update_specification(*prop_ptr);
		} else {
			specification.flags.field_found = false;
		}

		static const auto dsit_e = map_dispatch_set_default_spc.end();
		for (const auto& item_key : object) {
			const auto str_key = item_key.as_string();
			try {
				auto func = map_dispatch_document.at(str_key);
				(this->*func)(str_key, object.at(str_key));
			} catch (const std::out_of_range&) {
				if (is_valid(str_key) || map_dispatch_set_default_spc.find(str_key) != dsit_e) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(prop_ptr), std::ref(object.at(str_key)), std::ref(data_ptr), std::ref(doc), std::move(str_key)));
				}
			}
		}

		restart_specification();
		const specification_t spc_start = std::move(specification);
		for (auto& task : tasks) {
			specification = spc_start;
			task.get();
		}

		for (const auto& elem : map_values) {
			auto val_ser = elem.second.serialise();
			doc.add_value(elem.first, val_ser);
			L_INDEX(this, "Slot: %d  Values: %s", elem.first, repr(val_ser).c_str());
		}

		return data;
	} catch (...) {
		mut_schema.reset();
		throw;
	}
}


void
Schema::write_schema(const MsgPack& obj_schema, bool replace)
{
	L_CALL(this, "Schema::write_schema(%s, %d)", repr(obj_schema.to_string()).c_str(), replace);

	try {
		TaskVector tasks;
		tasks.reserve(obj_schema.size());
		auto prop_ptr = replace ? &clear() : &schema->at(RESERVED_SCHEMA);
		specification.flags.field_found = false;
		static const auto dsit_e = map_dispatch_set_default_spc.end();
		for (const auto& item_key : obj_schema) {
			const auto str_key = item_key.as_string();
			try {
				auto func = map_dispatch_document.at(str_key);
				(this->*func)(str_key, obj_schema.at(str_key));
			} catch (const std::out_of_range&) {
				if (is_valid(str_key) || map_dispatch_set_default_spc.find(str_key) != dsit_e) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::update_schema, this, std::ref(prop_ptr), std::ref(obj_schema.at(str_key)), std::move(str_key)));
				}
			}
		}

		restart_specification();
		const specification_t spc_start = std::move(specification);
		for (auto& task : tasks) {
			specification = spc_start;
			task.get();
		}
	} catch (...) {
		mut_schema.reset();
		throw;
	}
}


required_spc_t
Schema::get_data_id() const
{
	L_CALL(this, "Schema::get_data_id()");

	required_spc_t res;

	try {
		const auto& properties = mut_schema ? mut_schema->at(RESERVED_SCHEMA).at(ID_FIELD_NAME) : schema->at(RESERVED_SCHEMA).at(ID_FIELD_NAME);
		// update_specification(prop_id);
		const auto& sep_types = properties.at(RESERVED_TYPE);
		res.sep_types[0] = (FieldType)sep_types.at(0).as_u64();
		res.sep_types[1] = (FieldType)sep_types.at(1).as_u64();
		res.sep_types[2] = (FieldType)sep_types.at(2).as_u64();

		res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).as_u64());
		res.prefix = properties.at(RESERVED_PREFIX).as_string();
	} catch (const std::out_of_range&) { }

	return res;
}


std::pair<required_spc_t, std::string>
Schema::get_data_field(const std::string& field_name) const
{
	L_CALL(this, "Schema::get_data_field(%s)", repr(field_name).c_str());

	required_spc_t res;

	if (field_name.empty()) {
		return std::make_pair(res, std::string());
	}

	try {
		auto info = get_dynamic_subproperties(schema->at(RESERVED_SCHEMA), field_name);
		auto prefix_namespace = std::get<3>(info);
		if (prefix_namespace.empty()) {
			const auto& properties = std::get<2>(info);
			const auto& sep_types = properties.at(RESERVED_TYPE);
			res.sep_types[0] = (FieldType)sep_types.at(0).as_u64();
			res.sep_types[1] = (FieldType)sep_types.at(1).as_u64();
			res.sep_types[2] = (FieldType)sep_types.at(2).as_u64();

			if (res.sep_types[2] == FieldType::EMPTY) {
				return std::make_pair(res, std::string());
			}

			if (std::get<1>(info)) {
				res.slot = get_slot(std::get<0>(info));
				res.prefix = get_dynamic_prefix(std::get<0>(info), (char)res.sep_types[2]);

				// Get accuracy, acc_prefix and reserved word.
				switch (res.sep_types[2]) {
					case FieldType::GEO:
						res.flags.partials = properties.at(RESERVED_PARTIALS).as_bool();
						res.error = properties.at(RESERVED_ERROR).as_f64();
					case FieldType::FLOAT:
					case FieldType::INTEGER:
					case FieldType::POSITIVE:
					case FieldType::DATE:
						for (const auto& acc : properties.at(RESERVED_ACCURACY)) {
							res.accuracy.push_back(acc.as_u64());
							res.acc_prefix.push_back(get_dynamic_prefix(std::get<0>(info) + std::to_string(res.accuracy.back()), (char)res.sep_types[2]));
						}
						break;
					case FieldType::TEXT:
						res.language = properties.at(RESERVED_LANGUAGE).as_string();

						res.stop_strategy = (StopStrategy)properties.at(RESERVED_STOP_STRATEGY).as_u64();

						res.stem_strategy = (StemStrategy)properties.at(RESERVED_STEM_STRATEGY).as_u64();
						res.stem_language = properties.at(RESERVED_STEM_LANGUAGE).as_string();
						break;
					case FieldType::STRING:
						res.language = properties.at(RESERVED_LANGUAGE).as_string();
						break;
					case FieldType::TERM:
						res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).as_bool();
						break;
					default:
						break;
				}
			} else {
				res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).as_u64());
				res.prefix = properties.at(RESERVED_PREFIX).as_string();

				// Get accuracy, acc_prefix and reserved word.
				switch (res.sep_types[2]) {
					case FieldType::GEO:
						res.flags.partials = properties.at(RESERVED_PARTIALS).as_bool();
						res.error = properties.at(RESERVED_ERROR).as_f64();
					case FieldType::FLOAT:
					case FieldType::INTEGER:
					case FieldType::POSITIVE:
					case FieldType::DATE:
						for (const auto& acc : properties.at(RESERVED_ACCURACY)) {
							res.accuracy.push_back(acc.as_u64());
						}
						for (const auto& acc_p : properties.at(RESERVED_ACC_PREFIX)) {
							res.acc_prefix.push_back(acc_p.as_string());
						}
						break;
					case FieldType::TEXT:
						res.language = properties.at(RESERVED_LANGUAGE).as_string();

						res.stop_strategy = (StopStrategy)properties.at(RESERVED_STOP_STRATEGY).as_u64();

						res.stem_strategy = (StemStrategy)properties.at(RESERVED_STEM_STRATEGY).as_u64();
						res.stem_language = properties.at(RESERVED_STEM_LANGUAGE).as_string();
						break;
					case FieldType::STRING:
						res.language = properties.at(RESERVED_LANGUAGE).as_string();
						break;
					case FieldType::TERM:
						res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).as_bool();
						break;
					default:
						break;
				}
			}
		} else {
			res.flags.inside_namespace = true;
			res.prefix = std::move(prefix_namespace);
			return std::make_pair(std::move(res), std::get<4>(info));
		}
	} catch (const ClientError& exc) {
		L_EXC(this, "ERROR: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		L_EXC(this, "ERROR: %s", exc.what());
	}

	return std::make_pair(std::move(res), std::string());
}


required_spc_t
Schema::get_slot_field(const std::string& field_name) const
{
	L_CALL(this, "Schema::get_slot_field(%s)", repr(field_name).c_str());

	required_spc_t res;

	if (field_name.empty()) {
		return res;
	}

	try {
		auto info = get_dynamic_subproperties(schema->at(RESERVED_SCHEMA), field_name);
		auto prefix_namespace = std::get<3>(info);
		if (prefix_namespace.empty()) {
			const auto& properties = std::get<2>(info);

			const auto& sep_types = properties.at(RESERVED_TYPE);
			res.sep_types[0] = (FieldType)sep_types.at(0).as_u64();
			res.sep_types[1] = (FieldType)sep_types.at(1).as_u64();
			res.sep_types[2] = (FieldType)sep_types.at(2).as_u64();

			// Get partials and error if type is GEO.
			switch (res.sep_types[2]) {
				case FieldType::GEO:
					res.flags.partials = properties.at(RESERVED_PARTIALS).as_bool();
					res.error = properties.at(RESERVED_ERROR).as_f64();
					break;
				case FieldType::TEXT:
					res.language = properties.at(RESERVED_LANGUAGE).as_string();

					res.stop_strategy = (StopStrategy)properties.at(RESERVED_STOP_STRATEGY).as_u64();

					res.stem_strategy = (StemStrategy)properties.at(RESERVED_STEM_STRATEGY).as_u64();
					res.stem_language = properties.at(RESERVED_STEM_LANGUAGE).as_string();
					break;
				case FieldType::STRING:
					res.language = properties.at(RESERVED_LANGUAGE).as_string();
					break;
				case FieldType::TERM:
					res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).as_bool();
					break;
				default:
					break;
			}

			if (std::get<1>(info)) {
				res.slot = get_slot(std::get<0>(info));
			} else {
				res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).as_u64());
			}
		} else {
			auto namespace_spc = specification_t::get_global(FieldType::TERM);
			namespace_spc.flags.inside_namespace = true;
			namespace_spc.prefix.reserve(arraySize(DOCUMENT_NAMESPACE_TERM_PREFIX) + prefix_namespace.length() + 1);
			namespace_spc.prefix.assign(DOCUMENT_NAMESPACE_TERM_PREFIX).append(prefix_namespace).push_back(namespace_spc.get_prefix());
			namespace_spc.slot = get_slot(namespace_spc.prefix);
			return namespace_spc;
		}
	} catch (const ClientError& exc) {
		L_EXC(this, "ERROR: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		L_EXC(this, "ERROR: %s", exc.what());
	}

	return res;
}


const required_spc_t&
Schema::get_data_global(FieldType field_type)
{
	L_CALL(nullptr, "Schema::get_data_global('%c')", toUType(field_type));

	switch (field_type) {
		case FieldType::FLOAT: {
			static const required_spc_t prop(DB_SLOT_NUMERIC, FieldType::FLOAT, def_accuracy_num, global_acc_prefix_num);
			return prop;
		}
		case FieldType::INTEGER: {
			static const required_spc_t prop(DB_SLOT_NUMERIC, FieldType::INTEGER, def_accuracy_num, global_acc_prefix_num);
			return prop;
		}
		case FieldType::POSITIVE: {
			static const required_spc_t prop(DB_SLOT_NUMERIC, FieldType::POSITIVE, def_accuracy_num, global_acc_prefix_num);
			return prop;
		}
		case FieldType::TERM: {
			static const required_spc_t prop(DB_SLOT_STRING, FieldType::TEXT, default_spc.accuracy, default_spc.acc_prefix);
			return prop;
		}
		case FieldType::TEXT: {
			static const required_spc_t prop(DB_SLOT_STRING, FieldType::TEXT, default_spc.accuracy, default_spc.acc_prefix);
			return prop;
		}
		case FieldType::STRING: {
			static const required_spc_t prop(DB_SLOT_STRING, FieldType::TEXT, default_spc.accuracy, default_spc.acc_prefix);
			return prop;
		}
		case FieldType::BOOLEAN: {
			static const required_spc_t prop(DB_SLOT_BOOLEAN, FieldType::BOOLEAN, default_spc.accuracy, default_spc.acc_prefix);
			return prop;
		}
		case FieldType::DATE: {
			static const required_spc_t prop(DB_SLOT_DATE, FieldType::DATE, def_accuracy_date, global_acc_prefix_date);
			return prop;
		}
		case FieldType::GEO: {
			static const required_spc_t prop(DB_SLOT_GEO, FieldType::GEO, def_accuracy_geo, global_acc_prefix_geo);
			return prop;
		}
		case FieldType::UUID: {
			static const required_spc_t prop(DB_SLOT_UUID, FieldType::UUID, default_spc.accuracy, default_spc.acc_prefix);
			return prop;
		}
		default:
			THROW(ClientError, "Type: '%u' is an unknown type", toUType(field_type));
	}
}


std::tuple<std::string, bool, const MsgPack&, std::string, std::string>
Schema::get_dynamic_subproperties(const MsgPack& properties, const std::string& full_name) const
{
	L_CALL(this, "Schema::get_dynamic_subproperties(%s, %s)", repr(properties.to_string()).c_str(), repr(full_name).c_str());

	std::vector<std::string> field_names;
	stringTokenizer(full_name, DB_OFFSPRING_UNION, field_names);

	const MsgPack* subproperties = &properties;
	std::string full_normalized_name;
	full_normalized_name.reserve(full_name.length());
	bool dynamic_type = false;
	std::string prefix_namespace;

	static const auto dsit_e = map_dispatch_set_default_spc.end();

	const auto it_e = field_names.end();
	for (auto it = field_names.begin(); it != it_e; ++it) {
		const auto& field_name = *it;
		if (!is_valid(field_name)) {
			if (full_normalized_name.empty()) {
				if (map_dispatch_set_default_spc.find(field_name) == dsit_e) {
					THROW(ClientError, "The field name: %s (%s) is not valid", repr(full_name).c_str(), repr(field_name).c_str());
				}
			} else if (++it == it_e) {
				full_normalized_name.append(DB_OFFSPRING_UNION).append(field_name);
				prefix_namespace.append(dynamic_type ? Serialise::dynamic_namespace_field(full_normalized_name) : Serialise::namespace_field(full_normalized_name));
				return std::forward_as_tuple(std::move(full_normalized_name), dynamic_type, *subproperties, std::move(prefix_namespace), std::move(field_name));
			} else {
				THROW(ClientError, "The field name: %s (%s) is not valid", repr(full_name).c_str(), repr(field_name).c_str());
			}
		}
		try {
			subproperties = &subproperties->at(field_name);
			if (full_normalized_name.empty()) {
				full_normalized_name.assign(field_name);
			} else {
				full_normalized_name.append(DB_OFFSPRING_UNION).append(field_name);
			}
		} catch (const std::out_of_range&) {
			try {
				if (Serialise::isUUID(field_name)) {
					subproperties = &subproperties->at(UUID_FIELD_NAME);
					if (full_normalized_name.empty()) {
						full_normalized_name.assign(lower_string(field_name));
					} else {
						full_normalized_name.append(DB_OFFSPRING_UNION).append(lower_string(field_name));
					}
					dynamic_type = true;
					continue;
				}
			} catch (const std::out_of_range&) { }
			// Verify if parent is a namespace.
			try {
				if (subproperties->at(RESERVED_NAMESPACE).as_bool()) {
					int depth_namespace = std::distance(it, it_e);
					if (depth_namespace > NAMESPACE_LIMIT_DEPTH) {
						THROW(ClientError, "Namespace limit depth is %d, and the namespace provided has a depth of %d", NAMESPACE_LIMIT_DEPTH, depth_namespace);
					}
					prefix_namespace.assign(dynamic_type ? Serialise::dynamic_namespace_field(full_normalized_name) : Serialise::namespace_field(full_normalized_name));
					for ( ; it != it_e; ++it) {
						auto& field_namespace = *it;
						if (is_valid(field_namespace)) {
							if (Serialise::isUUID(field_namespace)) {
								to_lower(field_namespace);
								if (full_normalized_name.empty()) {
									full_normalized_name.assign(field_namespace);
								} else {
									full_normalized_name.append(DB_OFFSPRING_UNION).append(field_namespace);
								}
							} else if (full_normalized_name.empty()) {
								full_normalized_name.assign(field_namespace);
							} else {
								full_normalized_name.append(DB_OFFSPRING_UNION).append(field_namespace);
							}
							prefix_namespace.append(Serialise::dynamic_namespace_field(field_namespace));
						} else if (++it == it_e) {
							prefix_namespace.append(DB_OFFSPRING_UNION).append(field_namespace);
							return std::forward_as_tuple(std::move(full_normalized_name), dynamic_type, *subproperties, Serialise::dynamic_namespace_field(DOCUMENT_NAMESPACE_TERM_PREFIX + prefix_namespace), std::move(field_namespace));
						} else {
							THROW(ClientError, "The field name: %s (%s) is not valid", repr(full_name).c_str(), repr(field_namespace).c_str());
						}
					}
					break;
				} else {
					THROW(ClientError, "%s does not exist in schema", repr(field_name).c_str());
				}
			} catch (const std::out_of_range&) {
				THROW(ClientError, "%s does not exist in schema", repr(field_name).c_str());
			}
		}
	}

	return std::forward_as_tuple(std::move(full_normalized_name), dynamic_type, *subproperties, std::move(prefix_namespace), std::string());
}


#ifdef L_SCHEMA_DEFINED
#undef L_SCHEMA_DEFINED
#undef L_SCHEMA
#endif
