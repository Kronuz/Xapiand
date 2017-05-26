/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#if XAPIAND_CHAISCRIPT

#include <chaiscript/chaiscript.hpp>

#include "msgpack.h"


namespace chaipp {

inline static chaiscript::ModulePtr ModuleMsgPack() {
	chaiscript::ModulePtr module(new chaiscript::Module());

	module->add(chaiscript::type_conversion<const MsgPack, bool>([](const MsgPack& obj) { return obj.as_boolean(); }));
	module->add(chaiscript::type_conversion<const MsgPack, unsigned>([](const MsgPack& obj) { return obj.as_u64(); }));
	module->add(chaiscript::type_conversion<const MsgPack, int>([](const MsgPack& obj) { return obj.as_i64(); }));
	module->add(chaiscript::type_conversion<const MsgPack, unsigned long>([](const MsgPack& obj) { return obj.as_u64(); }));
	module->add(chaiscript::type_conversion<const MsgPack, long>([](const MsgPack& obj) { return obj.as_i64(); }));
	module->add(chaiscript::type_conversion<const MsgPack, unsigned long long>([](const MsgPack& obj) { return obj.as_u64(); }));
	module->add(chaiscript::type_conversion<const MsgPack, long long>([](const MsgPack& obj) { return obj.as_i64(); }));
	module->add(chaiscript::type_conversion<const MsgPack, float>([](const MsgPack& obj) { return obj.as_f64(); }));
	module->add(chaiscript::type_conversion<const MsgPack, double>([](const MsgPack& obj) { return obj.as_f64(); }));
	module->add(chaiscript::type_conversion<const MsgPack, std::string>([](const MsgPack& obj) { return obj.as_str(); }));

	module->add(chaiscript::type_conversion<unsigned, size_t>([](const unsigned& orig) { return static_cast<size_t>(orig); }));
	module->add(chaiscript::type_conversion<int, size_t>([](const int& orig) { return static_cast<size_t>(orig); }));
	module->add(chaiscript::type_conversion<unsigned long, size_t>([](const unsigned long& orig) { return static_cast<size_t>(orig); }));
	module->add(chaiscript::type_conversion<long, size_t>([](const long& orig) { return static_cast<size_t>(orig); }));

	chaiscript::utility::add_class<MsgPack>(
		*module,
		"MsgPack",
		{
			chaiscript::constructor<MsgPack()>(),
			chaiscript::constructor<MsgPack(MsgPack&&)>(),
			chaiscript::constructor<MsgPack(const MsgPack&)>(),
			// Specific instantiation of the template constructor.
			chaiscript::constructor<MsgPack(unsigned)>(),
			chaiscript::constructor<MsgPack(int)>(),
			chaiscript::constructor<MsgPack(unsigned long)>(),
			chaiscript::constructor<MsgPack(long)>(),
			chaiscript::constructor<MsgPack(unsigned long long)>(),
			chaiscript::constructor<MsgPack(long long)>(),
			chaiscript::constructor<MsgPack(float)>(),
			chaiscript::constructor<MsgPack(double)>(),
			chaiscript::constructor<MsgPack(bool)>(),
			chaiscript::constructor<MsgPack(std::string&&)>(),
			chaiscript::constructor<MsgPack(std::string&)>(),
			chaiscript::constructor<MsgPack(const std::string&)>(),
			chaiscript::constructor<MsgPack(std::vector<MsgPack>)>(),
			chaiscript::constructor<MsgPack(std::vector<MsgPack>&)>(),
			chaiscript::constructor<MsgPack(const std::vector<MsgPack>&)>(),
			chaiscript::constructor<MsgPack(const std::map<std::string, MsgPack>&)>(),
			chaiscript::constructor<MsgPack(const chaiscript::Boxed_Value&)>(),
			chaiscript::constructor<MsgPack(const std::vector<chaiscript::Boxed_Value>&)>(),
			chaiscript::constructor<MsgPack(const std::map<std::string, chaiscript::Boxed_Value>&)>(),
		},
		{
			// Specific instantiation of the template MsgPack::operator[](M&&).
			{ chaiscript::fun<MsgPack&, MsgPack, MsgPack&&>(&MsgPack::operator[]<MsgPack>),                          "[]" },
			{ chaiscript::fun<MsgPack&, MsgPack, MsgPack&>(&MsgPack::operator[]<MsgPack&>),                          "[]" },
			{ chaiscript::fun<MsgPack&, MsgPack, const MsgPack&>(&MsgPack::operator[]<const MsgPack&>),              "[]" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, MsgPack&&>(&MsgPack::operator[]<MsgPack>),              "[]" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, MsgPack&>(&MsgPack::operator[]<MsgPack&>),              "[]" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, const MsgPack&>(&MsgPack::operator[]<const MsgPack&>),  "[]" },
			// Specific instantiation of the template MsgPack::operator[](const std::string&).
			{ chaiscript::fun<MsgPack&, MsgPack, const std::string&>(&MsgPack::operator[]),                          "[]" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, const std::string&>(&MsgPack::operator[]),              "[]" },
			// Specific instantiation of the template MsgPack::operator[](size_t).
			{ chaiscript::fun<MsgPack&, MsgPack, size_t>(&MsgPack::operator[]),                                      "[]" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, size_t>(&MsgPack::operator[]),                          "[]" },

			// Specific instantiation of the template MsgPack::at(M&&).
			{ chaiscript::fun<MsgPack&, MsgPack, MsgPack&&>(&MsgPack::at<MsgPack>),                          "at" },
			{ chaiscript::fun<MsgPack&, MsgPack, MsgPack&>(&MsgPack::at<MsgPack&>),                          "at" },
			{ chaiscript::fun<MsgPack&, MsgPack, const MsgPack&>(&MsgPack::at<const MsgPack&>),              "at" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, MsgPack&&>(&MsgPack::at<MsgPack>),              "at" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, MsgPack&>(&MsgPack::at<MsgPack&>),              "at" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, const MsgPack&>(&MsgPack::at<const MsgPack&>),  "at" },
			// Specific instantiation of the template MsgPack::at(const std::string&).
			{ chaiscript::fun<MsgPack&, MsgPack, const std::string&>(&MsgPack::at),                          "at" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, const std::string&>(&MsgPack::at),              "at" },
			// Specific instantiation of the template MsgPack::at(size_t).
			{ chaiscript::fun<MsgPack&, MsgPack, size_t>(&MsgPack::at),                                      "at" },
			{ chaiscript::fun<const MsgPack&, const MsgPack, size_t>(&MsgPack::at),                          "at" },

			// Specific instantiation of the template MsgPack::find(M&&).
			{ chaiscript::fun<MsgPack::iterator, MsgPack, MsgPack&&>(&MsgPack::find<MsgPack>),                          "find" },
			{ chaiscript::fun<MsgPack::iterator, MsgPack, MsgPack&>(&MsgPack::find<MsgPack&>),                          "find" },
			{ chaiscript::fun<MsgPack::iterator, MsgPack, const MsgPack&>(&MsgPack::find<const MsgPack&>),              "find" },
			{ chaiscript::fun<MsgPack::const_iterator, const MsgPack, MsgPack&&>(&MsgPack::find<MsgPack>),              "find" },
			{ chaiscript::fun<MsgPack::const_iterator, const MsgPack, MsgPack&>(&MsgPack::find<MsgPack&>),              "find" },
			{ chaiscript::fun<MsgPack::const_iterator, const MsgPack, const MsgPack&>(&MsgPack::find<const MsgPack&>),  "find" },
			// Specific instantiation of the template MsgPack::find(const std::string&).
			{ chaiscript::fun<MsgPack::iterator, MsgPack, const std::string&>(&MsgPack::find),                          "find" },
			{ chaiscript::fun<MsgPack::const_iterator, const MsgPack, const std::string&>(&MsgPack::find),              "find" },
			// Specific instantiation of the template MsgPack::find(size_t).
			{ chaiscript::fun<MsgPack::iterator, MsgPack, size_t>(&MsgPack::find),                                      "find" },
			{ chaiscript::fun<MsgPack::const_iterator, const MsgPack, size_t>(&MsgPack::find),                          "find" },

			// Specific instantiation of the template MsgPack::update.
			{ chaiscript::fun(&MsgPack::update<MsgPack>),            "update" },
			{ chaiscript::fun(&MsgPack::update<MsgPack&>),           "update" },
			{ chaiscript::fun(&MsgPack::update<const MsgPack&>),     "update" },

			// Specific instantiation of the template MsgPack::count.
			{ chaiscript::fun(&MsgPack::count<MsgPack>),             "count" },
			{ chaiscript::fun(&MsgPack::count<MsgPack&>),            "count" },
			{ chaiscript::fun(&MsgPack::count<const MsgPack&>),      "count" },
			{ chaiscript::fun(&MsgPack::count<std::string>),         "count" },
			{ chaiscript::fun(&MsgPack::count<std::string&>),        "count" },
			{ chaiscript::fun(&MsgPack::count<const std::string&>),  "count" },
			{ chaiscript::fun(&MsgPack::count<size_t>),              "count" },
			{ chaiscript::fun(&MsgPack::count<size_t&>),             "count" },
			{ chaiscript::fun(&MsgPack::count<const size_t&>),       "count" },

			// Specific instantiation of the template MsgPack::erase(M&&).
			{ chaiscript::fun<size_t, MsgPack, MsgPack&&>(&MsgPack::erase<MsgPack>),              "erase" },
			{ chaiscript::fun<size_t, MsgPack, MsgPack&>(&MsgPack::erase<MsgPack&>),              "erase" },
			{ chaiscript::fun<size_t, MsgPack, const MsgPack&>(&MsgPack::erase<const MsgPack&>),  "erase" },
			// Specific instantiation of the template MsgPack::erase(const std::string&).
			{ chaiscript::fun<size_t, MsgPack, const std::string&>(&MsgPack::erase),              "erase" },
			// Specific instantiation of the template MsgPack::erase(size_t).
			{ chaiscript::fun<size_t, MsgPack, size_t>(&MsgPack::erase),                          "erase" },
			// Specific instantiation of the template MsgPack::erase(const MsgPack::iterator&).
			{ chaiscript::fun<MsgPack::iterator, MsgPack, MsgPack::iterator>(&MsgPack::erase),    "erase" },

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
			{ chaiscript::fun(&MsgPack::as_document),   "as_document"   },

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
			{ chaiscript::fun(&MsgPack::operator+),      "+"            },
			{ chaiscript::fun(&MsgPack::operator+=),     "+="           },
			{ chaiscript::fun(&MsgPack::operator<<),     "<<"           },

			{ chaiscript::fun(&MsgPack::lock),           "lock"         },

			{ chaiscript::fun(&MsgPack::unformatted_string),           "unformatted_string" },
			{ chaiscript::fun(&MsgPack::to_string),                    "to_string"          },
			{ chaiscript::fun(&MsgPack::serialise<msgpack::sbuffer>),  "serialise"          },

			{ chaiscript::fun<MsgPack&, MsgPack, MsgPack&&>(&MsgPack::operator=),           "=" },
			{ chaiscript::fun<MsgPack&, MsgPack, const MsgPack&>(&MsgPack::operator=),      "=" },
			// Specific instantiation of the template assigment operator.
			{ chaiscript::fun(&MsgPack::operator=<const unsigned&>),                        "=" },
			{ chaiscript::fun(&MsgPack::operator=<const int&>),                             "=" },
			{ chaiscript::fun(&MsgPack::operator=<const unsigned long&>),                   "=" },
			{ chaiscript::fun(&MsgPack::operator=<const long&>),                            "=" },
			{ chaiscript::fun(&MsgPack::operator=<const unsigned long long&>),              "=" },
			{ chaiscript::fun(&MsgPack::operator=<const long long&>),                       "=" },
			{ chaiscript::fun(&MsgPack::operator=<const float&>),                           "=" },
			{ chaiscript::fun(&MsgPack::operator=<const double&>),                          "=" },
			{ chaiscript::fun(&MsgPack::operator=<const bool&>),                            "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::string>),                            "=" },
			{ chaiscript::fun(&MsgPack::operator=<std::string&>),                           "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::string&>),                     "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::vector<MsgPack>&>),            "=" },
			{ chaiscript::fun(&MsgPack::operator=<const std::map<std::string, MsgPack>&>),  "=" },

			{ chaiscript::fun(&MsgPack::push_back<unsigned>),            "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<int>),                 "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<unsigned long>),       "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<long>),                "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<unsigned long long>),  "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<long long>),           "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<float>),               "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<double>),              "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<bool>),                "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<std::string>),         "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<std::string&>),        "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<const std::string&>),  "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<MsgPack>),             "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<MsgPack&>),            "push_back" },
			{ chaiscript::fun(&MsgPack::push_back<const MsgPack&>),      "push_back" },

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
			// Specific instantiation of the template MsgPack::put<cons std::string&, T>.
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, unsigned&&)>(&MsgPack::put<unsigned>)),                      "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, int&&)>(&MsgPack::put<int>)),                                "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, unsigned long&&)>(&MsgPack::put<unsigned long>)),            "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, long&&)>(&MsgPack::put<long>)),                              "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, unsigned long long&&)>(&MsgPack::put<unsigned long long>)),  "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, long long&&)>(&MsgPack::put<long long>)),                    "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, float&&)>(&MsgPack::put<float>)),                            "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, double&&)>(&MsgPack::put<double>)),                          "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, bool&&)>(&MsgPack::put<bool>)),                              "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, std::string&&)>(&MsgPack::put<std::string>)),                "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, std::string&)>(&MsgPack::put<std::string&>)),                "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, const std::string&)>(&MsgPack::put<const std::string&>)),    "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, MsgPack&&)>(&MsgPack::put<MsgPack>)),                        "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, MsgPack&)>(&MsgPack::put<MsgPack&>)),                        "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(const std::string&, const MsgPack&)>(&MsgPack::put<const MsgPack&>)),            "put" },
			// Specific instantiation of the template MsgPack::put<size_t, T>.
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, unsigned&&)>(&MsgPack::put<unsigned>)),                                  "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, int&&)>(&MsgPack::put<int>)),                                            "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, unsigned long&&)>(&MsgPack::put<unsigned long>)),                        "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, long&&)>(&MsgPack::put<long>)),                                          "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, unsigned long long&&)>(&MsgPack::put<unsigned long long>)),              "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, long long&&)>(&MsgPack::put<long long>)),                                "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, float&&)>(&MsgPack::put<float>)),                                        "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, double&&)>(&MsgPack::put<double>)),                                      "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, bool&&)>(&MsgPack::put<bool>)),                                          "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, std::string&&)>(&MsgPack::put<std::string>)),                            "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, std::string&)>(&MsgPack::put<std::string&>)),                            "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, const std::string&)>(&MsgPack::put<const std::string&>)),                "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, MsgPack&&)>(&MsgPack::put<MsgPack>)),                                    "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, MsgPack&)>(&MsgPack::put<MsgPack&>)),                                    "put" },
			{ chaiscript::fun(static_cast<MsgPack::iterator (MsgPack::*)(size_t, const MsgPack&)>(&MsgPack::put<const MsgPack&>)),                        "put" },

			// Specific instantiation of the template MsgPack::insert<M&&, T&&>.
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, unsigned&&)>(&MsgPack::insert<const MsgPack&, unsigned>)),                      "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, int&&)>(&MsgPack::insert<const MsgPack&, int>)),                                "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, unsigned long&&)>(&MsgPack::insert<const MsgPack&, unsigned long>)),            "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, long&&)>(&MsgPack::insert<const MsgPack&, long>)),                              "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, unsigned long long&&)>(&MsgPack::insert<const MsgPack&, unsigned long long>)),  "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, long long&&)>(&MsgPack::insert<const MsgPack&, long long>)),                    "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, float&&)>(&MsgPack::insert<const MsgPack&, float>)),                            "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, double&&)>(&MsgPack::insert<const MsgPack&, double>)),                          "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, bool&&)>(&MsgPack::insert<const MsgPack&, bool>)),                              "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, std::string&&)>(&MsgPack::insert<const MsgPack&, std::string>)),                "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, std::string&)>(&MsgPack::insert<const MsgPack&, std::string&>)),                "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, const std::string&)>(&MsgPack::insert<const MsgPack&, const std::string&>)),    "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, MsgPack&&)>(&MsgPack::insert<const MsgPack&, MsgPack>)),                        "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, MsgPack&)>(&MsgPack::insert<const MsgPack&, MsgPack&>)),                        "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const MsgPack&, const MsgPack&)>(&MsgPack::insert<const MsgPack&, const MsgPack&>)),            "insert" },
			// Specific instantiation of the template MsgPack::insert<cons std::string&, T&&>.
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, unsigned&&)>(&MsgPack::insert<unsigned>)),                                  "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, int&&)>(&MsgPack::insert<int>)),                                            "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, unsigned long&&)>(&MsgPack::insert<unsigned long>)),                        "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, long&&)>(&MsgPack::insert<long>)),                                          "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, unsigned long long&&)>(&MsgPack::insert<unsigned long long>)),              "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, long long&&)>(&MsgPack::insert<long long>)),                                "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, float&&)>(&MsgPack::insert<float>)),                                        "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, double&&)>(&MsgPack::insert<double>)),                                      "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, bool&&)>(&MsgPack::insert<bool>)),                                          "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, std::string&&)>(&MsgPack::insert<std::string>)),                            "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, std::string&)>(&MsgPack::insert<std::string&>)),                            "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, const std::string&)>(&MsgPack::insert<const std::string&>)),                "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, MsgPack&&)>(&MsgPack::insert<MsgPack>)),                                    "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, MsgPack&)>(&MsgPack::insert<MsgPack&>)),                                    "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(const std::string&, const MsgPack&)>(&MsgPack::insert<const MsgPack&>)),                        "insert" },
			// Specific instantiation of the template MsgPack::insert<size_t, T&&>.
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, unsigned&&)>(&MsgPack::insert<unsigned>)),                                              "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, int&&)>(&MsgPack::insert<int>)),                                                        "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, unsigned long&&)>(&MsgPack::insert<unsigned long>)),                                    "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, long&&)>(&MsgPack::insert<long>)),                                                      "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, unsigned long long&&)>(&MsgPack::insert<unsigned long long>)),                          "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, long long&&)>(&MsgPack::insert<long long>)),                                            "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, float&&)>(&MsgPack::insert<float>)),                                                    "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, double&&)>(&MsgPack::insert<double>)),                                                  "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, bool&&)>(&MsgPack::insert<bool>)),                                                      "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, std::string&&)>(&MsgPack::insert<std::string>)),                                        "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, std::string&)>(&MsgPack::insert<std::string&>)),                                        "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, const std::string&)>(&MsgPack::insert<const std::string&>)),                            "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, MsgPack&&)>(&MsgPack::insert<MsgPack>)),                                                "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, MsgPack&)>(&MsgPack::insert<MsgPack&>)),                                                "insert" },
			{ chaiscript::fun(static_cast<std::pair<MsgPack::iterator, bool> (MsgPack::*)(size_t, const MsgPack&)>(&MsgPack::insert<const MsgPack&>)),                                    "insert" },

			// Overload operator +
			{ chaiscript::fun([](const MsgPack& obj, unsigned value) { return obj.as_i64() + value; }),                     "+" },
			{ chaiscript::fun([](const MsgPack& obj, int value) { return obj.as_i64() + value; }),                          "+" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long value) { return obj.as_i64() + value; }),                "+" },
			{ chaiscript::fun([](const MsgPack& obj, long value) { return obj.as_i64() + value; }),                         "+" },
			{ chaiscript::fun([](const MsgPack& obj, unsigned long long value) { return obj.as_i64() + value; }),           "+" },
			{ chaiscript::fun([](const MsgPack& obj, long long value) { return obj.as_i64() + value; }),                    "+" },
			{ chaiscript::fun([](const MsgPack& obj, float value) { return obj.as_f64() + value; }),                        "+" },
			{ chaiscript::fun([](const MsgPack& obj, double value) { return obj.as_f64() + value; }),                       "+" },
			{ chaiscript::fun([](const MsgPack& obj, const std::string& value) { return obj.as_str().append(value); }),     "+" },

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
