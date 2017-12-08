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

#include "cast.h"                          // for Cast
#include "database_handler.h"              // for DatabaseHandler
#include "datetime.h"                      // for isDate, tm_t
#include "exception.h"                     // for ClientError
#include "geospatial/geospatial.h"         // for GeoSpatial
#include "ignore_unused.h"                 // for ignore_unused
#include "manager.h"                       // for XapiandManager, XapiandMan...
#include "multivalue/generate_terms.h"     // for integer, geo, date, positive
#include "script.h"                        // for Script
#include "serialise_list.h"                // for StringList
#include "split.h"                         // for Split


#ifndef L_SCHEMA
#define L_SCHEMA_DEFINED
#define L_SCHEMA L_TEST
#endif


const std::string NAMESPACE_PREFIX_ID_FIELD_NAME = get_prefix(ID_FIELD_NAME);


/*
 * 1. Try reading schema from the metadata.
 * 2. Feed specification_t with the read schema using update_*;
 *    sets field_found for all found fields.
 * 3. Feed specification_t with the object sent by the user using process_*,
 *    except those that are already fixed because are reserved to be and
 *    they already exist in the metadata.
 * 4. If the field in the schema is normal and still has no RESERVED_TYPE (concrete)
 *    and a value is received for the field, call validate_required_data() to
 *    initialize the specification with validated data sent by the user.
 * 5. If the field is namespace or has partial paths call validate_required_namespace_data() to
 *    initialize the specification with default specifications and sent by the user.
 * 6. If there are values sent by user, fills the document to be indexed by
 *    process_item_value(...)
 * 7. If the path has uuid field name the values are indexed accordint to index_uuid_field.
 * 8. index() does steps 1 to 3 and for each field call index_object(...)
 * 9. index_object() does step 1 to 7 and for each field call index_object(...).
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


const std::unordered_map<std::string, UnitTime> map_acc_time({
	{ "second",     UnitTime::SECOND     }, { "minute",  UnitTime::MINUTE  },
	{ "hour",       UnitTime::HOUR       },
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


const std::unordered_map<std::string, UUIDFieldIndex> map_index_uuid_field({
	{ "uuid",  UUIDFieldIndex::UUID }, { "uuid_field", UUIDFieldIndex::UUID_FIELD },
	{ "both",  UUIDFieldIndex::BOTH },
});


const std::unordered_map<std::string, std::array<FieldType, SPC_TOTAL_TYPES>> map_type({
	{ "array",                        {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::EMPTY         }} },
	{ "array/boolean",                {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::BOOLEAN       }} },
	{ "array/date",                   {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::DATE          }} },
	{ "array/float",                  {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::FLOAT         }} },
	{ "array/geospatial",             {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::GEO           }} },
	{ "array/integer",                {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::INTEGER       }} },
	{ "array/positive",               {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::POSITIVE      }} },
	{ "array/string",                 {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::STRING        }} },
	{ "array/term",                   {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::TERM          }} },
	{ "array/text",                   {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::TEXT          }} },
	{ "array/time",                   {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::TIME          }} },
	{ "array/timedelta",              {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::TIMEDELTA     }} },
	{ "array/uuid",                   {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::ARRAY, FieldType::UUID          }} },
	{ "boolean",                      {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::BOOLEAN       }} },
	{ "date",                         {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::DATE          }} },
	{ "float",                        {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::FLOAT         }} },
	{ "foreign",                      {{ FieldType::FOREIGN, FieldType::EMPTY,  FieldType::EMPTY, FieldType::EMPTY         }} },
	{ "foreign/object",               {{ FieldType::FOREIGN, FieldType::OBJECT, FieldType::EMPTY, FieldType::EMPTY         }} },
	{ "foreign/script",               {{ FieldType::FOREIGN, FieldType::EMPTY,  FieldType::EMPTY, FieldType::SCRIPT        }} },
	{ "geospatial",                   {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::GEO           }} },
	{ "integer",                      {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::INTEGER       }} },
	{ "object",                       {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::EMPTY         }} },
	{ "object/array",                 {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::EMPTY         }} },
	{ "object/array/boolean",         {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::BOOLEAN       }} },
	{ "object/array/date",            {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::DATE          }} },
	{ "object/array/float",           {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::FLOAT         }} },
	{ "object/array/geospatial",      {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::GEO           }} },
	{ "object/array/integer",         {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::INTEGER       }} },
	{ "object/array/positive",        {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::POSITIVE      }} },
	{ "object/array/string",          {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::STRING        }} },
	{ "object/array/term",            {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::TERM          }} },
	{ "object/array/text",            {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::TEXT          }} },
	{ "object/array/time",            {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::TIME          }} },
	{ "object/array/timedelta",       {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::TIMEDELTA     }} },
	{ "object/array/uuid",            {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::ARRAY, FieldType::UUID          }} },
	{ "object/boolean",               {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::BOOLEAN       }} },
	{ "object/date",                  {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::DATE          }} },
	{ "object/float",                 {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::FLOAT         }} },
	{ "object/geospatial",            {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::GEO           }} },
	{ "object/integer",               {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::INTEGER       }} },
	{ "object/positive",              {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::POSITIVE      }} },
	{ "object/string",                {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::STRING        }} },
	{ "object/term",                  {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::TERM          }} },
	{ "object/text",                  {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::TEXT          }} },
	{ "object/time",                  {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::TIME          }} },
	{ "object/timedelta",             {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::TIMEDELTA     }} },
	{ "object/uuid",                  {{ FieldType::EMPTY,   FieldType::OBJECT, FieldType::EMPTY, FieldType::UUID          }} },
	{ "positive",                     {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::POSITIVE      }} },
	{ "script",                       {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::SCRIPT        }} },
	{ "string",                       {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::STRING        }} },
	{ "term",                         {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::TERM          }} },
	{ "text",                         {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::TEXT          }} },
	{ "time",                         {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::TIME          }} },
	{ "timedelta",                    {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::TIMEDELTA     }} },
	{ "uuid",                         {{ FieldType::EMPTY,   FieldType::EMPTY,  FieldType::EMPTY, FieldType::UUID          }} },
});


/*
 * Default accuracies.
 */

static const std::vector<uint64_t> def_accuracy_num({ 100, 1000, 10000, 100000, 1000000, 10000000 });
static const std::vector<uint64_t> def_accuracy_date({ toUType(UnitTime::HOUR), toUType(UnitTime::DAY), toUType(UnitTime::MONTH), toUType(UnitTime::YEAR), toUType(UnitTime::DECADE), toUType(UnitTime::CENTURY) });
static const std::vector<uint64_t> def_accuracy_time({ toUType(UnitTime::MINUTE), toUType(UnitTime::HOUR) });
static const std::vector<uint64_t> def_accuracy_geo({ HTM_START_POS - 40, HTM_START_POS - 30, HTM_START_POS - 20, HTM_START_POS - 10, HTM_START_POS }); // HTM's level 20, 15, 10, 5, 0


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


inline static std::string readable_index_uuid_field(UUIDFieldIndex index_uuid_field) noexcept {
	switch (index_uuid_field) {
		case UUIDFieldIndex::UUID:       return "uuid";
		case UUIDFieldIndex::UUID_FIELD: return "uuid_field";
		case UUIDFieldIndex::BOTH:       return "both";
		default:                         return "UUIDFieldIndex::UNKNOWN";
	}
}


/*
 *  Function to generate a prefix given an field accuracy.
 */

static std::pair<std::string, FieldType> get_acc_data(const std::string& field_acc) {
	auto it = map_acc_date.find(field_acc.substr(1));
	static const auto it_e = map_acc_date.end();
	if (it == it_e) {
		try {
			switch (field_acc[1]) {
				case 'g':
					if (field_acc.length() > 4 && field_acc[2] == 'e' && field_acc[3] == 'o') {
						return std::make_pair(get_prefix(strict_stoull(field_acc.substr(4))), FieldType::GEO);
					}
					break;
				case 't': {
					static const auto tit_e = map_acc_time.end();
					if (field_acc[2] == 'd') {
						auto tit = map_acc_time.find(field_acc.substr(3));
						if (tit != tit_e) {
							return std::make_pair(get_prefix(toUType(tit->second)), FieldType::TIMEDELTA);
						}
					} else {
						auto tit = map_acc_time.find(field_acc.substr(2));
						if (tit != tit_e) {
							return std::make_pair(get_prefix(toUType(tit->second)), FieldType::TIME);
						}
					}
					break;
				}
				default:
					return std::make_pair(get_prefix(strict_stoull(field_acc.substr(1))), FieldType::INTEGER);
			}
		} catch (const InvalidArgument&) {
		} catch (const OutOfRange&) { }

		THROW(ClientError, "The field name: %s is not valid", repr(field_acc).c_str());
	}

	return std::make_pair(get_prefix(toUType(it->second)), FieldType::DATE);
}


/*
 * Default acc_prefixes for global values.
 */

static std::vector<std::string> get_acc_prefix(const std::vector<uint64_t> accuracy) {
	std::vector<std::string> res;
	res.reserve(accuracy.size());
	for (const auto& acc : accuracy) {
		res.push_back(get_prefix(acc));
	}
	return res;
}

static const std::vector<std::string> global_acc_prefix_num(get_acc_prefix(def_accuracy_num));
static const std::vector<std::string> global_acc_prefix_date(get_acc_prefix(def_accuracy_date));
static const std::vector<std::string> global_acc_prefix_time(get_acc_prefix(def_accuracy_time));
static const std::vector<std::string> global_acc_prefix_geo(get_acc_prefix(def_accuracy_geo));


/*
 * Acceptable values string used when there is a data inconsistency.
 */

static const std::string str_set_acc_date(get_map_keys(map_acc_date));
static const std::string str_set_acc_time(get_map_keys(map_acc_time));


specification_t default_spc;


const std::unordered_map<std::string, Schema::dispatch_set_default_spc> Schema::map_dispatch_set_default_spc({
	{ ID_FIELD_NAME,  &Schema::set_default_spc_id },
	{ CT_FIELD_NAME,  &Schema::set_default_spc_ct },
});


const std::unordered_map<std::string, Schema::dispatch_write_reserved> Schema::map_dispatch_write_properties({
	{ RESERVED_WEIGHT,                 &Schema::write_weight                 },
	{ RESERVED_POSITION,               &Schema::write_position               },
	{ RESERVED_SPELLING,               &Schema::write_spelling               },
	{ RESERVED_POSITIONS,              &Schema::write_positions              },
	{ RESERVED_INDEX,                  &Schema::write_index                  },
	{ RESERVED_STORE,                  &Schema::write_store                  },
	{ RESERVED_RECURSE,                &Schema::write_recurse                },
	{ RESERVED_DYNAMIC,                &Schema::write_dynamic                },
	{ RESERVED_STRICT,                 &Schema::write_strict                 },
	{ RESERVED_DATE_DETECTION,         &Schema::write_date_detection         },
	{ RESERVED_TIME_DETECTION,         &Schema::write_time_detection         },
	{ RESERVED_TIMEDELTA_DETECTION,    &Schema::write_timedelta_detection    },
	{ RESERVED_NUMERIC_DETECTION,      &Schema::write_numeric_detection      },
	{ RESERVED_GEO_DETECTION,          &Schema::write_geo_detection          },
	{ RESERVED_BOOL_DETECTION,         &Schema::write_bool_detection         },
	{ RESERVED_STRING_DETECTION,       &Schema::write_string_detection       },
	{ RESERVED_TEXT_DETECTION,         &Schema::write_text_detection         },
	{ RESERVED_TERM_DETECTION,         &Schema::write_term_detection         },
	{ RESERVED_UUID_DETECTION,         &Schema::write_uuid_detection         },
	{ RESERVED_NAMESPACE,              &Schema::write_namespace              },
	{ RESERVED_PARTIAL_PATHS,          &Schema::write_partial_paths          },
	{ RESERVED_INDEX_UUID_FIELD,       &Schema::write_index_uuid_field       },
	{ RESERVED_VERSION,                &Schema::write_version                },
	{ RESERVED_SCHEMA,                 &Schema::write_schema                 },
	{ RESERVED_SCRIPT,                 &Schema::write_script                 },
});


const std::unordered_map<std::string, Schema::dispatch_process_reserved> Schema::map_dispatch_document_properties_without_concrete_type({
	{ RESERVED_LANGUAGE,           &Schema::process_language        },
	{ RESERVED_SLOT,               &Schema::process_slot            },
	{ RESERVED_STOP_STRATEGY,      &Schema::process_stop_strategy   },
	{ RESERVED_STEM_STRATEGY,      &Schema::process_stem_strategy   },
	{ RESERVED_STEM_LANGUAGE,      &Schema::process_stem_language   },
	{ RESERVED_TYPE,               &Schema::process_type            },
	{ RESERVED_BOOL_TERM,          &Schema::process_bool_term       },
	{ RESERVED_ACCURACY,           &Schema::process_accuracy        },
	{ RESERVED_PARTIALS,           &Schema::process_partials        },
	{ RESERVED_ERROR,              &Schema::process_error           },
});


const std::unordered_map<std::string, Schema::dispatch_process_reserved> Schema::map_dispatch_document_properties({
	{ RESERVED_WEIGHT,                 &Schema::process_weight                     },
	{ RESERVED_POSITION,               &Schema::process_position                   },
	{ RESERVED_SPELLING,               &Schema::process_spelling                   },
	{ RESERVED_POSITIONS,              &Schema::process_positions                  },
	{ RESERVED_INDEX,                  &Schema::process_index                      },
	{ RESERVED_STORE,                  &Schema::process_store                      },
	{ RESERVED_RECURSE,                &Schema::process_recurse                    },
	{ RESERVED_PARTIAL_PATHS,          &Schema::process_partial_paths              },
	{ RESERVED_INDEX_UUID_FIELD,       &Schema::process_index_uuid_field           },
	{ RESERVED_VALUE,                  &Schema::process_value                      },
	{ RESERVED_SCRIPT,                 &Schema::process_script                     },
	{ RESERVED_FLOAT,                  &Schema::process_cast_object                },
	{ RESERVED_POSITIVE,               &Schema::process_cast_object                },
	{ RESERVED_INTEGER,                &Schema::process_cast_object                },
	{ RESERVED_BOOLEAN,                &Schema::process_cast_object                },
	{ RESERVED_TERM,                   &Schema::process_cast_object                },
	{ RESERVED_TEXT,                   &Schema::process_cast_object                },
	{ RESERVED_STRING,                 &Schema::process_cast_object                },
	{ RESERVED_DATE,                   &Schema::process_cast_object                },
	{ RESERVED_UUID,                   &Schema::process_cast_object                },
	{ RESERVED_EWKT,                   &Schema::process_cast_object                },
	{ RESERVED_POINT,                  &Schema::process_cast_object                },
	{ RESERVED_CIRCLE,                 &Schema::process_cast_object                },
	{ RESERVED_CONVEX,                 &Schema::process_cast_object                },
	{ RESERVED_POLYGON,                &Schema::process_cast_object                },
	{ RESERVED_CHULL,                  &Schema::process_cast_object                },
	{ RESERVED_MULTIPOINT,             &Schema::process_cast_object                },
	{ RESERVED_MULTICIRCLE,            &Schema::process_cast_object                },
	{ RESERVED_MULTICONVEX,            &Schema::process_cast_object                },
	{ RESERVED_MULTIPOLYGON,           &Schema::process_cast_object                },
	{ RESERVED_MULTICHULL,             &Schema::process_cast_object                },
	{ RESERVED_GEO_COLLECTION,         &Schema::process_cast_object                },
	{ RESERVED_GEO_INTERSECTION,       &Schema::process_cast_object                },
	{ RESERVED_CHAI,                   &Schema::process_cast_object                },
	{ RESERVED_ECMA,                   &Schema::process_cast_object                },
	// Next functions only check the consistency of user provided data.
	{ RESERVED_LANGUAGE,               &Schema::consistency_language               },
	{ RESERVED_STOP_STRATEGY,          &Schema::consistency_stop_strategy          },
	{ RESERVED_STEM_STRATEGY,          &Schema::consistency_stem_strategy          },
	{ RESERVED_STEM_LANGUAGE,          &Schema::consistency_stem_language          },
	{ RESERVED_TYPE,                   &Schema::consistency_type                   },
	{ RESERVED_BOOL_TERM,              &Schema::consistency_bool_term              },
	{ RESERVED_ACCURACY,               &Schema::consistency_accuracy               },
	{ RESERVED_PARTIALS,               &Schema::consistency_partials               },
	{ RESERVED_ERROR,                  &Schema::consistency_error                  },
	{ RESERVED_DYNAMIC,                &Schema::consistency_dynamic                },
	{ RESERVED_STRICT,                 &Schema::consistency_strict                 },
	{ RESERVED_DATE_DETECTION,         &Schema::consistency_date_detection         },
	{ RESERVED_TIME_DETECTION,         &Schema::consistency_time_detection         },
	{ RESERVED_TIMEDELTA_DETECTION,    &Schema::consistency_timedelta_detection    },
	{ RESERVED_NUMERIC_DETECTION,      &Schema::consistency_numeric_detection      },
	{ RESERVED_GEO_DETECTION,          &Schema::consistency_geo_detection          },
	{ RESERVED_BOOL_DETECTION,         &Schema::consistency_bool_detection         },
	{ RESERVED_STRING_DETECTION,       &Schema::consistency_string_detection       },
	{ RESERVED_TEXT_DETECTION,         &Schema::consistency_text_detection         },
	{ RESERVED_TERM_DETECTION,         &Schema::consistency_term_detection         },
	{ RESERVED_UUID_DETECTION,         &Schema::consistency_uuid_detection         },
	{ RESERVED_NAMESPACE,              &Schema::consistency_namespace              },
	{ RESERVED_VERSION,                &Schema::consistency_version                },
	{ RESERVED_SCHEMA,                 &Schema::consistency_schema                 },
});


const std::unordered_map<std::string, Schema::dispatch_update_reserved> Schema::map_dispatch_update_properties({
	{ RESERVED_WEIGHT,                 &Schema::update_weight                 },
	{ RESERVED_POSITION,               &Schema::update_position               },
	{ RESERVED_SPELLING,               &Schema::update_spelling               },
	{ RESERVED_POSITIONS,              &Schema::update_positions              },
	{ RESERVED_TYPE,                   &Schema::update_type                   },
	{ RESERVED_PREFIX,                 &Schema::update_prefix                 },
	{ RESERVED_SLOT,                   &Schema::update_slot                   },
	{ RESERVED_INDEX,                  &Schema::update_index                  },
	{ RESERVED_STORE,                  &Schema::update_store                  },
	{ RESERVED_RECURSE,                &Schema::update_recurse                },
	{ RESERVED_DYNAMIC,                &Schema::update_dynamic                },
	{ RESERVED_STRICT,                 &Schema::update_strict                 },
	{ RESERVED_DATE_DETECTION,         &Schema::update_date_detection         },
	{ RESERVED_TIME_DETECTION,         &Schema::update_time_detection         },
	{ RESERVED_TIMEDELTA_DETECTION,    &Schema::update_timedelta_detection    },
	{ RESERVED_NUMERIC_DETECTION,      &Schema::update_numeric_detection      },
	{ RESERVED_GEO_DETECTION,          &Schema::update_geo_detection          },
	{ RESERVED_BOOL_DETECTION,         &Schema::update_bool_detection         },
	{ RESERVED_STRING_DETECTION,       &Schema::update_string_detection       },
	{ RESERVED_TEXT_DETECTION,         &Schema::update_text_detection         },
	{ RESERVED_TERM_DETECTION,         &Schema::update_term_detection         },
	{ RESERVED_UUID_DETECTION,         &Schema::update_uuid_detection         },
	{ RESERVED_BOOL_TERM,              &Schema::update_bool_term              },
	{ RESERVED_ACCURACY,               &Schema::update_accuracy               },
	{ RESERVED_ACC_PREFIX,             &Schema::update_acc_prefix             },
	{ RESERVED_LANGUAGE,               &Schema::update_language               },
	{ RESERVED_STOP_STRATEGY,          &Schema::update_stop_strategy          },
	{ RESERVED_STEM_STRATEGY,          &Schema::update_stem_strategy          },
	{ RESERVED_STEM_LANGUAGE,          &Schema::update_stem_language          },
	{ RESERVED_PARTIALS,               &Schema::update_partials               },
	{ RESERVED_ERROR,                  &Schema::update_error                  },
	{ RESERVED_NAMESPACE,              &Schema::update_namespace              },
	{ RESERVED_PARTIAL_PATHS,          &Schema::update_partial_paths          },
	{ RESERVED_INDEX_UUID_FIELD,       &Schema::update_index_uuid_field       },
	{ RESERVED_SCRIPT,                 &Schema::update_script                 },
});


const std::unordered_map<std::string, Schema::dispatch_readable> Schema::map_get_readable({
	{ RESERVED_TYPE,                &Schema::readable_type               },
	{ RESERVED_PREFIX,              &Schema::readable_prefix             },
	{ RESERVED_STOP_STRATEGY,       &Schema::readable_stop_strategy      },
	{ RESERVED_STEM_STRATEGY,       &Schema::readable_stem_strategy      },
	{ RESERVED_STEM_LANGUAGE,       &Schema::readable_stem_language      },
	{ RESERVED_INDEX,               &Schema::readable_index              },
	{ RESERVED_ACC_PREFIX,          &Schema::readable_acc_prefix         },
	{ RESERVED_INDEX_UUID_FIELD,    &Schema::readable_index_uuid_field   },
	{ RESERVED_SCRIPT,              &Schema::readable_script             },
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
	  is_recurse(true),
	  dynamic(true),
	  strict(false),
	  date_detection(true),
	  time_detection(true),
	  timedelta_detection(true),
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
	  concrete(false),
	  complete(false),
	  uuid_field(false),
	  uuid_path(false),
	  inside_namespace(false),
#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	  normalized_script(false),
#endif
	  has_uuid_prefix(false),
	  has_bool_term(false),
	  has_index(false),
	  has_namespace(false),
	  has_partial_paths(false) { }


std::string
required_spc_t::prefix_t::to_string() const
{
	auto res = repr(field);
	if (uuid.empty()) {
		return res;
	}
	res.insert(0, 1, '(').append(", ").append(repr(uuid)).push_back(')');
	return res;
}


std::string
required_spc_t::prefix_t::operator()() const noexcept
{
	return field;
}


required_spc_t::required_spc_t()
	: sep_types({{ FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY }}),
	  slot(Xapian::BAD_VALUENO),
	  language(DEFAULT_LANGUAGE),
	  stop_strategy(DEFAULT_STOP_STRATEGY),
	  stem_strategy(DEFAULT_STEM_STRATEGY),
	  stem_language(DEFAULT_LANGUAGE),
	  error(DEFAULT_GEO_ERROR) { }


required_spc_t::required_spc_t(Xapian::valueno _slot, FieldType type, const std::vector<uint64_t>& acc,
	const std::vector<std::string>& _acc_prefix)
	: sep_types({{ FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY, type }}),
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


std::array<FieldType, SPC_TOTAL_TYPES>
required_spc_t::get_types(const std::string& str_type)
{
	L_CALL(nullptr, "required_spc_t::get_types(%s)", repr(str_type).c_str());

	static const auto tit_e = map_type.end();
	auto tit = map_type.find(lower_string(str_type));
	if (tit == tit_e) {
		THROW(ClientError, "%s not supported, '%s' must be one of { 'date', 'float', 'geospatial', 'integer', 'positive', 'script', 'string', 'term', 'text', 'time', 'timedelta', 'uuid' } or any of their { 'object/<type>', 'array/<type>', 'object/array/<t,ype>', 'foreign/<type>', 'foreign/object/<type>,', 'foreign/array/<type>', 'foreign/object/array/<type>' } variations.", repr(str_type).c_str(), RESERVED_TYPE);
	}
	return tit->second;
}


std::string
required_spc_t::get_str_type(const std::array<FieldType, SPC_TOTAL_TYPES>& sep_types)
{
	L_CALL(nullptr, "required_spc_t::get_str_type({ %d, %d, %d, %d })", toUType(sep_types[SPC_FOREIGN_TYPE]), toUType(sep_types[SPC_OBJECT_TYPE]),
		toUType(sep_types[SPC_ARRAY_TYPE]), toUType(sep_types[SPC_CONCRETE_TYPE]));

	std::string result;
	if (sep_types[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
		result += Serialise::type(sep_types[SPC_FOREIGN_TYPE]);
	}
	if (sep_types[SPC_OBJECT_TYPE] == FieldType::OBJECT) {
		if (!result.empty()) result += "/";
		result += Serialise::type(sep_types[SPC_OBJECT_TYPE]);
	}
	if (sep_types[SPC_ARRAY_TYPE] == FieldType::ARRAY) {
		if (!result.empty()) result += "/";
		result += Serialise::type(sep_types[SPC_ARRAY_TYPE]);
	}
	if (sep_types[SPC_CONCRETE_TYPE] != FieldType::EMPTY) {
		if (!result.empty()) result += "/";
		result += Serialise::type(sep_types[SPC_CONCRETE_TYPE]);
	}
	return result;
}


void
required_spc_t::set_types(const std::string& str_type)
{
	L_CALL(this, "required_spc_t::set_types(%s)", repr(str_type).c_str());

	sep_types = get_types(str_type);
}


index_spc_t::index_spc_t(required_spc_t&& spc)
	: type(std::move(spc.sep_types[SPC_CONCRETE_TYPE])),
	  prefix(std::move(spc.prefix.field)),
	  slot(std::move(spc.slot)),
	  accuracy(std::move(spc.accuracy)),
	  acc_prefix(std::move(spc.acc_prefix)) { }


index_spc_t::index_spc_t(const required_spc_t& spc)
	: type(spc.sep_types[SPC_CONCRETE_TYPE]),
	  prefix(spc.prefix.field),
	  slot(spc.slot),
	  accuracy(spc.accuracy),
	  acc_prefix(spc.acc_prefix) { }


specification_t::specification_t()
	: position({ 0 }),
	  weight({ 1 }),
	  spelling({ DEFAULT_SPELLING }),
	  positions({ DEFAULT_POSITIONS }),
	  index(DEFAULT_INDEX),
	  index_uuid_field(DEFAULT_INDEX_UUID_FIELD) { }


specification_t::specification_t(Xapian::valueno _slot, FieldType type, const std::vector<uint64_t>& acc,
	const std::vector<std::string>& _acc_prefix)
	: required_spc_t(_slot, type, acc, _acc_prefix),
	  position({ 0 }),
	  weight({ 1 }),
	  spelling({ DEFAULT_SPELLING }),
	  positions({ DEFAULT_POSITIONS }),
	  index(DEFAULT_INDEX),
	  index_uuid_field(DEFAULT_INDEX_UUID_FIELD) { }


specification_t::specification_t(const specification_t& o)
	: required_spc_t(o),
	  local_prefix(o.local_prefix),
	  position(o.position),
	  weight(o.weight),
	  spelling(o.spelling),
	  positions(o.positions),
	  index(o.index),
	  index_uuid_field(o.index_uuid_field),
	  meta_name(o.meta_name),
	  full_meta_name(o.full_meta_name),
	  aux_stem_lan(o.aux_stem_lan),
	  aux_lan(o.aux_lan),
	  partial_prefixes(o.partial_prefixes),
	  partial_index_spcs(o.partial_index_spcs) { }


specification_t::specification_t(specification_t&& o) noexcept
	: required_spc_t(std::move(o)),
	  local_prefix(std::move(o.local_prefix)),
	  position(std::move(o.position)),
	  weight(std::move(o.weight)),
	  spelling(std::move(o.spelling)),
	  positions(std::move(o.positions)),
	  index(std::move(o.index)),
	  index_uuid_field(std::move(o.index_uuid_field)),
	  meta_name(std::move(o.meta_name)),
	  full_meta_name(std::move(o.full_meta_name)),
	  aux_stem_lan(std::move(o.aux_stem_lan)),
	  aux_lan(std::move(o.aux_lan)),
	  partial_prefixes(std::move(o.partial_prefixes)),
	  partial_index_spcs(std::move(o.partial_index_spcs)) { }


specification_t&
specification_t::operator=(const specification_t& o)
{
	local_prefix = o.local_prefix;
	position = o.position;
	weight = o.weight;
	spelling = o.spelling;
	positions = o.positions;
	index = o.index;
	index_uuid_field = o.index_uuid_field;
	value_rec.reset();
	value.reset();
	doc_acc.reset();
#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	script.reset();
#endif
	meta_name = o.meta_name;
	full_meta_name = o.full_meta_name;
	aux_stem_lan = o.aux_stem_lan;
	aux_lan = o.aux_lan;
	partial_prefixes = o.partial_prefixes;
	partial_index_spcs = o.partial_index_spcs;
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
	index_uuid_field = std::move(o.index_uuid_field);
	value_rec.reset();
	value.reset();
	doc_acc.reset();
#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	script.reset();
#endif
	meta_name = std::move(o.meta_name);
	full_meta_name = std::move(o.full_meta_name);
	aux_stem_lan = std::move(o.aux_stem_lan);
	aux_lan = std::move(o.aux_lan);
	partial_prefixes = std::move(o.partial_prefixes);
	partial_index_spcs = std::move(o.partial_index_spcs);
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
		case FieldType::TIME:
		case FieldType::TIMEDELTA:
		case FieldType::GEO:
		case FieldType::UUID:
		case FieldType::TERM:
			return field_type;

		case FieldType::TEXT:
		case FieldType::STRING:
			return FieldType::STRING;

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
		case FieldType::TIME: {
			static const specification_t spc(DB_SLOT_TIME, FieldType::TIME, def_accuracy_time, global_acc_prefix_time);
			return spc;
		}
		case FieldType::TIMEDELTA: {
			static const specification_t spc(DB_SLOT_TIMEDELTA, FieldType::TIMEDELTA, def_accuracy_time, global_acc_prefix_time);
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
		case FieldType::TERM: {
			static const specification_t spc(DB_SLOT_STRING, FieldType::TERM, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		case FieldType::TEXT:
		case FieldType::STRING: {
			static const specification_t spc(DB_SLOT_STRING, FieldType::STRING, default_spc.accuracy, default_spc.acc_prefix);
			return spc;
		}
		default:
			THROW(ClientError, "Type: '%c' is an unknown type", field_type);
	}
}


void
specification_t::update(index_spc_t&& spc)
{
	sep_types[SPC_CONCRETE_TYPE] = std::move(spc.type);
	prefix.field = std::move(spc.prefix);
	slot = std::move(spc.slot);
	accuracy = std::move(spc.accuracy);
	acc_prefix = std::move(spc.acc_prefix);
}


void
specification_t::update(const index_spc_t& spc)
{
	sep_types[SPC_CONCRETE_TYPE] = spc.type;
	prefix.field = spc.prefix;
	slot = spc.slot;
	accuracy = spc.accuracy;
	acc_prefix = spc.acc_prefix;
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

	str << "\t" << RESERVED_WEIGHT << ": [ ";
	for (const auto& _w : weight) {
		str << _w << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_SPELLING << ": [ ";
	for (const auto& spel : spelling) {
		str << (spel ? "true" : "false") << " ";
	}
	str << "]\n";

	str << "\t" << RESERVED_POSITIONS << ": [ ";
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

	str << "\t" << RESERVED_ACC_PREFIX << ": [ ";
	for (const auto& acc_p : acc_prefix) {
		str << repr(acc_p) << " ";
	}
	str << "]\n";

	str << "\t" << "partial_prefixes" << ": [ ";
	for (const auto& partial_prefix : partial_prefixes) {
		str << partial_prefix.to_string() << " ";
	}
	str << "]\n";

	str << "\t" << "partial_index_spcs" << ": [ ";
	for (const auto& spc : partial_index_spcs) {
		str << "{" << repr(spc.prefix) << ", " << spc.slot << "} ";
	}
	str << "]\n";

	str << "\t" << "value_rec"                  << ": " << (value_rec ? value_rec->to_string() : "null")   << "\n";
	str << "\t" << RESERVED_VALUE               << ": " << (value     ? value->to_string()     : "null")   << "\n";
	str << "\t" << "doc_acc"                    << ": " << (doc_acc   ? doc_acc->to_string()   : "null")   << "\n";
#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	str << "\t" << RESERVED_SCRIPT              << ": " << (script    ? script->to_string()    : "null")   << "\n";
#endif

	str << "\t" << RESERVED_SLOT                << ": " << slot                                        << "\n";
	str << "\t" << RESERVED_TYPE                << ": " << get_str_type(sep_types)                     << "\n";
	str << "\t" << RESERVED_PREFIX              << ": " << prefix.to_string()                          << "\n";
	str << "\t" << "local_prefix"               << ": " << local_prefix.to_string()                    << "\n";
	str << "\t" << RESERVED_INDEX               << ": " << readable_index(index)                       << "\n";
	str << "\t" << RESERVED_INDEX_UUID_FIELD    << ": " << readable_index_uuid_field(index_uuid_field) << "\n";
	str << "\t" << RESERVED_ERROR               << ": " << error                                       << "\n";

	str << "\t" << RESERVED_PARTIALS            << ": " << (flags.partials              ? "true" : "false") << "\n";
	str << "\t" << RESERVED_STORE               << ": " << (flags.store                 ? "true" : "false") << "\n";
	str << "\t" << "parent_store"               << ": " << (flags.parent_store          ? "true" : "false") << "\n";
	str << "\t" << RESERVED_RECURSE             << ": " << (flags.is_recurse            ? "true" : "false") << "\n";
	str << "\t" << RESERVED_DYNAMIC             << ": " << (flags.dynamic               ? "true" : "false") << "\n";
	str << "\t" << RESERVED_STRICT              << ": " << (flags.strict                ? "true" : "false") << "\n";
	str << "\t" << RESERVED_DATE_DETECTION      << ": " << (flags.date_detection        ? "true" : "false") << "\n";
	str << "\t" << RESERVED_TIME_DETECTION      << ": " << (flags.time_detection        ? "true" : "false") << "\n";
	str << "\t" << RESERVED_TIMEDELTA_DETECTION << ": " << (flags.timedelta_detection   ? "true" : "false") << "\n";
	str << "\t" << RESERVED_NUMERIC_DETECTION   << ": " << (flags.numeric_detection     ? "true" : "false") << "\n";
	str << "\t" << RESERVED_GEO_DETECTION       << ": " << (flags.geo_detection         ? "true" : "false") << "\n";
	str << "\t" << RESERVED_BOOL_DETECTION      << ": " << (flags.bool_detection        ? "true" : "false") << "\n";
	str << "\t" << RESERVED_STRING_DETECTION    << ": " << (flags.string_detection      ? "true" : "false") << "\n";
	str << "\t" << RESERVED_TEXT_DETECTION      << ": " << (flags.text_detection        ? "true" : "false") << "\n";
	str << "\t" << RESERVED_TERM_DETECTION      << ": " << (flags.term_detection        ? "true" : "false") << "\n";
	str << "\t" << RESERVED_UUID_DETECTION      << ": " << (flags.uuid_detection        ? "true" : "false") << "\n";
	str << "\t" << RESERVED_BOOL_TERM           << ": " << (flags.bool_term             ? "true" : "false") << "\n";
	str << "\t" << RESERVED_NAMESPACE           << ": " << (flags.is_namespace          ? "true" : "false") << "\n";
	str << "\t" << RESERVED_PARTIAL_PATHS       << ": " << (flags.partial_paths         ? "true" : "false") << "\n";
	str << "\t" << "optimal"                    << ": " << (flags.optimal               ? "true" : "false") << "\n";
	str << "\t" << "field_found"                << ": " << (flags.field_found           ? "true" : "false") << "\n";
	str << "\t" << "concrete"                   << ": " << (flags.concrete       ? "true" : "false") << "\n";
	str << "\t" << "complete"                   << ": " << (flags.complete              ? "true" : "false") << "\n";
	str << "\t" << "uuid_field"                 << ": " << (flags.uuid_field            ? "true" : "false") << "\n";
	str << "\t" << "uuid_path"                  << ": " << (flags.uuid_path             ? "true" : "false") << "\n";
	str << "\t" << "inside_namespace"           << ": " << (flags.inside_namespace      ? "true" : "false") << "\n";
#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	str << "\t" << "normalized_script"          << ": " << (flags.normalized_script     ? "true" : "false") << "\n";
#endif
	str << "\t" << "has_uuid_prefix"            << ": " << (flags.has_uuid_prefix       ? "true" : "false") << "\n";
	str << "\t" << "has_bool_term"              << ": " << (flags.has_bool_term         ? "true" : "false") << "\n";
	str << "\t" << "has_index"                  << ": " << (flags.has_index             ? "true" : "false") << "\n";
	str << "\t" << "has_namespace"              << ": " << (flags.has_namespace         ? "true" : "false") << "\n";

	str << "\t" << "meta_name"                  << ": " << meta_name            << "\n";
	str << "\t" << "full_meta_name"             << ": " << full_meta_name       << "\n";
	str << "\t" << "aux_stem_lan"               << ": " << aux_stem_lan         << "\n";
	str << "\t" << "aux_lan"                    << ": " << aux_lan              << "\n";

	str << "}\n";

	return str.str();
}


Schema::Schema(const std::shared_ptr<const MsgPack>& other)
	: schema(other)
{
	try {
		const auto& version = get_properties().at(RESERVED_VERSION);
		if (version.f64() != DB_VERSION_SCHEMA) {
			THROW(Error, "Different database's version schemas, the current version is %1.1f", DB_VERSION_SCHEMA);
		}
	} catch (const std::out_of_range&) {
		THROW(Error, "Schema is corrupt: '%s' does not exist", RESERVED_VERSION);
	} catch (const msgpack::type_error&) {
		THROW(Error, "Schema is corrupt: '%s' has an invalid version", RESERVED_VERSION);
	}
}


std::shared_ptr<const MsgPack>
Schema::get_initial_schema()
{
	L_CALL(nullptr, "Schema::get_initial_schema()");

	MsgPack new_schema({ {
		DB_SCHEMA, {
			{ RESERVED_TYPE,  std::array<FieldType, SPC_TOTAL_TYPES>{ { FieldType::EMPTY, FieldType::OBJECT, FieldType::EMPTY, FieldType::EMPTY } } },
			{ RESERVED_VALUE, { { RESERVED_VERSION, DB_VERSION_SCHEMA } } }
		}
	} });
	new_schema.lock();
	return std::make_shared<const MsgPack>(std::move(new_schema));
}


const MsgPack&
Schema::get_properties(const std::string& full_meta_name)
{
	L_CALL(this, "Schema::get_properties(%s)", repr(full_meta_name).c_str());

	const MsgPack* prop = &get_properties();
	Split<char> field_names(full_meta_name, DB_OFFSPRING_UNION);
	for (const auto& field_name : field_names) {
		prop = &(*prop).at(field_name);
	}
	return *prop;
}


MsgPack&
Schema::get_mutable_properties(const std::string& full_meta_name)
{
	L_CALL(this, "Schema::get_mutable_properties(%s)", repr(full_meta_name).c_str());

	MsgPack* prop = &get_mutable_properties();
	Split<char> field_names(full_meta_name, DB_OFFSPRING_UNION);
	for (const auto& field_name : field_names) {
		prop = &(*prop)[field_name];
	}
	return *prop;
}


const MsgPack&
Schema::get_newest_properties(const std::string& full_meta_name)
{
	L_CALL(this, "Schema::get_newest_properties(%s)", repr(full_meta_name).c_str());

	const MsgPack* prop = &get_newest_properties();
	Split<char> field_names(full_meta_name, DB_OFFSPRING_UNION);
	for (const auto& field_name : field_names) {
		prop = &(*prop).at(field_name);
	}
	return *prop;
}


MsgPack&
Schema::clear()
{
	L_CALL(this, "Schema::clear()");

	auto& prop = get_mutable_properties();
	prop.clear();
	prop[RESERVED_VERSION] = DB_VERSION_SCHEMA;
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

	specification.flags.concrete             = default_spc.flags.concrete;
	specification.flags.complete             = default_spc.flags.complete;
	specification.flags.uuid_field           = default_spc.flags.uuid_field;

	specification.sep_types                  = default_spc.sep_types;
	specification.local_prefix               = default_spc.local_prefix;
	specification.slot                       = default_spc.slot;
	specification.accuracy                   = default_spc.accuracy;
	specification.acc_prefix                 = default_spc.acc_prefix;
	specification.aux_stem_lan               = default_spc.aux_stem_lan;
	specification.aux_lan                    = default_spc.aux_lan;

	specification.partial_index_spcs         = default_spc.partial_index_spcs;
}


inline void
Schema::restart_namespace_specification()
{
	L_CALL(this, "Schema::restart_namespace_specification()");

	specification.flags.bool_term        = default_spc.flags.bool_term;
	specification.flags.has_bool_term    = default_spc.flags.has_bool_term;

	specification.flags.concrete         = default_spc.flags.concrete;
	specification.flags.complete         = default_spc.flags.complete;
	specification.flags.uuid_field       = default_spc.flags.uuid_field;

	specification.sep_types              = default_spc.sep_types;
	specification.aux_stem_lan           = default_spc.aux_stem_lan;
	specification.aux_lan                = default_spc.aux_lan;

	specification.partial_index_spcs     = default_spc.partial_index_spcs;
}


void
Schema::index_object(const MsgPack*& parent_properties, const MsgPack& object, MsgPack*& parent_data, Xapian::Document& doc, const std::string& name)
{
	L_CALL(this, "Schema::index_object(%s, %s, %s, <Xapian::Document>, %s)", repr(parent_properties->to_string()).c_str(), repr(object.to_string()).c_str(), repr(parent_data->to_string()).c_str(), repr(name).c_str());

	if (name.empty()) {
		THROW(ClientError, "Field name must not be empty");
	}

	if (!specification.flags.is_recurse) {
		if (specification.flags.store) {
			(*parent_data)[name] = object;
		}
		return;
	}

	switch (object.getType()) {
		case MsgPack::Type::MAP: {
			auto spc_start = specification;
			auto properties = &*parent_properties;
			auto data = parent_data;
			FieldVector fields;
			properties = &get_subproperties(properties, data, name, object, fields);
			process_item_value(properties, doc, data, fields);
			if (specification.flags.store && (data->is_undefined() || data->is_null())) {
				parent_data->erase(name);
			}
			specification = std::move(spc_start);
			return;
		}

		case MsgPack::Type::ARRAY: {
			return index_array(parent_properties, object, parent_data, doc, name);
		}

		case MsgPack::Type::NIL:
		case MsgPack::Type::UNDEFINED: {
			auto spc_start = specification;
			auto properties = &*parent_properties;
			auto data = parent_data;
			get_subproperties(properties, data, name);
			index_partial_paths(doc);
			if (specification.flags.store) {
				parent_data->erase(name);
			}
			specification = std::move(spc_start);
			return;
		}

		default: {
			auto spc_start = specification;
			auto properties = &*parent_properties;
			auto data = parent_data;
			get_subproperties(properties, data, name);
			process_item_value(doc, *data, object, 0);
			specification = std::move(spc_start);
			return;
		}
	}
}


void
Schema::index_array(const MsgPack*& parent_properties, const MsgPack& array, MsgPack*& parent_data, Xapian::Document& doc, const std::string& name)
{
	L_CALL(this, "Schema::index_array(%s, %s, <MsgPack*>, <Xapian::Document>, %s)", repr(parent_properties->to_string()).c_str(), repr(array.to_string()).c_str(), repr(name).c_str());

	if (array.empty()) {
		set_type_to_array();
		if (specification.flags.store) {
			(*parent_data)[name] = MsgPack(MsgPack::Type::ARRAY);
		}
		return;
	}

	auto spc_start = specification;
	size_t pos = 0;
	for (const auto& item : array) {
		switch (item.getType()) {
			case MsgPack::Type::MAP: {
				auto properties = &*parent_properties;
				auto data = parent_data;
				FieldVector fields;
				properties = &get_subproperties(properties, data, name, item, fields);
				auto data_pos = specification.flags.store ? &(*data)[pos] : data;
				process_item_value(properties, doc, data_pos, fields);
				specification = spc_start;
				break;
			}

			case MsgPack::Type::ARRAY: {
				auto properties = &*parent_properties;
				auto data = parent_data;
				get_subproperties(properties, data, name);
				auto data_pos = specification.flags.store ? &(*data)[pos] : data;
				process_item_value(doc, data_pos, item);
				specification = spc_start;
				break;
			}

			case MsgPack::Type::NIL:
			case MsgPack::Type::UNDEFINED: {
				auto properties = &*parent_properties;
				auto data = parent_data;
				get_subproperties(properties, data, name);
				index_partial_paths(doc);
				if (specification.flags.store) {
					(*data)[pos] = item;
				}
				specification = spc_start;
				break;
			}

			default: {
				auto properties = &*parent_properties;
				auto data = parent_data;
				get_subproperties(properties, data, name);
				process_item_value(doc, specification.flags.store ? (*data)[pos] : *data, item, pos);
				specification = spc_start;
				break;
			}
		}
		++pos;
	}
}


void
Schema::process_item_value(Xapian::Document& doc, MsgPack& data, const MsgPack& item_value, size_t pos)
{
	L_CALL(this, "Schema::process_item_value(<doc>, %s, %s, %zu)", repr(data.to_string()).c_str(), repr(item_value.to_string()).c_str(), pos);

	if (!specification.flags.complete) {
		if (specification.flags.inside_namespace) {
			complete_namespace_specification(item_value);
		} else {
			complete_specification(item_value);
		}
	}

	if (specification.partial_index_spcs.empty()) {
		index_item(doc, item_value, data, pos);
	} else {
		bool add_value = true;
		index_spc_t start_index_spc(specification.sep_types[SPC_CONCRETE_TYPE], std::move(specification.prefix.field), specification.slot, std::move(specification.accuracy), std::move(specification.acc_prefix));
		for (const auto& index_spc : specification.partial_index_spcs) {
			specification.update(index_spc);
			index_item(doc, item_value, data, pos, add_value);
			add_value = false;
		}
		specification.update(std::move(start_index_spc));
	}

	if (specification.flags.store) {
		data = data[RESERVED_VALUE];
	}
}


inline void
Schema::process_item_value(Xapian::Document& doc, MsgPack*& data, const MsgPack& item_value)
{
	L_CALL(this, "Schema::process_item_value(<doc>, %s, %s)", repr(data->to_string()).c_str(), repr(item_value.to_string()).c_str());

	switch (item_value.getType()) {
		case MsgPack::Type::ARRAY: {
			bool valid = false;
			for (const auto& item : item_value) {
				if (!item.is_null() && !item.is_undefined()) {
					if (!specification.flags.complete) {
						if (specification.flags.inside_namespace) {
							complete_namespace_specification(item);
						} else {
							complete_specification(item);
						}
					}
					valid = true;
					break;
				}
			}
			if (valid) {
				break;
			}
		}
		case MsgPack::Type::NIL:
		case MsgPack::Type::UNDEFINED:
			index_partial_paths(doc);
			if (specification.flags.store) {
				*data = item_value;
			}
			return;
		default:
			if (!specification.flags.complete) {
				if (specification.flags.inside_namespace) {
					complete_namespace_specification(item_value);
				} else {
					complete_specification(item_value);
				}
			}
			break;
	}

	if (specification.partial_index_spcs.empty()) {
		index_item(doc, item_value, *data);
	} else {
		bool add_value = true;
		index_spc_t start_index_spc(specification.sep_types[SPC_CONCRETE_TYPE], std::move(specification.prefix.field), specification.slot,
			std::move(specification.accuracy), std::move(specification.acc_prefix));
		for (const auto& index_spc : specification.partial_index_spcs) {
			specification.update(index_spc);
			index_item(doc, item_value, *data, add_value);
			add_value = false;
		}
		specification.update(std::move(start_index_spc));
	}

	if (specification.flags.store && data->size() == 1) {
		*data = (*data)[RESERVED_VALUE];
	}
}


inline void
Schema::process_item_value(const MsgPack*& properties, Xapian::Document& doc, MsgPack*& data, const FieldVector& fields)
{
	L_CALL(this, "Schema::process_item_value(<MsgPack*>, <doc>, %s, <FieldVector>)", repr(data->to_string()).c_str());

	auto val = specification.value ? std::move(specification.value) : std::move(specification.value_rec);
	if (val) {
		switch (val->getType()) {
			case MsgPack::Type::ARRAY: {
				bool valid = false;
				for (const auto& item : *val) {
					if (!item.is_null() && !item.is_undefined()) {
						if (!specification.flags.complete) {
							if (specification.flags.inside_namespace) {
								complete_namespace_specification(item);
							} else {
								complete_specification(item);
							}
						}
						valid = true;
						break;
					}
				}
				if (valid) {
					break;
				}
			}
			case MsgPack::Type::NIL:
			case MsgPack::Type::UNDEFINED:
				if (!specification.flags.concrete && specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::EMPTY) {
					_validate_required_data(get_mutable_properties(specification.full_meta_name));
				}
				index_partial_paths(doc);
				if (specification.flags.store) {
					*data = *val;
				}
				return;
			default:
				if (!specification.flags.complete) {
					if (specification.flags.inside_namespace) {
						complete_namespace_specification(*val);
					} else {
						complete_specification(*val);
					}
				}
				break;
		}

		if (specification.partial_index_spcs.empty()) {
			index_item(doc, *val, *data);
		} else {
			bool add_value = true;
			index_spc_t start_index_spc(specification.sep_types[SPC_CONCRETE_TYPE], std::move(specification.prefix.field), specification.slot,
				std::move(specification.accuracy), std::move(specification.acc_prefix));
			for (const auto& index_spc : specification.partial_index_spcs) {
				specification.update(index_spc);
				index_item(doc, *val, *data, add_value);
				add_value = false;
			}
			specification.update(std::move(start_index_spc));
		}

		if (fields.empty()) {
			if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::EMPTY && specification.sep_types[SPC_OBJECT_TYPE] == FieldType::EMPTY && specification.sep_types[SPC_ARRAY_TYPE] == FieldType::EMPTY) {
				set_type_to_object();
			}
			if (specification.flags.store) {
				*data = (*data)[RESERVED_VALUE];
			}
		} else {
			set_type_to_object();
			const auto spc_object = std::move(specification);
			for (const auto& field : fields) {
				specification = spc_object;
				index_object(properties, *field.second, data, doc, field.first);
			}
		}
	} else {
		if (!specification.flags.concrete && specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::EMPTY) {
			_validate_required_data(get_mutable_properties(specification.full_meta_name));
		}

		if (fields.empty()) {
			if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::EMPTY && specification.sep_types[SPC_OBJECT_TYPE] == FieldType::EMPTY && specification.sep_types[SPC_ARRAY_TYPE] == FieldType::EMPTY) {
				set_type_to_object();
			}
			index_partial_paths(doc);
			if (specification.flags.store && specification.sep_types[SPC_OBJECT_TYPE] == FieldType::OBJECT) {
				*data = MsgPack(MsgPack::Type::MAP);
			}
		} else {
			set_type_to_object();
			const auto spc_object = std::move(specification);
			for (const auto& field : fields) {
				specification = spc_object;
				index_object(properties, *field.second, data, doc, field.first);
			}
		}
	}
}


std::unordered_set<std::string>
Schema::get_partial_paths(const std::vector<required_spc_t::prefix_t>& partial_prefixes, bool uuid_path)
{
	L_CALL(nullptr, "Schema::get_partial_paths(%zu, %s)", partial_prefixes.size(), uuid_path ? "true" : "false");

	if (partial_prefixes.size() > LIMIT_PARTIAL_PATHS_DEPTH) {
		THROW(ClientError, "Partial paths limit depth is %zu, and partial paths provided has a depth of %zu", LIMIT_PARTIAL_PATHS_DEPTH, partial_prefixes.size());
	}

	std::vector<std::string> paths;
	paths.reserve(std::pow(2, partial_prefixes.size() - 2));
	auto it = partial_prefixes.begin();
	paths.push_back(it->field);

	if (uuid_path) {
		if (!it->uuid.empty() && it->field != it->uuid) {
			paths.push_back(it->uuid);
		}
		const auto it_last = partial_prefixes.end() - 1;
		for (++it; it != it_last; ++it) {
			const auto size = paths.size();
			for (size_t i = 0; i < size; ++i) {
				std::string path;
				path.reserve(paths[i].length() + it->field.length());
				path.assign(paths[i]).append(it->field);
				paths.push_back(std::move(path));
				if (!it->uuid.empty() && it->field != it->uuid) {
					path.reserve(paths[i].length() + it->uuid.length());
					path.assign(paths[i]).append(it->uuid);
					paths.push_back(std::move(path));
				}
			}
		}

		if (!it_last->uuid.empty() && it_last->field != it_last->uuid) {
			const auto size = paths.size();
			for (size_t i = 0; i < size; ++i) {
				std::string path;
				path.reserve(paths[i].length() + it_last->uuid.length());
				path.assign(paths[i]).append(it_last->uuid);
				paths.push_back(std::move(path));
				paths[i].append(it_last->field);
			}
		} else {
			for (auto& path : paths) {
				path.append(it_last->field);
			}
		}
	} else {
		const auto it_last = partial_prefixes.end() - 1;
		for (++it; it != it_last; ++it) {
			const auto size = paths.size();
			for (size_t i = 0; i < size; ++i) {
				std::string path;
				path.reserve(paths[i].length() + it->field.length());
				path.assign(paths[i]).append(it->field);
				paths.push_back(std::move(path));
			}
		}

		for (auto& path : paths) {
			path.append(it_last->field);
		}
	}

	return std::unordered_set<std::string>(std::make_move_iterator(paths.begin()), std::make_move_iterator(paths.end()));
}


void
Schema::complete_namespace_specification(const MsgPack& item_value)
{
	L_CALL(this, "Schema::complete_namespace_specification(%s)", repr(item_value.to_string()).c_str());

	validate_required_namespace_data(item_value);

	if (specification.partial_prefixes.size() > 2) {
		auto paths = get_partial_paths(specification.partial_prefixes, specification.flags.uuid_path);
		specification.partial_index_spcs.reserve(paths.size());

		if (toUType(specification.index & TypeIndex::VALUES)) {
			for (auto& path : paths) {
				specification.partial_index_spcs.emplace_back(get_namespace_specification(specification.sep_types[SPC_CONCRETE_TYPE], std::move(path)));
			}
		} else {
			auto global_type = specification_t::global_type(specification.sep_types[SPC_CONCRETE_TYPE]);
			for (auto& path : paths) {
				specification.partial_index_spcs.emplace_back(global_type, std::move(path));
			}
		}
	} else {
		if (specification.flags.uuid_path) {
			switch (specification.index_uuid_field) {
				case UUIDFieldIndex::UUID: {
					if (specification.prefix.uuid.empty()) {
						auto global_type = specification_t::global_type(specification.sep_types[SPC_CONCRETE_TYPE]);
						if (specification.sep_types[SPC_CONCRETE_TYPE] == global_type) {
							// Use specification directly because path has never been indexed as UIDFieldIndex::BOTH and type is the same as global_type.
							if (toUType(specification.index & TypeIndex::VALUES)) {
								specification.slot = get_slot(specification.prefix.field, specification.get_ctype());
								for (auto& acc_prefix : specification.acc_prefix) {
									acc_prefix.insert(0, specification.prefix.field);
								}
							}
						} else if (toUType(specification.index & TypeIndex::VALUES)) {
							specification.partial_index_spcs.emplace_back(get_namespace_specification(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.field));
						} else {
							specification.partial_index_spcs.emplace_back(global_type, specification.prefix.field);
						}
					} else if (toUType(specification.index & TypeIndex::VALUES)) {
						specification.partial_index_spcs.emplace_back(get_namespace_specification(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.uuid));
					} else {
						specification.partial_index_spcs.emplace_back(specification_t::global_type(specification.sep_types[SPC_CONCRETE_TYPE]), specification.prefix.uuid);
					}
					break;
				}
				case UUIDFieldIndex::UUID_FIELD: {
					auto global_type = specification_t::global_type(specification.sep_types[SPC_CONCRETE_TYPE]);
					if (specification.sep_types[SPC_CONCRETE_TYPE] == global_type) {
						// Use specification directly because type is the same as global_type.
						if (toUType(specification.index & TypeIndex::FIELD_VALUES)) {
							if (specification.flags.has_uuid_prefix) {
								specification.slot = get_slot(specification.prefix.field, specification.get_ctype());
							}
							for (auto& acc_prefix : specification.acc_prefix) {
								acc_prefix.insert(0, specification.prefix.field);
							}
						}
					} else if (toUType(specification.index & TypeIndex::VALUES)) {
						specification.partial_index_spcs.emplace_back(get_namespace_specification(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.field));
					} else {
						specification.partial_index_spcs.emplace_back(global_type, specification.prefix.field);
					}
					break;
				}
				case UUIDFieldIndex::BOTH: {
					if (toUType(specification.index & TypeIndex::VALUES)) {
						specification.partial_index_spcs.emplace_back(get_namespace_specification(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.field));
						specification.partial_index_spcs.emplace_back(get_namespace_specification(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.uuid));
					} else {
						auto global_type = specification_t::global_type(specification.sep_types[SPC_CONCRETE_TYPE]);
						specification.partial_index_spcs.emplace_back(global_type, std::move(specification.prefix.field));
						specification.partial_index_spcs.emplace_back(global_type, specification.prefix.uuid);
					}
					break;
				}
			}
		} else {
			auto global_type = specification_t::global_type(specification.sep_types[SPC_CONCRETE_TYPE]);
			if (specification.sep_types[SPC_CONCRETE_TYPE] == global_type) {
				// Use specification directly because path is not uuid and type is the same as global_type.
				if (toUType(specification.index & TypeIndex::FIELD_VALUES)) {
					for (auto& acc_prefix : specification.acc_prefix) {
						acc_prefix.insert(0, specification.prefix.field);
					}
				}
			} else if (toUType(specification.index & TypeIndex::VALUES)) {
				specification.partial_index_spcs.emplace_back(get_namespace_specification(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.field));
			} else {
				specification.partial_index_spcs.emplace_back(global_type, specification.prefix.field);
			}
		}
	}

	specification.flags.complete = true;
}


void
Schema::complete_specification(const MsgPack& item_value)
{
	L_CALL(this, "Schema::complete_specification(%s)", repr(item_value.to_string()).c_str());

	if (!specification.flags.concrete) {
		validate_required_data(item_value);
	}

	if (specification.partial_prefixes.size() > 2) {
		auto paths = get_partial_paths(specification.partial_prefixes, specification.flags.uuid_path);
		specification.partial_index_spcs.reserve(paths.size());
		paths.erase(specification.prefix.field);
		if (!specification.local_prefix.uuid.empty()) {
			// local_prefix.uuid tell us if the last field is indexed as UIDFieldIndex::BOTH.
			paths.erase(specification.prefix.uuid);
		}

		if (toUType(specification.index & TypeIndex::VALUES)) {
			for (auto& path : paths) {
				specification.partial_index_spcs.emplace_back(get_namespace_specification(specification.sep_types[SPC_CONCRETE_TYPE], std::move(path)));
			}
		} else {
			auto global_type = specification_t::global_type(specification.sep_types[SPC_CONCRETE_TYPE]);
			for (auto& path : paths) {
				specification.partial_index_spcs.emplace_back(global_type, std::move(path));
			}
		}
	}

	if (specification.flags.uuid_path) {
		switch (specification.index_uuid_field) {
			case UUIDFieldIndex::UUID: {
				if (specification.prefix.uuid.empty()) {
					// Use specification directly because path has never been indexed as UIDFieldIndex::BOTH.
					if (toUType(specification.index & TypeIndex::FIELD_VALUES)) {
						specification.slot = get_slot(specification.prefix.field, specification.get_ctype());
						for (auto& acc_prefix : specification.acc_prefix) {
							acc_prefix.insert(0, specification.prefix.field);
						}
					}
				} else if (toUType(specification.index & TypeIndex::FIELD_VALUES)) {
					index_spc_t spc_uuid(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.uuid, get_slot(specification.prefix.uuid, specification.get_ctype()),
						specification.accuracy, specification.acc_prefix);
					for (auto& acc_prefix : spc_uuid.acc_prefix) {
						acc_prefix.insert(0, spc_uuid.prefix);
					}
					specification.partial_index_spcs.push_back(std::move(spc_uuid));
				} else {
					specification.partial_index_spcs.emplace_back(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.uuid);
				}
				break;
			}
			case UUIDFieldIndex::UUID_FIELD: {
				// Use specification directly.
				if (toUType(specification.index & TypeIndex::FIELD_VALUES)) {
					if (specification.flags.has_uuid_prefix) {
						specification.slot = get_slot(specification.prefix.field, specification.get_ctype());
					}
					for (auto& acc_prefix : specification.acc_prefix) {
						acc_prefix.insert(0, specification.prefix.field);
					}
				}
				break;
			}
			case UUIDFieldIndex::BOTH: {
				if (toUType(specification.index & TypeIndex::FIELD_VALUES)) {
					index_spc_t spc_field(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.field,
						specification.flags.has_uuid_prefix ? get_slot(specification.prefix.field, specification.get_ctype()) : specification.slot,
						specification.accuracy, specification.acc_prefix);
					for (auto& acc_prefix : spc_field.acc_prefix) {
						acc_prefix.insert(0, spc_field.prefix);
					}
					index_spc_t spc_uuid(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.uuid, get_slot(specification.prefix.uuid, specification.get_ctype()),
						specification.accuracy, specification.acc_prefix);
					for (auto& acc_prefix : spc_uuid.acc_prefix) {
						acc_prefix.insert(0, spc_uuid.prefix);
					}
					specification.partial_index_spcs.push_back(std::move(spc_field));
					specification.partial_index_spcs.push_back(std::move(spc_uuid));
				} else {
					specification.partial_index_spcs.emplace_back(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.field);
					specification.partial_index_spcs.emplace_back(specification.sep_types[SPC_CONCRETE_TYPE], specification.prefix.uuid);
				}
				break;
			}
		}
	} else {
		if (toUType(specification.index & TypeIndex::FIELD_VALUES)) {
			for (auto& acc_prefix : specification.acc_prefix) {
				acc_prefix.insert(0, specification.prefix.field);
			}
		}
	}

	specification.flags.complete = true;
}


inline void
Schema::set_type_to_object()
{
	L_CALL(this, "Schema::set_type_to_object()");

	if unlikely(specification.sep_types[SPC_OBJECT_TYPE] == FieldType::EMPTY && !specification.flags.inside_namespace) {
		auto& _types = get_mutable_properties(specification.full_meta_name)[RESERVED_TYPE];
		if (_types.is_undefined()) {
			_types = std::array<FieldType, SPC_TOTAL_TYPES>{ { FieldType::EMPTY, FieldType::OBJECT, FieldType::EMPTY, FieldType::EMPTY } };
			specification.sep_types[SPC_OBJECT_TYPE] = FieldType::OBJECT;
		} else {
			_types[SPC_OBJECT_TYPE] = FieldType::OBJECT;
			specification.sep_types[SPC_OBJECT_TYPE] = FieldType::OBJECT;
		}
	}
}


inline void
Schema::set_type_to_array()
{
	L_CALL(this, "Schema::set_type_to_array()");

	if unlikely(specification.sep_types[SPC_ARRAY_TYPE] == FieldType::EMPTY && !specification.flags.inside_namespace) {
		auto& _types = get_mutable_properties(specification.full_meta_name)[RESERVED_TYPE];
		if (_types.is_undefined()) {
			_types = std::array<FieldType, SPC_TOTAL_TYPES>{ { FieldType::EMPTY, FieldType::EMPTY, FieldType::ARRAY, FieldType::EMPTY } };
			specification.sep_types[SPC_ARRAY_TYPE] = FieldType::ARRAY;
		} else {
			_types[SPC_ARRAY_TYPE] = FieldType::ARRAY;
			specification.sep_types[SPC_ARRAY_TYPE] = FieldType::ARRAY;
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
	switch (specification.sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::GEO: {
			// Set partials and error.
			mut_properties[RESERVED_PARTIALS] = static_cast<bool>(specification.flags.partials);
			mut_properties[RESERVED_ERROR] = specification.error;

			if (specification.doc_acc) {
				try {
					for (const auto& _accuracy : *specification.doc_acc) {
						const auto val_acc = _accuracy.u64();
						if (val_acc <= HTM_MAX_LEVEL) {
							set_acc.insert(HTM_START_POS - 2 * val_acc);
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
						const auto str_accuracy = lower_string(_accuracy.str());
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
		case FieldType::TIME:
		case FieldType::TIMEDELTA: {
			if (specification.doc_acc) {
				try {
					static const auto adit_e = map_acc_time.end();
					for (const auto& _accuracy : *specification.doc_acc) {
						const auto str_accuracy = lower_string(_accuracy.str());
						const auto adit = map_acc_time.find(str_accuracy);
						if (adit == adit_e) {
							THROW(ClientError, "Data inconsistency, '%s': '%s' must be a subset of %s (%s not supported)", RESERVED_ACCURACY, Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]).c_str(), repr(str_set_acc_time).c_str(), repr(str_accuracy).c_str());
						} else {
							set_acc.insert(toUType(adit->second));
						}
					}
				} catch (const msgpack::type_error&) {
					THROW(ClientError, "Data inconsistency, '%s' in '%s' must be a subset of %s", RESERVED_ACCURACY, Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]).c_str(), repr(str_set_acc_time).c_str());
				}
			} else {
				set_acc.insert(def_accuracy_time.begin(), def_accuracy_time.end());
			}
			break;
		}
		case FieldType::INTEGER:
		case FieldType::POSITIVE:
		case FieldType::FLOAT: {
			if (specification.doc_acc) {
				try {
					for (const auto& _accuracy : *specification.doc_acc) {
						set_acc.insert(_accuracy.u64());
					}
				} catch (const msgpack::type_error&) {
					THROW(ClientError, "Data inconsistency, %s in %s must be an array of positive numbers", RESERVED_ACCURACY, Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]).c_str());
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

			// It is needed for soundex.
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

			// It is needed for soundex.
			mut_properties[RESERVED_LANGUAGE] = specification.language;
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

			// It is needed for soundex.
			mut_properties[RESERVED_LANGUAGE] = specification.language;
			break;
		}
		case FieldType::SCRIPT: {
			if (!specification.flags.has_index) {
				const auto index = TypeIndex::NONE; // Fallback to index anything.
				if (specification.index != index) {
					specification.index = index;
					mut_properties[RESERVED_INDEX] = index;
				}
				specification.flags.has_index = true;
			}
			break;
		}
		case FieldType::BOOLEAN:
		case FieldType::UUID:
			break;
		default:
			THROW(ClientError, "%s: '%s' is not supported", RESERVED_TYPE, Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]).c_str());
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

	// Process RESERVED_SLOT
	if (specification.slot == Xapian::BAD_VALUENO) {
		specification.slot = get_slot(specification.prefix.field, specification.get_ctype());
	}
	mut_properties[RESERVED_SLOT] = specification.slot;

	// If field is namespace fallback to index anything but values.
	if (!specification.flags.has_index && !specification.partial_prefixes.empty()) {
		const auto index = specification.index & ~TypeIndex::VALUES;
		if (specification.index != index) {
			specification.index = index;
			mut_properties[RESERVED_INDEX] = index;
		}
		specification.flags.has_index = true;
	}

	// Process RESERVED_TYPE
	mut_properties[RESERVED_TYPE] = specification.sep_types;

	specification.flags.concrete = true;

	// L_DEBUG(this, "\nspecification = %s\nmut_properties = %s", specification.to_string().c_str(), mut_properties.to_string(true).c_str());
}


void
Schema::validate_required_namespace_data(const MsgPack& value)
{
	L_CALL(this, "Schema::validate_required_namespace_data(%s)", repr(value.to_string()).c_str());

	L_SCHEMA(this, "Specification heritable and sent by user: %s", specification.to_string().c_str());

	if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::EMPTY) {
		guess_field_type(value);
	}

	switch (specification.sep_types[SPC_CONCRETE_TYPE]) {
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

		case FieldType::SCRIPT:
			if (!specification.flags.has_index) {
				specification.index = TypeIndex::NONE; // Fallback to index anything.
				specification.flags.has_index = true;
			}
			break;

		case FieldType::DATE:
		case FieldType::TIME:
		case FieldType::TIMEDELTA:
		case FieldType::INTEGER:
		case FieldType::POSITIVE:
		case FieldType::FLOAT:
		case FieldType::BOOLEAN:
		case FieldType::UUID:
			break;

		default:
			THROW(ClientError, "%s: '%s' is not supported", RESERVED_TYPE, Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]).c_str());
	}

	specification.flags.concrete = true;
}


void
Schema::validate_required_data(const MsgPack& value)
{
	L_CALL(this, "Schema::validate_required_data(%s)", repr(value.to_string()).c_str());

	L_SCHEMA(this, "Specification heritable and sent by user: %s", specification.to_string().c_str());

	if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::EMPTY) {
		if (specification.flags.strict) {
			THROW(MissingTypeError, "Type of field %s is missing", repr(specification.full_meta_name).c_str());
		}
		guess_field_type(value);
	}

	_validate_required_data(get_mutable_properties(specification.full_meta_name));
}


void
Schema::guess_field_type(const MsgPack& item_doc)
{
	L_CALL(this, "Schema::guess_field_type(%s)", repr(item_doc.to_string()).c_str());

	switch (item_doc.getType()) {
		case MsgPack::Type::POSITIVE_INTEGER:
			if (specification.flags.numeric_detection) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::POSITIVE;
				return;
			}
			break;
		case MsgPack::Type::NEGATIVE_INTEGER:
			if (specification.flags.numeric_detection) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::INTEGER;
				return;
			}
			break;
		case MsgPack::Type::FLOAT:
			if (specification.flags.numeric_detection) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::FLOAT;
				return;
			}
			break;
		case MsgPack::Type::BOOLEAN:
			if (specification.flags.bool_detection) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::BOOLEAN;
				return;
			}
			break;
		case MsgPack::Type::STR: {
			const auto str_value = item_doc.str();
			if (specification.flags.uuid_detection && Serialise::isUUID(str_value)) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::UUID;
				return;
			}
			if (specification.flags.date_detection && Datetime::isDate(str_value)) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::DATE;
				return;
			}
			if (specification.flags.time_detection && Datetime::isTime(str_value)) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::TIME;
				return;
			}
			if (specification.flags.timedelta_detection && Datetime::isTimedelta(str_value)) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::TIMEDELTA;
				return;
			}
			if (specification.flags.geo_detection && EWKT::isEWKT(str_value)) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::GEO;
				return;
			}
			if (specification.flags.text_detection && (!specification.flags.string_detection && Serialise::isText(str_value, specification.flags.bool_term))) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::TEXT;
				return;
			}
			if (specification.flags.string_detection && !specification.flags.bool_term) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::STRING;
				return;
			}
			if (specification.flags.term_detection) {
				specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::TERM;
				return;
			}
			if (specification.flags.bool_detection) {
				try {
					Serialise::boolean(str_value);
					specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::BOOLEAN;
					return;
				} catch (const SerialisationError&) { }
			}
			break;
		}
		case MsgPack::Type::ARRAY:
			THROW(ClientError, "'%s' cannot be array of arrays", RESERVED_VALUE);
		case MsgPack::Type::MAP:
			if (item_doc.size() == 1) {
				specification.sep_types[SPC_CONCRETE_TYPE] = Cast::getType(item_doc.begin()->str());
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
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::UUID) {
			switch (data_value.getType()) {
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
			if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::UUID) {
				switch (data_value.getType()) {
					case MsgPack::Type::UNDEFINED:
						data_value = MsgPack(MsgPack::Type::ARRAY);
						for (const auto& value : values) {
							data_value.push_back(normalize_uuid(value));
						}
						break;
					case MsgPack::Type::ARRAY:
						for (const auto& value : values) {
							data_value.push_back(normalize_uuid(value));
						}
						break;
					default:
						data_value = MsgPack({ data_value });
						for (const auto& value : values) {
							data_value.push_back(normalize_uuid(value));
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

	if (toUType(specification.index & TypeIndex::FIELD_TERMS)) {
		if (specification.partial_prefixes.size() > 2) {
			const auto paths = get_partial_paths(specification.partial_prefixes, specification.flags.uuid_path);
			for (const auto& path : paths) {
				doc.add_term(path);
			}
		} else {
			doc.add_term(specification.prefix.field);
		}
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
				if (value.is_null() || value.is_undefined()) {
					doc.add_term(specification.prefix.field);
				} else {
					index_term(doc, Serialise::MsgPack(specification, value), specification, pos++);
				}
			}
			break;
		}
		case TypeIndex::FIELD_VALUES: {
			std::set<std::string>& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				if (!(value.is_null() || value.is_undefined())) {
					index_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, specification, pos++);
				}
			}
			break;
		}
		case TypeIndex::FIELD_ALL: {
			std::set<std::string>& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				if (value.is_null() || value.is_undefined()) {
					doc.add_term(specification.prefix.field);
				} else {
					index_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, specification, pos++, &specification);
				}
			}
			break;
		}
		case TypeIndex::GLOBAL_TERMS: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			for (const MsgPack& value : values) {
				if (!(value.is_null() || value.is_undefined())) {
					index_term(doc, Serialise::MsgPack(global_spc, value), global_spc, pos++);
				}
			}
			break;
		}
		case TypeIndex::TERMS: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			for (const MsgPack& value : values) {
				if (value.is_null() || value.is_undefined()) {
					doc.add_term(specification.prefix.field);
				} else {
					index_all_term(doc, value, specification, global_spc, pos++);
				}
			}
			break;
		}
		case TypeIndex::GLOBAL_TERMS_FIELD_VALUES: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				if (!(value.is_null() || value.is_undefined())) {
					index_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, specification, pos++, nullptr, &global_spc);
				}
			}
			break;
		}
		case TypeIndex::GLOBAL_TERMS_FIELD_ALL: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				if (value.is_null() || value.is_undefined()) {
					doc.add_term(specification.prefix.field);
				} else {
					index_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, specification, pos++, &specification, &global_spc);
				}
			}
			break;
		}
		case TypeIndex::GLOBAL_VALUES: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				if (!(value.is_null() || value.is_undefined())) {
					index_value(doc, value.is_map() ? Cast::cast(value) : value, s_g, global_spc, pos++);
				}
			}
			break;
		}
		case TypeIndex::GLOBAL_VALUES_FIELD_TERMS: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				if (value.is_null() || value.is_undefined()) {
					doc.add_term(specification.prefix.field);
				} else {
					index_value(doc, value.is_map() ? Cast::cast(value) : value, s_g, global_spc, pos++, &specification);
				}
			}
			break;
		}
		case TypeIndex::VALUES: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_g = map_values[global_spc.slot];
			std::set<std::string>& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				if (!(value.is_null() || value.is_undefined())) {
					index_all_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, s_g, specification, global_spc, pos++);
				}
			}
			break;
		}
		case TypeIndex::GLOBAL_VALUES_FIELD_ALL: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_g = map_values[global_spc.slot];
			std::set<std::string>& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				if (value.is_null() || value.is_undefined()) {
					doc.add_term(specification.prefix.field);
				} else {
					index_all_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, s_g, specification, global_spc, pos++);
				}
			}
			break;
		}
		case TypeIndex::GLOBAL_ALL: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				if (!(value.is_null() || value.is_undefined())) {
					index_value(doc, value.is_map() ? Cast::cast(value) : value, s_g, global_spc, pos++, nullptr, &global_spc);
				}
			}
			break;
		}
		case TypeIndex::GLOBAL_ALL_FIELD_TERMS: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				if (value.is_null() || value.is_undefined()) {
					doc.add_term(specification.prefix.field);
				} else {
					index_value(doc, value.is_map() ? Cast::cast(value) : value, s_g, global_spc, pos++, &specification, &global_spc);
				}
			}
			break;
		}
		case TypeIndex::GLOBAL_ALL_FIELD_VALUES: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_g = map_values[global_spc.slot];
			std::set<std::string>& s_f = map_values[specification.slot];
			for (const MsgPack& value : values) {
				if (!(value.is_null() || value.is_undefined())) {
					index_all_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, s_g, specification, global_spc, pos++);
				}
			}
			break;
		}
		case TypeIndex::ALL: {
			const auto& global_spc = specification_t::get_global(specification.sep_types[SPC_CONCRETE_TYPE]);
			std::set<std::string>& s_f = map_values[specification.slot];
			std::set<std::string>& s_g = map_values[global_spc.slot];
			for (const MsgPack& value : values) {
				if (value.is_null() || value.is_undefined()) {
					doc.add_term(specification.prefix.field);
				} else {
					index_all_value(doc, value.is_map() ? Cast::cast(value) : value, s_f, s_g, specification, global_spc, pos++);
				}
			}
			break;
		}
	}
}


void
Schema::index_term(Xapian::Document& doc, std::string serialise_val, const specification_t& field_spc, size_t pos)
{
	L_CALL(nullptr, "Schema::index_term(<Xapian::Document>, %s, <specification_t>, %zu)", repr(serialise_val).c_str(), pos);

	switch (field_spc.sep_types[SPC_CONCRETE_TYPE]) {
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
				term_generator.index_text(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix.field + field_spc.get_ctype());
			} else {
				term_generator.index_text_without_positions(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix.field + field_spc.get_ctype());
			}
			L_INDEX(nullptr, "Field Text to Index [%d] => %s:%s [Positions: %s]", pos, field_spc.prefix.field.c_str(), serialise_val.c_str(), positions ? "true" : "false");
			break;
		}

		case FieldType::STRING: {
			Xapian::TermGenerator term_generator;
			term_generator.set_document(doc);
			const auto position = field_spc.position[getPos(pos, field_spc.position.size())]; // String uses position (not positions) which is off by default
			if (position) {
				term_generator.index_text(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix.field + field_spc.get_ctype());
				L_INDEX(nullptr, "Field String to Index [%d] => %s:%s [Positions: %s]", pos, field_spc.prefix.field.c_str(), serialise_val.c_str(), position ? "true" : "false");
			} else {
				term_generator.index_text_without_positions(serialise_val, field_spc.weight[getPos(pos, field_spc.weight.size())], field_spc.prefix.field + field_spc.get_ctype());
				L_INDEX(nullptr, "Field String to Index [%d] => %s:%s", pos, field_spc.prefix.field.c_str(), serialise_val.c_str());
			}
			break;
		}

		case FieldType::TERM:
			if (!field_spc.flags.bool_term) {
				to_lower(serialise_val);
			}

		default: {
			serialise_val = prefixed(serialise_val, field_spc.prefix.field, field_spc.get_ctype());
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

	auto serialise_val = Serialise::MsgPack(field_spc, value);
	index_term(doc, serialise_val, field_spc, pos);
	index_term(doc, serialise_val, global_spc, pos);
}


void
Schema::merge_geospatial_values(std::set<std::string>& s, std::vector<range_t> ranges, std::vector<Cartesian> centroids)
{
	L_CALL(nullptr, "Schema::merge_geospatial_values(...)");

	if (s.empty()) {
		s.insert(Serialise::ranges_centroids(ranges, centroids));
	} else {
		auto prev_value = Unserialise::ranges_centroids(*s.begin());
		s.clear();
		ranges = HTM::range_union(std::move(ranges), std::vector<range_t>(std::make_move_iterator(prev_value.first.begin()), std::make_move_iterator(prev_value.first.end())));
		if (prev_value.second.empty()) {
			s.insert(Serialise::ranges_centroids(ranges, centroids));
		} else {
			for (const auto& _centroid : prev_value.second) {
				if (std::find(centroids.begin(), centroids.end(), _centroid) == centroids.end()) {
					centroids.push_back(_centroid);
				}
			}
			s.insert(Serialise::ranges_centroids(ranges, centroids));
		}
	}
}


void
Schema::index_value(Xapian::Document& doc, const MsgPack& value, std::set<std::string>& s, const specification_t& spc, size_t pos, const specification_t* field_spc, const specification_t* global_spc)
{
	L_CALL(nullptr, "Schema::index_value(<Xapian::Document>, %s, <std::set<std::string>>, <specification_t>, %zu, <specification_t*>, <specification_t*>)", repr(value.to_string()).c_str(), pos);

	switch (spc.sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::FLOAT: {
			try {
				const auto f_val = value.f64();
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
				const auto i_val = value.i64();
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
				const auto u_val = value.u64();
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
		}
		case FieldType::TIME: {
			double t_val = 0.0;
			auto ser_value = Serialise::time(value, t_val);
			if (field_spc) {
				index_term(doc, ser_value, *field_spc, pos);
			}
			if (global_spc) {
				index_term(doc, ser_value, *global_spc, pos);
			}
			s.insert(std::move(ser_value));
			GenerateTerms::integer(doc, spc.accuracy, spc.acc_prefix, t_val);
			return;
		}
		case FieldType::TIMEDELTA: {
			double t_val = 0.0;
			auto ser_value = Serialise::timedelta(value, t_val);
			if (field_spc) {
				index_term(doc, ser_value, *field_spc, pos);
			}
			if (global_spc) {
				index_term(doc, ser_value, *global_spc, pos);
			}
			s.insert(std::move(ser_value));
			GenerateTerms::integer(doc, spc.accuracy, spc.acc_prefix, t_val);
			return;
		}
		case FieldType::GEO: {
			GeoSpatial geo(value);
			const auto& geometry = geo.getGeometry();
			auto ranges = geometry->getRanges(spc.flags.partials, spc.error);
			if (ranges.empty()) {
				return;
			}
			std::string term;
			if (field_spc) {
				if (spc.flags.partials == DEFAULT_GEO_PARTIALS && spc.error == DEFAULT_GEO_ERROR) {
					term = Serialise::ranges(ranges);
					index_term(doc, term, *field_spc, pos);
				} else {
					const auto f_ranges = geometry->getRanges(DEFAULT_GEO_PARTIALS, DEFAULT_GEO_ERROR);
					term = Serialise::ranges(f_ranges);
					index_term(doc, term, *field_spc, pos);
				}
			}
			if (global_spc) {
				if (field_spc) {
					index_term(doc, std::move(term), *global_spc, pos);
				} else {
					if (spc.flags.partials == DEFAULT_GEO_PARTIALS && spc.error == DEFAULT_GEO_ERROR) {
						index_term(doc, Serialise::ranges(ranges), *global_spc, pos);
					} else {
						const auto g_ranges = geometry->getRanges(DEFAULT_GEO_PARTIALS, DEFAULT_GEO_ERROR);
						index_term(doc, Serialise::ranges(g_ranges), *global_spc, pos);
					}
				}
			}
			GenerateTerms::geo(doc, spc.accuracy, spc.acc_prefix, ranges);
			merge_geospatial_values(s, std::move(ranges), geometry->getCentroids());
			return;
		}
		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING: {
			try {
				auto ser_value = value.str();
				if (field_spc) {
					index_term(doc, ser_value, *field_spc, pos);
				}
				if (global_spc) {
					index_term(doc, ser_value, *global_spc, pos);
				}
				s.insert(std::move(ser_value));
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for %s type: %s", Serialise::type(spc.sep_types[SPC_CONCRETE_TYPE]).c_str(), repr(value.to_string()).c_str());
			}
		}
		case FieldType::BOOLEAN: {
			auto ser_value = Serialise::MsgPack(spc, value);
			if (field_spc) {
				index_term(doc, ser_value, *field_spc, pos);
			}
			if (global_spc) {
				index_term(doc, ser_value, *global_spc, pos);
			}
			s.insert(std::move(ser_value));
			return;
		}
		case FieldType::UUID: {
			try {
				auto ser_value = Serialise::uuid(value.str());
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
			}
		}
		default:
			THROW(ClientError, "Type: '%c' is an unknown type", spc.sep_types[SPC_CONCRETE_TYPE]);
	}
}


void
Schema::index_all_value(Xapian::Document& doc, const MsgPack& value, std::set<std::string>& s_f, std::set<std::string>& s_g, const specification_t& field_spc, const specification_t& global_spc, size_t pos)
{
	L_CALL(nullptr, "Schema::index_all_value(<Xapian::Document>, %s, <std::set<std::string>>, <std::set<std::string>>, <specification_t>, <specification_t>, %zu)", repr(value.to_string()).c_str(), pos);

	switch (field_spc.sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::FLOAT: {
			try {
				const auto f_val = value.f64();
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
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for float type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::INTEGER: {
			try {
				const auto i_val = value.i64();
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
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for integer type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::POSITIVE: {
			try {
				const auto u_val = value.u64();
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
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for positive type: %s", repr(value.to_string()).c_str());
			}
		}
		case FieldType::DATE: {
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
			return;
		}
		case FieldType::TIME: {
			double t_val = 0.0;
			auto ser_value = Serialise::time(value, t_val);
			if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
				index_term(doc, ser_value, field_spc, pos);
			}
			if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
				index_term(doc, ser_value, global_spc, pos);
			}
			s_f.insert(ser_value);
			s_g.insert(std::move(ser_value));
			if (field_spc.accuracy == global_spc.accuracy) {
				GenerateTerms::integer(doc, field_spc.accuracy, field_spc.acc_prefix, global_spc.acc_prefix, t_val);
			} else {
				GenerateTerms::integer(doc, field_spc.accuracy, field_spc.acc_prefix, t_val);
				GenerateTerms::integer(doc, global_spc.accuracy, global_spc.acc_prefix, t_val);
			}
			return;
		}
		case FieldType::TIMEDELTA: {
			double t_val;
			auto ser_value = Serialise::timedelta(value, t_val);
			if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
				index_term(doc, ser_value, field_spc, pos);
			}
			if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
				index_term(doc, ser_value, global_spc, pos);
			}
			s_f.insert(ser_value);
			s_g.insert(std::move(ser_value));
			if (field_spc.accuracy == global_spc.accuracy) {
				GenerateTerms::integer(doc, field_spc.accuracy, field_spc.acc_prefix, global_spc.acc_prefix, t_val);
			} else {
				GenerateTerms::integer(doc, field_spc.accuracy, field_spc.acc_prefix, t_val);
				GenerateTerms::integer(doc, global_spc.accuracy, global_spc.acc_prefix, t_val);
			}
			return;
		}
		case FieldType::GEO: {
			GeoSpatial geo(value);
			const auto& geometry = geo.getGeometry();
			auto ranges = geometry->getRanges(field_spc.flags.partials, field_spc.error);
			if (ranges.empty()) {
				return;
			}
			if (field_spc.flags.partials == global_spc.flags.partials && field_spc.error == global_spc.error) {
				if (toUType(field_spc.index & TypeIndex::TERMS)) {
					auto ser_value = Serialise::ranges(ranges);
					if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
						index_term(doc, ser_value, field_spc, pos);
					}
					if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
						index_term(doc, std::move(ser_value), global_spc, pos);
					}
				}
				if (field_spc.accuracy == global_spc.accuracy) {
					GenerateTerms::geo(doc, field_spc.accuracy, field_spc.acc_prefix, global_spc.acc_prefix, ranges);
				} else {
					GenerateTerms::geo(doc, field_spc.accuracy, field_spc.acc_prefix, ranges);
					GenerateTerms::geo(doc, global_spc.accuracy, global_spc.acc_prefix, ranges);
				}
				merge_geospatial_values(s_f, ranges, geometry->getCentroids());
				merge_geospatial_values(s_g, std::move(ranges), geometry->getCentroids());
			} else {
				auto g_ranges = geometry->getRanges(global_spc.flags.partials, global_spc.error);
				if (toUType(field_spc.index & TypeIndex::TERMS)) {
					const auto ser_value = Serialise::ranges(g_ranges);
					if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
						index_term(doc, ser_value, field_spc, pos);
					}
					if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
						index_term(doc, std::move(ser_value), global_spc, pos);
					}
				}
				GenerateTerms::geo(doc, field_spc.accuracy, field_spc.acc_prefix, ranges);
				GenerateTerms::geo(doc, global_spc.accuracy, global_spc.acc_prefix, g_ranges);
				merge_geospatial_values(s_f, std::move(ranges), geometry->getCentroids());
				merge_geospatial_values(s_g, std::move(g_ranges), geometry->getCentroids());
			}
			return;
		}
		case FieldType::TERM:
		case FieldType::TEXT:
		case FieldType::STRING: {
			try {
				auto ser_value = value.str();
				if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
					index_term(doc, ser_value, field_spc, pos);
				}
				if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
					index_term(doc, ser_value, global_spc, pos);
				}
				s_f.insert(ser_value);
				s_g.insert(std::move(ser_value));
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for %s type: %s", Serialise::type(field_spc.sep_types[SPC_CONCRETE_TYPE]).c_str(), repr(value.to_string()).c_str());
			}
		}
		case FieldType::BOOLEAN: {
			auto ser_value = Serialise::MsgPack(field_spc, value);
			if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
				index_term(doc, ser_value, field_spc, pos);
			}
			if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
				index_term(doc, ser_value, global_spc, pos);
			}
			s_f.insert(ser_value);
			s_g.insert(std::move(ser_value));
			return;
		}
		case FieldType::UUID: {
			try {
				auto ser_value = Serialise::uuid(value.str());
				if (toUType(field_spc.index & TypeIndex::FIELD_TERMS)) {
					index_term(doc, ser_value, field_spc, pos);
				}
				if (toUType(field_spc.index & TypeIndex::GLOBAL_TERMS)) {
					index_term(doc, ser_value, global_spc, pos);
				}
				s_f.insert(ser_value);
				s_g.insert(std::move(ser_value));
				return;
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "Format invalid for uuid type: %s", repr(value.to_string()).c_str());
			}
		}
		default:
			THROW(ClientError, "Type: '%c' is an unknown type", field_spc.sep_types[SPC_CONCRETE_TYPE]);
	}
}


void
Schema::update_schema(MsgPack*& mut_parent_properties, const std::string& name, const MsgPack& obj_schema)
{
	L_CALL(this, "Schema::update_schema(%s, %s, %s)", repr(mut_parent_properties->to_string()).c_str(), repr(name).c_str(), repr(obj_schema.to_string()).c_str());

	if (name.empty()) {
		THROW(ClientError, "Field name must not be empty");
	}

	switch (obj_schema.getType()) {
		case MsgPack::Type::MAP: {
			const auto spc_start = specification;
			FieldVector fields;
			auto mut_properties = mut_parent_properties;

			mut_properties = &get_subproperties_write(mut_properties, name, obj_schema, fields);

			if (!specification.flags.concrete && specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::EMPTY) {
				_validate_required_data(*mut_properties);
			}

			if (specification.flags.is_namespace && fields.size()) {
				specification = std::move(spc_start);
				return;
			}

			if (!fields.empty() || (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::EMPTY && specification.sep_types[SPC_OBJECT_TYPE] == FieldType::EMPTY && specification.sep_types[SPC_ARRAY_TYPE] == FieldType::EMPTY)) {
				set_type_to_object();
			}

			const auto spc_object = std::move(specification);
			for (const auto& field : fields) {
				specification = spc_object;
				update_schema(mut_properties, field.first, *field.second);
			}

			specification = std::move(spc_start);
			return;
		}
		case MsgPack::Type::ARRAY:
			if (!is_valid(name)) {
				THROW(ClientError, "Field name: %s in %s is not valid", repr(name).c_str(), repr(specification.full_meta_name).c_str());
			}
			for (const auto& item : obj_schema) {
				if (item.is_map()) {
					update_schema(mut_parent_properties, name, item);
				}
			}
			return;
		default:
			if (!is_valid(name)) {
				THROW(ClientError, "Field name: %s in %s is not valid", repr(name).c_str(), repr(specification.full_meta_name).c_str());
			}
			return;
	}
}


inline void
Schema::update_prefixes()
{
	L_CALL(this, "Schema::update_prefixes()");

	if (specification.flags.uuid_path) {
		if (specification.flags.uuid_field) {
			switch (specification.index_uuid_field) {
				case UUIDFieldIndex::UUID: {
					specification.flags.has_uuid_prefix = true;
					specification.prefix.field.append(specification.local_prefix.uuid);
					if (!specification.prefix.uuid.empty()) {
						specification.prefix.uuid.append(specification.local_prefix.uuid);
					}
					specification.local_prefix.field = std::move(specification.local_prefix.uuid);
					specification.local_prefix.uuid.clear();
					break;
				}
				case UUIDFieldIndex::UUID_FIELD: {
					specification.prefix.field.append(specification.local_prefix.field);
					if (!specification.prefix.uuid.empty()) {
						specification.prefix.uuid.append(specification.local_prefix.field);
					}
					specification.local_prefix.uuid.clear();
					break;
				}
				case UUIDFieldIndex::BOTH: {
					if (specification.prefix.uuid.empty()) {
						specification.prefix.uuid = specification.prefix.field;
					}
					specification.prefix.field.append(specification.local_prefix.field);
					specification.prefix.uuid.append(specification.local_prefix.uuid);
					break;
				}
			}
		} else {
			specification.prefix.field.append(specification.local_prefix.field);
			if (!specification.prefix.uuid.empty()) {
				specification.prefix.uuid.append(specification.local_prefix.field);
			}
		}
	} else {
		specification.prefix.field.append(specification.local_prefix.field);
	}

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


template <typename T>
inline void
Schema::_get_subproperties(T& properties, const std::string& meta_name)
{
	L_CALL(this, "Schema::_get_subproperties(%s, %s)", repr(properties->to_string()).c_str(), repr(meta_name).c_str());

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
		specification.full_meta_name.append(1, DB_OFFSPRING_UNION).append(meta_name);
	}

	update_specification(*properties);
}


const MsgPack&
Schema::get_subproperties(const MsgPack*& properties, MsgPack*& data, const std::string& name, const MsgPack& object, FieldVector& fields)
{
	L_CALL(this, "Schema::get_subproperties(%s, <data>, %s, %s, <fields>)", repr(properties->to_string()).c_str(), repr(name).c_str(), repr(object.to_string()).c_str());

	std::vector<std::string> field_names;
	Split<>::split(name, DB_OFFSPRING_UNION, std::back_inserter(field_names));

	const auto it_last = field_names.end() - 1;
	if (specification.flags.is_namespace) {
		restart_namespace_specification();
		for (auto it = field_names.begin(); it != it_last; ++it) {
			auto norm_field_name = detect_dynamic(*it);
			update_prefixes();
			if (specification.flags.store) {
				data = &(*data)[norm_field_name];
			}
		}
		process_document_properties(object, fields);
		auto norm_field_name = detect_dynamic(*it_last);
		update_prefixes();
		specification.flags.inside_namespace = true;
		if (specification.flags.store) {
			data = &(*data)[norm_field_name];
		}
	} else {
		for (auto it = field_names.begin(); it != it_last; ++it) {
			const auto& field_name = *it;
			if (!is_valid(field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
				THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(field_name).c_str(), repr(specification.full_meta_name).c_str());
			}
			restart_specification();
			try {
				_get_subproperties(properties, field_name);
				update_prefixes();
				if (specification.flags.store) {
					data = &(*data)[field_name];
				}
			} catch (const std::out_of_range&) {
				auto norm_field_name = detect_dynamic(field_name);
				if (specification.flags.uuid_field) {
					try {
						_get_subproperties(properties, specification.meta_name);
						update_prefixes();
						if (specification.flags.store) {
							data = &(*data)[norm_field_name];
						}
						continue;
					} catch (const std::out_of_range&) { }
				}

				auto mut_properties = &get_mutable_properties(specification.full_meta_name);
				add_field(mut_properties);
				if (specification.flags.store) {
					data = &(*data)[norm_field_name];
				}
				for (++it; it != it_last; ++it) {
					const auto& n_field_name = *it;
					if (!is_valid(n_field_name)) {
						THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(n_field_name).c_str(), repr(specification.full_meta_name).c_str());
					} else {
						norm_field_name = detect_dynamic(n_field_name);
						add_field(mut_properties);
						if (specification.flags.store) {
							data = &(*data)[norm_field_name];
						}
					}
				}
				const auto& n_field_name = *it_last;
				if (!is_valid(n_field_name)) {
					THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(n_field_name).c_str(), repr(specification.full_meta_name).c_str());
				} else {
					norm_field_name = detect_dynamic(n_field_name);
					add_field(mut_properties, object, fields);
					if (specification.flags.store) {
						data = &(*data)[norm_field_name];
					}
				}
				return *mut_properties;
			}
		}

		const auto& field_name = *it_last;
		if (!is_valid(field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
			THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(field_name).c_str(), repr(specification.full_meta_name).c_str());
		}
		restart_specification();
		try {
			_get_subproperties(properties, field_name);
			process_document_properties(object, fields);
			update_prefixes();
			if (specification.flags.store) {
				data = &(*data)[field_name];
			}
		} catch (const std::out_of_range&) {
			auto norm_field_name = detect_dynamic(field_name);
			if (specification.flags.uuid_field) {
				try {
					_get_subproperties(properties, specification.meta_name);
					process_document_properties(object, fields);
					update_prefixes();
					if (specification.flags.store) {
						data = &(*data)[norm_field_name];
					}
					return *properties;
				} catch (const std::out_of_range&) { }
			}

			auto mut_properties = &get_mutable_properties(specification.full_meta_name);
			add_field(mut_properties, object, fields);
			if (specification.flags.store) {
				data = &(*data)[norm_field_name];
			}
			return *mut_properties;
		}
	}

	return *properties;
}


const MsgPack&
Schema::get_subproperties(const MsgPack*& properties, MsgPack*& data, const std::string& name)
{
	L_CALL(this, "Schema::get_subproperties(%s, <data>, %s)", repr(properties->to_string()).c_str(), repr(name).c_str());

	Split<char> field_names(name, DB_OFFSPRING_UNION);

	if (specification.flags.is_namespace) {
		restart_namespace_specification();
		if (specification.flags.store) {
			for (const auto& field_name : field_names) {
				data = &(*data)[detect_dynamic(field_name)];
				update_prefixes();
			}
		} else {
			for (const auto& field_name : field_names) {
				detect_dynamic(field_name);
				update_prefixes();
			}
		}
		specification.flags.inside_namespace = true;
	} else {
		const auto it_e = field_names.end();
		for (auto it = field_names.begin(); it != it_e; ++it) {
			const auto& field_name = *it;
			if (!is_valid(field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
				THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(field_name).c_str(), repr(specification.full_meta_name).c_str());
			}
			restart_specification();
			try {
				_get_subproperties(properties, field_name);
				update_prefixes();
				if (specification.flags.store) {
					data = &(*data)[field_name];
				}
			} catch (const std::out_of_range&) {
				auto norm_field_name = detect_dynamic(field_name);
				if (specification.flags.uuid_field) {
					try {
						_get_subproperties(properties, specification.meta_name);
						update_prefixes();
						if (specification.flags.store) {
							data = &(*data)[norm_field_name];
						}
						continue;
					} catch (const std::out_of_range&) { }
				}

				auto mut_properties = &get_mutable_properties(specification.full_meta_name);
				add_field(mut_properties);
				if (specification.flags.store) {
					data = &(*data)[norm_field_name];
					for (++it; it != it_e; ++it) {
						const auto& n_field_name = *it;
						if (!is_valid(n_field_name)) {
							THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(n_field_name).c_str(), repr(specification.full_meta_name).c_str());
						} else {
							data = &(*data)[detect_dynamic(n_field_name)];
							add_field(mut_properties);
						}
					}
				} else {
					for (++it; it != it_e; ++it) {
						const auto& n_field_name = *it;
						if (!is_valid(n_field_name)) {
							THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(n_field_name).c_str(), repr(specification.full_meta_name).c_str());
						} else {
							detect_dynamic(n_field_name);
							add_field(mut_properties);
						}
					}
				}
				return *mut_properties;
			}
		}
	}

	return *properties;
}


MsgPack&
Schema::get_subproperties_write(MsgPack*& mut_properties, const std::string& name, const MsgPack& object, FieldVector& fields)
{
	L_CALL(this, "Schema::get_subproperties_write(%s, %s, %s, <fields>)", repr(mut_properties->to_string()).c_str(), repr(name).c_str(), repr(object.to_string()).c_str());

	std::vector<std::string> field_names;
	Split<>::split(name, DB_OFFSPRING_UNION, std::back_inserter(field_names));

	const auto it_last = field_names.end() - 1;
	for (auto it = field_names.begin(); it != it_last; ++it) {
		const auto& field_name = *it;
		if (!is_valid(field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
			THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(field_name).c_str(), repr(specification.full_meta_name).c_str());
		}
		restart_specification();
		try {
			_get_subproperties(mut_properties, field_name);
		} catch (const std::out_of_range&) {
			for ( ; it != it_last; ++it) {
				const auto& n_field_name = *it;
				if (!is_valid(n_field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(n_field_name))) {
					THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(n_field_name).c_str(), repr(specification.full_meta_name).c_str());
				} else {
					verify_dynamic(n_field_name);
					add_field(mut_properties);
				}
			}

			const auto& n_field_name = *it_last;
			if (!is_valid(n_field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(n_field_name))) {
				THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(n_field_name).c_str(), repr(specification.full_meta_name).c_str());
			} else {
				verify_dynamic(n_field_name);
				add_field(mut_properties, object, fields);
			}

			return *mut_properties;
		}
	}

	const auto& field_name = *it_last;
	if (!is_valid(field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
		THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(field_name).c_str(), repr(specification.full_meta_name).c_str());
	}
	restart_specification();
	try {
		_get_subproperties(mut_properties, field_name);
		process_document_properties_write(mut_properties, object, fields);
	} catch (const std::out_of_range&) {
		if (!is_valid(field_name) && !(specification.full_meta_name.empty() && map_dispatch_set_default_spc.count(field_name))) {
			THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(name).c_str(), repr(field_name).c_str(), repr(specification.full_meta_name).c_str());
		} else {
			verify_dynamic(field_name);
			add_field(mut_properties, object, fields);
		}
	}
	return *mut_properties;
}


inline void
Schema::verify_dynamic(const std::string& field_name)
{
	L_CALL(this, "Schema::verify_dynamic(%s)", repr(field_name).c_str());

	if (field_name == UUID_FIELD_NAME) {
		specification.meta_name.assign(field_name);
		specification.flags.uuid_field = true;
		specification.flags.uuid_path = true;
	} else {
		specification.local_prefix.uuid = get_prefix(field_name);
		specification.meta_name.assign(field_name);
		specification.flags.uuid_field = false;
	}
}


inline std::string
Schema::detect_dynamic(const std::string& field_name)
{
	L_CALL(this, "Schema::detect_dynamic(%s)", repr(field_name).c_str());

	if (Serialise::isUUID(field_name)) {
		auto ser_uuid = Serialise::uuid(field_name);
		specification.local_prefix.uuid.assign(ser_uuid);
		static const auto uuid_field_prefix = get_prefix(UUID_FIELD_NAME);
		specification.local_prefix.field.assign(uuid_field_prefix);
		specification.meta_name.assign(UUID_FIELD_NAME);
		specification.flags.uuid_field = true;
		specification.flags.uuid_path = true;
		return normalize_uuid(field_name);
	} else {
		specification.local_prefix.field.assign(get_prefix(field_name));
		specification.meta_name.assign(field_name);
		specification.flags.uuid_field = false;
		return field_name;
	}
}


void
Schema::process_document_properties(const MsgPack& object, FieldVector& fields)
{
	L_CALL(this, "Schema::process_document_properties(%s, <fields>)", repr(object.to_string()).c_str());

	static const auto ddit_e = map_dispatch_document_properties.end();
	const auto it_e = object.end();
	if (specification.flags.concrete) {
		for (auto it = object.begin(); it != it_e; ++it) {
			auto str_key = it->str();
			const auto ddit = map_dispatch_document_properties.find(str_key);
			if (ddit == ddit_e) {
				fields.emplace_back(std::move(str_key), &it.value());
			} else {
				(this->*ddit->second)(str_key, it.value());
			}
		}
	} else {
		static const auto wtit_e = map_dispatch_document_properties_without_concrete_type.end();
		for (auto it = object.begin(); it != it_e; ++it) {
			auto str_key = it->str();
			const auto wtit = map_dispatch_document_properties_without_concrete_type.find(str_key);
			if (wtit == wtit_e) {
				const auto ddit = map_dispatch_document_properties.find(str_key);
				if (ddit == ddit_e) {
					fields.emplace_back(std::move(str_key), &it.value());
				} else {
					(this->*ddit->second)(str_key, it.value());
				}
			} else {
				(this->*wtit->second)(str_key, it.value());
			}
		}
	}
#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	normalize_script();
#endif
}


void
Schema::process_document_properties_write(MsgPack*& mut_properties, const MsgPack& object, FieldVector& fields)
{
	L_CALL(this, "Schema::process_document_properties_write(%s, %s, <fields>)", repr(mut_properties->to_string()).c_str(), repr(object.to_string()).c_str());

	static const auto wpit_e = map_dispatch_write_properties.end();
	static const auto ddit_e = map_dispatch_document_properties.end();
	const auto it_e = object.end();
	if (specification.flags.concrete) {
		for (auto it = object.begin(); it != it_e; ++it) {
			auto str_key = it->str();
			const auto wpit = map_dispatch_write_properties.find(str_key);
			if (wpit == wpit_e) {
				const auto ddit = map_dispatch_document_properties.find(str_key);
				if (ddit == ddit_e) {
					fields.emplace_back(std::move(str_key), &it.value());
				} else {
					(this->*ddit->second)(str_key, it.value());
				}
			} else {
				(this->*wpit->second)(*mut_properties, str_key, it.value());
			}
		}
	} else {
		static const auto wtit_e = map_dispatch_document_properties_without_concrete_type.end();
		for (auto it = object.begin(); it != it_e; ++it) {
			auto str_key = it->str();
			const auto wpit = map_dispatch_write_properties.find(str_key);
			if (wpit == wpit_e) {
				const auto wtit = map_dispatch_document_properties_without_concrete_type.find(str_key);
				if (wtit == wtit_e) {
					const auto ddit = map_dispatch_document_properties.find(str_key);
					if (ddit == ddit_e) {
						fields.emplace_back(std::move(str_key), &it.value());
					} else {
						(this->*ddit->second)(str_key, it.value());
					}
				} else {
					(this->*wtit->second)(str_key, it.value());
				}
			} else {
				(this->*wpit->second)(*mut_properties, str_key, it.value());
			}
		}
	}
#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	write_script(*mut_properties);
#endif
}


void
Schema::add_field(MsgPack*& mut_properties, const MsgPack& object, FieldVector& fields)
{
	L_CALL(this, "Schema::add_field(%s, %s, <fields>)", repr(mut_properties->to_string()).c_str(), repr(object.to_string()).c_str());

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
		specification.full_meta_name.append(1, DB_OFFSPRING_UNION).append(specification.meta_name);
	}

	// Write obj specifications.
	static const auto wpit_e = map_dispatch_write_properties.end();
	static const auto wtit_e = map_dispatch_document_properties_without_concrete_type.end();
	static const auto ddit_e = map_dispatch_document_properties.end();
	const auto it_e = object.end();
	for (auto it = object.begin(); it != it_e; ++it) {
		auto str_key = it->str();
		const auto wpit = map_dispatch_write_properties.find(str_key);
		if (wpit == wpit_e) {
			const auto wtit = map_dispatch_document_properties_without_concrete_type.find(str_key);
			if (wtit == wtit_e) {
				const auto ddit = map_dispatch_document_properties.find(str_key);
				if (ddit == ddit_e) {
					fields.emplace_back(std::move(str_key), &it.value());
				} else {
					(this->*ddit->second)(str_key, it.value());
				}
			} else {
				(this->*wtit->second)(str_key, it.value());
			}
		} else {
			(this->*wpit->second)(*mut_properties, str_key, it.value());
		}
	}
#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	write_script(*mut_properties);
#endif

	// Load default specifications.
	static const auto dsit_e = map_dispatch_set_default_spc.end();
	const auto dsit = map_dispatch_set_default_spc.find(specification.full_meta_name);
	if (dsit != dsit_e) {
		(this->*dsit->second)(*mut_properties);
	}

	// Write prefix in properties.
	(*mut_properties)[RESERVED_PREFIX] = specification.local_prefix.field;

	update_prefixes();
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
		specification.full_meta_name.append(1, DB_OFFSPRING_UNION).append(specification.meta_name);
	}

	// Load default specifications.
	static const auto dsit_e = map_dispatch_set_default_spc.end();
	const auto dsit = map_dispatch_set_default_spc.find(specification.full_meta_name);
	if (dsit != dsit_e) {
		(this->*dsit->second)(*mut_properties);
	}

	// Write prefix in properties.
	(*mut_properties)[RESERVED_PREFIX] = specification.local_prefix.field;

	update_prefixes();
}


void
Schema::update_specification(const MsgPack& properties)
{
	L_CALL(this, "Schema::update_specification(%s)", repr(properties.to_string()).c_str());

	static const auto dpit_e = map_dispatch_update_properties.end();
	const auto it_e = properties.end();
	for (auto it = properties.begin(); it != it_e; ++it) {
		const auto dpit = map_dispatch_update_properties.find(it->str());
		if (dpit != dpit_e) {
			(this->*dpit->second)(it.value());
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
			specification.position.push_back(static_cast<Xapian::termpos>(_position.u64()));
		}
	} else {
		specification.position.push_back(static_cast<Xapian::termpos>(prop_position.u64()));
	}
}


void
Schema::update_weight(const MsgPack& prop_weight)
{
	L_CALL(this, "Schema::update_weight(%s)", repr(prop_weight.to_string()).c_str());

	specification.weight.clear();
	if (prop_weight.is_array()) {
		for (const auto& _weight : prop_weight) {
			specification.weight.push_back(static_cast<Xapian::termpos>(_weight.u64()));
		}
	} else {
		specification.weight.push_back(static_cast<Xapian::termpos>(prop_weight.u64()));
	}
}


void
Schema::update_spelling(const MsgPack& prop_spelling)
{
	L_CALL(this, "Schema::update_spelling(%s)", repr(prop_spelling.to_string()).c_str());

	specification.spelling.clear();
	if (prop_spelling.is_array()) {
		for (const auto& _spelling : prop_spelling) {
			specification.spelling.push_back(_spelling.boolean());
		}
	} else {
		specification.spelling.push_back(prop_spelling.boolean());
	}
}


void
Schema::update_positions(const MsgPack& prop_positions)
{
	L_CALL(this, "Schema::update_positions(%s)", repr(prop_positions.to_string()).c_str());

	specification.positions.clear();
	if (prop_positions.is_array()) {
		for (const auto& _positions : prop_positions) {
			specification.positions.push_back(_positions.boolean());
		}
	} else {
		specification.positions.push_back(prop_positions.boolean());
	}
}


void
Schema::update_language(const MsgPack& prop_language)
{
	L_CALL(this, "Schema::update_language(%s)", repr(prop_language.to_string()).c_str());

	specification.language = prop_language.str();
}


void
Schema::update_stop_strategy(const MsgPack& prop_stop_strategy)
{
	L_CALL(this, "Schema::update_stop_strategy(%s)", repr(prop_stop_strategy.to_string()).c_str());

	specification.stop_strategy = static_cast<StopStrategy>(prop_stop_strategy.u64());
}


void
Schema::update_stem_strategy(const MsgPack& prop_stem_strategy)
{
	L_CALL(this, "Schema::update_stem_strategy(%s)", repr(prop_stem_strategy.to_string()).c_str());

	specification.stem_strategy = static_cast<StemStrategy>(prop_stem_strategy.u64());
}


void
Schema::update_stem_language(const MsgPack& prop_stem_language)
{
	L_CALL(this, "Schema::update_stem_language(%s)", repr(prop_stem_language.to_string()).c_str());

	specification.stem_language = prop_stem_language.str();
}


void
Schema::update_type(const MsgPack& prop_type)
{
	L_CALL(this, "Schema::update_type(%s)", repr(prop_type.to_string()).c_str());

	try {
		specification.sep_types[SPC_FOREIGN_TYPE]  = (FieldType)prop_type.at(SPC_FOREIGN_TYPE).u64();
		specification.sep_types[SPC_OBJECT_TYPE]   = (FieldType)prop_type.at(SPC_OBJECT_TYPE).u64();
		specification.sep_types[SPC_ARRAY_TYPE]    = (FieldType)prop_type.at(SPC_ARRAY_TYPE).u64();
		specification.sep_types[SPC_CONCRETE_TYPE] = (FieldType)prop_type.at(SPC_CONCRETE_TYPE).u64();
		specification.flags.concrete = specification.sep_types[SPC_CONCRETE_TYPE] != FieldType::EMPTY;
	} catch (const msgpack::type_error&) {
	} catch (const std::out_of_range&) {
		THROW(Error, "Schema is corrupt: '%s' in %s is not valid.", RESERVED_TYPE, repr(specification.full_meta_name).c_str());
	}
}


void
Schema::update_accuracy(const MsgPack& prop_accuracy)
{
	L_CALL(this, "Schema::update_accuracy(%s)", repr(prop_accuracy.to_string()).c_str());

	specification.accuracy.reserve(prop_accuracy.size());
	for (const auto& acc : prop_accuracy) {
		specification.accuracy.push_back(acc.f64());
	}
}


void
Schema::update_acc_prefix(const MsgPack& prop_acc_prefix)
{
	L_CALL(this, "Schema::update_acc_prefix(%s)", repr(prop_acc_prefix.to_string()).c_str());

	specification.acc_prefix.reserve(prop_acc_prefix.size());
	for (const auto& acc_p : prop_acc_prefix) {
		specification.acc_prefix.push_back(acc_p.str());
	}
}


void
Schema::update_prefix(const MsgPack& prop_prefix)
{
	L_CALL(this, "Schema::update_prefix(%s)", repr(prop_prefix.to_string()).c_str());

	specification.local_prefix.field = prop_prefix.str();
}


void
Schema::update_slot(const MsgPack& prop_slot)
{
	L_CALL(this, "Schema::update_slot(%s)", repr(prop_slot.to_string()).c_str());

	specification.slot = static_cast<Xapian::valueno>(prop_slot.u64());
}


void
Schema::update_index(const MsgPack& prop_index)
{
	L_CALL(this, "Schema::update_index(%s)", repr(prop_index.to_string()).c_str());

	specification.index = static_cast<TypeIndex>(prop_index.u64());
	specification.flags.has_index = true;
}


void
Schema::update_store(const MsgPack& prop_store)
{
	L_CALL(this, "Schema::update_store(%s)", repr(prop_store.to_string()).c_str());

	specification.flags.parent_store = specification.flags.store;
	specification.flags.store = prop_store.boolean() && specification.flags.parent_store;
}


void
Schema::update_recurse(const MsgPack& prop_recurse)
{
	L_CALL(this, "Schema::update_recurse(%s)", repr(prop_recurse.to_string()).c_str());

	specification.flags.is_recurse = prop_recurse.boolean();
}


void
Schema::update_dynamic(const MsgPack& prop_dynamic)
{
	L_CALL(this, "Schema::update_dynamic(%s)", repr(prop_dynamic.to_string()).c_str());

	specification.flags.dynamic = prop_dynamic.boolean();
}


void
Schema::update_strict(const MsgPack& prop_strict)
{
	L_CALL(this, "Schema::update_strict(%s)", repr(prop_strict.to_string()).c_str());

	specification.flags.strict = prop_strict.boolean();
}


void
Schema::update_date_detection(const MsgPack& prop_date_detection)
{
	L_CALL(this, "Schema::update_date_detection(%s)", repr(prop_date_detection.to_string()).c_str());

	specification.flags.date_detection = prop_date_detection.boolean();
}


void
Schema::update_time_detection(const MsgPack& prop_time_detection)
{
	L_CALL(this, "Schema::update_time_detection(%s)", repr(prop_time_detection.to_string()).c_str());

	specification.flags.time_detection = prop_time_detection.boolean();
}


void
Schema::update_timedelta_detection(const MsgPack& prop_timedelta_detection)
{
	L_CALL(this, "Schema::update_timedelta_detection(%s)", repr(prop_timedelta_detection.to_string()).c_str());

	specification.flags.timedelta_detection = prop_timedelta_detection.boolean();
}


void
Schema::update_numeric_detection(const MsgPack& prop_numeric_detection)
{
	L_CALL(this, "Schema::update_numeric_detection(%s)", repr(prop_numeric_detection.to_string()).c_str());

	specification.flags.numeric_detection = prop_numeric_detection.boolean();
}


void
Schema::update_geo_detection(const MsgPack& prop_geo_detection)
{
	L_CALL(this, "Schema::update_geo_detection(%s)", repr(prop_geo_detection.to_string()).c_str());

	specification.flags.geo_detection = prop_geo_detection.boolean();
}


void
Schema::update_bool_detection(const MsgPack& prop_bool_detection)
{
	L_CALL(this, "Schema::update_bool_detection(%s)", repr(prop_bool_detection.to_string()).c_str());

	specification.flags.bool_detection = prop_bool_detection.boolean();
}


void
Schema::update_string_detection(const MsgPack& prop_string_detection)
{
	L_CALL(this, "Schema::update_string_detection(%s)", repr(prop_string_detection.to_string()).c_str());

	specification.flags.string_detection = prop_string_detection.boolean();
}


void
Schema::update_text_detection(const MsgPack& prop_text_detection)
{
	L_CALL(this, "Schema::update_text_detection(%s)", repr(prop_text_detection.to_string()).c_str());

	specification.flags.text_detection = prop_text_detection.boolean();
}


void
Schema::update_term_detection(const MsgPack& prop_tm_detection)
{
	L_CALL(this, "Schema::update_term_detection(%s)", repr(prop_tm_detection.to_string()).c_str());

	specification.flags.term_detection = prop_tm_detection.boolean();
}


void
Schema::update_uuid_detection(const MsgPack& prop_uuid_detection)
{
	L_CALL(this, "Schema::update_uuid_detection(%s)", repr(prop_uuid_detection.to_string()).c_str());

	specification.flags.uuid_detection = prop_uuid_detection.boolean();
}


void
Schema::update_bool_term(const MsgPack& prop_bool_term)
{
	L_CALL(this, "Schema::update_bool_term(%s)", repr(prop_bool_term.to_string()).c_str());

	specification.flags.bool_term = prop_bool_term.boolean();
}


void
Schema::update_partials(const MsgPack& prop_partials)
{
	L_CALL(this, "Schema::update_partials(%s)", repr(prop_partials.to_string()).c_str());

	specification.flags.partials = prop_partials.boolean();
}


void
Schema::update_error(const MsgPack& prop_error)
{
	L_CALL(this, "Schema::update_error(%s)", repr(prop_error.to_string()).c_str());

	specification.error = prop_error.f64();
}


void
Schema::update_namespace(const MsgPack& prop_namespace)
{
	L_CALL(this, "Schema::update_namespace(%s)", repr(prop_namespace.to_string()).c_str());

	specification.flags.is_namespace = prop_namespace.boolean();
	specification.flags.has_namespace = true;
}


void
Schema::update_partial_paths(const MsgPack& prop_partial_paths)
{
	L_CALL(this, "Schema::update_partial_paths(%s)", repr(prop_partial_paths.to_string()).c_str());

	specification.flags.partial_paths = prop_partial_paths.boolean();
	specification.flags.has_partial_paths = true;
}


void
Schema::update_index_uuid_field(const MsgPack& prop_index_uuid_field)
{
	L_CALL(this, "Schema::update_index_uuid_field(%s)", repr(prop_index_uuid_field.to_string()).c_str());

	specification.index_uuid_field = static_cast<UUIDFieldIndex>(prop_index_uuid_field.u64());
}


void
Schema::update_script(const MsgPack& prop_script)
{
	L_CALL(this, "Schema::update_script(%s)", repr(prop_script.to_string()).c_str());

#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	specification.script = std::make_unique<const MsgPack>(prop_script);
	specification.flags.normalized_script = true;
#else
	ignore_unused(prop_script);
	THROW(ClientError, "%s only is allowed when ChaiScript or ECMAScript/JavaScript is actived", RESERVED_SCRIPT);
#endif
}


void
Schema::write_position(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_position)
{
	// RESERVED_POSITION is heritable and can change between documents.
	L_CALL(this, "Schema::write_position(%s)", repr(doc_position.to_string()).c_str());

	process_position(prop_name, doc_position);
	properties[prop_name] = specification.position;
}


void
Schema::write_weight(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_weight)
{
	// RESERVED_WEIGHT property is heritable and can change between documents.
	L_CALL(this, "Schema::write_weight(%s)", repr(doc_weight.to_string()).c_str());

	process_weight(prop_name, doc_weight);
	properties[prop_name] = specification.weight;
}


void
Schema::write_spelling(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_spelling)
{
	// RESERVED_SPELLING is heritable and can change between documents.
	L_CALL(this, "Schema::write_spelling(%s)", repr(doc_spelling.to_string()).c_str());

	process_spelling(prop_name, doc_spelling);
	properties[prop_name] = specification.spelling;
}


void
Schema::write_positions(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_positions)
{
	// RESERVED_POSITIONS is heritable and can change between documents.
	L_CALL(this, "Schema::write_positions(%s)", repr(doc_positions.to_string()).c_str());

	process_positions(prop_name, doc_positions);
	properties[prop_name] = specification.positions;
}


void
Schema::write_index(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_index)
{
	// RESERVED_INDEX is heritable and can change.
	L_CALL(this, "Schema::write_index(%s)", repr(doc_index.to_string()).c_str());

	process_index(prop_name, doc_index);
	properties[prop_name] = specification.index;
}


void
Schema::write_store(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_store)
{
	L_CALL(this, "Schema::write_store(%s)", repr(doc_store.to_string()).c_str());

	/*
	 * RESERVED_STORE is heritable and can change, but once fixed in false
	 * it cannot change in its offsprings.
	 */

	process_store(prop_name, doc_store);
	properties[prop_name] = doc_store.boolean();
}


void
Schema::write_recurse(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_recurse)
{
	L_CALL(this, "Schema::write_recurse(%s)", repr(doc_recurse.to_string()).c_str());

	/*
	 * RESERVED_RECURSE is heritable and can change, but once fixed in false
	 * it does not process its children.
	 */

	process_recurse(prop_name, doc_recurse);
	properties[prop_name] = static_cast<bool>(specification.flags.is_recurse);
}


void
Schema::write_dynamic(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_dynamic)
{
	// RESERVED_DYNAMIC is heritable but can't change.
	L_CALL(this, "Schema::write_dynamic(%s)", repr(doc_dynamic.to_string()).c_str());

	try {
		specification.flags.dynamic = doc_dynamic.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.dynamic);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_strict(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_strict)
{
	// RESERVED_STRICT is heritable but can't change.
	L_CALL(this, "Schema::write_strict(%s)", repr(doc_strict.to_string()).c_str());

	try {
		specification.flags.strict = doc_strict.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.strict);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_date_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_date_detection)
{
	// RESERVED_D_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_date_detection(%s)", repr(doc_date_detection.to_string()).c_str());

	try {
		specification.flags.date_detection = doc_date_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.date_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_time_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_time_detection)
{
	// RESERVED_TI_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_time_detection(%s)", repr(doc_time_detection.to_string()).c_str());

	try {
		specification.flags.time_detection = doc_time_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.time_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_timedelta_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_timedelta_detection)
{
	// RESERVED_TD_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_timedelta_detection(%s)", repr(doc_timedelta_detection.to_string()).c_str());

	try {
		specification.flags.timedelta_detection = doc_timedelta_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.timedelta_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_numeric_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_numeric_detection)
{
	// RESERVED_N_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_numeric_detection(%s)", repr(doc_numeric_detection.to_string()).c_str());

	try {
		specification.flags.numeric_detection = doc_numeric_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.numeric_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_geo_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_geo_detection)
{
	// RESERVED_G_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_geo_detection(%s)", repr(doc_geo_detection.to_string()).c_str());

	try {
		specification.flags.geo_detection = doc_geo_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.geo_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_bool_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_bool_detection)
{
	// RESERVED_B_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_bool_detection(%s)", repr(doc_bool_detection.to_string()).c_str());

	try {
		specification.flags.bool_detection = doc_bool_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.bool_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_string_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_string_detection)
{
	// RESERVED_S_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_string_detection(%s)", repr(doc_string_detection.to_string()).c_str());

	try {
		specification.flags.string_detection = doc_string_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.string_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_text_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_text_detection)
{
	// RESERVED_T_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_text_detection(%s)", repr(doc_text_detection.to_string()).c_str());

	try {
		specification.flags.text_detection = doc_text_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.text_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_term_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_tm_detection)
{
	// RESERVED_TE_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_term_detection(%s)", repr(doc_tm_detection.to_string()).c_str());

	try {
		specification.flags.term_detection = doc_tm_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.term_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_uuid_detection(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_uuid_detection)
{
	// RESERVED_U_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::write_uuid_detection(%s)", repr(doc_uuid_detection.to_string()).c_str());

	try {
		specification.flags.uuid_detection = doc_uuid_detection.boolean();
		properties[prop_name] = static_cast<bool>(specification.flags.uuid_detection);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_namespace(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_namespace)
{
	// RESERVED_NAMESPACE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::write_namespace(%s)", repr(doc_namespace.to_string()).c_str());

	try {
		if (specification.flags.field_found) {
			return consistency_namespace(prop_name, doc_namespace);
		}

		// Only save in Schema if RESERVED_NAMESPACE is true.
		specification.flags.is_namespace = doc_namespace.boolean();
		if (specification.flags.is_namespace && !specification.flags.has_partial_paths) {
			specification.flags.partial_paths = specification.flags.partial_paths || !default_spc.flags.optimal;
		}
		specification.flags.has_namespace = true;
		properties[prop_name] = static_cast<bool>(specification.flags.is_namespace);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::write_partial_paths(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_partial_paths)
{
	L_CALL(this, "Schema::write_partial_paths(%s)", repr(doc_partial_paths.to_string()).c_str());

	/*
	 * RESERVED_PARTIAL_PATHS is heritable and can change.
	 */

	process_partial_paths(prop_name, doc_partial_paths);
	properties[prop_name] = static_cast<bool>(specification.flags.partial_paths);
}


void
Schema::write_index_uuid_field(MsgPack& properties, const std::string& prop_name, const MsgPack& doc_index_uuid_field)
{
	L_CALL(this, "Schema::write_index_uuid_field(%s)", repr(doc_index_uuid_field.to_string()).c_str());

	/*
	 * RESERVED_INDEX_UUID_FIELD is heritable and can change.
	 */

	process_index_uuid_field(prop_name, doc_index_uuid_field);
	properties[prop_name] = specification.index_uuid_field;
}


void
Schema::write_version(MsgPack&, const std::string& prop_name, const MsgPack& doc_version)
{
	L_CALL(this, "Schema::write_version(%s)", repr(doc_version.to_string()).c_str());

	/*
	 * RESERVED_VERSION must be DB_VERSION_SCHEMA.
	 */

	consistency_version(prop_name, doc_version);
}


void
Schema::write_schema(MsgPack&, const std::string& prop_name, const MsgPack& doc_schema)
{
	L_CALL(this, "Schema::write_schema(%s)", repr(doc_schema.to_string()).c_str());

	consistency_schema(prop_name, doc_schema);
}


void
Schema::write_script(MsgPack&, const std::string& prop_name, const MsgPack& doc_script)
{
	L_CALL(this, "Schema::write_script(%s)", repr(doc_script.to_string()).c_str());

	process_script(prop_name, doc_script);
}


void
Schema::process_language(const std::string& prop_name, const MsgPack& doc_language)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_language(%s)", repr(doc_language.to_string()).c_str());

	try {
		const auto _str_language = lower_string(doc_language.str());
		static const auto slit_e = map_stem_language.end();
		const auto slit = map_stem_language.find(_str_language);
		if (slit != slit_e && slit->second.first) {
			specification.language = slit->second.second;
			specification.aux_lan = slit->second.second;
			return;
		}

		THROW(ClientError, "%s: %s is not supported", prop_name.c_str(), _str_language.c_str());
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::process_slot(const std::string& prop_name, const MsgPack& doc_slot)
{
	// RESERVED_SLOT isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_slot(%s)", repr(doc_slot.to_string()).c_str());

	try {
		auto slot = static_cast<Xapian::valueno>(doc_slot.u64());
		if (slot < DB_SLOT_RESERVED || slot == Xapian::BAD_VALUENO) {
			THROW(ClientError, "%s: %u is not supported", prop_name.c_str(), slot);
		}
		specification.slot = slot;
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be integer", prop_name.c_str());
	}
}


void
Schema::process_stop_strategy(const std::string& prop_name, const MsgPack& doc_stop_strategy)
{
	// RESERVED_STOP_STRATEGY isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_stop_strategy(%s)", repr(doc_stop_strategy.to_string()).c_str());

	try {
		const auto _stop_strategy = lower_string(doc_stop_strategy.str());
		static const auto ssit_e = map_stop_strategy.end();
		const auto ssit = map_stop_strategy.find(_stop_strategy);
		if (ssit == ssit_e) {
			static const std::string str_set_stop_strategy(get_map_keys(map_stop_strategy));
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
		const auto _stem_strategy = lower_string(doc_stem_strategy.str());
		static const auto ssit_e = map_stem_strategy.end();
		const auto ssit = map_stem_strategy.find(_stem_strategy);
		if (ssit == ssit_e) {
			static const std::string str_set_stem_strategy(get_map_keys(map_stem_strategy));
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
		const auto _stem_language = lower_string(doc_stem_language.str());
		static const auto slit_e = map_stem_language.end();
		const auto slit = map_stem_language.find(_stem_language);
		if (slit == slit_e) {
			THROW(ClientError, "%s: %s is not supported", prop_name.c_str(), _stem_language.c_str());
		} else {
			specification.stem_language = _stem_language;
			specification.aux_stem_lan = slit->second.second;
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::process_type(const std::string& prop_name, const MsgPack& doc_type)
{
	// RESERVED_TYPE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::process_type(%s)", repr(doc_type.to_string()).c_str());

	try {
		specification.set_types(doc_type.str());
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
		specification.doc_acc = std::make_unique<const MsgPack>(doc_accuracy);
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
		specification.flags.bool_term = doc_bool_term.boolean();
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
		specification.flags.partials = doc_partials.boolean();
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
		specification.error = doc_error.f64();
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
				specification.position.push_back(static_cast<unsigned>(_position.u64()));
			}
		} else {
			specification.position.push_back(static_cast<unsigned>(doc_position.u64()));
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
				specification.weight.push_back(static_cast<unsigned>(_weight.u64()));
			}
		} else {
			specification.weight.push_back(static_cast<unsigned>(doc_weight.u64()));
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
				specification.spelling.push_back(_spelling.boolean());
			}
		} else {
			specification.spelling.push_back(doc_spelling.boolean());
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
				specification.positions.push_back(_positions.boolean());
			}
		} else {
			specification.positions.push_back(doc_positions.boolean());
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
		const auto str_index = lower_string(doc_index.str());
		static const auto miit_e = map_index.end();
		const auto miit = map_index.find(str_index);
		if (miit == miit_e) {
			static const std::string str_set_index(get_map_keys(map_index));
			THROW(ClientError, "%s not supported, %s must be one of %s", repr(str_index).c_str(), repr(prop_name).c_str(), str_set_index.c_str());
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
		specification.flags.store = specification.flags.parent_store && doc_store.boolean();
		specification.flags.parent_store = specification.flags.store;
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_recurse(const std::string& prop_name, const MsgPack& doc_recurse)
{
	L_CALL(this, "Schema::process_recurse(%s)", repr(doc_recurse.to_string()).c_str());

	/*
	 * RESERVED_RECURSE is heritable and can change, but once fixed in false
	 * it does not process its children.
	 */

	try {
		specification.flags.is_recurse = doc_recurse.boolean();
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
		specification.flags.partial_paths = doc_partial_paths.boolean();
		specification.flags.has_partial_paths = true;
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::process_index_uuid_field(const std::string& prop_name, const MsgPack& doc_index_uuid_field)
{
	L_CALL(this, "Schema::process_index_uuid_field(%s)", repr(doc_index_uuid_field.to_string()).c_str());

	/*
	 * RESERVED_INDEX_UUID_FIELD is heritable and can change.
	 */

	try {
		const auto str_index_uuid_field = lower_string(doc_index_uuid_field.str());
		static const auto mdit_e = map_index_uuid_field.end();
		const auto mdit = map_index_uuid_field.find(str_index_uuid_field);
		if (mdit == mdit_e) {
			static const std::string str_set_index_uuid_field(get_map_keys(map_index_uuid_field));
			THROW(ClientError, "%s not supported, %s must be one of %s (%s not supported)", repr(str_index_uuid_field).c_str(), repr(prop_name).c_str(), str_set_index_uuid_field.c_str());
		} else {
			specification.index_uuid_field = mdit->second;
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
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
	// RESERVED_SCRIPT isn't heritable.
	L_CALL(this, "Schema::process_script(%s)", repr(doc_script.to_string()).c_str());

#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
	specification.script = std::make_unique<const MsgPack>(doc_script);
	specification.flags.normalized_script = false;
#else
	ignore_unused(doc_script);
	THROW(ClientError, "%s only is allowed when ChaiScript or ECMAScript/JavaScript is actived", RESERVED_SCRIPT);
#endif
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
Schema::consistency_language(const std::string& prop_name, const MsgPack& doc_language)
{
	// RESERVED_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::consistency_language(%s)", repr(doc_language.to_string()).c_str());

	try {
		const auto _str_language = lower_string(doc_language.str());
		if (specification.language != _str_language) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s] in %s", prop_name.c_str(), specification.language.c_str(), _str_language.c_str(), specification.full_meta_name.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::consistency_stop_strategy(const std::string& prop_name, const MsgPack& doc_stop_strategy)
{
	// RESERVED_STOP_STRATEGY isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::consistency_stop_strategy(%s)", repr(doc_stop_strategy.to_string()).c_str());

	try {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::TEXT) {
			const auto _stop_strategy = lower_string(doc_stop_strategy.str());
			const auto stop_strategy = ::readable_stop_strategy(specification.stop_strategy);
			if (stop_strategy != _stop_strategy) {
				THROW(ClientError, "It is not allowed to change %s [%s  ->  %s] in %s", prop_name.c_str(), stop_strategy.c_str(), _stop_strategy.c_str(), specification.full_meta_name.c_str());
			}
		} else {
			THROW(ClientError, "%s only is allowed in text type fields", prop_name.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::consistency_stem_strategy(const std::string& prop_name, const MsgPack& doc_stem_strategy)
{
	// RESERVED_STEM_STRATEGY isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::consistency_stem_strategy(%s)", repr(doc_stem_strategy.to_string()).c_str());

	try {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::TEXT) {
			const auto _stem_strategy = lower_string(doc_stem_strategy.str());
			const auto stem_strategy = ::readable_stem_strategy(specification.stem_strategy);
			if (stem_strategy != _stem_strategy) {
				THROW(ClientError, "It is not allowed to change %s [%s  ->  %s] in %s", prop_name.c_str(), stem_strategy.c_str(), _stem_strategy.c_str(), specification.full_meta_name.c_str());
			}
		} else {
			THROW(ClientError, "%s only is allowed in text type fields", prop_name.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::consistency_stem_language(const std::string& prop_name, const MsgPack& doc_stem_language)
{
	// RESERVED_STEM_LANGUAGE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::consistency_stem_language(%s)", repr(doc_stem_language.to_string()).c_str());

	try {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::TEXT) {
			const auto _stem_language = lower_string(doc_stem_language.str());
			if (specification.stem_language != _stem_language) {
				THROW(ClientError, "It is not allowed to change %s [%s  ->  %s] in %s", prop_name.c_str(), specification.stem_language.c_str(), _stem_language.c_str(), specification.full_meta_name.c_str());
			}
		} else {
			THROW(ClientError, "%s only is allowed in text type fields", prop_name.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", repr(prop_name).c_str());
	}
}


void
Schema::consistency_type(const std::string& prop_name, const MsgPack& doc_type)
{
	// RESERVED_TYPE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::consistency_type(%s)", repr(doc_type.to_string()).c_str());

	try {
		const auto _str_type = lower_string(doc_type.str());
		auto init_pos = _str_type.rfind('/');
		if (init_pos == std::string::npos) {
			init_pos = 0;
		} else {
			++init_pos;
		}
		const auto str_type = Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]);
		if (_str_type.compare(init_pos, std::string::npos, str_type) != 0) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s] in %s", prop_name.c_str(), str_type.c_str(), _str_type.substr(init_pos).c_str(), specification.full_meta_name.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be string", prop_name.c_str());
	}
}


void
Schema::consistency_accuracy(const std::string& prop_name, const MsgPack& doc_accuracy)
{
	// RESERVED_ACCURACY isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::consistency_accuracy(%s)", repr(doc_accuracy.to_string()).c_str());

	if (doc_accuracy.is_array()) {
		std::set<uint64_t> set_acc;
		switch (specification.sep_types[SPC_CONCRETE_TYPE]) {
			case FieldType::GEO: {
				try {
					for (const auto& _accuracy : doc_accuracy) {
						set_acc.insert(HTM_START_POS - 2 * _accuracy.u64());
					}
				} catch (const msgpack::type_error&) {
					THROW(ClientError, "Data inconsistency, level value in '%s': '%s' must be a positive number between 0 and %d", RESERVED_ACCURACY, GEO_STR, HTM_MAX_LEVEL);
				}
				if (!std::equal(specification.accuracy.begin(), specification.accuracy.end(), set_acc.begin(), set_acc.end())) {
					std::string str_accuracy, _str_accuracy;
					for (const auto& acc : set_acc) {
						str_accuracy.append(std::to_string((HTM_START_POS - acc) / 2)).push_back(' ');
					}
					for (const auto& acc : specification.accuracy) {
						_str_accuracy.append(std::to_string((HTM_START_POS - acc) / 2)).push_back(' ');
					}
					THROW(ClientError, "It is not allowed to change %s [{ %s}  ->  { %s}] in %s", prop_name.c_str(), str_accuracy.c_str(), _str_accuracy.c_str(), specification.full_meta_name.c_str());
				}
				return;
			}
			case FieldType::DATE: {
				try {
					static const auto adit_e = map_acc_date.end();
					for (const auto& _accuracy : doc_accuracy) {
						const auto str_accuracy = lower_string(_accuracy.str());
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
				if (!std::equal(specification.accuracy.begin(), specification.accuracy.end(), set_acc.begin(), set_acc.end())) {
					std::string str_accuracy, _str_accuracy;
					for (const auto& acc : set_acc) {
						str_accuracy.append(std::to_string(readable_acc_date((UnitTime)acc))).push_back(' ');
					}
					for (const auto& acc : specification.accuracy) {
						_str_accuracy.append(std::to_string(readable_acc_date((UnitTime)acc))).push_back(' ');
					}
					THROW(ClientError, "It is not allowed to change %s [{ %s}  ->  { %s}] in %s", prop_name.c_str(), str_accuracy.c_str(), _str_accuracy.c_str(), specification.full_meta_name.c_str());
				}
				return;
			}
			case FieldType::TIME:
			case FieldType::TIMEDELTA: {
				try {
					static const auto adit_e = map_acc_time.end();
					for (const auto& _accuracy : doc_accuracy) {
						const auto str_accuracy = lower_string(_accuracy.str());
						const auto adit = map_acc_time.find(str_accuracy);
						if (adit == adit_e) {
							THROW(ClientError, "Data inconsistency, '%s': '%s' must be a subset of %s (%s not supported)", RESERVED_ACCURACY, Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]).c_str(), repr(str_set_acc_time).c_str(), repr(str_accuracy).c_str());
						} else {
							set_acc.insert(toUType(adit->second));
						}
					}
				} catch (const msgpack::type_error&) {
					THROW(ClientError, "Data inconsistency, '%s' in '%s' must be a subset of %s", RESERVED_ACCURACY, Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]).c_str(), repr(str_set_acc_time).c_str());
				}
				if (!std::equal(specification.accuracy.begin(), specification.accuracy.end(), set_acc.begin(), set_acc.end())) {
					std::string str_accuracy, _str_accuracy;
					for (const auto& acc : set_acc) {
						str_accuracy.append(std::to_string(readable_acc_date((UnitTime)acc))).push_back(' ');
					}
					for (const auto& acc : specification.accuracy) {
						_str_accuracy.append(std::to_string(readable_acc_date((UnitTime)acc))).push_back(' ');
					}
					THROW(ClientError, "It is not allowed to change %s [{ %s}  ->  { %s}] in %s", prop_name.c_str(), str_accuracy.c_str(), _str_accuracy.c_str(), specification.full_meta_name.c_str());
				}
				return;
			}
			case FieldType::INTEGER:
			case FieldType::POSITIVE:
			case FieldType::FLOAT: {
				try {
					for (const auto& _accuracy : doc_accuracy) {
						set_acc.insert(_accuracy.u64());
					}
				} catch (const msgpack::type_error&) {
					THROW(ClientError, "Data inconsistency, %s in %s must be an array of positive numbers in %s", RESERVED_ACCURACY, Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]).c_str(), specification.full_meta_name.c_str());
				}
				if (!std::equal(specification.accuracy.begin(), specification.accuracy.end(), set_acc.begin(), set_acc.end())) {
					std::string str_accuracy, _str_accuracy;
					for (const auto& acc : set_acc) {
						str_accuracy.append(std::to_string(acc)).push_back(' ');
					}
					for (const auto& acc : specification.accuracy) {
						_str_accuracy.append(std::to_string(acc)).push_back(' ');
					}
					THROW(ClientError, "It is not allowed to change %s [{ %s}  ->  { %s}] in %s", prop_name.c_str(), str_accuracy.c_str(), _str_accuracy.c_str(), specification.full_meta_name.c_str());
				}
				return;
			}
			default:
				THROW(ClientError, "%s is not allowed in %s type fields", prop_name.c_str(), Serialise::type(specification.sep_types[SPC_CONCRETE_TYPE]).c_str());
		}
	} else {
		THROW(ClientError, "Data inconsistency, %s must be array", prop_name.c_str());
	}
}


void
Schema::consistency_bool_term(const std::string& prop_name, const MsgPack& doc_bool_term)
{
	// RESERVED_BOOL_TERM isn't heritable and can't change.
	L_CALL(this, "Schema::consistency_bool_term(%s)", repr(doc_bool_term.to_string()).c_str());

	try {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::TERM) {
			const auto _bool_term = doc_bool_term.boolean();
			if (specification.flags.bool_term != _bool_term) {
				THROW(ClientError, "It is not allowed to change %s [%s  ->  %s] in %s", prop_name.c_str(), specification.flags.bool_term ? "true" : "false", _bool_term ? "true" : "false", specification.full_meta_name.c_str());
			}
		} else {
			THROW(ClientError, "%s only is allowed in term type fields", prop_name.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a boolean", prop_name.c_str());
	}
}


void
Schema::consistency_partials(const std::string& prop_name, const MsgPack& doc_partials)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::consistency_partials(%s)", repr(doc_partials.to_string()).c_str());

	try {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::GEO) {
			const auto _partials = doc_partials.boolean();
			if (specification.flags.partials != _partials) {
				THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.partials ? "true" : "false", _partials ? "true" : "false");
			}
		} else {
			THROW(ClientError, "%s only is allowed in geospatial type fields", prop_name.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_error(const std::string& prop_name, const MsgPack& doc_error)
{
	// RESERVED_PARTIALS isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::consistency_error(%s)", repr(doc_error.to_string()).c_str());

	try {
		if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::GEO) {
			const auto _error = doc_error.f64();
			if (specification.error != _error) {
				THROW(ClientError, "It is not allowed to change %s [%.2f  ->  %.2f]", prop_name.c_str(), specification.error, _error);
			}
		} else {
			THROW(ClientError, "%s only is allowed in geospatial type fields", prop_name.c_str());
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be a double", prop_name.c_str());
	}
}


void
Schema::consistency_dynamic(const std::string& prop_name, const MsgPack& doc_dynamic)
{
	// RESERVED_DYNAMIC is heritable but can't change.
	L_CALL(this, "Schema::consistency_dynamic(%s)", repr(doc_dynamic.to_string()).c_str());

	try {
		const auto _dynamic = doc_dynamic.boolean();
		if (specification.flags.dynamic != _dynamic) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.dynamic ? "true" : "false", _dynamic ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_strict(const std::string& prop_name, const MsgPack& doc_strict)
{
	// RESERVED_STRICT is heritable but can't change.
	L_CALL(this, "Schema::consistency_strict(%s)", repr(doc_strict.to_string()).c_str());

	try {
		const auto _strict = doc_strict.boolean();
		if (specification.flags.strict != _strict) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.strict ? "true" : "false", _strict ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_date_detection(const std::string& prop_name, const MsgPack& doc_date_detection)
{
	// RESERVED_D_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_date_detection(%s)", repr(doc_date_detection.to_string()).c_str());

	try {
		const auto _date_detection = doc_date_detection.boolean();
		if (specification.flags.date_detection != _date_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.date_detection ? "true" : "false", _date_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_time_detection(const std::string& prop_name, const MsgPack& doc_time_detection)
{
	// RESERVED_TI_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_time_detection(%s)", repr(doc_time_detection.to_string()).c_str());

	try {
		const auto _time_detection = doc_time_detection.boolean();
		if (specification.flags.time_detection != _time_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.time_detection ? "true" : "false", _time_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_timedelta_detection(const std::string& prop_name, const MsgPack& doc_timedelta_detection)
{
	// RESERVED_TD_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_timedelta_detection(%s)", repr(doc_timedelta_detection.to_string()).c_str());

	try {
		const auto _timedelta_detection = doc_timedelta_detection.boolean();
		if (specification.flags.timedelta_detection != _timedelta_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.timedelta_detection ? "true" : "false", _timedelta_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_numeric_detection(const std::string& prop_name, const MsgPack& doc_numeric_detection)
{
	// RESERVED_N_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_numeric_detection(%s)", repr(doc_numeric_detection.to_string()).c_str());

	try {
		const auto _numeric_detection = doc_numeric_detection.boolean();
		if (specification.flags.numeric_detection != _numeric_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.numeric_detection ? "true" : "false", _numeric_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_geo_detection(const std::string& prop_name, const MsgPack& doc_geo_detection)
{
	// RESERVED_G_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_geo_detection(%s)", repr(doc_geo_detection.to_string()).c_str());

	try {
		const auto _geo_detection = doc_geo_detection.boolean();
		if (specification.flags.geo_detection != _geo_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.geo_detection ? "true" : "false", _geo_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_bool_detection(const std::string& prop_name, const MsgPack& doc_bool_detection)
{
	// RESERVED_B_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_bool_detection(%s)", repr(doc_bool_detection.to_string()).c_str());

	try {
		const auto _bool_detection = doc_bool_detection.boolean();
		if (specification.flags.bool_detection != _bool_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.bool_detection ? "true" : "false", _bool_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_string_detection(const std::string& prop_name, const MsgPack& doc_string_detection)
{
	// RESERVED_S_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_string_detection(%s)", repr(doc_string_detection.to_string()).c_str());

	try {
		const auto _string_detection = doc_string_detection.boolean();
		if (specification.flags.string_detection != _string_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.string_detection ? "true" : "false", _string_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_text_detection(const std::string& prop_name, const MsgPack& doc_text_detection)
{
	// RESERVED_T_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_text_detection(%s)", repr(doc_text_detection.to_string()).c_str());

	try {
		const auto _text_detection = doc_text_detection.boolean();
		if (specification.flags.text_detection != _text_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.text_detection ? "true" : "false", _text_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_term_detection(const std::string& prop_name, const MsgPack& doc_tm_detection)
{
	// RESERVED_TE_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_term_detection(%s)", repr(doc_tm_detection.to_string()).c_str());

	try {
		const auto _term_detection = doc_tm_detection.boolean();
		if (specification.flags.term_detection != _term_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.term_detection ? "true" : "false", _term_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_uuid_detection(const std::string& prop_name, const MsgPack& doc_uuid_detection)
{
	// RESERVED_U_DETECTION is heritable and can't change.
	L_CALL(this, "Schema::consistency_uuid_detection(%s)", repr(doc_uuid_detection.to_string()).c_str());

	try {
		const auto _uuid_detection = doc_uuid_detection.boolean();
		if (specification.flags.uuid_detection != _uuid_detection) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.uuid_detection ? "true" : "false", _uuid_detection ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_namespace(const std::string& prop_name, const MsgPack& doc_namespace)
{
	// RESERVED_NAMESPACE isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::consistency_namespace(%s)", repr(doc_namespace.to_string()).c_str());

	try {
		const auto _is_namespace = doc_namespace.boolean();
		if (specification.flags.is_namespace != _is_namespace) {
			THROW(ClientError, "It is not allowed to change %s [%s  ->  %s]", prop_name.c_str(), specification.flags.is_namespace ? "true" : "false", _is_namespace ? "true" : "false");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "Data inconsistency, %s must be boolean", prop_name.c_str());
	}
}


void
Schema::consistency_version(const std::string& prop_name, const MsgPack& doc_version)
{
	// RESERVED_VERSION isn't heritable and is only allowed in root object.
	L_CALL(this, "Schema::consistency_version(%s)", repr(doc_version.to_string()).c_str());

	if (specification.full_meta_name.empty()) {
		try {
			const auto _version = doc_version.f64();
			if (_version != DB_VERSION_SCHEMA) {
				THROW(ClientError, "It is not allowed to change %s [%.2f  ->  %.2f]", prop_name.c_str(), DB_VERSION_SCHEMA, _version);
			}
		} catch (const msgpack::type_error&) {
			THROW(ClientError, "%s must be a double", prop_name.c_str());
		}
	} else {
		THROW(ClientError, "%s is only allowed in root object", prop_name.c_str());
	}
}


void
Schema::consistency_schema(const std::string& prop_name, const MsgPack& doc_schema)
{
	// RESERVED_SCHEMA isn't heritable and is only allowed in root object.
	L_CALL(this, "Schema::consistency_schema(%s)", repr(doc_schema.to_string()).c_str());

	if (specification.full_meta_name.empty()) {
		if (!doc_schema.is_string() && !doc_schema.is_map()) {
			THROW(ClientError, "%s must be string or map", prop_name.c_str());
		}
	} else {
		THROW(ClientError, "%s is only allowed in root object", prop_name.c_str());
	}
}


#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
void
Schema::write_script(MsgPack& properties)
{
	// RESERVED_SCRIPT isn't heritable and can't change once fixed.
	L_CALL(this, "Schema::write_script()");

	if (specification.script) {
		Script script(*specification.script);
		specification.script = std::make_unique<const MsgPack>(script.process_script(specification.flags.strict));
		properties[RESERVED_SCRIPT] = *specification.script;
		specification.flags.normalized_script = true;
	}
}


void
Schema::normalize_script()
{
	// RESERVED_SCRIPT isn't heritable.
	L_CALL(this, "Schema::normalize_script()");

	if (specification.script && !specification.flags.normalized_script) {
		Script script(*specification.script);
		specification.script = std::make_unique<const MsgPack>(script.process_script(specification.flags.strict));
		specification.flags.normalized_script = true;
	}
}
#endif


void
Schema::set_namespace_spc_id(required_spc_t& spc)
{
	L_CALL(nullptr, "Schema::set_namespace_spc_id(<spc>)");

	// ID_FIELD_NAME cannot be text or string.
	if (spc.sep_types[SPC_CONCRETE_TYPE] == FieldType::TEXT || spc.sep_types[SPC_CONCRETE_TYPE] == FieldType::STRING) {
		spc.sep_types[SPC_CONCRETE_TYPE] = FieldType::TERM;
	}
	spc.prefix.field = NAMESPACE_PREFIX_ID_FIELD_NAME;
	spc.slot = get_slot(spc.prefix.field, spc.get_ctype());
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

	// ID_FIELD_NAME cannot be TEXT nor STRING.
	if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::TEXT || specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::STRING) {
		specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::TERM;
		L_DEBUG(this, "%s cannot be type text or string, it's type was changed to term", ID_FIELD_NAME);
	}

	// Set default prefix
	specification.local_prefix.field = DOCUMENT_ID_TERM_PREFIX;

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
	if (specification.sep_types[SPC_CONCRETE_TYPE] == FieldType::EMPTY) {
		specification.sep_types[SPC_CONCRETE_TYPE] = FieldType::TERM;
	}

	// Set default prefix
	specification.local_prefix.field = DOCUMENT_CONTENT_TYPE_TERM_PREFIX;

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
	auto& schema_prop = schema_readable.at(DB_SCHEMA);
	readable_type(schema_prop.at(RESERVED_TYPE), schema_prop);
	auto& properties = schema_prop.at(RESERVED_VALUE);
	readable(properties, true);
	return properties;
}


void
Schema::readable(MsgPack& item_schema, bool is_root)
{
	L_CALL(nullptr, "Schema::readable(%s, %d)", repr(item_schema.to_string()).c_str(), is_root);

	// Change this item of schema in readable form.
	static const auto drit_e = map_get_readable.end();
	for (auto it = item_schema.begin(); it != item_schema.end(); ) {
		const auto str_key = it->str();
		const auto drit = map_get_readable.find(str_key);
		if (drit == drit_e) {
			if (is_valid(str_key) || (is_root && map_dispatch_set_default_spc.count(str_key))) {
				readable(it.value(), false);
			}
		} else {
			if (!(*drit->second)(it.value(), item_schema)) {
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

	std::array<FieldType, SPC_TOTAL_TYPES> sep_types({{
		(FieldType)prop_type.at(SPC_FOREIGN_TYPE).u64(),
		(FieldType)prop_type.at(SPC_OBJECT_TYPE).u64(),
		(FieldType)prop_type.at(SPC_ARRAY_TYPE).u64(),
		(FieldType)prop_type.at(SPC_CONCRETE_TYPE).u64()
	}});
	prop_type = required_spc_t::get_str_type(sep_types);

	// Readable accuracy.
	switch (sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::DATE:
		case FieldType::TIME:
		case FieldType::TIMEDELTA:
			for (auto& _accuracy : properties.at(RESERVED_ACCURACY)) {
				_accuracy = readable_acc_date((UnitTime)_accuracy.u64());
			}
			break;
		case FieldType::GEO:
			for (auto& _accuracy : properties.at(RESERVED_ACCURACY)) {
				_accuracy = (HTM_START_POS - _accuracy.u64()) / 2;
			}
			break;
		default:
			break;
	}

	return true;
}


bool
Schema::readable_prefix(MsgPack&, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_prefix(...)");

	return false;
}


bool
Schema::readable_stop_strategy(MsgPack& prop_stop_strategy, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_stop_strategy(%s)", repr(prop_stop_strategy.to_string()).c_str());

	prop_stop_strategy = ::readable_stop_strategy((StopStrategy)prop_stop_strategy.u64());

	return true;
}


bool
Schema::readable_stem_strategy(MsgPack& prop_stem_strategy, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_stem_strategy(%s)", repr(prop_stem_strategy.to_string()).c_str());

	prop_stem_strategy = ::readable_stem_strategy((StemStrategy)prop_stem_strategy.u64());

	return true;
}


bool
Schema::readable_stem_language(MsgPack& prop_stem_language, MsgPack& properties)
{
	L_CALL(nullptr, "Schema::readable_stem_language(%s)", repr(prop_stem_language.to_string()).c_str());

	const auto language = properties[RESERVED_LANGUAGE].str();
	const auto stem_language = prop_stem_language.str();

	return (language != stem_language);
}


bool
Schema::readable_index(MsgPack& prop_index, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_index(%s)", repr(prop_index.to_string()).c_str());

	prop_index = ::readable_index((TypeIndex)prop_index.u64());

	return true;
}


bool
Schema::readable_acc_prefix(MsgPack&, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_acc_prefix(...)");

	return false;
}


bool
Schema::readable_index_uuid_field(MsgPack& prop_index_uuid_field, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_index_uuid_field(%s)", repr(prop_index_uuid_field.to_string()).c_str());

	prop_index_uuid_field = ::readable_index_uuid_field((UUIDFieldIndex)prop_index_uuid_field.u64());

	return true;
}


bool
Schema::readable_script(MsgPack& prop_script, MsgPack&)
{
	L_CALL(nullptr, "Schema::readable_script(%s)", repr(prop_script.to_string()).c_str());

	readable(prop_script, false);
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


#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
MsgPack
Schema::index(MsgPack& object, const std::string& term_id, std::shared_ptr<std::pair<size_t, const MsgPack>>& old_document_pair, DatabaseHandler* db_handler, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index(%s, %s, <old_document_pair>, <db_handler>, <doc>)", repr(object.to_string()).c_str(), repr(term_id).c_str());

	try {
		specification = default_spc;

		FieldVector fields;
		auto properties = &get_newest_properties();

		const auto it_e = object.end();
		if (properties->size() == 1) {
			specification.flags.field_found = false;
			auto mut_properties = &get_mutable_properties(specification.full_meta_name);
			static const auto wpit_e = map_dispatch_write_properties.end();
			for (auto it = object.begin(); it != it_e; ++it) {
				auto str_key = it->str();
				const auto wpit = map_dispatch_write_properties.find(str_key);
				if (wpit == wpit_e) {
					fields.emplace_back(std::move(str_key), &it.value());
				} else {
					(this->*wpit->second)(*mut_properties, str_key, it.value());
				}
			}
			write_script(*mut_properties);
			properties = &*mut_properties;
		} else {
			update_specification(*properties);
			static const auto ddit_e = map_dispatch_document_properties.end();
			for (auto it = object.begin(); it != it_e; ++it) {
				auto str_key = it->str();
				const auto ddit = map_dispatch_document_properties.find(str_key);
				if (ddit == ddit_e) {
					fields.emplace_back(std::move(str_key), &it.value());
				} else {
					(this->*ddit->second)(str_key, it.value());
				}
			}
			normalize_script();
		}

		if (specification.script) {
			object = db_handler->run_script(object, term_id, old_document_pair, *specification.script);
			if (!object.is_map()) {
				THROW(ClientError, "Script must return an object, it returned %s", object.getStrType().c_str());
			}
			// Rebuild fields with new values.
			fields.clear();
			static const auto ddit_e = map_dispatch_document_properties.end();
			for (auto it = object.begin(); it != it_e; ++it) {
				auto str_key = it->str();
				const auto ddit = map_dispatch_document_properties.find(str_key);
				if (ddit == ddit_e) {
					fields.emplace_back(std::move(str_key), &it.value());
				}
			}
		}

		MsgPack data;
		auto data_ptr = &data;

		restart_specification();
		const auto spc_start = std::move(specification);
		for (const auto& field : fields) {
			specification = spc_start;
			index_object(properties, *field.second, data_ptr, doc, field.first);
		}

		for (const auto& elem : map_values) {
			const auto val_ser = StringList::serialise(elem.second.begin(), elem.second.end());
			doc.add_value(elem.first, val_ser);
			L_INDEX(this, "Slot: %d  Values: %s", elem.first, repr(val_ser).c_str());
		}

		return data;
	} catch (...) {
		mut_schema.reset();
		throw;
	}
}

#else

MsgPack
Schema::index(const MsgPack& object, Xapian::Document& doc)
{
	L_CALL(this, "Schema::index(%s, <doc>)", repr(object.to_string()).c_str());

	try {
		specification = default_spc;

		FieldVector fields;
		auto properties = &get_newest_properties();

		const auto it_e = object.end();
		if (properties->size() == 1) {
			specification.flags.field_found = false;
			auto mut_properties = &get_mutable_properties(specification.full_meta_name);
			static const auto wpit_e = map_dispatch_write_properties.end();
			for (auto it = object.begin(); it != it_e; ++it) {
				auto str_key = it->str();
				const auto wpit = map_dispatch_write_properties.find(str_key);
				if (wpit == wpit_e) {
					fields.emplace_back(std::move(str_key), &it.value());
				} else {
					(this->*wpit->second)(*mut_properties, str_key, it.value());
				}
			}
			properties = &*mut_properties;
		} else {
			update_specification(*properties);
			static const auto ddit_e = map_dispatch_document_properties.end();
			for (auto it = object.begin(); it != it_e; ++it) {
				auto str_key = it->str();
				const auto ddit = map_dispatch_document_properties.find(str_key);
				if (ddit == ddit_e) {
					fields.emplace_back(std::move(str_key), &it.value());
				} else {
					(this->*ddit->second)(str_key, it.value());
				}
			}
		}

		MsgPack data;
		auto data_ptr = &data;

		restart_specification();
		const auto spc_start = std::move(specification);
		for (const auto& field : fields) {
			specification = spc_start;
			index_object(properties, *field.second, data_ptr, doc, field.first);
		}

		for (const auto& elem : map_values) {
			const auto val_ser = StringList::serialise(elem.second.begin(), elem.second.end());
			doc.add_value(elem.first, val_ser);
			L_INDEX(this, "Slot: %d  Values: %s", elem.first, repr(val_ser).c_str());
		}

		return data;
	} catch (...) {
		mut_schema.reset();
		throw;
	}
}
#endif


void
Schema::write_schema(const MsgPack& obj_schema, bool replace)
{
	L_CALL(this, "Schema::write_schema(%s, %d)", repr(obj_schema.to_string()).c_str(), replace);

	if (!obj_schema.is_map()) {
		THROW(ClientError, "Schema must be an object [%s]", obj_schema.getStrType().c_str());
	}

	try {
		specification = default_spc;

		FieldVector fields;
		auto mut_properties = replace ? &clear() : &get_mutable_properties();

		if (mut_properties->size() == 1) {
			specification.flags.field_found = false;
		} else {
			update_specification(*mut_properties);
		}

		static const auto wpit_e = map_dispatch_write_properties.end();
		const auto it_e = obj_schema.end();
		for (auto it = obj_schema.begin(); it != it_e; ++it) {
			auto str_key = it->str();
			const auto wpit = map_dispatch_write_properties.find(str_key);
			if (wpit == wpit_e) {
				fields.emplace_back(std::move(str_key), &it.value());
			} else {
				(this->*wpit->second)(*mut_properties, str_key, it.value());
			}
		}

#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)
		write_script(*mut_properties);
#endif

		if (specification.flags.is_namespace && fields.size()) {
			return;
		}

		restart_specification();
		const auto spc_start = std::move(specification);
		for (const auto& field : fields) {
			specification = spc_start;
			update_schema(mut_properties, field.first, *field.second);
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
		const auto& properties = get_newest_properties().at(ID_FIELD_NAME);
		res.sep_types[SPC_CONCRETE_TYPE] = (FieldType)properties.at(RESERVED_TYPE).at(SPC_CONCRETE_TYPE).u64();
		res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).u64());
		res.prefix.field = properties.at(RESERVED_PREFIX).str();
		// Get required specification.
		switch (res.sep_types[SPC_CONCRETE_TYPE]) {
			case FieldType::GEO:
				res.flags.partials = properties.at(RESERVED_PARTIALS).boolean();
				res.error = properties.at(RESERVED_ERROR).f64();
				break;
			case FieldType::TERM:
				res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).boolean();
				break;
			default:
				break;
		}
	} catch (const std::out_of_range&) { }

	return res;
}


MsgPack
Schema::get_data_script() const
{
	L_CALL(this, "Schema::get_data_script()");

	try {
		return get_newest_properties().at(RESERVED_SCRIPT);
	} catch (const std::out_of_range&) {
		return MsgPack();
	}
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
		auto spc = get_dynamic_subproperties(get_properties(), field_name);
		res.flags.inside_namespace = spc.inside_namespace;
		res.prefix.field = std::move(spc.prefix);

		if (!spc.acc_field.empty()) {
			res.sep_types[SPC_CONCRETE_TYPE] = spc.acc_field_type;
			return std::make_pair(res, std::move(spc.acc_field));
		}

		if (!res.flags.inside_namespace) {
			const auto& properties = *spc.properties;

			res.sep_types[SPC_CONCRETE_TYPE] = (FieldType)properties.at(RESERVED_TYPE).at(SPC_CONCRETE_TYPE).u64();
			if (res.sep_types[SPC_CONCRETE_TYPE] == FieldType::EMPTY) {
				return std::make_pair(std::move(res), std::string());
			}

			if (is_range) {
				if (spc.has_uuid_prefix) {
					res.slot = get_slot(res.prefix.field, res.get_ctype());
				} else {
					res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).u64());
				}

				// Get required specification.
				switch (res.sep_types[SPC_CONCRETE_TYPE]) {
					case FieldType::GEO:
						res.flags.partials = properties.at(RESERVED_PARTIALS).boolean();
						res.error = properties.at(RESERVED_ERROR).f64();
					case FieldType::FLOAT:
					case FieldType::INTEGER:
					case FieldType::POSITIVE:
					case FieldType::DATE:
					case FieldType::TIME:
					case FieldType::TIMEDELTA:
						for (const auto& acc : properties.at(RESERVED_ACCURACY)) {
							res.accuracy.push_back(acc.u64());
						}
						for (const auto& acc_p : properties.at(RESERVED_ACC_PREFIX)) {
							res.acc_prefix.push_back(res.prefix.field + acc_p.str());
						}
						break;
					case FieldType::TEXT:
						res.language = properties.at(RESERVED_LANGUAGE).str();
						res.stop_strategy = (StopStrategy)properties.at(RESERVED_STOP_STRATEGY).u64();
						res.stem_strategy = (StemStrategy)properties.at(RESERVED_STEM_STRATEGY).u64();
						res.stem_language = properties.at(RESERVED_STEM_LANGUAGE).str();
						break;
					case FieldType::STRING:
						res.language = properties.at(RESERVED_LANGUAGE).str();
						break;
					case FieldType::TERM:
						res.language = properties.at(RESERVED_LANGUAGE).str();
						res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).boolean();
						break;
					default:
						break;
				}
			} else {
				// Get required specification.
				switch (res.sep_types[SPC_CONCRETE_TYPE]) {
					case FieldType::GEO:
						res.flags.partials = properties.at(RESERVED_PARTIALS).boolean();
						res.error = properties.at(RESERVED_ERROR).f64();
						break;
					case FieldType::TEXT:
						res.language = properties.at(RESERVED_LANGUAGE).str();
						res.stop_strategy = (StopStrategy)properties.at(RESERVED_STOP_STRATEGY).u64();
						res.stem_strategy = (StemStrategy)properties.at(RESERVED_STEM_STRATEGY).u64();
						res.stem_language = properties.at(RESERVED_STEM_LANGUAGE).str();
						break;
					case FieldType::STRING:
						res.language = properties.at(RESERVED_LANGUAGE).str();
						break;
					case FieldType::TERM:
						res.language = properties.at(RESERVED_LANGUAGE).str();
						res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).boolean();
						break;
					default:
						break;
				}
			}
		}
	} catch (const ClientError& exc) {
		L_DEBUG(this, "%s", exc.what());
	} catch (const std::out_of_range& exc) {
		L_DEBUG(this, "%s", exc.what());
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
		auto spc = get_dynamic_subproperties(get_properties(), field_name);
		res.flags.inside_namespace = spc.inside_namespace;

		if (!spc.acc_field.empty()) {
			THROW(ClientError, "Field name: %s is an accuracy, therefore does not have slot", repr(field_name).c_str());
		}

		if (res.flags.inside_namespace) {
			res.sep_types[SPC_CONCRETE_TYPE] = FieldType::TERM;
			res.slot = get_slot(spc.prefix, res.get_ctype());
		} else {
			const auto& properties = *spc.properties;

			res.sep_types[SPC_CONCRETE_TYPE] = (FieldType)properties.at(RESERVED_TYPE).at(SPC_CONCRETE_TYPE).u64();

			if (spc.has_uuid_prefix) {
				res.slot = get_slot(spc.prefix, res.get_ctype());
			} else {
				res.slot = static_cast<Xapian::valueno>(properties.at(RESERVED_SLOT).u64());
			}

			// Get required specification.
			switch (res.sep_types[SPC_CONCRETE_TYPE]) {
				case FieldType::GEO:
					res.flags.partials = properties.at(RESERVED_PARTIALS).boolean();
					res.error = properties.at(RESERVED_ERROR).f64();
					break;
				case FieldType::TEXT:
					res.language = properties.at(RESERVED_LANGUAGE).str();
					res.stop_strategy = (StopStrategy)properties.at(RESERVED_STOP_STRATEGY).u64();
					res.stem_strategy = (StemStrategy)properties.at(RESERVED_STEM_STRATEGY).u64();
					res.stem_language = properties.at(RESERVED_STEM_LANGUAGE).str();
					break;
				case FieldType::STRING:
					res.language = properties.at(RESERVED_LANGUAGE).str();
					break;
				case FieldType::TERM:
					res.language = properties.at(RESERVED_LANGUAGE).str();
					res.flags.bool_term = properties.at(RESERVED_BOOL_TERM).boolean();
					break;
				default:
					break;
			}
		}
	} catch (const ClientError& exc) {
		L_DEBUG(this, "%s", exc.what());
	} catch (const std::out_of_range& exc) {
		L_DEBUG(this, "%s", exc.what());
	}

	return res;
}


Schema::dynamic_spc_t
Schema::get_dynamic_subproperties(const MsgPack& properties, const std::string& full_name) const
{
	L_CALL(this, "Schema::get_dynamic_subproperties(%s, %s)", repr(properties.to_string()).c_str(), repr(full_name).c_str());

	Split<char> field_names(full_name, DB_OFFSPRING_UNION);

	dynamic_spc_t spc(&properties);

	const auto it_e = field_names.end();
	const auto it_b = field_names.begin();
	for (auto it = it_b; it != it_e; ++it) {
		auto& field_name = *it;
		if (!is_valid(field_name)) {
			// Check if the field_name is accuracy.
			if (it == it_b) {
				if (!map_dispatch_set_default_spc.count(field_name)) {
					if (++it == it_e) {
						auto acc_data = get_acc_data(field_name);
						spc.prefix.append(acc_data.first);
						spc.acc_field.assign(std::move(field_name));
						spc.acc_field_type = acc_data.second;
						return spc;
					}
					THROW(ClientError, "The field name: %s (%s) in %s is not valid", repr(full_name).c_str(), repr(field_name).c_str(), repr(specification.full_meta_name).c_str());
				}
			} else if (++it == it_e) {
				auto acc_data = get_acc_data(field_name);
				spc.prefix.append(acc_data.first);
				spc.acc_field.assign(std::move(field_name));
				spc.acc_field_type = acc_data.second;
				return spc;
			} else {
				THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(full_name).c_str(), repr(field_name).c_str(), repr(specification.full_meta_name).c_str());
			}
		}

		try {
			spc.properties = &spc.properties->at(field_name);
			spc.prefix.append(spc.properties->at(RESERVED_PREFIX).str());
		} catch (const std::out_of_range&) {
			try {
				const auto prefix_uuid = Serialise::uuid(field_name);
				spc.has_uuid_prefix = true;
				try {
					spc.properties = &spc.properties->at(UUID_FIELD_NAME);
					spc.prefix.append(prefix_uuid);
					continue;
				} catch (const std::out_of_range&) {
					spc.prefix.append(prefix_uuid);
				}
			} catch (const SerialisationError&) {
				spc.prefix.append(get_prefix(field_name));
			}

			// It is a search using partial prefix.
			size_t depth_partials = std::distance(it, it_e);
			if (depth_partials > LIMIT_PARTIAL_PATHS_DEPTH) {
				THROW(ClientError, "Partial paths limit depth is %zu, and partial paths provided has a depth of %zu", LIMIT_PARTIAL_PATHS_DEPTH, depth_partials);
			}
			spc.inside_namespace = true;
			for (++it; it != it_e; ++it) {
				auto& partial_field = *it;
				if (is_valid(partial_field)) {
					try {
						spc.prefix.append(Serialise::uuid(partial_field));
						spc.has_uuid_prefix = true;
					} catch (const SerialisationError&) {
						spc.prefix.append(get_prefix(partial_field));
					}
				} else if (++it == it_e) {
					auto acc_data = get_acc_data(partial_field);
					spc.prefix.append(acc_data.first);
					spc.acc_field.assign(std::move(partial_field));
					spc.acc_field_type = acc_data.second;
					return spc;
				} else {
					THROW(ClientError, "Field name: %s (%s) in %s is not valid", repr(full_name).c_str(), repr(partial_field).c_str(), repr(specification.full_meta_name).c_str());
				}
			}
			return spc;
		}
	}

	return spc;
}


#ifdef L_SCHEMA_DEFINED
#undef L_SCHEMA_DEFINED
#undef L_SCHEMA
#endif
