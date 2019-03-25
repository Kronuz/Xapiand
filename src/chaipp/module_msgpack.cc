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

#include "module.h"

#if XAPIAND_CHAISCRIPT

#include "msgpack.h"


namespace chaipp {

void
Module::msgpack(chaiscript::Module& m)
{
	m.add(chaiscript::type_conversion<const MsgPack&, unsigned>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_u64();
			}
		}
		return obj.as_u64();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, int>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_i64();
			}
		}
		return obj.as_i64();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, unsigned long>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_u64();
			}
		}
		return obj.as_u64();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, long>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_i64();
			}
		}
		return obj.as_i64();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, unsigned long long>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_u64();
			}
		}
		return obj.as_u64();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, long long>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_i64();
			}
		}
		return obj.as_i64();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, float>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_f64();
			}
		}
		return obj.as_f64();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, double>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_f64();
			}
		}
		return obj.as_f64();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, bool>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_boolean();
			}
		}
		return obj.as_boolean();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, std::string>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().as_str();
			}
		}
		return obj.as_str();
	}));
	m.add(chaiscript::type_conversion<const MsgPack&, std::string_view>([](const MsgPack& obj) {
		if (obj.is_map()) {
			auto it = obj.find("_value");
			if (it != obj.end()) {
				return it.value().str_view();
			}
		}
		return obj.str_view();
	}));

	m.add(chaiscript::type_conversion<const std::string&, std::string_view>([](const std::string& obj) { return std::string_view(obj); }));

	m.add(chaiscript::type_conversion<const unsigned&, size_t>([](const unsigned& orig) { return static_cast<size_t>(orig); }));
	m.add(chaiscript::type_conversion<const int&, size_t>([](const int& orig) { return static_cast<size_t>(orig); }));
	m.add(chaiscript::type_conversion<const unsigned long&, size_t>([](const unsigned long& orig) { return static_cast<size_t>(orig); }));
	m.add(chaiscript::type_conversion<const long&, size_t>([](const long& orig) { return static_cast<size_t>(orig); }));
	m.add(chaiscript::type_conversion<const unsigned long long&, size_t>([](const unsigned long long& orig) { return static_cast<size_t>(orig); }));
	m.add(chaiscript::type_conversion<const long long&, size_t>([](const long long& orig) { return static_cast<size_t>(orig); }));

	chaiscript::utility::add_class<MsgPack>(
		m,
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
			{ chaiscript::fun([](MsgPack& obj, const std::string_view& str) -> MsgPack& {
				if (obj.is_map()) {
					return obj.operator[](str);
				}
				return MsgPack::undefined();
			}), "[]" },
			{ chaiscript::fun([](const MsgPack& obj, const std::string_view& str) -> const MsgPack& {
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
			{ chaiscript::fun([](MsgPack& obj, const std::string_view& str) -> MsgPack& {
				if (obj.is_map()) {
					return obj.at(str);
				}
				return MsgPack::undefined();
			}), "at" },
			{ chaiscript::fun([](const MsgPack& obj, const std::string_view& str) -> const MsgPack& {
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
			{ chaiscript::fun([](MsgPack& obj, const std::string_view& str) -> MsgPack::iterator {
				return obj.find(str);
			}), "find" },
			{ chaiscript::fun([](const MsgPack& obj, const std::string_view& str) -> MsgPack::const_iterator {
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
			{ chaiscript::fun([](const MsgPack& obj, const std::string_view& str) -> size_t {
				return obj.count(str);
			}), "count" },

			// method erase()
			{ chaiscript::fun([](MsgPack& obj, size_t idx) -> size_t {
				if (obj.is_array()) {
					return obj.erase(idx);
				}
				return 0;
			}), "erase" },
			{ chaiscript::fun([](MsgPack& obj, const std::string_view& str) -> size_t {
				if (obj.is_map()) {
					return obj.erase(str);
				} else if (obj.is_array()) {
					for (auto it = obj.begin(); it != obj.end(); ++it) {
						if (it.value() == str) {
							obj.erase(it);
							return 1;
						}
					}
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
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)()>(&MsgPack::operator++)),     "++"           },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)()>(&MsgPack::operator--)),     "--"           },

			{ chaiscript::fun(&MsgPack::operator+<const unsigned&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const unsigned&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const unsigned&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const unsigned&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const unsigned&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const unsigned&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const unsigned&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const unsigned&>),     "/="           },

			{ chaiscript::fun(static_cast<unsigned (*)(const unsigned&, const MsgPack&)>(&::operator+<unsigned, const MsgPack&>)),    "+"            },
			{ chaiscript::fun(static_cast<unsigned (*)(const unsigned&, const MsgPack&)>(&::operator-<unsigned, const MsgPack&>)),    "-"            },
			{ chaiscript::fun(static_cast<unsigned (*)(const unsigned&, const MsgPack&)>(&::operator*<unsigned, const MsgPack&>)),    "*"            },
			{ chaiscript::fun(static_cast<unsigned (*)(const unsigned&, const MsgPack&)>(&::operator/<unsigned, const MsgPack&>)),    "/"            },

			{ chaiscript::fun(static_cast<unsigned& (*)(unsigned&, const MsgPack&)>(&::operator+=<unsigned, const MsgPack&>)),        "+="           },
			{ chaiscript::fun(static_cast<unsigned& (*)(unsigned&, const MsgPack&)>(&::operator-=<unsigned, const MsgPack&>)),        "-="           },
			{ chaiscript::fun(static_cast<unsigned& (*)(unsigned&, const MsgPack&)>(&::operator*=<unsigned, const MsgPack&>)),        "*="           },
			{ chaiscript::fun(static_cast<unsigned& (*)(unsigned&, const MsgPack&)>(&::operator/=<unsigned, const MsgPack&>)),        "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const int&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const int&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const int&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const int&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const int&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const int&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const int&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const int&>),     "/="           },

			{ chaiscript::fun(static_cast<int (*)(const int&, const MsgPack&)>(&::operator+<int, const MsgPack&>)),    "+"            },
			{ chaiscript::fun(static_cast<int (*)(const int&, const MsgPack&)>(&::operator-<int, const MsgPack&>)),    "-"            },
			{ chaiscript::fun(static_cast<int (*)(const int&, const MsgPack&)>(&::operator*<int, const MsgPack&>)),    "*"            },
			{ chaiscript::fun(static_cast<int (*)(const int&, const MsgPack&)>(&::operator/<int, const MsgPack&>)),    "/"            },

			{ chaiscript::fun(static_cast<int& (*)(int&, const MsgPack&)>(&::operator+=<int, const MsgPack&>)),        "+="           },
			{ chaiscript::fun(static_cast<int& (*)(int&, const MsgPack&)>(&::operator-=<int, const MsgPack&>)),        "-="           },
			{ chaiscript::fun(static_cast<int& (*)(int&, const MsgPack&)>(&::operator*=<int, const MsgPack&>)),        "*="           },
			{ chaiscript::fun(static_cast<int& (*)(int&, const MsgPack&)>(&::operator/=<int, const MsgPack&>)),        "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const unsigned long&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const unsigned long&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const unsigned long&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const unsigned long&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const unsigned long&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const unsigned long&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const unsigned long&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const unsigned long&>),     "/="           },

			{ chaiscript::fun(static_cast<unsigned long (*)(const unsigned long&, const MsgPack&)>(&::operator+<unsigned long, const MsgPack&>)),    "+"            },
			{ chaiscript::fun(static_cast<unsigned long (*)(const unsigned long&, const MsgPack&)>(&::operator-<unsigned long, const MsgPack&>)),    "-"            },
			{ chaiscript::fun(static_cast<unsigned long (*)(const unsigned long&, const MsgPack&)>(&::operator*<unsigned long, const MsgPack&>)),    "*"            },
			{ chaiscript::fun(static_cast<unsigned long (*)(const unsigned long&, const MsgPack&)>(&::operator/<unsigned long, const MsgPack&>)),    "/"            },

			{ chaiscript::fun(static_cast<unsigned long& (*)(unsigned long&, const MsgPack&)>(&::operator+=<unsigned long, const MsgPack&>)),        "+="           },
			{ chaiscript::fun(static_cast<unsigned long& (*)(unsigned long&, const MsgPack&)>(&::operator-=<unsigned long, const MsgPack&>)),        "-="           },
			{ chaiscript::fun(static_cast<unsigned long& (*)(unsigned long&, const MsgPack&)>(&::operator*=<unsigned long, const MsgPack&>)),        "*="           },
			{ chaiscript::fun(static_cast<unsigned long& (*)(unsigned long&, const MsgPack&)>(&::operator/=<unsigned long, const MsgPack&>)),        "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const long&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const long&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const long&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const long&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const long&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const long&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const long&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const long&>),     "/="           },

			{ chaiscript::fun(static_cast<long (*)(const long&, const MsgPack&)>(&::operator+<long, const MsgPack&>)),    "+"            },
			{ chaiscript::fun(static_cast<long (*)(const long&, const MsgPack&)>(&::operator-<long, const MsgPack&>)),    "-"            },
			{ chaiscript::fun(static_cast<long (*)(const long&, const MsgPack&)>(&::operator*<long, const MsgPack&>)),    "*"            },
			{ chaiscript::fun(static_cast<long (*)(const long&, const MsgPack&)>(&::operator/<long, const MsgPack&>)),    "/"            },

			{ chaiscript::fun(static_cast<long& (*)(long&, const MsgPack&)>(&::operator+=<long, const MsgPack&>)),        "+="           },
			{ chaiscript::fun(static_cast<long& (*)(long&, const MsgPack&)>(&::operator-=<long, const MsgPack&>)),        "-="           },
			{ chaiscript::fun(static_cast<long& (*)(long&, const MsgPack&)>(&::operator*=<long, const MsgPack&>)),        "*="           },
			{ chaiscript::fun(static_cast<long& (*)(long&, const MsgPack&)>(&::operator/=<long, const MsgPack&>)),        "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const long long&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const long long&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const long long&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const long long&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const long long&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const long long&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const long long&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const long long&>),     "/="           },

			{ chaiscript::fun(static_cast<long long (*)(const long long&, const MsgPack&)>(&::operator+<long long, const MsgPack&>)),    "+"            },
			{ chaiscript::fun(static_cast<long long (*)(const long long&, const MsgPack&)>(&::operator-<long long, const MsgPack&>)),    "-"            },
			{ chaiscript::fun(static_cast<long long (*)(const long long&, const MsgPack&)>(&::operator*<long long, const MsgPack&>)),    "*"            },
			{ chaiscript::fun(static_cast<long long (*)(const long long&, const MsgPack&)>(&::operator/<long long, const MsgPack&>)),    "/"            },

			{ chaiscript::fun(static_cast<long long& (*)(long long&, const MsgPack&)>(&::operator+=<long long, const MsgPack&>)),        "+="           },
			{ chaiscript::fun(static_cast<long long& (*)(long long&, const MsgPack&)>(&::operator-=<long long, const MsgPack&>)),        "-="           },
			{ chaiscript::fun(static_cast<long long& (*)(long long&, const MsgPack&)>(&::operator*=<long long, const MsgPack&>)),        "*="           },
			{ chaiscript::fun(static_cast<long long& (*)(long long&, const MsgPack&)>(&::operator/=<long long, const MsgPack&>)),        "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const unsigned long long&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const unsigned long long&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const unsigned long long&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const unsigned long long&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const unsigned long long&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const unsigned long long&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const unsigned long long&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const unsigned long long&>),     "/="           },

			{ chaiscript::fun(static_cast<unsigned long long (*)(const unsigned long long&, const MsgPack&)>(&::operator+<unsigned long long, const MsgPack&>)),    "+"            },
			{ chaiscript::fun(static_cast<unsigned long long (*)(const unsigned long long&, const MsgPack&)>(&::operator-<unsigned long long, const MsgPack&>)),    "-"            },
			{ chaiscript::fun(static_cast<unsigned long long (*)(const unsigned long long&, const MsgPack&)>(&::operator*<unsigned long long, const MsgPack&>)),    "*"            },
			{ chaiscript::fun(static_cast<unsigned long long (*)(const unsigned long long&, const MsgPack&)>(&::operator/<unsigned long long, const MsgPack&>)),    "/"            },

			{ chaiscript::fun(static_cast<unsigned long long& (*)(unsigned long long&, const MsgPack&)>(&::operator+=<unsigned long long, const MsgPack&>)),        "+="           },
			{ chaiscript::fun(static_cast<unsigned long long& (*)(unsigned long long&, const MsgPack&)>(&::operator-=<unsigned long long, const MsgPack&>)),        "-="           },
			{ chaiscript::fun(static_cast<unsigned long long& (*)(unsigned long long&, const MsgPack&)>(&::operator*=<unsigned long long, const MsgPack&>)),        "*="           },
			{ chaiscript::fun(static_cast<unsigned long long& (*)(unsigned long long&, const MsgPack&)>(&::operator/=<unsigned long long, const MsgPack&>)),        "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const float&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const float&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const float&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const float&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const float&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const float&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const float&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const float&>),     "/="           },

			{ chaiscript::fun(static_cast<float (*)(const float&, const MsgPack&)>(&::operator+<float, const MsgPack&>)),    "+"            },
			{ chaiscript::fun(static_cast<float (*)(const float&, const MsgPack&)>(&::operator-<float, const MsgPack&>)),    "-"            },
			{ chaiscript::fun(static_cast<float (*)(const float&, const MsgPack&)>(&::operator*<float, const MsgPack&>)),    "*"            },
			{ chaiscript::fun(static_cast<float (*)(const float&, const MsgPack&)>(&::operator/<float, const MsgPack&>)),    "/"            },

			{ chaiscript::fun(static_cast<float& (*)(float&, const MsgPack&)>(&::operator+=<float, const MsgPack&>)),        "+="           },
			{ chaiscript::fun(static_cast<float& (*)(float&, const MsgPack&)>(&::operator-=<float, const MsgPack&>)),        "-="           },
			{ chaiscript::fun(static_cast<float& (*)(float&, const MsgPack&)>(&::operator*=<float, const MsgPack&>)),        "*="           },
			{ chaiscript::fun(static_cast<float& (*)(float&, const MsgPack&)>(&::operator/=<float, const MsgPack&>)),        "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const double&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const double&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const double&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const double&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const double&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const double&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const double&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const double&>),     "/="           },

			{ chaiscript::fun(static_cast<double (*)(const double&, const MsgPack&)>(&::operator+<double, const MsgPack&>)),    "+"            },
			{ chaiscript::fun(static_cast<double (*)(const double&, const MsgPack&)>(&::operator-<double, const MsgPack&>)),    "-"            },
			{ chaiscript::fun(static_cast<double (*)(const double&, const MsgPack&)>(&::operator*<double, const MsgPack&>)),    "*"            },
			{ chaiscript::fun(static_cast<double (*)(const double&, const MsgPack&)>(&::operator/<double, const MsgPack&>)),    "/"            },

			{ chaiscript::fun(static_cast<double& (*)(double&, const MsgPack&)>(&::operator+=<double, const MsgPack&>)),        "+="           },
			{ chaiscript::fun(static_cast<double& (*)(double&, const MsgPack&)>(&::operator-=<double, const MsgPack&>)),        "-="           },
			{ chaiscript::fun(static_cast<double& (*)(double&, const MsgPack&)>(&::operator*=<double, const MsgPack&>)),        "*="           },
			{ chaiscript::fun(static_cast<double& (*)(double&, const MsgPack&)>(&::operator/=<double, const MsgPack&>)),        "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const bool&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const bool&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const bool&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const bool&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const bool&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const bool&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const bool&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const bool&>),     "/="           },

			{ chaiscript::fun(static_cast<bool (*)(const bool&, const MsgPack&)>(&::operator+<bool, const MsgPack&>)),    "+"            },
			{ chaiscript::fun(static_cast<bool (*)(const bool&, const MsgPack&)>(&::operator-<bool, const MsgPack&>)),    "-"            },
			{ chaiscript::fun(static_cast<bool (*)(const bool&, const MsgPack&)>(&::operator*<bool, const MsgPack&>)),    "*"            },
			{ chaiscript::fun(static_cast<bool (*)(const bool&, const MsgPack&)>(&::operator/<bool, const MsgPack&>)),    "/"            },

			{ chaiscript::fun(static_cast<bool& (*)(bool&, const MsgPack&)>(&::operator+=<bool, const MsgPack&>)),        "+="           },
			{ chaiscript::fun(static_cast<bool& (*)(bool&, const MsgPack&)>(&::operator-=<bool, const MsgPack&>)),        "-="           },
			{ chaiscript::fun(static_cast<bool& (*)(bool&, const MsgPack&)>(&::operator*=<bool, const MsgPack&>)),        "*="           },
			{ chaiscript::fun(static_cast<bool& (*)(bool&, const MsgPack&)>(&::operator/=<bool, const MsgPack&>)),        "/="           },

			{ chaiscript::fun(&MsgPack::operator+<const MsgPack&>),      "+"            },
			{ chaiscript::fun(&MsgPack::operator-<const MsgPack&>),      "-"            },
			{ chaiscript::fun(&MsgPack::operator*<const MsgPack&>),      "*"            },
			{ chaiscript::fun(&MsgPack::operator/<const MsgPack&>),      "/"            },

			{ chaiscript::fun(&MsgPack::operator+=<const MsgPack&>),     "+="           },
			{ chaiscript::fun(&MsgPack::operator-=<const MsgPack&>),     "-="           },
			{ chaiscript::fun(&MsgPack::operator*=<const MsgPack&>),     "*="           },
			{ chaiscript::fun(&MsgPack::operator/=<const MsgPack&>),     "/="           },

			{ chaiscript::fun(&MsgPack::lock),           "lock"         },

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

			{ chaiscript::fun(&MsgPack::append<const unsigned&>),            "append" },
			{ chaiscript::fun(&MsgPack::append<const int&>),                 "append" },
			{ chaiscript::fun(&MsgPack::append<const unsigned long&>),       "append" },
			{ chaiscript::fun(&MsgPack::append<const long&>),                "append" },
			{ chaiscript::fun(&MsgPack::append<const unsigned long long&>),  "append" },
			{ chaiscript::fun(&MsgPack::append<const long long&>),           "append" },
			{ chaiscript::fun(&MsgPack::append<const float&>),               "append" },
			{ chaiscript::fun(&MsgPack::append<const double&>),              "append" },
			{ chaiscript::fun(&MsgPack::append<const bool&>),                "append" },
			{ chaiscript::fun(&MsgPack::append<const std::string_view&>),    "append" },
			{ chaiscript::fun(&MsgPack::append<MsgPack&&>),                  "append" },
			{ chaiscript::fun(&MsgPack::append<const MsgPack&>),             "append" },

			{ chaiscript::fun<MsgPack&, MsgPack, const std::vector<std::string>&>(&MsgPack::path),              "path" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, const std::vector<std::string>&>(&MsgPack::path),  "path" },

			// Specific instantiation of the template MsgPack::put<const MsgPack&, T>.
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const unsigned&>),                                                                             "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const int&>),                                                                                  "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const unsigned long&>),                                                                        "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const long&>),                                                                                 "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const unsigned long long&>),                                                                   "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const long long&>),                                                                            "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const float&>),                                                                                "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const double&>),                                                                               "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const bool&>),                                                                                 "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const std::string_view&>),                                                                     "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, MsgPack&&>),                                                                                   "put" },
			{ chaiscript::fun(&MsgPack::put<const MsgPack&, const MsgPack&>),                                                                              "put" },
			// Specific instantiation of the template MsgPack::put<std::string_view, T>.
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const unsigned&)>(&MsgPack::put<const unsigned&>)),                      "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const int&)>(&MsgPack::put<const int&>)),                                "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const unsigned long&)>(&MsgPack::put<const unsigned long&>)),            "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const long&)>(&MsgPack::put<const long&>)),                              "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const unsigned long long&)>(&MsgPack::put<const unsigned long long&>)),  "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const long long&)>(&MsgPack::put<const long long&>)),                    "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const float&)>(&MsgPack::put<const float&>)),                            "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const double&)>(&MsgPack::put<const double&>)),                          "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const bool&)>(&MsgPack::put<const bool&>)),                              "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const std::string_view&)>(&MsgPack::put<const std::string_view&>)),      "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, MsgPack&&)>(&MsgPack::put<MsgPack&&>)),                                  "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const MsgPack&)>(&MsgPack::put<const MsgPack&>)),                        "put" },
			// Specific instantiation of the template MsgPack::put<size_t, T>.
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const unsigned&)>(&MsgPack::put<const unsigned&>)),                                "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const int&)>(&MsgPack::put<const int&>)),                                          "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const unsigned long&)>(&MsgPack::put<const unsigned long&>)),                      "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const long&)>(&MsgPack::put<const long&>)),                                        "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const unsigned long long&)>(&MsgPack::put<const unsigned long long&>)),            "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const long long&)>(&MsgPack::put<const long long&>)),                              "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const float&)>(&MsgPack::put<const float&>)),                                      "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const double&)>(&MsgPack::put<const double&>)),                                    "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const bool&)>(&MsgPack::put<const bool&>)),                                        "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const std::string_view&)>(&MsgPack::put<const std::string_view&>)),                "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, MsgPack&&)>(&MsgPack::put<MsgPack&&>)),                                            "put" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const MsgPack&)>(&MsgPack::put<const MsgPack&>)),                                  "put" },

			// Specific instantiation of the template MsgPack::add<M&&, T&&>.
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const unsigned&>),                                                                             "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const int&>),                                                                                  "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const unsigned long&>),                                                                        "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const long&>),                                                                                 "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const unsigned long long&>),                                                                   "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const long long&>),                                                                            "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const float&>),                                                                                "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const double&>),                                                                               "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const bool&>),                                                                                 "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const std::string_view&>),                                                                     "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, MsgPack&&>),                                                                                   "add" },
			{ chaiscript::fun(&MsgPack::add<const MsgPack&, const MsgPack&>),                                                                              "add" },
			// Specific instantiation of the template MsgPack::add<std::string_view, T&&>.
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const unsigned&)>(&MsgPack::add<const unsigned&>)),                      "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const int&)>(&MsgPack::add<const int&>)),                                "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const unsigned long&)>(&MsgPack::add<const unsigned long&>)),            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const long&)>(&MsgPack::add<const long&>)),                              "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const unsigned long long&)>(&MsgPack::add<const unsigned long long&>)),  "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const long long&)>(&MsgPack::add<const long long&>)),                    "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const float&)>(&MsgPack::add<const float&>)),                            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const double&)>(&MsgPack::add<const double&>)),                          "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const bool&)>(&MsgPack::add<const bool&>)),                              "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const std::string_view&)>(&MsgPack::add<const std::string_view&>)),      "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, MsgPack&&)>(&MsgPack::add<MsgPack&&>)),                                  "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(std::string_view, const MsgPack&)>(&MsgPack::add<const MsgPack&>)),                        "add" },
			// Specific instantiation of the template MsgPack::add<size_t, T&&>.
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const unsigned&)>(&MsgPack::add<const unsigned&>)),                                "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const int&)>(&MsgPack::add<const int&>)),                                          "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const unsigned long&)>(&MsgPack::add<const unsigned long&>)),                      "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const long&)>(&MsgPack::add<const long&>)),                                        "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const unsigned long long&)>(&MsgPack::add<const unsigned long long&>)),            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const long long&)>(&MsgPack::add<const long long&>)),                              "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const float&)>(&MsgPack::add<const float&>)),                                      "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const double&)>(&MsgPack::add<const double&>)),                                    "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const bool&)>(&MsgPack::add<const bool&>)),                                        "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const std::string_view&)>(&MsgPack::add<const std::string_view&>)),                "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, MsgPack&&)>(&MsgPack::add<MsgPack&&>)),                                            "add" },
			{ chaiscript::fun(static_cast<MsgPack& (MsgPack::*)(size_t, const MsgPack&)>(&MsgPack::add<const MsgPack&>)),                                  "add" },

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
}

}; // End namespace chaipp

#endif
