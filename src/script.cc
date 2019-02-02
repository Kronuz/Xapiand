/*
 * Copyright (c) 2015-2018 Dubalu LLC
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


#if defined(XAPIAND_CHAISCRIPT) || defined(XAPIAND_V8)

#include "chaipp/chaipp.h"
#include "ignore_unused.h"
#include "serialise.h"
#include "v8pp/v8pp.h"
#include "hashes.hh"        // for fnv1ah32
#include "phf.hh"           // for phf


static const auto str_set_dispatch_script(string::join<std::string>({
	RESERVED_TYPE,
	RESERVED_VALUE,
	RESERVED_CHAI,
	RESERVED_ECMA,
	RESERVED_BODY,
	RESERVED_NAME,
}, ",", " or "));


static const auto str_set_dispatch_value(string::join<std::string>({
	RESERVED_BODY,
	RESERVED_NAME,
}, ",", " or "));


Script::Script(const MsgPack& _obj)
	: type(Type::EMPTY),
	  with_value(false),
	  with_data(false),
	  sep_types({ { FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY, FieldType::EMPTY } })
{
	switch (_obj.getType()) {
		case MsgPack::Type::STR: {
			body = _obj.str();
			break;
		}
		case MsgPack::Type::MAP: {
			const auto it_e = _obj.end();
			for (auto it = _obj.begin(); it != it_e; ++it) {
				const auto str_key = it->str_view();
				auto& value = it.value();
				constexpr static auto _ = phf::make_phf({
					hh(RESERVED_TYPE),
					hh(RESERVED_VALUE),
					hh(RESERVED_CHAI),
					hh(RESERVED_ECMA),
					hh(RESERVED_BODY),
					hh(RESERVED_NAME),
				});
				switch (_.fhh(str_key)) {
					case _.fhh(RESERVED_TYPE):
						process_type(value);
						break;
					case _.fhh(RESERVED_VALUE):
						process_value(value);
						break;
					case _.fhh(RESERVED_CHAI):
						process_chai(value);
						break;
					case _.fhh(RESERVED_ECMA):
						process_ecma(value);
						break;
					case _.fhh(RESERVED_BODY):
						process_body(value);
						break;
					case _.fhh(RESERVED_NAME):
						process_name(value);
						break;
					default:
						THROW(ClientError, "%s in %s is not valid, only can use %s", repr(str_key), RESERVED_SCRIPT, str_set_dispatch_script);
				}
			}
			if (body.empty()) {
				THROW(ClientError, "%s must be defined", RESERVED_BODY);
			}
			break;
		}
		default:
			THROW(ClientError, "%s must be string or a valid script object", RESERVED_SCRIPT);
	}
}


void
Script::process_body(const MsgPack& _body)
{
	L_CALL("Script::process_body(%s)", repr(_body.to_string()));

	if (with_value) {
		THROW(ClientError, "%s is ill-formed", RESERVED_SCRIPT);
	}

	try {
		body = _body.str();
		with_data = true;
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "%s must be string", RESERVED_BODY);
	}
}


void
Script::process_name(const MsgPack& _name)
{
	L_CALL("Script::process_name(%s)", repr(_name.to_string()));

	if (with_value) {
		THROW(ClientError, "%s is ill-formed", RESERVED_SCRIPT);
	}

	try {
		name = _name.str();
		with_data = true;
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "%s must be string", RESERVED_NAME);
	}
}


void
Script::process_type(const MsgPack& _type)
{
	L_CALL("Script::process_type(%s)", repr(_type.to_string()));

	try {
		sep_types = required_spc_t::get_types(_type.str());
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "%s must be string", RESERVED_TYPE);
	}
}


void
Script::process_value(const MsgPack& _value)
{
	L_CALL("Script::process_value(%s)", repr(_value.to_string()));

	if (with_data || with_value) {
		THROW(ClientError, "%s is ill-formed", RESERVED_SCRIPT);
	}

	switch (_value.getType()) {
		case MsgPack::Type::STR:
			body = _value.str();
			break;
		case MsgPack::Type::MAP: {
			const auto it_e = _value.end();
			for (auto it = _value.begin(); it != it_e; ++it) {
				const auto str_key = it->str_view();
				auto& value = it.value();
				constexpr static auto _ = phf::make_phf({
					hh(RESERVED_BODY),
					hh(RESERVED_NAME),
				});
				switch (_.fhh(str_key)) {
					case _.fhh(RESERVED_BODY):
						process_body(value);
						break;
					case _.fhh(RESERVED_NAME):
						process_name(value);
						break;
					default:
						THROW(ClientError, "%s in %s is not valid, only can use %s", repr(str_key), RESERVED_VALUE, str_set_dispatch_value);
				}
			}
			if (body.empty()) {
				THROW(ClientError, "%s must be defined in %s", RESERVED_BODY, RESERVED_VALUE);
			}
			break;
		}
		default:
			THROW(ClientError, "%s must be string or a valid object", RESERVED_VALUE);
	}
	with_value = true;
}


void
Script::process_chai(const MsgPack& _chai)
{
	L_CALL("Script::process_chai(%s)", repr(_chai.to_string()));

	process_value(_chai);
	type = Type::CHAI;
}


void
Script::process_ecma(const MsgPack& _ecma)
{
	L_CALL("Script::process_ecma(%s)", repr(_ecma.to_string()));

	process_value(_ecma);
	type = Type::ECMA;
}


MsgPack
Script::process_chai(bool strict)
{
	L_CALL("Script::process_chai(%s)", strict ? "true" : "false");

#if defined(XAPIAND_CHAISCRIPT)
	switch (sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::EMPTY:
			if (strict) {
				THROW(MissingTypeError, "Type of field %s is missing", RESERVED_SCRIPT);
			}
			sep_types[SPC_CONCRETE_TYPE] = FieldType::SCRIPT;
			/* FALLTHROUGH */
		case FieldType::SCRIPT: {
			if (sep_types[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
				if (name.empty()) {
					return MsgPack({
						{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
						{ RESERVED_CHAI, body      },
					});
				}
				THROW(ClientError, "For type %s, %s must be string", Serialise::type(FieldType::FOREIGN), RESERVED_VALUE);
			} else if (name.empty()) {
				auto body_hash = chaipp::hash(body);
				try {
					chaipp::Processor::compile(body_hash, body_hash, body);
					return MsgPack({
						{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
						{ RESERVED_CHAI, {
							{ RESERVED_HASH,      body_hash },
							{ RESERVED_BODY_HASH, body_hash },
							{ RESERVED_BODY,      body      },
						}}
					});
				} catch (...) {
					THROW(ScriptNotFoundError, "Script not found: %s", repr(body));
				}
			} else {
				auto script_hash = chaipp::hash(name);
				auto body_hash = chaipp::hash(body);
				try {
					chaipp::Processor::compile(script_hash, body_hash, body);
					return MsgPack({
						{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
						{ RESERVED_CHAI, {
							{ RESERVED_HASH,      script_hash },
							{ RESERVED_BODY_HASH, body_hash },
							{ RESERVED_BODY,      body      },
						}}
					});
				} catch (...) {
					THROW(ScriptNotFoundError, "Script not found: %s", repr(body));
				}
			}
		}
		default:
			THROW(ClientError, "Only type %s is allowed in %s", Serialise::type(FieldType::SCRIPT), RESERVED_SCRIPT);
	}
#else
	ignore_unused(strict);
	THROW(ClientError, "Script type 'chai' (ChaiScript) not available.");
#endif
}


MsgPack
Script::process_ecma(bool strict)
{
	L_CALL("Script::process_ecma(%s)", strict ? "true" : "false");

#if defined(XAPIAND_V8)
	switch (sep_types[SPC_CONCRETE_TYPE]) {
		case FieldType::EMPTY:
			if (strict) {
				THROW(MissingTypeError, "Type of field %s is missing", RESERVED_SCRIPT);
			}
			sep_types[SPC_CONCRETE_TYPE] = FieldType::SCRIPT;
			/* FALLTHROUGH */
		case FieldType::SCRIPT: {
			if (sep_types[SPC_FOREIGN_TYPE] == FieldType::FOREIGN) {
				if (name.empty()) {
					return MsgPack({
						{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
						{ RESERVED_ECMA, body      },
					});
				} else {
					THROW(ClientError, "For type %s, %s must be string", Serialise::type(FieldType::FOREIGN), RESERVED_VALUE);
				}
			} else if (name.empty()) {
				uint64_t body_hash = v8pp::hash(body);
				try {
					v8pp::Processor::compile(body_hash, body_hash, body);
					return MsgPack({
						{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
						{ RESERVED_ECMA, {
							{ RESERVED_HASH,      body_hash },
							{ RESERVED_BODY_HASH, body_hash },
							{ RESERVED_BODY,      body      },
						}}
					});
				} catch (...) {
					THROW(ScriptNotFoundError, "Script not found: %s", repr(body));
				}
			} else {
				auto script_hash = v8pp::hash(name);
				auto body_hash = v8pp::hash(body);
				try {
					v8pp::Processor::compile(body_hash, body_hash, body);
					return MsgPack({
						{ RESERVED_TYPE, required_spc_t::get_str_type(sep_types) },
						{ RESERVED_ECMA, {
							{ RESERVED_HASH,      script_hash },
							{ RESERVED_BODY_HASH, body_hash },
							{ RESERVED_BODY,      body      },
						}}
					});
				} catch (...) {
					THROW(ScriptNotFoundError, "Script not found: %s", repr(body));
				}
			}
		}
		default:
			THROW(ClientError, "Only type %s is allowed in %s", Serialise::type(FieldType::SCRIPT), RESERVED_SCRIPT);
	}
#else
	ignore_unused(strict);
	THROW(ClientError, "Script type 'ecma' (ECMAScript or JavaScript) not available.");
#endif
}


MsgPack
Script::process_script(bool strict)
{
	L_CALL("Script::process_script(%s)", strict ? "true" : "false");

	switch (type) {
		case Type::CHAI:
			return process_chai(strict);
		case Type::ECMA:
			return process_ecma(strict);
		case Type::EMPTY:
#if defined(XAPIAND_V8)
			try {
				return process_ecma(strict);
			} catch (const ScriptNotFoundError& er) {
#if defined(XAPIAND_CHAISCRIPT)
				try {
					return process_chai(strict);
				} catch (const ScriptNotFoundError& er) {
					THROW(ClientError, "%s", er.what());
				}
#else
				THROW(ClientError, "%s", er.what());
#endif
			}
#elif defined(XAPIAND_CHAISCRIPT)
			try {
				return process_chai(strict);
			} catch (const ScriptNotFoundError& er) {
				THROW(ClientError, "%s", er.what());
			}
#endif
		default:
			THROW(ClientError, "Type %s is not allowed in %s", Serialise::type(sep_types[SPC_CONCRETE_TYPE]), RESERVED_SCRIPT);
	}
}

#endif
