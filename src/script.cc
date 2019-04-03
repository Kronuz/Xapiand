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

#include "script.h"


#ifdef XAPIAND_CHAISCRIPT

#include "chaipp/chaipp.h"
#include "hash/md5.h"
#include "reserved/schema.h"
#include "serialise.h"
#include "hashes.hh"        // for fnv1ah32
#include "phf.hh"           // for phf


Script::Script(const MsgPack& _obj)
	: type(Type::EMPTY),
	  _sep_types({ { FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY } })
{
	switch (_obj.get_type()) {
		case MsgPack::Type::STR: {
			body = _obj.str_view();
			break;
		}
		case MsgPack::Type::MAP: {
			const auto it_e = _obj.end();
			for (auto it = _obj.begin(); it != it_e; ++it) {
				const auto str_key = it->str_view();
				auto& val = it.value();
				constexpr static auto _ = phf::make_phf({
					hh(RESERVED_TYPE),
					hh(RESERVED_VALUE),
					hh(RESERVED_CHAI),
					hh(RESERVED_BODY),
					hh(RESERVED_NAME),
					hh(RESERVED_PARAMS),
					hh(RESERVED_ENDPOINT),
				});
				switch (_.fhh(str_key)) {
					case _.fhh(RESERVED_TYPE):
						if (!val.is_string()) {
							THROW(ClientError, "'{}' must be string", RESERVED_TYPE);
						}
						_sep_types = required_spc_t::get_types(val.str());
						break;
					case _.fhh(RESERVED_VALUE):
						process_value(val);
						break;
					case _.fhh(RESERVED_CHAI):
						type = Type::CHAI;
						process_value(val);
						break;
					case _.fhh(RESERVED_BODY):
						if (!val.is_string()) {
							THROW(ClientError, "'{}' must be string", RESERVED_BODY);
						}
						body = val.str_view();
						break;
					case _.fhh(RESERVED_NAME):
						if (!val.is_string()) {
							THROW(ClientError, "'{}' must be string", RESERVED_NAME);
						}
						name = val.str_view();
						break;
					case _.fhh(RESERVED_PARAMS):
						if (!val.is_map() && !val.is_undefined()) {
							THROW(ClientError, "'{}' must be an object", RESERVED_PARAMS);
						}
						params = val;
						break;
					case _.fhh(RESERVED_ENDPOINT):
						if (!val.is_string()) {
							THROW(ClientError, "'{}' must be string", RESERVED_ENDPOINT);
						}
						endpoint = val.str_view();
						break;
					default:
						if (_sep_types[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
							THROW(ClientError, "{} in '{}' must be one of {}, {} or {}", repr(str_key), RESERVED_VALUE, RESERVED_TYPE, RESERVED_ENDPOINT, RESERVED_PARAMS);
						} else {
							THROW(ClientError, "{} in '{}' must be one of {}, {} or {}", repr(str_key), RESERVED_VALUE, RESERVED_TYPE, RESERVED_CHAI, RESERVED_PARAMS);
						}
				}
			}
			break;
		}
		default:
			THROW(ClientError, "'{}' must be string or a valid script object", RESERVED_SCRIPT);
	}
}


void
Script::process_value(const MsgPack& _value)
{
	L_CALL("Script::process_value({})", repr(_value.to_string()));

	switch (_value.get_type()) {
		case MsgPack::Type::STR:
			value = _value.str_view();
			break;
		case MsgPack::Type::MAP: {
			const auto it_e = _value.end();
			for (auto it = _value.begin(); it != it_e; ++it) {
				const auto str_key = it->str_view();
				auto& val = it.value();
				constexpr static auto _ = phf::make_phf({
					hh(RESERVED_BODY),
					hh(RESERVED_NAME),
					hh(RESERVED_PARAMS),
				});
				switch (_.fhh(str_key)) {
					case _.fhh(RESERVED_BODY):
						if (!val.is_string()) {
							THROW(ClientError, "'{}' must be string", RESERVED_BODY);
						}
						body = val.str_view();
						break;
					case _.fhh(RESERVED_NAME):
						if (!val.is_string()) {
							THROW(ClientError, "'{}' must be string", RESERVED_NAME);
						}
						name = val.str_view();
						break;
					case _.fhh(RESERVED_PARAMS):
						if (!val.is_map() && !val.is_undefined()) {
							THROW(ClientError, "'{}' must be an object", RESERVED_PARAMS);
						}
						params = val;
						break;
					default:
						THROW(ClientError, "{} in '{}' must be one of {}, {} or {}", repr(str_key), RESERVED_VALUE, RESERVED_BODY, RESERVED_NAME, RESERVED_PARAMS);
				}
			}
			break;
		}
		default:
			THROW(ClientError, "'{}' must be string or a valid object", RESERVED_VALUE);
	}
}


const std::array<FieldType, SPC_TOTAL_TYPES>&
Script::get_types(bool strict) const
{
	if (_sep_types[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
		if (!body.empty()) {
			THROW(ClientError, "For type {}, '{}' should not be defined", Serialise::type(FieldType::FOREIGN), RESERVED_BODY);
		}
		if (!name.empty()) {
			THROW(ClientError, "For type {}, '{}' should not be defined", Serialise::type(FieldType::FOREIGN), RESERVED_NAME);
		}
		if (!value.empty() && !endpoint.empty()) {
			THROW(ClientError, "Script already specified value in '{}' and '{}'", RESERVED_ENDPOINT);
		}
	} else {
		if (strict) {
			THROW(MissingTypeError, "Type of field '{}' is missing", RESERVED_TYPE);
		}
		_sep_types[SPC_CONCRETE_TYPE] = FieldType::SCRIPT;
		if (_sep_types[SPC_CONCRETE_TYPE] != FieldType::SCRIPT) {
			THROW(ClientError, "Only type {} is allowed in '{}'", Serialise::type(FieldType::SCRIPT), RESERVED_SCRIPT);
		}
		if (!endpoint.empty()) {
			THROW(ClientError, "'{}' must exist only for type {}", RESERVED_ENDPOINT, Serialise::type(FieldType::FOREIGN));
		}
		if (!value.empty() && !body.empty() && !name.empty()) {
			THROW(ClientError, "Script already specified value in '{}' and '{}'", RESERVED_NAME, RESERVED_BODY);
		}
	}

	return _sep_types;
}


std::string_view
Script::get_endpoint() const
{
	auto script_endpoint = endpoint.empty() ? value : endpoint;
	if (script_endpoint.empty()) {
		THROW(ClientError, "Script must specify '{}'", RESERVED_ENDPOINT);
	}
	return script_endpoint;
}


std::pair<std::string_view, std::string_view>
Script::get_name_body() const
{
	auto script_name = name.empty() ? value : name;
	auto script_body = body.empty() ? value : body;
	if (script_name.empty() && script_body.empty()) {
		THROW(ClientError, "Script must specify '{}' or '{}'", RESERVED_NAME, RESERVED_BODY);
	}
	if (script_name.empty()) {
		if (name_holder.empty()) {
			MD5 md5;
			name_holder = md5(script_body);
		}
		script_name = name_holder;
	}
	return { script_name, script_body };
}


const MsgPack&
Script::get_params() const
{
	return params;
}


MsgPack
Script::process_script([[maybe_unused]] bool strict) const
{
	L_CALL("Script::process_script({})", strict);

#ifdef XAPIAND_CHAISCRIPT
	auto sep_types = get_types(strict);

	if (sep_types[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
		chaipp::Processor::compile(*this);
		MsgPack script_data({
			{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
			{ RESERVED_ENDPOINT,  get_endpoint() },
		});
		auto& script_params = get_params();
		if (!script_params.empty()) {
			script_data[RESERVED_PARAMS] = script_params;
		}
		return script_data;
	} else {
		auto name_body = get_name_body();
		chaipp::Processor::compile(*this);
		MsgPack script_data({
			{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
			{ RESERVED_CHAI, {
				{ RESERVED_NAME,      name_body.first },
				{ RESERVED_BODY,      name_body.second },
			}}
		});
		auto& script_params = get_params();
		if (!script_params.empty()) {
			script_data[RESERVED_PARAMS] = script_params;
		}
		return script_data;
	}
#else
	THROW(ClientError, "Script type 'chai' (ChaiScript) not available.");
#endif
}

#endif
