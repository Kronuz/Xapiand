/*
 * Copyright (C) 2015,2016,2017 deipi.com LLC and contributors. All rights reserved.
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

#include <algorithm>                       // for move
#include <cmath>                           // for pow
#include <cstdint>                         // for uint64_t
#include <cstring>                         // for size_t, strlen
#include <ctype.h>                         // for tolower
#include <functional>                      // for ref, reference_wrapper
#include <mutex>                           // for mutex
#include <ostream>                         // for operator<<, basic_ostream
#include <set>                             // for __tree_const_iterator, set
#include <stdexcept>                       // for out_of_range
#include <type_traits>                     // for remove_reference<>::type
#include <unordered_set>                   // for unordered_set

#include "datetime.h"                      // for isDate, tm_t
#include "exception.h"                     // for ClientError
#include "geo/wkt_parser.h"                // for EWKT_Parser
#include "log.h"                           // for L_CALL
#include "manager.h"                       // for XapiandManager, XapiandMan...
#include "multivalue/generate_terms.h"     // for integer, geo, date, positive
#include "serialise.h"                     // for type
#include "split.h"                         // for Split


#ifndef L_SCHEMA
#define L_SCHEMA_DEFINED
#define L_SCHEMA L_TEST
#endif


#define DEFAULT_STOP_STRATEGY  StopStrategy::STOP_ALL
#define DEFAULT_STEM_STRATEGY  StemStrategy::STEM_SOME
#define DEFAULT_LANGUAGE       "en"
#define DEFAULT_GEO_PARTIALS   true
#define DEFAULT_GEO_ERROR      HTM_MIN_ERROR
#define DEFAULT_POSITIONS      true
#define DEFAULT_SPELLING       false
#define DEFAULT_BOOL_TERM      false
#define DEFAULT_INDEX          TypeIndex::ALL


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
	{ "field",                     TypeIndex::FIELD_ALL                 },
	{ "field_all",                 TypeIndex::FIELD_ALL                 },
	{ "global_terms",              TypeIndex::GLOBAL_TERMS              },
	{ "terms",                     TypeIndex::TERMS                     },
	{ "global_terms,field_values", TypeIndex::GLOBAL_TERMS_FIELD_VALUES },
	{ "field_values,global_terms", TypeIndex::GLOBAL_TERMS_FIELD_VALUES },
	{ "global_terms,field",        TypeIndex::GLOBAL_TERMS_FIELD_ALL    },
	{ "global_terms,field_all",    TypeIndex::GLOBAL_TERMS_FIELD_ALL    },
	{ "field,global_terms",        TypeIndex::GLOBAL_TERMS_FIELD_ALL    },
	{ "field_all,global_terms",    TypeIndex::GLOBAL_TERMS_FIELD_ALL    },
	{ "global_values",             TypeIndex::GLOBAL_VALUES             },
	{ "global_values,field_terms", TypeIndex::GLOBAL_VALUES_FIELD_TERMS },
	{ "field_terms,global_values", TypeIndex::GLOBAL_VALUES_FIELD_TERMS },
	{ "values",                    TypeIndex::VALUES                    },
	{ "global_values,field",       TypeIndex::GLOBAL_VALUES_FIELD_ALL   },
	{ "global_values,field_all",   TypeIndex::GLOBAL_VALUES_FIELD_ALL   },
	{ "field,global_values",       TypeIndex::GLOBAL_VALUES_FIELD_ALL   },
	{ "field_all,global_values",   TypeIndex::GLOBAL_VALUES_FIELD_ALL   },
	{ "global",                    TypeIndex::GLOBAL_ALL                },
	{ "global_all",                TypeIndex::GLOBAL_ALL                },
	{ "global,field_terms",        TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "global,field_terms",        TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "global_all,field_terms",    TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "field_terms,global",        TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "field_terms,global_all",    TypeIndex::GLOBAL_ALL_FIELD_TERMS    },
	{ "global_all,field_values",   TypeIndex::GLOBAL_ALL_FIELD_VALUES   },
	{ "global,field_values",       TypeIndex::GLOBAL_ALL_FIELD_VALUES   },
	{ "field_values,global",       TypeIndex::GLOBAL_ALL_FIELD_VALUES   },
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
		case TypeIndex::FIELD_ALL:                  return "field";
		case TypeIndex::GLOBAL_TERMS:               return "global_terms";
		case TypeIndex::TERMS:                      return "terms";
		case TypeIndex::GLOBAL_TERMS_FIELD_VALUES:  return "global_terms,field_values";
		case TypeIndex::GLOBAL_TERMS_FIELD_ALL:     return "global_terms,field";
		case TypeIndex::GLOBAL_VALUES:              return "global_values";
		case TypeIndex::GLOBAL_VALUES_FIELD_TERMS:  return "global_values,field_terms";
		case TypeIndex::VALUES:                     return "values";
		case TypeIndex::GLOBAL_VALUES_FIELD_ALL:    return "global_values,field";
		case TypeIndex::GLOBAL_ALL:                 return "global";
		case TypeIndex::GLOBAL_ALL_FIELD_TERMS:     return "global,field_terms";
		case TypeIndex::GLOBAL_ALL_FIELD_VALUES:    return "global,field_values";
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
 *  Function to generate a prefix given an field accuracy.
 */

static std::pair<std::string, FieldType> get_acc_data(const std::string& field_acc) {
	auto it = map_acc_date.find(field_acc.substr(1));
	if (it == map_acc_date.end()) {
		try {
			if (field_acc.find("_geo") == 0) {
				return std::make_pair(get_prefix(stox(std::stoull, field_acc.substr(4))), FieldType::GEO);
			} else {
				return std::make_pair(get_prefix(stox(std::stoull, field_acc.substr(1))), FieldType::INTEGER);
			}
		} catch (const InvalidArgument&) {
			THROW(ClientError, "The field name: %s is not valid", repr(field_acc).c_str());
		} catch (const OutOfRange&) {
			THROW(ClientError, "The field name: %s is not valid", repr(field_acc).c_str());
		}
	} else {
		return std::make_pair(get_prefix(toUType(it->second)), FieldType::DATE);
	}
}


/*
 * Default acc_prefixes for global values.
 */

static const std::vector<std::string> global_acc_prefix_num = []() {
	std::vector<std::string> res;
	res.reserve(def_accuracy_num.size());
	for (const auto& acc : def_accuracy_num) {
		res.push_back(get_prefix(acc));
	}
	return res;
}();

static const std::vector<std::string> global_acc_prefix_date = []() {
	std::vector<std::string> res;
	res.reserve(def_accuracy_date.size());
	for (const auto& acc : def_accuracy_date) {
		res.push_back(get_prefix(acc));
	}
	return res;
}();

static const std::vector<std::string> global_acc_prefix_geo = []() {
	std::vector<std::string> res;
	res.reserve(def_accuracy_geo.size());
	for (const auto& acc : def_accuracy_geo) {
		res.push_back(get_prefix(acc));
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


specification_t default_spc;


const std::unordered_map<std::string, Schema::dispatch_set_default_spc> Schema::map_dispatch_set_default_spc({
	{ ID_FIELD_NAME,  &Schema::set_default_spc_id },
	{ CT_FIELD_NAME,  &Schema::set_default_spc_ct },
});


const std::unordered_map<std::string, Schema::dispatch_write_reserved> Schema::map_dispatch_write_properties({
	{ RESERVED_WEIGHT,             &Schema::write_weight          },
	{ RESERVED_POSITION,           &Schema::write_position        },
	{ RESERVED_SPELLING,           &Schema::write_spelling        },
	{ RESERVED_POSITIONS,          &Schema::write_positions       },
	{ RESERVED_INDEX,              &Schema::write_index           },
	{ RESERVED_STORE,              &Schema::write_store           },
	{ RESERVED_RECURSIVE,          &Schema::write_recursive       },
	{ RESERVED_DYNAMIC,            &Schema::write_dynamic         },
	{ RESERVED_STRICT,             &Schema::write_strict          },
	{ RESERVED_D_DETECTION,        &Schema::write_d_detection     },
	{ RESERVED_N_DETECTION,        &Schema::write_n_detection     },
	{ RESERVED_G_DETECTION,        &Schema::write_g_detection     },
	{ RESERVED_B_DETECTION,        &Schema::write_b_detection     },
	{ RESERVED_S_DETECTION,        &Schema::write_s_detection     },
	{ RESERVED_T_DETECTION,        &Schema::write_t_detection     },
	{ RESERVED_TM_DETECTION,       &Schema::write_tm_detection    },
	{ RESERVED_U_DETECTION,        &Schema::write_u_detection     },
	{ RESERVED_NAMESPACE,          &Schema::write_namespace       },
	{ RESERVED_PARTIAL_PATHS,      &Schema::write_partial_paths   },
});


const std::unordered_map<std::string, Schema::dispatch_process_reserved> Schema::map_dispatch_without_type({
	{ RESERVED_LANGUAGE,           &Schema::process_language        },
	{ RESERVED_STOP_STRATEGY,      &Schema::process_stop_strategy   },
	{ RESERVED_STEM_STRATEGY,      &Schema::process_stem_strategy   },
	{ RESERVED_STEM_LANGUAGE,      &Schema::process_stem_language   },
	{ RESERVED_TYPE,               &Schema::process_type            },
	{ RESERVED_BOOL_TERM,          &Schema::process_bool_term       },
	{ RESERVED_ACCURACY,           &Schema::process_accuracy        },
	{ RESERVED_PARTIALS,           &Schema::process_partials        },
	{ RESERVED_ERROR,              &Schema::process_error           },
});


const std::unordered_map<std::string, Schema::dispatch_process_reserved> Schema::map_dispatch_document({
	{ RESERVED_WEIGHT,             &Schema::process_weight          },
	{ RESERVED_POSITION,           &Schema::process_position        },
	{ RESERVED_SPELLING,           &Schema::process_spelling        },
	{ RESERVED_POSITIONS,          &Schema::process_positions       },
	{ RESERVED_INDEX,              &Schema::process_index           },
	{ RESERVED_STORE,              &Schema::process_store           },
	{ RESERVED_RECURSIVE,          &Schema::process_recursive       },
	{ RESERVED_PARTIAL_PATHS,      &Schema::process_partial_paths   },
	{ RESERVED_VALUE,              &Schema::process_value           },
	{ RESERVED_SCRIPT,             &Schema::process_script          },
	{ RESERVED_FLOAT,              &Schema::process_cast_object     },
	{ RESERVED_POSITIVE,           &Schema::process_cast_object     },
	{ RESERVED_INTEGER,            &Schema::process_cast_object     },
	{ RESERVED_BOOLEAN,            &Schema::process_cast_object     },
	{ RESERVED_TERM,               &Schema::process_cast_object     },
	{ RESERVED_TEXT,               &Schema::process_cast_object     },
	{ RESERVED_STRING,             &Schema::process_cast_object     },
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
	{ RESERVED_TM_DETECTION,    &Schema::update_tm_detection     },
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
	{ RESERVED_PARTIAL_PATHS,   &Schema::update_partial_paths    },
});


const std::unordered_map<std::string, Schema::dispatch_readable> Schema::map_dispatch_readable({
	{ RESERVED_TYPE,            &Schema::readable_type            },
	{ RESERVED_PREFIX,          &Schema::readable_prefix          },
	{ RESERVED_STOP_STRATEGY,   &Schema::readable_stop_strategy   },
	{ RESERVED_STEM_STRATEGY,   &Schema::readable_stem_strategy   },
	{ RESERVED_STEM_LANGUAGE,   &Schema::readable_stem_language   },
	{ RESERVED_INDEX,           &Schema::readable_index           },
	{ RESERVED_ACC_PREFIX,      &Schema::readable_acc_prefix      },
});


const std::unordered_set<std::string> set_reserved_words({
	RESERVED_WEIGHT,           RESERVED_POSITION,         RESERVED_SPELLING,
	RESERVED_POSITIONS,        RESERVED_TYPE,             RESERVED_INDEX,
	RESERVED_STORE,            RESERVED_RECURSIVE,        RESERVED_DYNAMIC,
	RESERVED_STRICT,           RESERVED_D_DETECTION,      RESERVED_N_DETECTION,
	RESERVED_G_DETECTION,      RESERVED_B_DETECTION,      RESERVED_S_DETECTION,
	RESERVED_T_DETECTION,      RESERVED_TM_DETECTION,     RESERVED_U_DETECTION,
	RESERVED_BOOL_TERM,        RESERVED_ACCURACY,         RESERVED_LANGUAGE,
	RESERVED_STOP_STRATEGY,    RESERVED_STEM_STRATEGY,    RESERVED_STEM_LANGUAGE,
	RESERVED_PARTIALS,         RESERVED_ERROR,            RESERVED_NAMESPACE,
	RESERVED_PARTIAL_PATHS,    RESERVED_VALUE,            RESERVED_SCRIPT,
	RESERVED_FLOAT,            RESERVED_POSITIVE,         RESERVED_INTEGER,
	RESERVED_BOOLEAN,          RESERVED_TERM,             RESERVED_TEXT,
	RESERVED_STRING,           RESERVED_DATE,             RESERVED_UUID,
	RESERVED_EWKT,             RESERVED_POINT,            RESERVED_POLYGON,
	RESERVED_CIRCLE,           RESERVED_CHULL,            RESERVED_MULTIPOINT,
	RESERVED_MULTIPOLYGON,     RESERVED_MULTICIRCLE,      RESERVED_MULTICHULL,
	RESERVED_GEO_COLLECTION,   RESERVED_GEO_INTERSECTION,
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
	static std::mutex mtx;
	static std::string path_stopwords(getenv("XAPIAN_PATH_STOPWORDS") ? getenv("XAPIAN_PATH_STOPWORDS") : PATH_STOPWORDS);
	static std::unordered_map<std::string, std::unique_ptr<Xapian::SimpleStopper>> stoppers;
	std::lock_guard<std::mutex> lk(mtx);
	auto it = stoppers.find(language);
	if (it == stoppers.end()) {
		auto path = path_stopwords + "/" + language + ".txt";
		std::ifstream words;
		words.open(path);
		auto& stopper = stoppers[language];
		if (words.is_open()) {
			stopper = std::make_unique<Xapian::SimpleStopper>(std::istream_iterator<std::string>(words), std::istream_iterator<std::string>());
		} else {
			L_WARNING(nullptr, "Cannot open stop words file: %s", path.c_str());
		}
		return stopper;
	} else {
		return it->second;
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
	  term_detection(true),
	  uuid_detection(true),
	  partial_paths(false),
	  is_namespace(false),
	  optimal(false),
	  field_found(true),
	  field_with_type(false),
	  complete(false),
	  dynamic_type(false),
	  inside_namespace(false),
	  dynamic_type_path(false),
	  has_bool_term(false),
	  has_index(false),
	  has_namespace(false),
	  has_partial_paths(false) { }


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
	  error(o.error) { }


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
	  error(std::move(o.error)) { }


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
	  local_prefix(o.local_prefix),
	  position(o.position),
	  weight(o.weight),
	  spelling(o.spelling),
	  positions(o.positions),
	  index(o.index),
	  script(o.script),
	  name(o.name),
	  meta_name(o.meta_name),
	  full_meta_name(o.full_meta_name),
	  aux_stem_lan(o.aux_stem_lan),
	  aux_lan(o.aux_lan),
	  partial_prefixes(o.partial_prefixes) { }


specification_t::specification_t(specification_t&& o) noexcept
	: required_spc_t(std::move(o)),
	  local_prefix(std::move(o.local_prefix)),
	  position(std::move(o.position)),
	  weight(std::move(o.weight)),
	  spelling(std::move(o.spelling)),
	  positions(std::move(o.positions)),
	  index(std::move(o.index)),
	  script(std::move(o.script)),
	  name(std::move(o.name)),
	  meta_name(std::move(o.meta_name)),
	  full_meta_name(std::move(o.full_meta_name)),
	  aux_stem_lan(std::move(o.aux_stem_lan)),
	  aux_lan(std::move(o.aux_lan)),
	  partial_prefixes(std::move(o.partial_prefixes)) { }


specification_t&
specification_t::operator=(const specification_t& o)
{
	local_prefix = o.local_prefix;
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
	aux_stem_lan = o.aux_stem_lan;
	aux_lan = o.aux_lan;
	partial_prefixes = o.partial_prefixes;
	required_spc_t::operator=(o);
	return *this;
}


specification_t&
specification_t::operator=(specification_t&& o) noexcept
{
	local_prefix = std::move(o.local_prefix);
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
	aux_stem_lan = std::move(o.aux_stem_lan);
	aux_lan = std::move(o.aux_lan);
	partial_prefixes = std::move(o.partial_prefixes);
	required_spc_t::operator=(std::move(o));
	return *this;
}


FieldType
specification_t::global_type(FieldType field_type)
{
	switch (field_type) {
		case FieldType::FLOAT:
		case FieldType::INTEGER:
		case FieldType::POSITIVE:
		case FieldType::BOOLEAN:
		case FieldType::DATE:
		case FieldType::GEO:
		case FieldType::UUID:
			return field_type;

		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING:
			return FieldType::TEXT;

		default:
			THROW(ClientError, "Type: '%u' is an unknown type", field_type);
	}
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
		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING: {
			static const specification_t spc(DB_SLOT_STRING, FieldType::TEXT, default_spc.accuracy, default_spc.acc_prefix);
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

	str << "\t" << RESERVED_LANGUAGE          << ": " << language          << "\n";
	str << "\t" << RESERVED_STOP_STRATEGY     << ": " << readable_stop_strategy(stop_strategy) << "\n";
	str << "\t" << RESERVED_STEM_STRATEGY     << ": " << readable_stem_strategy(stem_strategy) << "\n";
	str << "\t" << RESERVED_STEM_LANGUAGE     << ": " << stem_language     << "\n";

	str << "\t" << RESERVED_ACCURACY << ": [ ";
	for (const auto& acc : accuracy) {
		str << acc << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_ACC_PREFIX  << ": [ ";
	for (const auto& acc_p : acc_prefix) {
		str << repr(acc_p) << " ";
	}
	str << "]\n";

	str << "\t" << "partial_prefixes"  << ": [ ";
	for (const auto& partial_prefix : partial_prefixes) {
		str << repr(partial_prefix) << " ";
	}
	str << "]\n";

	str << "\t" << "partial_spcs"    << ": [ ";
	for (const auto& spc : partial_spcs) {
		str << "{" << repr(spc.prefix) << ", " << spc.slot << "} ";
	}
	str << "]\n";

	str << "\t" << RESERVED_VALUE             << ": " << (value     ? value->to_string()     : "null")   << "\n";
	str << "\t" << "value_rec"                << ": " << (value_rec ? value_rec->to_string() : "null")   << "\n";
	str << "\t" << RESERVED_SCRIPT            << ": " << (script    ? script->to_string()    : "null")   << "\n";

	str << "\t" << RESERVED_SLOT              << ": " << slot                           << "\n";
	str << "\t" << RESERVED_TYPE              << ": " << readable_type(sep_types)       << "\n";
	str << "\t" << RESERVED_PREFIX            << ": " << repr(prefix)                   << "\n";
	str << "\t" << "local_prefix"             << ": " << repr(local_prefix)             << "\n";
	str << "\t" << RESERVED_INDEX             << ": " << readable_index(index)          << "\n";
	str << "\t" << RESERVED_ERROR             << ": " << error                          << "\n";

	str << "\t" << RESERVED_PARTIALS          << ": " << (flags.partials          ? "true" : "false") << "\n";
	str << "\t" << RESERVED_STORE             << ": " << (flags.store             ? "true" : "false") << "\n";
	str << "\t" << "parent_store"             << ": " << (flags.parent_store      ? "true" : "false") << "\n";
	str << "\t" << RESERVED_RECURSIVE         << ": " << (flags.is_recursive      ? "true" : "false") << "\n";
	str << "\t" << RESERVED_DYNAMIC           << ": " << (flags.dynamic           ? "true" : "false") << "\n";
	str << "\t" << RESERVED_STRICT            << ": " << (flags.strict            ? "true" : "false") << "\n";
	str << "\t" << RESERVED_D_DETECTION       << ": " << (flags.date_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_N_DETECTION       << ": " << (flags.numeric_detection ? "true" : "false") << "\n";
	str << "\t" << RESERVED_G_DETECTION       << ": " << (flags.geo_detection     ? "true" : "false") << "\n";
	str << "\t" << RESERVED_B_DETECTION       << ": " << (flags.bool_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_S_DETECTION       << ": " << (flags.string_detection  ? "true" : "false") << "\n";
	str << "\t" << RESERVED_T_DETECTION       << ": " << (flags.text_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_TM_DETECTION      << ": " << (flags.term_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_U_DETECTION       << ": " << (flags.uuid_detection    ? "true" : "false") << "\n";
	str << "\t" << RESERVED_BOOL_TERM         << ": " << (flags.bool_term         ? "true" : "false") << "\n";
	str << "\t" << RESERVED_NAMESPACE         << ": " << (flags.is_namespace      ? "true" : "false") << "\n";
	str << "\t" << RESERVED_PARTIAL_PATHS     << ": " << (flags.partial_paths     ? "true" : "false") << "\n";
	str << "\t" << "optimal"                  << ": " << (flags.optimal           ? "true" : "false") << "\n";
	str << "\t" << "field_found"              << ": " << (flags.field_found       ? "true" : "false") << "\n";
	str << "\t" << "field_with_type"          << ": " << (flags.field_with_type   ? "true" : "false") << "\n";
	str << "\t" << "complete"                 << ": " << (flags.complete          ? "true" : "false") << "\n";
	str << "\t" << "dynamic_type"             << ": " << (flags.dynamic_type      ? "true" : "false") << "\n";
	str << "\t" << "inside_namespace"         << ": " << (flags.inside_namespace  ? "true" : "false") << "\n";
	str << "\t" << "dynamic_type_path"        << ": " << (flags.dynamic_type_path ? "true" : "false") << "\n";
	str << "\t" << "has_bool_term"            << ": " << (flags.has_bool_term     ? "true" : "false") << "\n";
	str << "\t" << "has_index"                << ": " << (flags.has_index         ? "true" : "false") << "\n";
	str << "\t" << "has_namespace"            << ": " << (flags.has_namespace     ? "true" : "false") << "\n";

	str << "\t" << "name"                     << ": " << name                 << "\n";
	str << "\t" << "meta_name"                << ": " << meta_name            << "\n";
	str << "\t" << "full_meta_name"           << ": " << full_meta_name       << "\n";
	str << "\t" << "aux_stem_lan"             << ": " << aux_stem_lan         << "\n";
	str << "\t" << "aux_lan"                  << ": " << aux_lan              << "\n";

	str << "}\n";

	return str.str();
}


Schema::Schema(const std::shared_ptr<const MsgPack>& other)
	: schema(other)
{
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


std::shared_ptr<const MsgPack>
Schema::get_initial_schema()
{
	L_CALL(nullptr, "Schema::get_initial_schema()");

	MsgPack new_schema = {
		{ RESERVED_VERSION, DB_VERSION_SCHEMA },
		{ RESERVED_SCHEMA, MsgPack() },
	};
	new_schema.lock();
	return std::make_shared<const MsgPack>(std::move(new_schema));
}


MsgPack&
Schema::get_mutable()
{
	L_CALL(this, "Schema::get_mutable()");

	if (!mut_schema) {
		mut_schema = std::make_unique<MsgPack>(*schema);
	}

	MsgPack* prop = &mut_schema->at(RESERVED_SCHEMA);
	Split _split(specification.full_meta_name, DB_OFFSPRING_UNION);
	for (const auto& field_name : _split) {
		prop = &(*prop)[field_name];
	}
	return *prop;
}


MsgPack&
Schema::clear()
{
	L_CALL(this, "Schema::clear()");

	if (!mut_schema) {
		mut_schema = std::make_unique<MsgPack>(*schema);
	}

	auto& prop = mut_schema->at(RESERVED_SCHEMA);
	prop.clear();
	return prop;
}


inline void
Schema::restart_specification()
{
	L_CALL(this, "Schema::restart_specification()");

	specification.flags.partials             = default_spc.flags.partials;
	specification.error                      = default_spc.error;

	specification.language                   = default_spc.language;
	specification.stop_strategy              = default_spc.stop_strategy;
	specification.stem_strategy              = default_spc.stem_strategy;
	specification.stem_language              = default_spc.stem_language;

	specification.flags.bool_term            = default_spc.flags.bool_term;
	specification.flags.has_bool_term        = default_spc.flags.has_bool_term;
	specification.flags.has_index            = default_spc.flags.has_index;
	specification.flags.has_namespace        = default_spc.flags.has_namespace;
	specification.flags.has_partial_paths    = default_spc.flags.has_partial_paths;

	specification.flags.field_with_type      = default_spc.flags.field_with_type;
	specification.flags.complete             = default_spc.flags.complete;
	specification.flags.dynamic_type         = default_spc.flags.dynamic_type;

	specification.sep_types                  = default_spc.sep_types;
	specification.local_prefix               = default_spc.local_prefix;
	specification.slot                       = default_spc.slot;
	specification.accuracy                   = default_spc.accuracy;
	specification.acc_prefix                 = default_spc.acc_prefix;
	specification.aux_stem_lan               = default_spc.aux_stem_lan;
	specification.aux_lan                    = default_spc.aux_lan;

	specification.partial_spcs.clear();
}


inline void
Schema::restart_namespace_specification()
{
	L_CALL(this, "Schema::restart_namespace_specification()");

	specification.flags.bool_term        = default_spc.flags.bool_term;
	specification.flags.has_bool_term    = default_spc.flags.has_bool_term;

	specification.flags.field_with_type  = default_spc.flags.field_with_type;
	specification.flags.complete         = default_spc.flags.complete;
	specification.flags.dynamic_type     = default_spc.flags.dynamic_type;

	specification.sep_types              = default_spc.sep_types;
	specification.aux_stem_lan           = default_spc.aux_stem_lan;
	specification.aux_lan                = default_spc.aux_lan;

	specification.partial_spcs.clear();
}


void
Schema::index_object(const MsgPack*& parent_properties, const MsgPack& object, MsgPack*& parent_data, Xapian::Document& doc, const std::string& name)
{
	L_CALL(this, "Schema::index_object(%s, %s, %s, <Xapian::Document>, %s)", repr(parent_properties->to_string()).c_str(), repr(object.to_string()).c_str(), repr(parent_data->to_string()).c_str(), repr(name).c_str());

	if (name.empty()) {
		THROW(ClientError, "Field name must not be empty");
	}

	if (!specification.flags.is_recursive) {
		if (specification.flags.store) {
			(*parent_data)[name] = object;
		}
		return;
	}

	const auto spc_start = specification;
	specification.name.assign(name);
	auto properties = &*parent_properties;

	switch (object.getType()) {
		case MsgPack::Type::MAP: {
			TaskVector tasks;
			MsgPack* data = nullptr;

			properties = &get_subproperties(properties, object, data, doc, tasks);

			data = specification.flags.store ? &(*parent_data)[name] : parent_data;

			process_item_value(doc, data, tasks.size());

			const auto spc_object = std::move(specification);
			for (auto& task : tasks) {
				specification = spc_object;
				task.get();
			}
			break;
		}

		case MsgPack::Type::ARRAY: {
			properties = &get_subproperties(properties);
			auto data = specification.flags.store ? &(*parent_data)[name] : parent_data;
			index_array(properties, object, data, doc);
			break;
		}

		default: {
			get_subproperties(properties);
			auto data = specification.flags.store ? &(*parent_data)[name] : parent_data;
			process_item_value(doc, data, object);
			break;
		}
	}

	specification = std::move(spc_start);
}


void
Schema::index_array(const MsgPack*& properties, const MsgPack& array, MsgPack*& data, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index_array(%s, %s, <MsgPack*>, <Xapian::Document>)", repr(properties->to_string()).c_str(), repr(array.to_string()).c_str());

	const auto spc_start = specification;
	size_t pos = 0;

	for (const auto& item : array) {
		switch (item.getType()) {
			case MsgPack::Type::MAP: {
				TaskVector tasks;
				specification.value = nullptr;
				specification.value_rec = nullptr;
				MsgPack* data_pos = nullptr;
				auto subproperties = properties;

				subproperties = &get_subproperties(subproperties, item, data_pos, doc, tasks);

				data_pos = specification.flags.store ? &(*data)[pos] : data;

				process_item_value(doc, data_pos, tasks.size());

				const auto spc_item = std::move(specification);
				for (auto& task : tasks) {
					specification = spc_item;
					task.get();
				}

				specification = spc_start;
				break;
			}

			case MsgPack::Type::ARRAY: {
				auto data_pos = specification.flags.store ? &(*data)[pos] : data;
				process_item_value(doc, data_pos, item);
				break;
			}

			default:
				process_item_value(doc, specification.flags.store ? (*data)[pos] : *data, item, pos);
				break;
		}
		++pos;
	}
}


void
Schema::process_item_value(Xapian::Document& doc, MsgPack& data, const MsgPack& item_value, size_t pos)
{
	L_CALL(this, "Schema::process_item_value(<doc>, %s, %s, %zu)", data.to_string().c_str(), item_value.to_string().c_str(), pos);

	if (item_value.is_null()) {
		index_partial_paths(doc);
		if (specification.flags.store) {
			data = item_value;
		}
		return;
	}

	if (!specification.flags.complete) {
		if (specification.flags.inside_namespace) {
			complete_namespace_specification(item_value);
		} else {
			complete_specification(item_value);
		}
	}

	bool add_value = true;
	for (const auto& spc : specification.partial_spcs) {
		specification.sep_types[2] = spc.sep_types[2];
		specification.prefix       = spc.prefix;
		specification.slot         = spc.slot;
		specification.accuracy     = spc.accuracy;
		specification.acc_prefix   = spc.acc_prefix;
		index_item(doc, item_value, data, pos, add_value);
		add_value = false;
	}

	if (specification.flags.store) {
		data = data[RESERVED_VALUE];
	}
}


inline void
Schema::process_item_value(Xapian::Document& doc, MsgPack*& data, const MsgPack& item_value)
{
	L_CALL(this, "Schema::process_item_value(<doc>, %s, %s)", data->to_string().c_str(), item_value.to_string().c_str());

	if (item_value.is_null()) {
		index_partial_paths(doc);
		if (specification.flags.store) {
			*data = item_value;
		}
		return;
	}

	if (!specification.flags.complete) {
		if (specification.flags.inside_namespace) {
			complete_namespace_specification(item_value);
		} else {
			complete_specification(item_value);
		}
	}

	bool add_value = true;
	for (const auto& spc : specification.partial_spcs) {
		specification.sep_types[2] = spc.sep_types[2];
		specification.prefix       = spc.prefix;
		specification.slot         = spc.slot;
		specification.accuracy     = spc.accuracy;
		specification.acc_prefix   = spc.acc_prefix;
		index_item(doc, item_value, *data, add_value);
		add_value = false;
	}

	if (specification.flags.store && data->size() == 1) {
		*data = (*data)[RESERVED_VALUE];
	}
}


inline void
Schema::process_item_value(Xapian::Document& doc, MsgPack*& data, size_t offsprings)
{
	L_CALL(this, "Schema::process_item_value(<doc>, %s, %zu)", repr(data->to_string()).c_str(), offsprings);

	set_type_to_object(offsprings);

	auto val = specification.value ? std::move(specification.value) : std::move(specification.value_rec);
	if (val) {
		if (val->is_null()) {
			index_partial_paths(doc);
			if (specification.flags.store) {
				*data = *val;
			}
			return;
		}

		if (!specification.flags.complete) {
			if (specification.flags.inside_namespace) {
				complete_namespace_specification(*val);
			} else {
				complete_specification(*val);
			}
		}

		bool add_value = true;
		for (const auto& spc : specification.partial_spcs) {
			specification.sep_types[2] = spc.sep_types[2];
			specification.prefix       = spc.prefix;
			specification.slot         = spc.slot;
			specification.accuracy     = spc.accuracy;
			specification.acc_prefix   = spc.acc_prefix;
			index_item(doc, *val, *data, add_value);
			add_value = false;
		}

		if (specification.flags.store && !offsprings) {
			*data = (*data)[RESERVED_VALUE];
		}
	} else {
		index_partial_paths(doc);
	}
}


std::vector<std::string>
Schema::get_partial_paths(const std::vector<std::string>& partial_prefixes)
{
	L_CALL(nullptr, "Schema::get_partial_paths(%zu)", partial_prefixes.size());

	if (partial_prefixes.size() > LIMIT_PARTIAL_PATHS_DEPTH) {
		THROW(ClientError, "Partial paths limit depth is %d, and partial paths provided has a depth of %zu", LIMIT_PARTIAL_PATHS_DEPTH, partial_prefixes.size());
	}

	std::vector<std::string> prefixes;
	prefixes.reserve(std::pow(2, partial_prefixes.size() - 2));
	auto it = partial_prefixes.begin();
	prefixes.push_back(*it);
	const auto it_last = partial_prefixes.end() - 1;
	for (++it; it != it_last; ++it) {
		const auto size = prefixes.size();
		for (size_t i = 0; i < size; ++i) {
			std::string prefix;
			prefix.reserve(prefixes[i].length() + it->length());
			prefix.assign(prefixes[i]).append(*it);
			prefixes.push_back(std::move(prefix));
		}
	}

	for (auto& prefix : prefixes) {
		prefix.append(*it_last);
	}

	return prefixes;
}


required_spc_t
Schema::get_namespace_specification(FieldType namespace_type, const std::string& prefix_namespace)
{
	L_CALL(nullptr, "Schema::get_namespace_specification('%c', %s)", toUType(namespace_type), repr(prefix_namespace).c_str());

	auto spc = specification_t::get_global(namespace_type);

	spc.prefix.assign(prefix_namespace);
	spc.slot = get_slot(prefix_namespace, spc.sep_types[2]);

	switch (spc.sep_types[2]) {
		case FieldType::INTEGER:
		case FieldType::POSITIVE:
		case FieldType::FLOAT:
		case FieldType::DATE:
		case FieldType::GEO:
			for (auto& acc_prefix : spc.acc_prefix) {
				acc_prefix.insert(0, prefix_namespace);
			}
			break;

		default:
			break;
	}

	return spc;
}


void
Schema::complete_namespace_specification(const MsgPack& item_value)
{
	L_CALL(this, "Schema::complete_namespace_specification(%s)", repr(item_value.to_string()).c_str());

	validate_required_namespace_data(item_value);

	if (specification.partial_prefixes.size() > 2) {
		const auto paths = get_partial_paths(specification.partial_prefixes);
		specification.partial_spcs.reserve(paths.size());

		if (toUType(specification.index & TypeIndex::VALUES)) {
			for (const auto& path : paths) {
				specification.partial_spcs.push_back(get_namespace_specification(specification.sep_types[2], path));
			}
		} else {
			required_spc_t spc;
			spc.sep_types[2] = specification_t::global_type(specification.sep_types[2]);
			for (const auto& path : paths) {
				spc.prefix = path;
				specification.partial_spcs.push_back(spc);
			}
		}
	} else if (toUType(specification.index & TypeIndex::VALUES)) {
		specification.partial_spcs.push_back(get_namespace_specification(specification.sep_types[2], specification.prefix));
	} else {
		required_spc_t spc;
		spc.sep_types[2] = specification_t::global_type(specification.sep_types[2]);
		spc.prefix = specification.prefix;
		specification.partial_spcs.push_back(std::move(spc));
	}

	specification.flags.complete = true;
}


void
Schema::complete_specification(const MsgPack& item_value)
{
	L_CALL(this, "Schema::complete_specification(%s)", repr(item_value.to_string()).c_str());

	if (!specification.flags.field_found && !specification.flags.dynamic) {
		THROW(ClientError, "%s is not dynamic", specification.full_meta_name.c_str());
	}

	if (!specification.flags.field_with_type) {
		validate_required_data(item_value);
	}

	if (specification.partial_prefixes.size() > 2) {
		required_spc_t prev_spc = specification;

		auto paths = get_partial_paths(specification.partial_prefixes);
		specification.partial_spcs.reserve(paths.size());
		paths.pop_back();

		if (toUType(specification.index & TypeIndex::VALUES)) {
			for (const auto& path : paths) {
				specification.partial_spcs.push_back(get_namespace_specification(specification.sep_types[2], path));
			}
		} else {
			required_spc_t spc;
			spc.sep_types[2] = specification_t::global_type(specification.sep_types[2]);
			for (const auto& path : paths) {
				spc.prefix = path;
				specification.partial_spcs.push_back(spc);
			}
		}

		// Full path is process like normal field.
		specification.partial_spcs.push_back(std::move(prev_spc));
	} else {
		if (toUType(specification.index & TypeIndex::FIELD_VALUES)) {
			if (specification.flags.dynamic_type_path) {
				specification.slot = get_slot(specification.prefix, specification.sep_types[2]);
			}

			for (auto& acc_prefix : specification.acc_prefix) {
				acc_prefix.insert(0, specification.prefix);
			}
		}

		specification.partial_spcs.push_back(specification);
	}

	specification.flags.complete = true;
}


inline void
Schema::set_type_to_object(size_t offsprings)
{
	L_CALL(this, "Schema::set_type_to_object(%zu)", offsprings);

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


inline void
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
Schema::_validate_required_data(MsgPack& mut_properties)
{
	L_CALL(this, "Schema::_validate_required_data(%s)", repr(mut_properties.to_string()).c_str());

	static const auto dsit_e = map_dispatch_set_default_spc.end();
	const auto dsit = map_dispatch_set_default_spc.find(specification.full_meta_name);
	if (dsit != dsit_e) {
		(this->*dsit->second)(mut_properties);
	}

	std::set<uint64_t> set_acc;
	switch (specification.sep_types[2]) {
		case FieldType::GEO: {
			// Set partials and error.
			mut_properties[RESERVED_PARTIALS] = static_cast<bool>(specification.flags.partials);
			mut_properties[RESERVED_ERROR] = specification.error;

			if (specification.doc_acc) {
				try {
					for (const auto& _accuracy : *specification.doc_acc) {
						const auto val_acc = _accuracy.as_u64();
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
			break;
		}
		case FieldType::DATE: {
			if (specification.doc_acc) {
				try {
					static const auto adit_e = map_acc_date.end();
					for (const auto& _accuracy : *specification.doc_acc) {
						const auto str_accuracy = lower_string(_accuracy.as_string());
						const auto adit = map_acc_date.find(str_accuracy);
						if (adit == adit_e) {
							THROW(ClientError, "Data inconsistency, '%s': '%s' must be a subset of %s (%s not supported)", RESERVED_ACCURACY, DATE_STR, repr(str_set_acc_date).c_str(), repr(str_accuracy).c_str());
						} else {
							set_acc.insert(toUType(adit->second));
						}
					}
				} catch (const msgpack::type_error&) {
					THROW(ClientError, "Data inconsistency, '%s' in '%s' must be a subset of %s", RESERVED_ACCURACY, DATE_STR, repr(str_set_acc_date).c_str());
				}
			} else {
				set_acc.insert(def_accuracy_date.begin(), def_accuracy_date.end());
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
			break;
		}
		case FieldType::TEXT: {
			if (!specification.flags.has_index) {
				const auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
				if (specification.index != index) {
					specification.index = index;
					mut_properties[RESERVED_INDEX] = index;
				}
				specification.flags.has_index = true;
			}

			mut_properties[RESERVED_LANGUAGE] = specification.language;

			mut_properties[RESERVED_STOP_STRATEGY] = specification.stop_strategy;

			mut_properties[RESERVED_STEM_STRATEGY] = specification.stem_strategy;
			if (specification.aux_stem_lan.empty() && !specification.aux_lan.empty()) {
				specification.stem_language = specification.aux_lan;
			}
			mut_properties[RESERVED_STEM_LANGUAGE] = specification.stem_language;

			if (specification.aux_lan.empty() && !specification.aux_stem_lan.empty()) {
				specification.language = specification.aux_stem_lan;
			}
			break;
		}
		case FieldType::STRING: {
			if (!specification.flags.has_index) {
				const auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
				if (specification.index != index) {
					specification.index = index;
					mut_properties[RESERVED_INDEX] = index;
				}
				specification.flags.has_index = true;
			}
			break;
		}
		case FieldType::TERM: {
			if (!specification.flags.has_index) {
				const auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
				if (specification.index != index) {
					specification.index = index;
					mut_properties[RESERVED_INDEX] = index;
				}
				specification.flags.has_index = true;
			}

			// Process RESERVED_BOOL_TERM
			if (!specification.flags.has_bool_term) {
				// By default, if normalized name has upper characters then it is consider bool term.
				specification.flags.bool_term = strhasupper(specification.meta_name);
				specification.flags.has_bool_term = true;
			}
			mut_properties[RESERVED_BOOL_TERM] = static_cast<bool>(specification.flags.bool_term);
			break;
		}
		case FieldType::BOOLEAN:
		case FieldType::UUID:
			break;
		default:
			THROW(ClientError, "%s '%c' is not supported", RESERVED_TYPE, specification.sep_types[2]);
	}

	// Process RESERVED_ACCURACY and RESERVED_ACC_PREFIX
	if (set_acc.size()) {
		for (const auto& acc : set_acc) {
			specification.acc_prefix.push_back(get_prefix(acc));
		}
		specification.accuracy.insert(specification.accuracy.end(), set_acc.begin(), set_acc.end());
		mut_properties[RESERVED_ACCURACY]   = specification.accuracy;
		mut_properties[RESERVED_ACC_PREFIX] = specification.acc_prefix;
	}

	if (specification.flags.dynamic_type) {
		if (!specification.flags.has_index) {
			const auto index = specification.index & ~TypeIndex::VALUES; // Fallback to index anything but values
			if (specification.index != index) {
				specification.index = index;
				mut_properties[RESERVED_INDEX] = index;
			}
			specification.flags.has_index = true;
		}
	} else {
		// Process RESERVED_SLOT
		if (!specification.flags.dynamic_type_path) {
			if (specification.slot == Xapian::BAD_VALUENO) {
				specification.slot = get_slot(specification.prefix, specification.sep_types[2]);
			}
			mut_properties[RESERVED_SLOT] = specification.slot;
		}

		// If field is namespace fallback to index anything but values.
		if (!specification.flags.has_index && !specification.partial_prefixes.empty()) {
			const auto index = specification.index & ~TypeIndex::VALUES;
			if (specification.index != index) {
				specification.index = index;
				mut_properties[RESERVED_INDEX] = index;
			}
			specification.flags.has_index = true;
		}
	}

	// Process RESERVED_TYPE
	mut_properties[RESERVED_TYPE] = specification.sep_types;

	specification.flags.field_with_type = true;

	// L_DEBUG(this, "\nspecification = %s\nmut_properties = %s", specification.to_string().c_str(), mut_properties.to_string(true).c_str());
}


void
Schema::validate_required_namespace_data(const MsgPack& value)
{
	L_CALL(this, "Schema::validate_required_namespace_data(%s)", repr(value.to_string()).c_str());

	L_SCHEMA(this, "Specification heritable and sent by user: %s", specification.to_string().c_str());

	if (specification.sep_types[2] == FieldType::EMPTY) {
		guess_field_type(value);
	}

	switch (specification.sep_types[2]) {
		case FieldType::GEO:
			// Set partials and error.
			specification.flags.partials = default_spc.flags.partials;
			specification.error = default_spc.error;
			break;

		case FieldType::TEXT:
			if (!specification.flags.has_index) {
				specification.index &= ~TypeIndex::VALUES; // Fallback to index anything but values
				specification.flags.has_index = true;
			}

			specification.language = default_spc.language;

			specification.stop_strategy = default_spc.stop_strategy;

			specification.stem_strategy = default_spc.stem_strategy;
			specification.stem_language = default_spc.stem_language;
			break;

		case FieldType::STRING:
			if (!specification.flags.has_index) {
				specification.index &= ~TypeIndex::VALUES; // Fallback to index anything but values
				specification.flags.has_index = true;
			}
			break;

		case FieldType::TERM:
			if (!specification.flags.has_index) {
				specification.index &= ~TypeIndex::VALUES; // Fallback to index anything but values
				specification.flags.has_index = true;
			}

			if (!specification.flags.has_bool_term) {
				specification.flags.bool_term = strhasupper(specification.meta_name);
				specification.flags.has_bool_term = true;
			}
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


void
Schema::validate_required_data(const MsgPack& value)
{
	L_CALL(this, "Schema::validate_required_data(%s)", repr(value.to_string()).c_str());

	L_SCHEMA(this, "Specification heritable and sent by user: %s", specification.to_string().c_str());

	if (specification.sep_types[2] == FieldType::EMPTY) {
		if (XapiandManager::manager->strict || specification.flags.strict) {
			THROW(MissingTypeError, "Type of field %s is missing", repr(specification.full_meta_name).c_str());
		}
		guess_field_type(value);
	}

	_validate_required_data(get_mutable());
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
			const auto str_value = field.as_string();
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
			if (specification.flags.term_detection) {
				specification.sep_types[2] = FieldType::TERM;
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
				const auto cast_word = item_doc.begin()->as_string();
				specification.sep_types[2] = Cast::getType(cast_word);
				return;
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

	L_SCHEMA(this, "Final Specification: %s", specification.to_string().c_str());

	_index_item(doc, std::array<std::reference_wrapper<const MsgPack>, 1>({{ value }}), pos);
	if (specification.flags.store && add_value) {
		// Add value to data.
		auto& data_value = data[RESERVED_VALUE];
		if (specification.sep_types[2] == FieldType::UUID) {
			// Compact uuid value
			switch (data_value.getType()) {
				case MsgPack::Type::UNDEFINED:
					data_value = normalize_uuid(value.is_map() ? Cast::string(value) : value.as_string());
					break;
				case MsgPack::Type::ARRAY:
					data_value.push_back(normalize_uuid(value.is_map() ? Cast::string(value) : value.as_string()));
					break;
				default:
					data_value = MsgPack({ data_value, normalize_uuid(value.is_map() ? Cast::string(value) : value.as_string()) });
					break;
			}
		} else {
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
			if (specification.sep_types[2] == FieldType::UUID) {
				// Compact uuid value
				switch (data_value.getType()) {
					case MsgPack::Type::UNDEFINED:
						data_value = MsgPack(MsgPack::Type::ARRAY);
					case MsgPack::Type::ARRAY:
						for (const auto& value : values) {
							data_value.push_back(normalize_uuid(value.is_map() ? Cast::string(value) : value.as_string()));
						}
						break;
					default:
						data_value = MsgPack({ data_value });
						for (const auto& value : values) {
							data_value.push_back(normalize_uuid(value.is_map() ? Cast::string(value) : value.as_string()));
						}
						break;
				}
			} else {
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
		}
	} else {
		index_item(doc, values, data, 0, add_values);
	}
}


void
Schema::index_partial_paths(Xapian::Document& doc)
{
	L_CALL(this, "Schema::index_partial_paths(<Xapian::Document>)");

	if (specification.partial_prefixes.size() > 2) {
		const auto paths = get_partial_paths(specification.partial_prefixes);
		for (const auto& path : paths) {
			doc.add_term(path);
		}
	} else {
		doc.add_term(specification.prefix);
	}
}


template <typename T>
inline void
Schema::_index_item(Xapian::Document& doc, T&& values, size_t pos)
{
	L_CALL(this, "Schema::_index_item(<doc>, <values>, %zu)", pos);

	switch (specification.index) {
		case TypeIndex::NONE:
			return;

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
	L_CALL(nullptr, "Schema::index_term(<Xapian::Document>, %s, <specification_t>, %zu)", repr(serialise_val).c_str(), pos);

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
			const bool positions = field_spc.positions[getPos(pos, field_spc.positions.size())];
			if (positions) {
				term_generator.index_text(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix + field_spc.get_ctype());
			} else {
				term_generator.index_text_without_positions(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix + field_spc.get_ctype());
			}
			L_INDEX(nullptr, "Field Text to Index [%d] => %s:%s [Positions: %s]", pos, field_spc.prefix.c_str(), serialise_val.c_str(), positions ? "true" : "false");
			break;
		}

		case FieldType::STRING: {
			Xapian::TermGenerator term_generator;
			term_generator.set_document(doc);
			const auto position = field_spc.position[getPos(pos, field_spc.position.size())]; // String uses position (not positions) which is off by default
			if (position) {
				term_generator.index_text(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix + field_spc.get_ctype());
				L_INDEX(nullptr, "Field String to Index [%d] => %s:%s [Positions: %s]", pos, field_spc.prefix.c_str(), serialise_val.c_str(), position ? "true" : "false");
			} else {
				term_generator.index_text_without_positions(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix + field_spc.get_ctype());
				L_INDEX(nullptr, "Field String to Index [%d] => %s:%s", pos, field_spc.prefix.c_str(), serialise_val.c_str());
			}
			break;
		}

		case FieldType::TERM:
			if (!field_spc.flags.bool_term) {
				to_lower(serialise_val);
			}

		default: {
			serialise_val = prefixed(serialise_val, field_spc.prefix, field_spc.get_ctype());
			const auto weight = field_spc.flags.bool_term ? 0 : field_spc.weight[getPos(pos, field_spc.weight.size())];
			const auto position = field_spc.position[getPos(pos, field_spc.position.size())];
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
	L_CALL(nullptr, "Schema::index_all_term(<Xapian::Document>, %s, <specification_t>, <specification_t>, %zu)", repr(value.to_string()).c_str(), pos);

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
	L_CALL(nullptr, "Schema::index_value(<Xapian::Document>, %s, <StringSet>, <specification_t>, %zu, <specification_t*>, <specification_t*>)", repr(value.to_string()).c_str(), pos);

	switch (spc.sep_types[2]) {
		case FieldType::FLOAT: {
			try {
				const auto f_val = value.as_f64();
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
				const auto i_val = value.as_i64();
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
				const auto u_val = value.as_u64();
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
				const auto str_value = value.as_string();
				EWKT_Parser ewkt(str_value, spc.flags.partials, spc.error);
				auto ser_value = Serialise::trixels(ewkt.trixels);
				if (field_spc) {
					if (field_spc->flags.partials != spc.flags.partials || field_spc->error != spc.error) {
						EWKT_Parser f_ewkt(str_value, field_spc->flags.partials, field_spc->error);
						index_term(doc, Serialise::trixels(f_ewkt.trixels), *field_spc, pos);
					} else {
						index_term(doc, ser_value, *field_spc, pos);
					}
				}
				if (global_spc) {
					if (global_spc->flags.partials != spc.flags.partials || global_spc->error != spc.error) {
						EWKT_Parser g_ewkt(str_value, global_spc->flags.partials, global_spc->error);
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
	L_CALL(nullptr, "Schema::index_all_value(<Xapian::Document>, %s, <StringSet>, <StringSet>, <specification_t>, <specification_t>, %zu)", repr(value.to_string()).c_str(), pos);

	switch (field_spc.sep_types[2]) {
		case FieldType::FLOAT: {
			try {
				const auto f_val = value.as_f64();
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
				const auto i_val = value.as_i64();
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
				const auto u_val = value.as_u64();
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
				const auto str_ewkt = value.as_string();
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
Schema::update_schema(MsgPack*& mut_parent_properties, const MsgPack& obj_schema, const std::string& name)
{
	L_CALL(this, "Schema::update_schema(%s, %s, %s)", repr(mut_parent_properties->to_string()).c_str(), repr(obj_schema.to_string()).c_str(), repr(name).c_str());

	if (name.empty()) {
		THROW(ClientError, "Field name must not be empty");
	}

	const auto spc_start = specification;
	if (obj_schema.is_map()) {
		specification.name.assign(name);
		TaskVector tasks;
		auto mut_properties = mut_parent_properties;

		mut_properties = &get_subproperties(mut_properties, obj_schema, tasks);

		if (!specification.flags.field_with_type && specification.sep_types[2] != FieldType::EMPTY) {
			_validate_required_data(*mut_properties);
		}

		if (tasks.size() && specification.flags.inside_namespace) {
			THROW(ClientError, "An namespace object can not have children in Schema");
		}

		set_type_to_object(tasks.size());

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


inline void
Schema::update_partial_prefixes()
{
	L_CALL(this, "Schema::update_partial_prefixes()");

	if (specification.flags.partial_paths) {
		if (specification.partial_prefixes.empty()) {
			specification.partial_prefixes.push_back(specification.prefix);
		} else {
			specification.partial_prefixes.push_back(specification.local_prefix);
		}
	} else {
		specification.partial_prefixes.clear();
	}
}


void
Schema::get_subproperties(const MsgPack*& properties, const std::string& meta_name)
{
	L_CALL(this, "Schema::get_subproperties(%s, %s)", repr(properties->to_string()).c_str(), repr(meta_name).c_str());

	properties = &properties->at(meta_name);
	specification.flags.field_found = true;
	static const auto stit_e = map_stem_language.end();
	const auto stit = map_stem_language.find(meta_name);
	if (stit != stit_e && stit->second.first) {
		specification.language = stit->second.second;
		specification.aux_lan = stit->second.second;
	}

	if (specification.full_meta_name.empty()) {
		specification.full_meta_name.assign(meta_name);
	} else {
		specification.full_meta_name.append(DB_OFFSPRING_UNION).append(meta_name);
	}

	update_specification(*properties);
}


void
Schema::get_subproperties(MsgPack*& mut_properties, const std::string& meta_name)
{
	L_CALL(this, "Schema::get_subproperties(%s, %s)", repr(mut_properties->to_string()).c_str(), repr(meta_name).c_str());

	mut_properties = &mut_properties->at(meta_name);
	specification.flags.field_found = true;
	static const auto stit_e = map_stem_language.end();
	const auto stit = map_stem_language.find(meta_name);
	if (stit != stit_e && stit->second.first) {
		specification.language = stit->second.second;
		specification.aux_lan = stit->second.second;
	}

	update_specification(*mut_properties);
}


const MsgPack&
Schema::get_subproperties(const MsgPack*& properties, const MsgPack& object, MsgPack*& data, Xapian::Document& doc, TaskVector& tasks)
{
	L_CALL(this, "Schema::get_subproperties(%s, %s, <MsgPack*>, <Xapian::Document>, <tasks>)", repr(properties->to_string()).c_str(), repr(object.to_string()).c_str());

	const auto field_names = Split::split(specification.name, DB_OFFSPRING_UNION);

	const auto it_last = field_names.end() - 1;
	if (specification.flags.is_namespace) {
		restart_namespace_specification();
		for (auto it = field_names.begin(); it != it_last; ++it) {
			detect_dynamic(*it);
			specification.prefix.append(specification.local_prefix);
			update_partial_prefixes();
		}
		process_properties_document(properties, object, data, doc, tasks);
		detect_dynamic(*it_last);
		specification.prefix.append(specification.local_prefix);
		update_partial_prefixes();
		specification.flags.inside_namespace = true;
	} else {
		for (auto it = field_names.begin(); it != it_last; ++it) {
			const auto& field_name = *it;
			if ((!is_valid(field_name) || field_name == UUID_FIELD_NAME) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
				THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(field_name).c_str());
			}
			restart_specification();
			try {
				get_subproperties(properties, field_name);
				update_partial_prefixes();
			} catch (const std::out_of_range&) {
				detect_dynamic(field_name);
				if (specification.flags.dynamic_type) {
					try {
						get_subproperties(properties, specification.meta_name);
						specification.prefix.append(specification.local_prefix);
						update_partial_prefixes();
						continue;
					} catch (const std::out_of_range&) { }
				}

				auto mut_properties = &get_mutable();
				add_field(mut_properties);
				for (++it; it != it_last; ++it) {
					const auto& n_field_name = *it;
					if (!is_valid(n_field_name) || n_field_name == UUID_FIELD_NAME) {
						THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(n_field_name).c_str());
					} else {
						detect_dynamic(n_field_name);
						add_field(mut_properties);
					}
				}
				const auto& n_field_name = *it_last;
				if (!is_valid(n_field_name) || n_field_name == UUID_FIELD_NAME) {
					THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(n_field_name).c_str());
				} else {
					detect_dynamic(n_field_name);
					add_field(mut_properties, properties, object, data, doc, tasks);
				}
				return *mut_properties;
			}
		}

		const auto& field_name = *it_last;
		if ((!is_valid(field_name) || field_name == UUID_FIELD_NAME) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
			THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(field_name).c_str());
		}
		restart_specification();
		try {
			get_subproperties(properties, field_name);
			process_properties_document(properties, object, data, doc, tasks);
			update_partial_prefixes();
		} catch (const std::out_of_range&) {
			detect_dynamic(field_name);
			if (specification.flags.dynamic_type) {
				try {
					get_subproperties(properties, specification.meta_name);
					specification.prefix.append(specification.local_prefix);
					process_properties_document(properties, object, data, doc, tasks);
					update_partial_prefixes();
					return *properties;
				} catch (const std::out_of_range&) { }
			}

			auto mut_properties = &get_mutable();
			add_field(mut_properties, properties, object, data, doc, tasks);
			return *mut_properties;
		}
	}

	return *properties;
}


const MsgPack&
Schema::get_subproperties(const MsgPack*& properties)
{
	L_CALL(this, "Schema::get_subproperties(%s)", repr(properties->to_string()).c_str());

	Split _split(specification.name, DB_OFFSPRING_UNION);

	const auto it_e = _split.end();
	if (specification.flags.is_namespace) {
		restart_namespace_specification();
		for (auto it = _split.begin(); it != it_e; ++it) {
			detect_dynamic(*it);
			specification.prefix.append(specification.local_prefix);
			update_partial_prefixes();
		}
		specification.flags.inside_namespace = true;
	} else {
		for (auto it = _split.begin(); it != it_e; ++it) {
			const auto& field_name = *it;
			if ((!is_valid(field_name) || field_name == UUID_FIELD_NAME) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
				THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(field_name).c_str());
			}
			restart_specification();
			try {
				get_subproperties(properties, field_name);
				update_partial_prefixes();
			} catch (const std::out_of_range&) {
				detect_dynamic(field_name);
				if (specification.flags.dynamic_type) {
					try {
						get_subproperties(properties, specification.meta_name);
						specification.prefix.append(specification.local_prefix);
						update_partial_prefixes();
						continue;
					} catch (const std::out_of_range&) { }
				}

				auto mut_properties = &get_mutable();
				add_field(mut_properties);
				for (++it; it != it_e; ++it) {
					const auto& n_field_name = *it;
					if (!is_valid(n_field_name) || n_field_name == UUID_FIELD_NAME) {
						THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(n_field_name).c_str());
					} else {
						detect_dynamic(n_field_name);
						add_field(mut_properties);
					}
				}
				return *mut_properties;
			}
		}
	}

	return *properties;
}


MsgPack&
Schema::get_subproperties(MsgPack*& mut_properties, const MsgPack& object, TaskVector& tasks)
{
	L_CALL(this, "Schema::get_subproperties(%s, %s, <tasks>)", repr(mut_properties->to_string()).c_str(), repr(object.to_string()).c_str());

	auto field_names = Split::split(specification.name, DB_OFFSPRING_UNION);

	const auto it_last = field_names.end() - 1;
	for (auto it = field_names.begin(); it != it_last; ++it) {
		const auto& field_name = *it;
		if (!is_valid(field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
			THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(field_name).c_str());
		}
		restart_specification();
		try {
			get_subproperties(mut_properties, field_name);
		} catch (const std::out_of_range&) {
			mut_properties = &(*mut_properties)[field_name];
			for ( ; it != it_last; ++it) {
				specification.meta_name = *it;
				if (!is_valid(specification.meta_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(specification.meta_name))) {
					THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(specification.meta_name).c_str());
				} else {
					specification.flags.dynamic_type = (specification.meta_name == UUID_FIELD_NAME);
					add_field(mut_properties);
				}
			}

			specification.meta_name = *it_last;
			if (!is_valid(specification.meta_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(specification.meta_name))) {
				THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(specification.meta_name).c_str());
			} else {
				specification.flags.dynamic_type = (specification.meta_name == UUID_FIELD_NAME);
				add_field(mut_properties, object, tasks);
			}

			return *mut_properties;
		}
	}

	const auto& field_name = *it_last;
	if (!is_valid(field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
		THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(field_name).c_str());
	}
	restart_specification();
	try {
		get_subproperties(mut_properties, field_name);
		process_properties_document(mut_properties, object, tasks);
	} catch (const std::out_of_range&) {
		mut_properties = &(*mut_properties)[field_name];
		if (!is_valid(field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
			THROW(ClientError, "Field name: %s (%s) is not valid", repr(specification.name).c_str(), repr(field_name).c_str());
		} else {
			specification.meta_name = field_name;
			specification.flags.dynamic_type = (specification.meta_name == UUID_FIELD_NAME);
			add_field(mut_properties, object, tasks);
		}
	}
	return *mut_properties;
}


inline void
Schema::detect_dynamic(const std::string& field_name)
{
	L_CALL(this, "Schema::detect_dynamic(%s)", repr(field_name).c_str());

	try {
		auto ser_uuid = Serialise::uuid(field_name);
		specification.local_prefix.assign(ser_uuid);
		specification.meta_name.assign(UUID_FIELD_NAME);
		specification.flags.dynamic_type = true;
		specification.flags.dynamic_type_path = true;
	} catch (const SerialisationError&) {
		specification.local_prefix.assign(get_prefix(field_name));
		specification.meta_name.assign(field_name);
		specification.flags.dynamic_type = false;
	}
}


void
Schema::process_properties_document(const MsgPack*& properties, const MsgPack& object, MsgPack*& data, Xapian::Document& doc, TaskVector& tasks)
{
	L_CALL(this, "Schema::process_properties_document(%s, %s, <MsgPack*>, <Xapian::Document>, <TaskVector>)", repr(properties->to_string()).c_str(), repr(object.to_string()).c_str());

	static const auto ddit_e = map_dispatch_document.end();
	if (specification.flags.field_with_type) {
		for (const auto& item_key : object) {
			auto str_key = item_key.as_string();
			const auto ddit = map_dispatch_document.find(str_key);
			if (ddit == ddit_e) {
				if (!set_reserved_words.count(str_key)) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(data), std::ref(doc), std::move(str_key)));
				}
			} else {
				(this->*ddit->second)(str_key, object.at(str_key));
			}
		}
	} else {
		static const auto wtit_e = map_dispatch_without_type.end();
		for (const auto& item_key : object) {
			auto str_key = item_key.as_string();
			const auto ddit = map_dispatch_document.find(str_key);
			if (ddit == ddit_e) {
				const auto wtit = map_dispatch_without_type.find(str_key);
				if (wtit == wtit_e) {
					if (!set_reserved_words.count(str_key)) {
						tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(data), std::ref(doc), std::move(str_key)));
					}
				} else {
					(this->*wtit->second)(str_key, object.at(str_key));
				}
			} else {
				(this->*ddit->second)(str_key, object.at(str_key));
			}
		}
	}
}


void
Schema::process_properties_document(MsgPack*& mut_properties, const MsgPack& object, TaskVector& tasks)
{
	L_CALL(this, "Schema::process_properties_document(%s, %s, <TaskVector>)", repr(mut_properties->to_string()).c_str(), repr(object.to_string()).c_str());

	static const auto wpit_e = map_dispatch_write_properties.end();
	if (specification.flags.field_with_type) {
		for (const auto& item_key : object) {
			auto str_key = item_key.as_string();
			const auto wpit = map_dispatch_write_properties.find(str_key);
			if (wpit == wpit_e) {
				if (!set_reserved_words.count(str_key)) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::update_schema, this, std::ref(mut_properties), std::ref(object.at(str_key)), std::move(str_key)));
				}
			} else {
				(this->*wpit->second)(*mut_properties, str_key, object.at(str_key), false);
			}
		}
	} else {
		static const auto wtit_e = map_dispatch_without_type.end();
		for (const auto& item_key : object) {
			auto str_key = item_key.as_string();
			const auto wpit = map_dispatch_write_properties.find(str_key);
			if (wpit == wpit_e) {
				const auto wtit = map_dispatch_without_type.find(str_key);
				if (wtit == wtit_e) {
					if (!set_reserved_words.count(str_key)) {
						tasks.push_back(std::async(std::launch::deferred, &Schema::update_schema, this, std::ref(mut_properties), std::ref(object.at(str_key)), std::move(str_key)));
					}
				} else {
					(this->*wtit->second)(str_key, object.at(str_key));
				}
			} else {
				(this->*wpit->second)(*mut_properties, str_key, object.at(str_key), false);
			}
		}
	}
}


void
Schema::add_field(MsgPack*& mut_properties, const MsgPack*& properties, const MsgPack& object, MsgPack*& data, Xapian::Document& doc, TaskVector& tasks)
{
	L_CALL(this, "Schema::add_field(%s, %s, %s, <MsgPack*>, <Xapian::Document>, <TaskVector>)", repr(mut_properties->to_string()).c_str(), repr(object.to_string()).c_str());

	specification.flags.field_found = false;

	mut_properties = &(*mut_properties)[specification.meta_name];

	static const auto slit_e = map_stem_language.end();
	const auto slit = map_stem_language.find(specification.meta_name);
	if (slit != slit_e && slit->second.first) {
		specification.language = slit->second.second;
		specification.aux_lan = slit->second.second;
	}

	if (specification.full_meta_name.empty()) {
		specification.full_meta_name.assign(specification.meta_name);
	} else {
		specification.full_meta_name.append(DB_OFFSPRING_UNION).append(specification.meta_name);
	}

	// Write obj specifications.
	static const auto wpit_e = map_dispatch_write_properties.end();
	static const auto ddit_e = map_dispatch_document.end();
	static const auto wtit_e = map_dispatch_without_type.end();
	for (const auto& item_key : object) {
		auto str_key = item_key.as_string();
		const auto wpit = map_dispatch_write_properties.find(str_key);
		if (wpit == wpit_e) {
			const auto ddit = map_dispatch_document.find(str_key);
			if (ddit == ddit_e) {
				const auto wtit = map_dispatch_without_type.find(str_key);
				if (wtit == wtit_e) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(data), std::ref(doc), std::move(str_key)));
				} else {
					(this->*wtit->second)(str_key, object.at(str_key));
				}
			} else {
				(this->*ddit->second)(str_key, object.at(str_key));
			}
		} else {
			(this->*wpit->second)(*mut_properties, str_key, object.at(str_key), false);
		}
	}

	// Load default specifications.
	static const auto dsit_e = map_dispatch_set_default_spc.end();
	const auto dsit = map_dispatch_set_default_spc.find(specification.full_meta_name);
	if (dsit != dsit_e) {
		(this->*dsit->second)(*mut_properties);
	}

	// Write prefix in properties.
	if (!specification.flags.dynamic_type) {
		(*mut_properties)[RESERVED_PREFIX] = specification.local_prefix;
	}
	specification.prefix.append(specification.local_prefix);
	update_partial_prefixes();
}


void
Schema::add_field(MsgPack*& mut_properties, const MsgPack& object, TaskVector& tasks)
{
	L_CALL(this, "Schema::add_field(%s, %s, <TaskVector>)", repr(mut_properties->to_string()).c_str(), repr(object.to_string()).c_str());

	mut_properties = &(*mut_properties)[specification.meta_name];

	static const auto slit_e = map_stem_language.end();
	const auto slit = map_stem_language.find(specification.meta_name);
	if (slit != slit_e && slit->second.first) {
		specification.language = slit->second.second;
		specification.aux_lan = slit->second.second;
	}

	// Write obj specifications.
	static const auto wpit_e = map_dispatch_write_properties.end();
	static const auto ddit_e = map_dispatch_document.end();
	static const auto wtit_e = map_dispatch_without_type.end();
	for (const auto& item_key : object) {
		auto str_key = item_key.as_string();
		const auto wpit = map_dispatch_write_properties.find(str_key);
		if (wpit == wpit_e) {
			const auto ddit = map_dispatch_document.find(str_key);
			if (ddit == ddit_e) {
				const auto wtit = map_dispatch_without_type.find(str_key);
				if (wtit == wtit_e) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::update_schema, this, std::ref(mut_properties), std::ref(object.at(str_key)), std::move(str_key)));
				} else {
					(this->*wtit->second)(str_key, object.at(str_key));
				}
			} else {
				(this->*ddit->second)(str_key, object.at(str_key));
			}
		} else {
			(this->*wpit->second)(*mut_properties, str_key, object.at(str_key), false);
		}
	}

	// Load default specifications.
	static const auto dsit_e = map_dispatch_set_default_spc.end();
	const auto dsit = map_dispatch_set_default_spc.find(specification.full_meta_name);
	if (dsit != dsit_e) {
		(this->*dsit->second)(*mut_properties);
	}

	// Write prefix in properties.
	if (!specification.flags.dynamic_type) {
		(*mut_properties)[RESERVED_PREFIX] = specification.local_prefix;
	}
}


void
Schema::add_field(MsgPack*& mut_properties)
{
	L_CALL(this, "Schema::add_field(%s)", repr(mut_properties->to_string()).c_str());

	mut_properties = &(*mut_properties)[specification.meta_name];

	static const auto slit_e = map_stem_language.end();
	const auto slit = map_stem_language.find(specification.meta_name);
	if (slit != slit_e && slit->second.first) {
		specification.language = slit->second.second;
		specification.aux_lan = slit->second.second;
	}

	if (specification.full_meta_name.empty()) {
		specification.full_meta_name.assign(specification.meta_name);
	} else {
		specification.full_meta_name.append(DB_OFFSPRING_UNION).append(specification.meta_name);
	}

	// Load default specifications.
	static const auto dsit_e = map_dispatch_set_default_spc.end();
	const auto dsit = map_dispatch_set_default_spc.find(specification.full_meta_name);
	if (dsit != dsit_e) {
		(this->*dsit->second)(*mut_properties);
	}

	// Write prefix in properties.
	if (!specification.flags.dynamic_type) {
		(*mut_properties)[RESERVED_PREFIX] = specification.local_prefix;
	}
	specification.prefix.append(specification.local_prefix);
	update_partial_prefixes();
}


void
Schema::update_specification(const MsgPack& properties)
{
	L_CALL(this, "Schema::update_specification(%s)", repr(properties.to_string()).c_str());

	static const auto dpit_e = map_dispatch_properties.end();
	for (const auto& property : properties) {
		const auto str_prop = property.as_string();
		const auto dpit = map_dispatch_properties.find(str_prop);
		if (dpit != dpit_e) {
			(this->*dpit->second)(properties.at(str_prop));
		}
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

	specification.local_prefix = prop_prefix.as_string();
	specification.prefix.append(specification.local_prefix);
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
Schema::update_tm_detection(const MsgPack& prop_tm_detection)
{
	L_CALL(this, "Schema::update_tm_detection(%s)", repr(prop_tm_detection.to_string()).c_str());

	specification.flags.term_detection = prop_tm_detection.as_bool();
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

	specification.flags.is_namespace = prop_namespace.as_bool();
	specification.flags.has_namespace = true;
}


void
Schema::update_partial_paths(const MsgPack& prop_partial_paths)
{
	L_CALL(this, "Schema::update_partial_paths(%s)", repr(prop_partial_paths.to_string()).c_str());

	specification.flags.partial_paths = prop_partial_paths.as_bool();
	specification.flags.has_partial_paths = true;
}


void
Schema::write_position(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_position, bool)
{
	// RESERVED_POSITION is heritable and can change between documents.
	L_CALL(this, "Schema::write_position(%s)", repr(doc_position.to_string()).c_str());

	process_position(prop_name, doc_position);
	properties[prop_name] = specification.position;
}


void
Schema::write_weight(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_weight, bool)
{
	// RESERVED_WEIGHT property is heritable and can change between documents.
	L_CALL(this, "Schema::write_weight(%s)", repr(doc_weight.to_string()).c_str());

	process_weight(prop_name, doc_weight);
	properties[prop_name] = specification.weight;
}


void
Schema::write_spelling(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_spelling, bool)
{
	// RESERVED_SPELLING is heritable and can change between documents.
	L_CALL(this, "Schema::write_spelling(%s)", repr(doc_spelling.to_string()).c_str());

	process_spelling(prop_name, doc_spelling);
	properties[prop_name] = specification.spelling;
}


void
Schema::write_positions(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_positions, bool)
{
	// RESERVED_POSITIONS is heritable and can change between documents.
	L_CALL(this, "Schema::write_positions(%s)", repr(doc_positions.to_string()).c_str());

	process_positions(prop_name, doc_positions);
	properties[prop_name] = specification.positions;
}


void
Schema::write_index(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_index, bool)
{
	// RESERVED_INDEX is heritable and can change.
	L_CALL(this, "Schema::write_index(%s)", repr(doc_index.to_string()).c_str());

	process_index(prop_name, doc_index);
	properties[prop_name] = specification.index;
}


void
Schema::write_store(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_store, bool)
{
	L_CALL(this, "Schema::write_store(%s)", repr(doc_store.to_string()).c_str());

	/*
	 * RESERVED_STORE is heritable and can change, but once fixed in false
	 * it cannot change in its offsprings.
	 */

	process_store(prop_name, doc_store);
	properties[prop_name] = doc_store.as_bool();
}


void
Schema::write_recursive(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_recursive, bool)
{
	L_CALL(this, "Schema::write_recursive(%s)", repr(doc_recursive.to_string()).c_str());

	/*
	 * RESERVED_RECURSIVE is heritable and can change, but once fixed in false
	 * it does not process its children.
	 */

	process_recursive(prop_name, doc_recursive);
	properties[prop_name] = static_cast<bool>(specification.flags.is_recursive);
}


void
Schema::write_dynamic(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_dynamic, bool)
{
	// RESERVED_DYNAMIC is heritable but can't change.
	L_CALL(this, "Schema::write_dynamic(%s)", repr(doc_dynamic.to_string()).c_str());

	try {
		specification.flags.dynamic = doc_dynamic.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.dynamic);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_strict(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_strict, bool)
{
	// RESERVED_STRICT is heritable but can't change.
	L_CALL(this, "Schema::write_strict(%s)", repr(doc_strict.to_string()).c_str());

	try {
		specification.flags.strict = doc_strict.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.strict);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_d_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_d_detection, bool)
{
	// RESERVED_D_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_d_detection(%s)", repr(doc_d_detection.to_string()).c_str());

	try {
		specification.flags.date_detection = doc_d_detection.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.date_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_n_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_n_detection, bool)
{
	// RESERVED_N_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_n_detection(%s)", repr(doc_n_detection.to_string()).c_str());

	try {
		specification.flags.numeric_detection = doc_n_detection.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.numeric_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_g_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_g_detection, bool)
{
	// RESERVED_G_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_g_detection(%s)", repr(doc_g_detection.to_string()).c_str());

	try {
		specification.flags.geo_detection = doc_g_detection.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.geo_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_b_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_b_detection, bool)
{
	// RESERVED_B_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_b_detection(%s)", repr(doc_b_detection.to_string()).c_str());

	try {
		specification.flags.bool_detection = doc_b_detection.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.bool_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_s_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_s_detection, bool)
{
	// RESERVED_S_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_s_detection(%s)", repr(doc_s_detection.to_string()).c_str());

	try {
		specification.flags.string_detection = doc_s_detection.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.string_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_t_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_t_detection, bool)
{
	// RESERVED_T_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_t_detection(%s)", repr(doc_t_detection.to_string()).c_str());

	try {
		specification.flags.text_detection = doc_t_detection.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.text_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_tm_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_tm_detection, bool)
{
	// RESERVED_TM_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_tm_detection(%s)", repr(doc_tm_detection.to_string()).c_str());

	try {
		specification.flags.term_detection = doc_tm_detection.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.term_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_u_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_u_detection, bool)
{
	// RESERVED_U_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_u_detection(%s)", repr(doc_u_detection.to_string()).c_str());

	try {
		specification.flags.uuid_detection = doc_u_detection.as_bool();
		properties[prop_name] = static_cast<bool>(specification.flags.uuid_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_namespace(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_namespace, bool is_root)
{
	// RESERVED_NAMESPACE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::write_namespace(%s)", repr(doc_namespace.to_string()).c_str());

	try {
		// Only save in Schema if RESERVED_NAMESPACE is true.
		specification.flags.is_namespace = doc_namespace.as_bool();
		if (specification.flags.is_namespace && !specification.flags.has_partial_paths) {
			specification.flags.partial_paths = specification.flags.partial_paths || !default_spc.flags.optimal;
		}
		specification.flags.has_namespace = true;
		if (!is_root) {
			properties[prop_name] = static_cast<bool>(specification.flags.is_namespace);
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_partial_paths(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_partial_paths, bool)
{
	L_CALL(this, "Schema::write_partial_paths(%s)", repr(doc_partial_paths.to_string()).c_str());

	/*
	 * RESERVED_PARTIAL_PATHS is heritable and can change.
	 */

	process_partial_paths(prop_name, doc_partial_paths);
	properties[prop_name] = static_cast<bool>(specification.flags.partial_paths);
}


void
Schema::process_language(const std::string& prop_name, const MsgPack& doc_language)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_language(%s)", repr(doc_language.to_string()).c_str());

	try {
		const auto _str_language = lower_string(doc_language.as_string());
		static const auto slit_e = map_stem_language.end();
		const auto slit = map_stem_language.find(_str_language);
		if (slit != slit_e && slit->second.first) {
			specification.language = slit->second.second;
			specification.aux_lan = slit->second.second;
			return;
		}

		THROW(ClientError, "%s: %s is not supported", repr(prop_name).c_str(), repr(_str_language).c_str());
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", repr(prop_name).c_str());
	}
}


void
Schema::process_stop_strategy(const std::string& prop_name, const MsgPack& doc_stop_strategy)
{
	// RESERVED_STOP_STRATEGY isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_stop_strategy(%s)", repr(doc_stop_strategy.to_string()).c_str());

	try {
		const auto _stop_strategy = lower_string(doc_stop_strategy.as_string());
		static const auto ssit_e = map_stop_strategy.end();
		const auto ssit = map_stop_strategy.find(_stop_strategy);
		if (ssit == ssit_e) {
			THROW(ClientError, "%s can be in %s (%s not supported)", prop_name.c_str(), str_set_stop_strategy.c_str(), _stop_strategy.c_str());
		} else {
			specification.stop_strategy = ssit->second;
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

	try {
		const auto _stem_strategy = lower_string(doc_stem_strategy.as_string());
		static const auto ssit_e = map_stem_strategy.end();
		const auto ssit = map_stem_strategy.find(_stem_strategy);
		if (ssit == ssit_e) {
			THROW(ClientError, "%s can be in %s (%s not supported)", prop_name.c_str(), str_set_stem_strategy.c_str(), _stem_strategy.c_str());
		} else {
			specification.stem_strategy = ssit->second;
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

	try {
		const auto _stem_language = lower_string(doc_stem_language.as_string());
		static const auto slit_e = map_stem_language.end();
		const auto slit = map_stem_language.find(_stem_language);
		if (slit == slit_e) {
			THROW(ClientError, "%s: %s is not supported", repr(prop_name).c_str(), repr(_stem_language).c_str());
		} else {
			specification.stem_language = _stem_language;
			specification.aux_stem_lan = slit->second.second;
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

	try {
		const auto str_type = lower_string(doc_type.as_string());

		if (str_type.empty()) {
			THROW(ClientError, "%s must be in { object, array, [object/][array/]< %s > }", prop_name.c_str(), str_set_type.c_str());
		}

		static const auto tit_e = map_type.end();
		auto tit = map_type.find(str_type);
		if (tit != tit_e) {
			specification.sep_types[2] = tit->second;
		} else {
			const auto tokens = Split::split(str_type, "/");
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
							tit = map_type.find(tokens[1]);
							if (tit != tit_e) {
								specification.sep_types[2] = tit->second;
								return;
							}
						}
					} else if (tokens[0] == ARRAY_STR) {
						specification.sep_types[1] = FieldType::ARRAY;
						tit = map_type.find(tokens[1]);
						if (tit != tit_e) {
							specification.sep_types[2] = tit->second;
							return;
						}
					}
					break;

				case 3:
					if (tokens[0] == OBJECT_STR && tokens[1] == ARRAY_STR) {
						specification.sep_types[0] = FieldType::OBJECT;
						specification.sep_types[1] = FieldType::ARRAY;
						tit = map_type.find(tokens[2]);
						if (tit != tit_e) {
							specification.sep_types[2] = tit->second;
							return;
						}
					}
					break;
			}
			THROW(ClientError, "%s must be in { object, array, [object/][array/]< %s > }", prop_name.c_str(), str_set_type.c_str());
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
Schema::process_bool_term(const std::string& prop_name, const MsgPack& doc_bool_term)
{
	// RESERVED_BOOL_TERM isn't heritable and can't change.
	L_CALL(this, "Schema::process_bool_term(%s)", repr(doc_bool_term.to_string()).c_str());

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

	try {
		specification.error = doc_error.as_f64();
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a double", prop_name.c_str());
	}
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
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a boolean or a not-empty array of booleans", prop_name.c_str());
	}
}


void
Schema::process_index(const std::string& prop_name, const MsgPack& doc_index)
{
	// RESERVED_INDEX is heritable and can change.
	L_CALL(this, "Schema::process_index(%s)", repr(doc_index.to_string()).c_str());

	try {
		const auto str_index = lower_string(doc_index.as_string());
		static const auto miit_e = map_index.end();
		const auto miit = map_index.find(str_index);
		if (miit == miit_e) {
			THROW(ClientError, "%s must be in %s (%s not supported)", prop_name.c_str(), str_set_index.c_str(), str_index.c_str());
		} else {
			specification.index = miit->second;
			specification.flags.has_index = true;
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
		specification.flags.store = specification.flags.parent_store && doc_store.as_bool();
		specification.flags.parent_store = specification.flags.store;
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
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_partial_paths(const std::string& prop_name, const MsgPack& doc_partial_paths)
{
	L_CALL(this, "Schema::process_partial_paths(%s)", repr(doc_partial_paths.to_string()).c_str());

	/*
	 * RESERVED_PARTIAL_PATHS is heritable and can change.
	 */

	try {
		specification.flags.partial_paths = doc_partial_paths.as_bool();
		specification.flags.has_partial_paths = true;
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

	if (specification.value_rec) {
		THROW(ClientError, "Only one cast object can be defined");
	} else {
		specification.value_rec = std::make_unique<MsgPack>();
		(*specification.value_rec)[prop_name] = doc_cast_object;
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
		const auto index = specification.index | TypeIndex::FIELD_ALL;  // force field_all
		if (specification.index != index) {
			specification.index = index;
			properties[RESERVED_INDEX] = index;
		}
		specification.flags.has_index = true;
	}

	// ID_FIELD_NAME can not be TEXT nor STRING.
	if (specification.sep_types[2] == FieldType::TEXT || specification.sep_types[2] == FieldType::STRING) {
		specification.sep_types[2] = FieldType::TERM;
	}

	// Set default prefix
	specification.local_prefix = DOCUMENT_ID_TERM_PREFIX;

	// Set default RESERVED_SLOT
	specification.slot = DB_SLOT_ID;
}


void
Schema::set_default_spc_ct(MsgPack& properties)
{
	L_CALL(this, "Schema::set_default_spc_ct(%s)", repr(properties.to_string()).c_str());

	if (!specification.flags.has_index) {
		const auto index = (specification.index | TypeIndex::FIELD_VALUES) & ~TypeIndex::FIELD_TERMS; // Fallback to index anything but values
		if (specification.index != index) {
			specification.index = index;
			properties[RESERVED_INDEX] = index;
		}
		specification.flags.has_index = true;
	}

	// RESERVED_TYPE by default is TERM
	if (specification.sep_types[2] == FieldType::EMPTY) {
		specification.sep_types[2] = FieldType::TERM;
	}

	// Set default prefix
	specification.local_prefix = DOCUMENT_CONTENT_TYPE_TERM_PREFIX;

	// set default slot
	specification.slot = DB_SLOT_CONTENT_TYPE;

	if (!specification.flags.has_namespace) {
		specification.flags.is_namespace = true;
		specification.flags.has_namespace = true;
		properties[RESERVED_NAMESPACE] = true;
	}

	if (specification.flags.is_namespace && !specification.flags.has_partial_paths) {
		specification.flags.partial_paths = specification.flags.partial_paths || !default_spc.flags.optimal;
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
	static const auto drit_e = map_dispatch_readable.end();
	for (auto it = item_schema.begin(); it != item_schema.end(); ) {
		const auto str_key = it->as_string();
		const auto drit = map_dispatch_readable.find(str_key);
		if (drit == drit_e) {
			if (is_valid(str_key) || (is_root && map_dispatch_set_default_spc.count(str_key))) {
				readable(item_schema.at(str_key), false);
			}
		} else {
			if (!(*drit->second)(item_schema.at(str_key), item_schema)) {
				it = item_schema.erase(it);
				continue;
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
Schema::readable_stem_language(MsgPack& prop_stem_language, MsgPack& properties)
{
	L_CALL(nullptr, "Schema::readable_stem_language(%s)", repr(prop_stem_language.to_string()).c_str());

	const auto language = properties[RESERVED_LANGUAGE].as_string();
	const auto stem_language = prop_stem_language.as_string();

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
Schema::readable_acc_prefix(MsgPack& prop_acc_prefix, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_acc_prefix(%s)", repr(prop_acc_prefix.to_string()).c_str());

	for (auto& prop_prefix : prop_acc_prefix) {
		prop_prefix = prop_prefix.as_string();
	}

	return true;
}


std::shared_ptr<const MsgPack>
Schema::get_modified_schema()
{
	L_CALL(this, "Schema::get_modified_schema()");

	if (mut_schema) {
		auto m_schema = std::shared_ptr<const MsgPack>(mut_schema.release());
		m_schema->lock();
		return m_schema;
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
		specification = default_spc;

		MsgPack data;
		TaskVector tasks;
		auto properties = mut_schema ? &mut_schema->at(RESERVED_SCHEMA) : &schema->at(RESERVED_SCHEMA);
		auto data_ptr = &data;

		if (*properties) {
			update_specification(*properties);
			static const auto ddit_e = map_dispatch_document.end();
			for (const auto& item_key : object) {
				auto str_key = item_key.as_string();
				const auto ddit = map_dispatch_document.find(str_key);
				if (ddit == ddit_e) {
					if (!set_reserved_words.count(str_key)) {
						tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(data_ptr), std::ref(doc), std::move(str_key)));
					}
				} else {
					(this->*ddit->second)(str_key, object.at(str_key));
				}
			}
		} else {
			auto mut_properties = &get_mutable();
			static const auto wpit_e = map_dispatch_write_properties.end();
			static const auto ddit_e = map_dispatch_document.end();
			for (const auto& item_key : object) {
				auto str_key = item_key.as_string();
				const auto wpit = map_dispatch_write_properties.find(str_key);
				if (wpit == wpit_e) {
					const auto ddit = map_dispatch_document.find(str_key);
					if (ddit == ddit_e) {
						if (!set_reserved_words.count(str_key)) {
							tasks.push_back(std::async(std::launch::deferred, &Schema::index_object, this, std::ref(properties), std::ref(object.at(str_key)), std::ref(data_ptr), std::ref(doc), std::move(str_key)));
						}
					} else {
						(this->*ddit->second)(str_key, object.at(str_key));
					}
				} else {
					(this->*wpit->second)(*mut_properties, str_key, object.at(str_key), true);
				}
			}
			properties = &*mut_properties;
		}

		restart_specification();
		const auto spc_start = std::move(specification);
		for (auto& task : tasks) {
			specification = spc_start;
			task.get();
		}

		for (const auto& elem : map_values) {
			const auto val_ser = elem.second.serialise();
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
		specification = default_spc;

		TaskVector tasks;
		auto mut_properties = replace ? &clear() : &get_mutable();

		static const auto wpit_e = map_dispatch_write_properties.end();
		for (const auto& item_key : obj_schema) {
			auto str_key = item_key.as_string();
			const auto wpit = map_dispatch_write_properties.find(str_key);
			if (wpit == wpit_e) {
				if (!set_reserved_words.count(str_key)) {
					tasks.push_back(std::async(std::launch::deferred, &Schema::update_schema, this, std::ref(mut_properties), std::ref(obj_schema.at(str_key)), std::move(str_key)));
				}
			} else {
				(this->*wpit->second)(*mut_properties, str_key, obj_schema.at(str_key), true);
			}
		}

		restart_specification();
		const auto spc_start = std::move(specification);
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
		res.sep_types[2] = (FieldType)properties.at(RESERVED_TYPE).at(2).as_u64();
		res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).as_u64());
		res.prefix = properties.at(RESERVED_PREFIX).as_string();
		// Get required specification.
		switch (res.sep_types[2]) {
			case FieldType::GEO:
				res.flags.partials = properties.at(RESERVED_PARTIALS).as_bool();
				res.error = properties.at(RESERVED_ERROR).as_f64();
				break;
			case FieldType::TERM:
				res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).as_bool();
				break;
			default:
				break;
		}
	} catch (const std::out_of_range&) { }

	return res;
}


std::pair<required_spc_t, std::string>
Schema::get_data_field(const std::string& field_name, bool is_range) const
{
	L_CALL(this, "Schema::get_data_field(%s, %d)", repr(field_name).c_str(), is_range);

	required_spc_t res;

	if (field_name.empty()) {
		return std::make_pair(res, std::string());
	}

	try {
		auto info = get_dynamic_subproperties(schema->at(RESERVED_SCHEMA), field_name);

		res.flags.inside_namespace = std::get<2>(info);
		res.prefix = std::move(std::get<3>(info));

		auto& acc_field = std::get<4>(info);
		if (!acc_field.empty()) {
			res.sep_types[2] = std::get<5>(info);
			return std::make_pair(res, std::move(acc_field));
		}

		if (!res.flags.inside_namespace) {
			const auto& properties = std::get<0>(info);

			res.sep_types[2] = (FieldType)properties.at(RESERVED_TYPE).at(2).as_u64();
			if (res.sep_types[2] == FieldType::EMPTY) {
				return std::make_pair(std::move(res), std::string());
			}

			if (is_range) {
				if (std::get<1>(info)) {
					res.slot = get_slot(res.prefix, res.sep_types[2]);
				} else {
					res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).as_u64());
				}

				// Get required specification.
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
							res.acc_prefix.push_back(res.prefix + acc_p.as_string());
						}
						break;
					case FieldType::TEXT:
						res.language = properties.at(RESERVED_LANGUAGE).as_string();
						res.stop_strategy = (StopStrategy)properties.at(RESERVED_STOP_STRATEGY).as_u64();
						res.stem_strategy = (StemStrategy)properties.at(RESERVED_STEM_STRATEGY).as_u64();
						res.stem_language = properties.at(RESERVED_STEM_LANGUAGE).as_string();
						break;
					case FieldType::TERM:
						res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).as_bool();
						break;
					default:
						break;
				}
			} else {
				// Get required specification.
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
					case FieldType::TERM:
						res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).as_bool();
						break;
					default:
						break;
				}
			}
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
		res.flags.inside_namespace = std::get<2>(info);

		auto& acc_field = std::get<4>(info);
		if (!acc_field.empty()) {
			THROW(ClientError, "Field name: %s is an accuracy, therefore does not have slot", repr(field_name).c_str());
		}

		if (res.flags.inside_namespace) {
			res.sep_types[2] = FieldType::TERM;
			res.slot = get_slot(std::get<3>(info), res.sep_types[2]);
		} else {
			const auto& properties = std::get<0>(info);

			const auto& sep_types = properties.at(RESERVED_TYPE);
			res.sep_types[2] = (FieldType)sep_types.at(2).as_u64();

			if (std::get<1>(info)) {
				res.slot = get_slot(std::get<3>(info), res.sep_types[2]);
			} else {
				res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).as_u64());
			}

			// Get required specification.
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
				case FieldType::TERM:
					res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).as_bool();
					break;
				default:
					break;
			}
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


std::tuple<const MsgPack&, bool, bool, std::string, std::string, FieldType>
Schema::get_dynamic_subproperties(const MsgPack& properties, const std::string& full_name) const
{
	L_CALL(this, "Schema::get_dynamic_subproperties(%s, %s)", repr(properties.to_string()).c_str(), repr(full_name).c_str());

	Split _split(full_name, DB_OFFSPRING_UNION);

	const MsgPack* subproperties = &properties;
	std::string prefix;
	bool dynamic_type_path = false;

	const auto it_e = _split.end();
	const auto it_b = _split.begin();
	for (auto it = it_b; it != it_e; ++it) {
		auto& field_name = *it;
		if (!is_valid(field_name) || field_name == UUID_FIELD_NAME) {
			// Check if the field_name is accuracy.
			if (it == it_b) {
				if (!map_dispatch_set_default_spc.count(field_name)) {
					if (++it == it_e) {
						auto acc_data = get_acc_data(field_name);
						prefix.append(acc_data.first);
						return std::forward_as_tuple(*subproperties, dynamic_type_path, false, std::move(prefix), std::move(field_name), acc_data.second);
					}
					THROW(ClientError, "The field name: %s (%s) is not valid", repr(full_name).c_str(), repr(field_name).c_str());
				}
			} else if (++it == it_e) {
				auto acc_data = get_acc_data(field_name);
				prefix.append(acc_data.first);
				return std::forward_as_tuple(*subproperties, dynamic_type_path, false, std::move(prefix), std::move(field_name), acc_data.second);
			} else {
				THROW(ClientError, "Field name: %s (%s) is not valid", repr(full_name).c_str(), repr(field_name).c_str());
			}
		}

		try {
			subproperties = &subproperties->at(field_name);
			prefix.append(subproperties->at(RESERVED_PREFIX).as_string());
		} catch (const std::out_of_range&) {
			try {
				const auto dynamic_prefix = Serialise::uuid(field_name);
				dynamic_type_path = true;
				try {
					subproperties = &subproperties->at(UUID_FIELD_NAME);
					prefix.append(dynamic_prefix);
					continue;
				} catch (const std::out_of_range&) {
					prefix.append(dynamic_prefix);
				}
			} catch (const SerialisationError&) {
				prefix.append(get_prefix(field_name));
			}

			// It is a search using partial prefix.
			int depth_partials = std::distance(it, it_e);
			if (depth_partials > LIMIT_PARTIAL_PATHS_DEPTH) {
				THROW(ClientError, "Partial paths limit depth is %d, and partial paths provided has a depth of %d", LIMIT_PARTIAL_PATHS_DEPTH, depth_partials);
			}
			for (++it; it != it_e; ++it) {
				auto& partial_field = *it;
				if (is_valid(partial_field) && partial_field != UUID_FIELD_NAME) {
					try {
						prefix.append(Serialise::uuid(partial_field));
						dynamic_type_path = true;
					} catch (const SerialisationError&) {
						prefix.append(get_prefix(partial_field));
					}
				} else if (++it == it_e) {
					auto acc_data = get_acc_data(partial_field);
					prefix.append(acc_data.first);
					return std::forward_as_tuple(*subproperties, dynamic_type_path, true, std::move(prefix), std::move(partial_field), acc_data.second);
				} else {
					THROW(ClientError, "Field name: %s (%s) is not valid", repr(full_name).c_str(), repr(partial_field).c_str());
				}
			}
			return std::forward_as_tuple(*subproperties, dynamic_type_path, true, std::move(prefix), std::string(), FieldType::EMPTY);
		}
	}

	return std::forward_as_tuple(*subproperties, dynamic_type_path, false, std::move(prefix), std::string(), FieldType::EMPTY);
}


#ifdef L_SCHEMA_DEFINED
#undef L_SCHEMA_DEFINED
#undef L_SCHEMA
#endif
