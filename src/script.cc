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
#include "ignore_unused.h"
#include "serialise.h"
#include "hashes.hh"        // for fnv1ah32
#include "phf.hh"           // for phf


Script::Script(const MsgPack& _obj)
	: type(Type::EMPTY),
	  sep_types({ { FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY } })
{
	switch (_obj.getType()) {
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
						process_type(val);
						break;
					case _.fhh(RESERVED_VALUE):
						process_value(val);
						break;
					case _.fhh(RESERVED_CHAI):
						process_chai(val);
						break;
					case _.fhh(RESERVED_BODY):
						process_body(val);
						break;
					case _.fhh(RESERVED_NAME):
						process_name(val);
						break;
					case _.fhh(RESERVED_PARAMS):
						process_params(val);
						break;
					case _.fhh(RESERVED_ENDPOINT):
						process_endpoint(val);
						break;
					default:
						if (sep_types[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
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
Script::process_body(const MsgPack& _body)
{
	L_CALL("Script::process_body({})", repr(_body.to_string()));

	if (!_body.is_string()) {
		THROW(ClientError, "'{}' must be string", RESERVED_BODY);
	}

	body = _body.str_view();
}


void
Script::process_name(const MsgPack& _name)
{
	L_CALL("Script::process_name({})", repr(_name.to_string()));

	if (!_name.is_string()) {
		THROW(ClientError, "'{}' must be string", RESERVED_NAME);
	}

	name = _name.str_view();
}


void
Script::process_params(const MsgPack& _params)
{
	L_CALL("Script::process_params({})", repr(_params.to_string()));

	if (!_params.is_map()) {
		THROW(ClientError, "'{}' must be an object", RESERVED_PARAMS);
	}

	params = _params;
}


void
Script::process_endpoint(const MsgPack& _endpoint)
{
	L_CALL("Script::process_endpoint({})", repr(_endpoint.to_string()));

	if (!_endpoint.is_string()) {
		THROW(ClientError, "'{}' must be string", RESERVED_ENDPOINT);
	}

	endpoint = _endpoint.str_view();
}


void
Script::process_type(const MsgPack& _type)
{
	L_CALL("Script::process_type({})", repr(_type.to_string()));

	if (!_type.is_string()) {
		THROW(ClientError, "'{}' must be string", RESERVED_TYPE);
	}

	sep_types = required_spc_t::get_types(_type.str());
}


void
Script::process_value(const MsgPack& _value)
{
	L_CALL("Script::process_value({})", repr(_value.to_string()));

	switch (_value.getType()) {
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
						process_body(val);
						break;
					case _.fhh(RESERVED_NAME):
						process_name(val);
						break;
					case _.fhh(RESERVED_PARAMS):
						process_params(val);
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


void
Script::process_chai(const MsgPack& _chai)
{
	L_CALL("Script::process_chai({})", repr(_chai.to_string()));

	process_value(_chai);
	type = Type::CHAI;
}


MsgPack
Script::process_chai(bool strict)
{
	L_CALL("Script::process_chai({})", strict);

#ifdef XAPIAND_CHAISCRIPT
	switch (sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::EMPTY:
			if (strict) {
				THROW(MissingTypeError, "Type of field '{}' is missing", RESERVED_SCRIPT);
			}
			sep_types[SPC_CONCRETE_TYPE] = FieldType::SCRIPT;
			/* FALLTHROUGH */
		case FieldType::SCRIPT: {
			if (sep_types[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
				if (!body.empty()) {
					THROW(ClientError, "For type {}, '{}' should not be defined", Serialise::type(FieldType::FOREIGN), RESERVED_BODY);
				}
				if (!name.empty()) {
					THROW(ClientError, "For type {}, '{}' should not be defined", Serialise::type(FieldType::FOREIGN), RESERVED_NAME);
				}
				if (!value.empty() && !endpoint.empty()) {
					THROW(ClientError, "Script already specified value in '{}' and '{}'", RESERVED_ENDPOINT);
				}
				auto& script_endpoint = endpoint.empty() ? value : endpoint;
				if (script_endpoint.empty()) {
					THROW(ClientError, "Script must specify '{}'", RESERVED_ENDPOINT);
				}
				return MsgPack({
					{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
					{ RESERVED_ENDPOINT,  script_endpoint },
					{ RESERVED_PARAMS,    params },
				});
			} else {
				if (!endpoint.empty()) {
					THROW(ClientError, "'{}' must exist only for type {}", RESERVED_ENDPOINT, Serialise::type(FieldType::FOREIGN));
				}
				if (!value.empty() && !body.empty() && !name.empty()) {
					THROW(ClientError, "Script already specified value in '{}' and '{}'", RESERVED_NAME, RESERVED_BODY);
				}
				auto& script_name = name.empty() ? value : name;
				auto& script_body = body.empty() ? value : body;
				if (script_name.empty() && script_body.empty()) {
					THROW(ClientError, "Script must specify '{}' or '{}'", RESERVED_NAME, RESERVED_BODY);
				}
				if (script_name.empty()) {
					MD5 md5;
					script_name = md5(script_body);
				}
				try {
					chaipp::Processor::compile(script_name, script_body);
					return MsgPack({
						{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
						{ RESERVED_CHAI, {
							{ RESERVED_NAME,      script_name },
							{ RESERVED_BODY,      script_body },
							{ RESERVED_PARAMS,    params },
						}}
					});
				} catch (...) {
					THROW(ScriptNotFoundError, "Script not found: {}", repr(script_name));
				}
			}
		}
		default:
			THROW(ClientError, "Only type {} is allowed in '{}'", Serialise::type(FieldType::SCRIPT), RESERVED_SCRIPT);
	}
#else
	ignore_unused(strict);
	THROW(ClientError, "Script type 'chai' (ChaiScript) not available.");
#endif
}


MsgPack
Script::process_script(bool strict)
{
	L_CALL("Script::process_script({})", strict);

	switch (type) {
		case Type::CHAI:
		case Type::EMPTY:
#ifdef XAPIAND_CHAISCRIPT
			try {
				return process_chai(strict);
			} catch (const ScriptNotFoundError& er) {
				THROW(ClientError, "{}", er.what());
			}
#endif
		default:
			THROW(ClientError, "Type {} is not allowed in '{}'", Serialise::type(sep_types[SPC_CONCRETE_TYPE]), RESERVED_SCRIPT);
	}
}

#endif
