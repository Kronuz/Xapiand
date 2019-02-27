/*
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

#pragma once

#if XAPIAND_CHAISCRIPT

#include "chaiscript/chaiscript.hpp"

#include "msgpack.h"


namespace chaipp {

inline chaiscript::ModulePtr ModuleMsgPack() {
	chaiscript::ModulePtr module(new chaiscript::Module());

	module->add(chaiscript::type_conversion<const MsgPack, unsigned>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_u64();
			}
		}
		return obj.as_u64();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, int>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_i64();
			}
		}
		return obj.as_i64();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, unsigned long>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_u64();
			}
		}
		return obj.as_u64();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, long>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_i64();
			}
		}
		return obj.as_i64();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, unsigned long long>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_u64();
			}
		}
		return obj.as_u64();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, long long>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_i64();
			}
		}
		return obj.as_i64();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, float>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_f64();
			}
		}
		return obj.as_f64();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, double>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_f64();
			}
		}
		return obj.as_f64();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, bool>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_boolean();
			}
		}
		return obj.as_boolean();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, std::string>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_str();
			}
		}
		return obj.as_str();
	}));
	module->add(chaiscript::type_conversion<const MsgPack, std::string_view>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().str_view();
			}
		}
		return obj.str_view();
	}));

	module->add(chaiscript::type_conversion<unsigned, size_t>([](const unsigned& orig) { return static_cast<size_t>(orig); }));
	module->add(chaiscript::type_conversion<int, size_t>([](const int& orig) { return static_cast<size_t>(orig); }));
	module->add(chaiscript::type_conversion<unsigned long, size_t>([](const unsigned long& orig) { return static_cast<size_t>(orig); }));
	module->add(chaiscript::type_conversion<long, size_t>([](const long& orig) { return static_cast<size_t>(orig); }));

	module->add(chaiscript::type_conversion<const std::string, std::string_view>([](const std::string& obj) { return std::string_view(obj); }));

	chaiscript::utility::add_class<MsgPack>(
		*module,
		"MsgPack",
		{
			chaiscript::constructor<MsgPack()>(),
			chaiscript::constructor<MsgPack(MsgPack&&)>(),
			chaiscript::constructor<MsgPack(const MsgPack&)>(),
			// Specific instantiation of the template constructor.
			chaiscript::constructor<MsgPack(const unsigned&)>(),
			chaiscript::constructor<MsgPack(const int&)>(),
			chaiscript::constructor<MsgPack(const unsigned long&)>(),
			chaiscript::constructor<MsgPack(const long&)>(),
			chaiscript::constructor<MsgPack(const unsigned long long&)>(),
			chaiscript::constructor<MsgPack(const long long&)>(),
			chaiscript::constructor<MsgPack(const float&)>(),
			chaiscript::constructor<MsgPack(const double&)>(),
			chaiscript::constructor<MsgPack(const bool&)>(),
			chaiscript::constructor<MsgPack(std::string&&)>(),
			chaiscript::constructor<MsgPack(const std::string&)>(),
			chaiscript::constructor<MsgPack(std::string_view&&)>(),
			chaiscript::constructor<MsgPack(const std::string_view&)>(),
			chaiscript::constructor<MsgPack(std::vector<MsgPack>&&)>(),
			chaiscript::constructor<MsgPack(const std::vector<MsgPack>&)>(),
			chaiscript::constructor<MsgPack(std::map<std::string, MsgPack>&&)>(),
			chaiscript::constructor<MsgPack(const std::map<std::string, MsgPack>&)>(),
			chaiscript::constructor<MsgPack(std::map<std::string_view, MsgPack>&&)>(),
			chaiscript::constructor<MsgPack(const std::map<std::string_view, MsgPack>&)>(),
			chaiscript::constructor<MsgPack(chaiscript::Boxed_Value&&)>(),
			chaiscript::constructor<MsgPack(const chaiscript::Boxed_Value&)>(),
			chaiscript::constructor<MsgPack(std::vector<chaiscript::Boxed_Value>&&)>(),
			chaiscript::constructor<MsgPack(const std::vector<chaiscript::Boxed_Value>&)>(),
			chaiscript::constructor<MsgPack(std::map<std::string, chaiscript::Boxed_Value>&&)>(),
			chaiscript::constructor<MsgPack(const std::map<std::string, chaiscript::Boxed_Value>&)>(),
			chaiscript::constructor<MsgPack(std::map<std::string_view, chaiscript::Boxed_Value>&&)>(),
			chaiscript::constructor<MsgPack(const std::map<std::string_view, chaiscript::Boxed_Value>&)>(),
		},
		{
			// operator []
			{ chaiscript::fun([](MsgPack& obj, size_t idx) -> MsgPack& {
				if (obj.is_array()) {
					return obj.operator[](idx);
				}
				return MsgPack::undefined();
			}), "[]" },
			{ chaiscript::fun([](const MsgPack& obj, size_t idx) -> const MsgPack& {
				if (obj.is_array()) {
					return obj.operator[](idx);
				}
				return MsgPack::undefined();
			}), "[]" },
			{ chaiscript::fun([](MsgPack& obj, const std::string& str) -> MsgPack& {
				if (obj.is_map()) {
					return obj.operator[](str);
				}
				return MsgPack::undefined();
			}), "[]" },
			{ chaiscript::fun([](const MsgPack& obj, const std::string& str) -> const MsgPack& {
				if (obj.is_map()) {
					return obj.operator[](str);
				}
				return MsgPack::undefined();
			}), "[]" },

			// method at()
			{ chaiscript::fun([](MsgPack& obj, size_t idx) -> MsgPack& {
				if (obj.is_array()) {
					return obj.at(idx);
				}
				return MsgPack::undefined();
			}), "at" },
			{ chaiscript::fun([](const MsgPack& obj, size_t idx) -> const MsgPack& {
				if (obj.is_array()) {
					return obj.at(idx);
				}
				return MsgPack::undefined();
			}), "at" },
			{ chaiscript::fun([](MsgPack& obj, const std::string& str) -> MsgPack& {
				if (obj.is_map()) {
					return obj.at(str);
				}
				return MsgPack::undefined();
			}), "at" },
			{ chaiscript::fun([](const MsgPack& obj, const std::string& str) -> const MsgPack& {
				if (obj.is_map()) {
					return obj.at(str);
				}
				return MsgPack::undefined();
			}), "at" },

			// method find()
			{ chaiscript::fun([](MsgPack& obj, size_t idx) -> MsgPack::iterator {
				return obj.find(idx);
			}), "find" },
			{ chaiscript::fun([](const MsgPack& obj, size_t idx) -> MsgPack::const_iterator {
				return obj.find(idx);
			}), "find" },
			{ chaiscript::fun([](MsgPack& obj, const std::string& str) -> MsgPack::iterator {
				return obj.find(str);
			}), "find" },
			{ chaiscript::fun([](const MsgPack& obj, const std::string& str) -> MsgPack::const_iterator {
				return obj.find(str);
			}), "find" },

			// Specific instantiation of the template MsgPack::update.
			{ chaiscript::fun(&MsgPack::update<MsgPack>),            "update" },
			{ chaiscript::fun(&MsgPack::update<MsgPack&>),           "update" },
			{ chaiscript::fun(&MsgPack::update<const MsgPack&>),     "update" },

			// method count()
			{ chaiscript::fun([](const MsgPack& obj, size_t idx) -> size_t {
				return obj.count(idx);
			}), "count" },
			{ chaiscript::fun([](const MsgPack& obj, const std::string& str) -> size_t {
				return obj.count(str);
			}), "count" },

			// method erase()
			{ chaiscript::fun([](MsgPack& obj, size_t idx) -> size_t {
				if (obj.is_array()) {
					return obj.erase(idx);
				}
				return 0;
			}), "erase" },
			{ chaiscript::fun([](MsgPack& obj, const std::string& str) -> size_t {
				if (obj.is_map()) {
					return obj.erase(str);
				}
				return 0;
			}), "erase" },

			{ chaiscript::fun(&MsgPack::clear),         "clear"         },
			{ chaiscript::fun(&MsgPack::reserve),       "reserve"       },
			{ chaiscript::fun(&MsgPack::capacity),      "capacity"      },
			{ chaiscript::fun(&MsgPack::size),          "size"          },
			{ chaiscript::fun(&MsgPack::empty),         "empty"         },

			{ chaiscript::fun(&MsgPack::u64),           "u64"           },
			{ chaiscript::fun(&MsgPack::i64),           "i64"           },
			{ chaiscript::fun(&MsgPack::f64),           "f64"           },
			{ chaiscript::fun(&MsgPack::str),           "str"           },
			{ chaiscript::fun(&MsgPack::boolean),       "boolean"       },

			{ chaiscript::fun(&MsgPack::as_u64),        "as_u64"        },
			{ chaiscript::fun(&MsgPack::as_i64),        "as_i64"        },
			{ chaiscript::fun(&MsgPack::as_f64),        "as_f64"        },
			{ chaiscript::fun(&MsgPack::as_str),        "as_str"        },
			{ chaiscript::fun(&MsgPack::as_boolean),    "as_boolean"    },
#ifndef WITHOUT_RAPIDJSON
			{ chaiscript::fun(&MsgPack::as_document),   "as_document"   },
#endif
			{ chaiscript::fun(&MsgPack::is_undefined),  "is_undefined"  },
			{ chaiscript::fun(&MsgPack::is_null),       "is_null"       },
			{ chaiscript::fun(&MsgPack::is_boolean),    "is_boolean"    },
			{ chaiscript::fun(&MsgPack::is_number),     "is_number"     },
			{ chaiscript::fun(&MsgPack::is_integer),    "is_integer"    },
			{ chaiscript::fun(&MsgPack::is_float),      "is_float"      },
			{ chaiscript::fun(&MsgPack::is_map),        "is_map"        },
			{ chaiscript::fun(&MsgPack::is_array),      "is_array"      },
			{ chaiscript::fun(&MsgPack::is_string),     "is_string"     },

			{ chaiscript::fun(&MsgPack::getType),       "getType"       },
			{ chaiscript::fun(&MsgPack::hash),          "hash"          },

			{ chaiscript::fun(&MsgPack::operator==),     "=="           },
			{ chaiscript::fun(&MsgPack::operator!=),     "!="           },
			{ chaiscript::fun(&MsgPack::operator<<),     "<<"           },

			{ chaiscript::fun(&MsgPack::operator+<const unsigned&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const unsigned&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const unsigned&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const unsigned&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const unsigned&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const unsigned&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const unsigned&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const unsigned&>),     "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const int&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const int&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const int&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const int&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const int&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const int&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const int&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const int&>),     "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const unsigned long&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const unsigned long&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const unsigned long&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const unsigned long&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const unsigned long&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const unsigned long&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const unsigned long&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const unsigned long&>),     "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const long&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const long&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const long&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const long&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const long&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const long&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const long&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const long&>),     "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const long long&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const long long&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const long long&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const long long&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const long long&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const long long&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const long long&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const long long&>),     "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const unsigned long long&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const unsigned long long&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const unsigned long long&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const unsigned long long&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const unsigned long long&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const unsigned long long&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const unsigned long long&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const unsigned long long&>),     "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const float&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const float&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const float&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const float&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const float&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const float&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const float&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const float&>),     "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const double&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const double&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const double&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const double&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const double&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const double&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const double&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const double&>),     "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const bool&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const bool&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const bool&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const bool&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const bool&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const bool&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const bool&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const bool&>),     "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const MsgPack&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=<const MsgPack&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-<const MsgPack&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator-=<const MsgPack&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*<const MsgPack&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator*=<const MsgPack&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/<const MsgPack&>),      "/"            },
			{ chaiscript::fun(&MsgPack::operator/=<const MsgPack&>),     "/="           },

			{ chaiscript::fun(&MsgPack::lock),           "lock"         },

			{ chaiscript::fun(&MsgPack::unformatted_string),           "unformatted_string" },
			{ chaiscript::fun(&MsgPack::to_string),                    "to_string"          },
			{ chaiscript::fun(&MsgPack::serialise<msgpack::sbuffer>),  "serialise"          },

			{ chaiscript::fun<MsgPack&, MsgPack, MsgPack&&>(&MsgPack::operator=),                               "=" },
			{ chaiscript::fun<MsgPack&, MsgPack, const MsgPack&>(&MsgPack::operator=),                          "=" },
			// Specific instantiation of the template assigment operator.
			{ chaiscript::fun(&MsgPack::operator=<const unsigned&>),                                            "=" },
			{ chaiscript::fun(&MsgPack::operator=<const int&>),                                                 "=" },
			{ chaiscript::fun(&MsgPack::operator=<const unsigned long&>),                                       "=" },
			{ chaiscript::fun(&MsgPack::operator=<const long&>),                                                "=" },
			{ chaiscript::fun(&MsgPack::operator=<const unsigned long long&>),                                  "=" },
			{ chaiscript::fun(&MsgPack::operator=<const long long&>),                                           "=" },
			{ chaiscript::fun(&MsgPack::operator=<const float&>),                                               "=" },
			{ chaiscript::fun(&MsgPack::operator=<const double&>),                                              "=" },
			{ chaiscript::fun(&MsgPack::operator=<const bool&>),                                                "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::string&&>),                                              "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::string&>),                                         "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::string_view&&>),                                         "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::string_view&>),                                    "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::vector<MsgPack>&&>),                                     "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::vector<MsgPack>&>),                                "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::map<std::string, MsgPack>&&>),                           "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::map<std::string, MsgPack>&>),                      "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::map<std::string_view, MsgPack>&&>),                      "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::map<std::string_view, MsgPack>&>),                 "=" },
			{ chaiscript::fun(&MsgPack::operator=<chaiscript::Boxed_Value&&>),                                  "=" },
			{ chaiscript::fun(&MsgPack::operator=<const chaiscript::Boxed_Value&>),                             "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::vector<chaiscript::Boxed_Value>&&>),                     "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::vector<chaiscript::Boxed_Value>&>),                "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::map<std::string, chaiscript::Boxed_Value>&&>),           "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::map<std::string, chaiscript::Boxed_Value>&>),      "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::map<std::string_view, chaiscript::Boxed_Value>&&>),      "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::map<std::string_view, chaiscript::Boxed_Value>&>), "=" },

			{ chaiscript::fun(&MsgPack::append<unsigned>),            "append" },
			{ chaiscript::fun(&MsgPack::append<int>),                 "append" },
			{ chaiscript::fun(&MsgPack::append<unsigned long>),       "append" },
			{ chaiscript::fun(&MsgPack::append<long>),                "append" },
			{ chaiscript::fun(&MsgPack::append<unsigned long long>),  "append" },
			{ chaiscript::fun(&MsgPack::append<long long>),           "append" },
			{ chaiscript::fun(&MsgPack::append<float>),               "append" },
			{ chaiscript::fun(&MsgPack::append<double>),              "append" },
			{ chaiscript::fun(&MsgPack::append<bool>),                "append" },
			{ chaiscript::fun(&MsgPack::append<std::string>),         "append" },
			{ chaiscript::fun(&MsgPack::append<std::string&>),        "append" },
			{ chaiscript::fun(&MsgPack::append<const std::string&>),  "append" },
			{ chaiscript::fun(&MsgPack::append<MsgPack>),             "append" },
			{ chaiscript::fun(&MsgPack::append<MsgPack&>),            "append" },
			{ chaiscript::fun(&MsgPack::append<const MsgPack&>),      "append" },

			{ chaiscript::fun<MsgPack&, MsgPack, const std::vector<std::string>&>(&MsgPack::path),              "path" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, const std::vector<std::string>&>(&MsgPack::path),  "path" },

			// Specific instantiation of the template MsgPack::put<const MsgPack&, T>.
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, unsigned>),                                                                          "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, int>),                                                                               "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, unsigned long>),                                                                     "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, long>),                                                                              "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, unsigned long long>),                                                                "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, long long>),                                                                         "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, float>),                                                                             "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, double>),                                                                            "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, bool>),                                                                              "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, std::string>),                                                                       "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, std::string&>),                                                                      "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const std::string&>),                                                                "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, MsgPack>),                                                                           "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, MsgPack&>),                                                                          "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const MsgPack&>),                                                                    "put" },
			// Specific instantiation of the template MsgPack::put<std::string_view, T>.
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, unsigned&&)>(&MsgPack::put<unsigned>)),                        "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, int&&)>(&MsgPack::put<int>)),                                  "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, unsigned long&&)>(&MsgPack::put<unsigned long>)),              "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, long&&)>(&MsgPack::put<long>)),                                "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, unsigned long long&&)>(&MsgPack::put<unsigned long long>)),    "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, long long&&)>(&MsgPack::put<long long>)),                      "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, float&&)>(&MsgPack::put<float>)),                              "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, double&&)>(&MsgPack::put<double>)),                            "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, bool&&)>(&MsgPack::put<bool>)),                                "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, std::string&&)>(&MsgPack::put<std::string>)),                  "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, std::string&)>(&MsgPack::put<std::string&>)),                  "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const std::string&)>(&MsgPack::put<const std::string&>)),      "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, MsgPack&&)>(&MsgPack::put<MsgPack>)),                          "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, MsgPack&)>(&MsgPack::put<MsgPack&>)),                          "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const MsgPack&)>(&MsgPack::put<const MsgPack&>)),              "put" },
			// Specific instantiation of the template MsgPack::put<size_t, T>.
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, unsigned&&)>(&MsgPack::put<unsigned>)),                                  "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, int&&)>(&MsgPack::put<int>)),                                            "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, unsigned long&&)>(&MsgPack::put<unsigned long>)),                        "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, long&&)>(&MsgPack::put<long>)),                                          "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, unsigned long long&&)>(&MsgPack::put<unsigned long long>)),              "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, long long&&)>(&MsgPack::put<long long>)),                                "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, float&&)>(&MsgPack::put<float>)),                                        "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, double&&)>(&MsgPack::put<double>)),                                      "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, bool&&)>(&MsgPack::put<bool>)),                                          "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, std::string&&)>(&MsgPack::put<std::string>)),                            "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, std::string&)>(&MsgPack::put<std::string&>)),                            "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const std::string&)>(&MsgPack::put<const std::string&>)),                "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, MsgPack&&)>(&MsgPack::put<MsgPack>)),                                    "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, MsgPack&)>(&MsgPack::put<MsgPack&>)),                                    "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const MsgPack&)>(&MsgPack::put<const MsgPack&>)),                        "put" },

			// Specific instantiation of the template MsgPack::add<M&&, T&&>.
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, unsigned&&)>(&MsgPack::add<const MsgPack&, unsigned>)),                      "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, int&&)>(&MsgPack::add<const MsgPack&, int>)),                                "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, unsigned long&&)>(&MsgPack::add<const MsgPack&, unsigned long>)),            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, long&&)>(&MsgPack::add<const MsgPack&, long>)),                              "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, unsigned long long&&)>(&MsgPack::add<const MsgPack&, unsigned long long>)),  "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, long long&&)>(&MsgPack::add<const MsgPack&, long long>)),                    "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, float&&)>(&MsgPack::add<const MsgPack&, float>)),                            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, double&&)>(&MsgPack::add<const MsgPack&, double>)),                          "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, bool&&)>(&MsgPack::add<const MsgPack&, bool>)),                              "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, std::string&&)>(&MsgPack::add<const MsgPack&, std::string>)),                "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, std::string&)>(&MsgPack::add<const MsgPack&, std::string&>)),                "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, const std::string&)>(&MsgPack::add<const MsgPack&, const std::string&>)),    "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, MsgPack&&)>(&MsgPack::add<const MsgPack&, MsgPack>)),                        "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, MsgPack&)>(&MsgPack::add<const MsgPack&, MsgPack&>)),                        "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(const MsgPack&, const MsgPack&)>(&MsgPack::add<const MsgPack&, const MsgPack&>)),            "add" },
			// Specific instantiation of the template MsgPack::add<std::string_view, T&&>.
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, unsigned&&)>(&MsgPack::add<unsigned>)),                                    "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, int&&)>(&MsgPack::add<int>)),                                              "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, unsigned long&&)>(&MsgPack::add<unsigned long>)),                          "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, long&&)>(&MsgPack::add<long>)),                                            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, unsigned long long&&)>(&MsgPack::add<unsigned long long>)),                "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, long long&&)>(&MsgPack::add<long long>)),                                  "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, float&&)>(&MsgPack::add<float>)),                                          "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, double&&)>(&MsgPack::add<double>)),                                        "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, bool&&)>(&MsgPack::add<bool>)),                                            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, std::string&&)>(&MsgPack::add<std::string>)),                              "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, std::string&)>(&MsgPack::add<std::string&>)),                              "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const std::string&)>(&MsgPack::add<const std::string&>)),                  "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, MsgPack&&)>(&MsgPack::add<MsgPack>)),                                      "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, MsgPack&)>(&MsgPack::add<MsgPack&>)),                                      "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const MsgPack&)>(&MsgPack::add<const MsgPack&>)),                          "add" },
			// Specific instantiation of the template MsgPack::add<size_t, T&&>.
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, unsigned&&)>(&MsgPack::add<unsigned>)),                                              "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, int&&)>(&MsgPack::add<int>)),                                                        "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, unsigned long&&)>(&MsgPack::add<unsigned long>)),                                    "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, long&&)>(&MsgPack::add<long>)),                                                      "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, unsigned long long&&)>(&MsgPack::add<unsigned long long>)),                          "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, long long&&)>(&MsgPack::add<long long>)),                                            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, float&&)>(&MsgPack::add<float>)),                                                    "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, double&&)>(&MsgPack::add<double>)),                                                  "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, bool&&)>(&MsgPack::add<bool>)),                                                      "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, std::string&&)>(&MsgPack::add<std::string>)),                                        "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, std::string&)>(&MsgPack::add<std::string&>)),                                        "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const std::string&)>(&MsgPack::add<const std::string&>)),                            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, MsgPack&&)>(&MsgPack::add<MsgPack>)),                                                "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, MsgPack&)>(&MsgPack::add<MsgPack&>)),                                                "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const MsgPack&)>(&MsgPack::add<const MsgPack&>)),                                    "add" },

			// Overload operator +
			{ chaiscript::fun([](const MsgPack& obj, unsigned value) { return obj.as_i64() + value; }),                     "+" },
			{ chaiscript::fun([](const MsgPack& obj, int value) { return obj.as_i64() + value; }),                          "+" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long value) { return obj.as_i64() + value; }),                "+" },
			{ chaiscript::fun([](const MsgPack& obj, long value) { return obj.as_i64() + value; }),                         "+" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long long value) { return obj.as_i64() + value; }),           "+" },
			{ chaiscript::fun([](const MsgPack& obj, long long value) { return obj.as_i64() + value; }),                    "+" },
			{ chaiscript::fun([](const MsgPack& obj, float value) { return obj.as_f64() + value; }),                        "+" },
			{ chaiscript::fun([](const MsgPack& obj, double value) { return obj.as_f64() + value; }),                       "+" },
			{ chaiscript::fun([](const MsgPack& obj, const std::string& value) { return obj.as_str() + value; }),           "+" },

			{ chaiscript::fun([](unsigned value, const MsgPack& obj) { return value + obj.as_i64(); }),                     "+" },
			{ chaiscript::fun([](int value, const MsgPack& obj) { return value + obj.as_i64(); }),                          "+" },
			{ chaiscript::fun([](unsigned long value, const MsgPack& obj) { return value + obj.as_i64(); }),                "+" },
			{ chaiscript::fun([](long value, const MsgPack& obj) { return value + obj.as_i64(); }),                         "+" },
			{ chaiscript::fun([](unsigned long long value, const MsgPack& obj) { return value + obj.as_i64(); }),           "+" },
			{ chaiscript::fun([](long long value, const MsgPack& obj) { return value + obj.as_i64(); }),                    "+" },
			{ chaiscript::fun([](float value, const MsgPack& obj) { return value + obj.as_f64(); }),                        "+" },
			{ chaiscript::fun([](double value, const MsgPack& obj) { return value + obj.as_f64(); }),                       "+" },
			{ chaiscript::fun([](const std::string& value, const MsgPack& obj) { return value + obj.as_str(); }),           "+" },

			// Overload operator -
			{ chaiscript::fun([](const MsgPack& obj, unsigned value) { return obj.as_i64() - value; }),                     "-" },
			{ chaiscript::fun([](const MsgPack& obj, int value) { return obj.as_i64() - value; }),                          "-" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long value) { return obj.as_i64() - value; }),                "-" },
			{ chaiscript::fun([](const MsgPack& obj, long value) { return obj.as_i64() - value; }),                         "-" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long long value) { return obj.as_i64() - value; }),           "-" },
			{ chaiscript::fun([](const MsgPack& obj, long long value) { return obj.as_i64() - value; }),                    "-" },
			{ chaiscript::fun([](const MsgPack& obj, float value) { return obj.as_f64() - value; }),                        "-" },
			{ chaiscript::fun([](const MsgPack& obj, double value) { return obj.as_f64() - value; }),                       "-" },

			{ chaiscript::fun([](unsigned value, const MsgPack& obj) { return value - obj.as_i64(); }),                     "-" },
			{ chaiscript::fun([](int value, const MsgPack& obj) { return value - obj.as_i64(); }),                          "-" },
			{ chaiscript::fun([](unsigned long value, const MsgPack& obj) { return value - obj.as_i64(); }),                "-" },
			{ chaiscript::fun([](long value, const MsgPack& obj) { return value - obj.as_i64(); }),                         "-" },
			{ chaiscript::fun([](unsigned long long value, const MsgPack& obj) { return value - obj.as_i64(); }),           "-" },
			{ chaiscript::fun([](long long value, const MsgPack& obj) { return value - obj.as_i64(); }),                    "-" },
			{ chaiscript::fun([](float value, const MsgPack& obj) { return value - obj.as_f64(); }),                        "-" },
			{ chaiscript::fun([](double value, const MsgPack& obj) { return value - obj.as_f64(); }),                       "-" },

			// Overload operator *
			{ chaiscript::fun([](const MsgPack& obj, unsigned value) { return obj.as_i64() * value; }),                     "*" },
			{ chaiscript::fun([](const MsgPack& obj, int value) { return obj.as_i64() * value; }),                          "*" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long value) { return obj.as_i64() * value; }),                "*" },
			{ chaiscript::fun([](const MsgPack& obj, long value) { return obj.as_i64() * value; }),                         "*" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long long value) { return obj.as_i64() * value; }),           "*" },
			{ chaiscript::fun([](const MsgPack& obj, long long value) { return obj.as_i64() * value; }),                    "*" },
			{ chaiscript::fun([](const MsgPack& obj, float value) { return obj.as_f64() * value; }),                        "*" },
			{ chaiscript::fun([](const MsgPack& obj, double value) { return obj.as_f64() * value; }),                       "*" },

			{ chaiscript::fun([](unsigned value, const MsgPack& obj) { return value * obj.as_i64(); }),                     "*" },
			{ chaiscript::fun([](int value, const MsgPack& obj) { return value * obj.as_i64(); }),                          "*" },
			{ chaiscript::fun([](unsigned long value, const MsgPack& obj) { return value * obj.as_i64(); }),                "*" },
			{ chaiscript::fun([](long value, const MsgPack& obj) { return value * obj.as_i64(); }),                         "*" },
			{ chaiscript::fun([](unsigned long long value, const MsgPack& obj) { return value * obj.as_i64(); }),           "*" },
			{ chaiscript::fun([](long long value, const MsgPack& obj) { return value * obj.as_i64(); }),                    "*" },
			{ chaiscript::fun([](float value, const MsgPack& obj) { return value * obj.as_f64(); }),                        "*" },
			{ chaiscript::fun([](double value, const MsgPack& obj) { return value * obj.as_f64(); }),                       "*" },

			// Overload operator /
			{ chaiscript::fun([](const MsgPack& obj, unsigned value) { return obj.as_i64() / value; }),                     "/" },
			{ chaiscript::fun([](const MsgPack& obj, int value) { return obj.as_i64() / value; }),                          "/" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long value) { return obj.as_i64() / value; }),                "/" },
			{ chaiscript::fun([](const MsgPack& obj, long value) { return obj.as_i64() / value; }),                         "/" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long long value) { return obj.as_i64() / value; }),           "/" },
			{ chaiscript::fun([](const MsgPack& obj, long long value) { return obj.as_i64() / value; }),                    "/" },
			{ chaiscript::fun([](const MsgPack& obj, float value) { return obj.as_f64() / value; }),                        "/" },
			{ chaiscript::fun([](const MsgPack& obj, double value) { return obj.as_f64() / value; }),                       "/" },

			{ chaiscript::fun([](unsigned value, const MsgPack& obj) { return value / obj.as_i64(); }),                     "/" },
			{ chaiscript::fun([](int value, const MsgPack& obj) { return value / obj.as_i64(); }),                          "/" },
			{ chaiscript::fun([](unsigned long value, const MsgPack& obj) { return value / obj.as_i64(); }),                "/" },
			{ chaiscript::fun([](long value, const MsgPack& obj) { return value / obj.as_i64(); }),                         "/" },
			{ chaiscript::fun([](unsigned long long value, const MsgPack& obj) { return value / obj.as_i64(); }),           "/" },
			{ chaiscript::fun([](long long value, const MsgPack& obj) { return value / obj.as_i64(); }),                    "/" },
			{ chaiscript::fun([](float value, const MsgPack& obj) { return value / obj.as_f64(); }),                        "/" },
			{ chaiscript::fun([](double value, const MsgPack& obj) { return value / obj.as_f64(); }),                       "/" },

			// Adding special value method.
			{
				chaiscript::fun([](const MsgPack& obj) {
					if (obj.is_map()) {
						auto it = obj.find("_value");
						if (it == obj.end()) {
							return obj;
						} else {
							return it.value();
						}
					} else {
						return obj;
					}
				}), "value"
			},
		}
	);

	return module;
}

}; // End namespace chaipp

#endif
