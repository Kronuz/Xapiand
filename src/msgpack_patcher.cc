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

#include "msgpack_patcher.h"

#include <exception>             // for exception

#include "exception.h"           // for ClientError, MSG_ClientError, Error, MSG_Error
#include "hashes.hh"             // for fnv1ah32
#include "phf.hh"                // for phf
#include "repr.hh"               // for repr
#include "strict_stox.hh"        // for strict_stoull


void apply_patch(const MsgPack& patch, MsgPack& object) {
	if (patch.is_array()) {
		constexpr static auto _ = phf::make_phf({
			hhl(PATCH_ADD),
			hhl(PATCH_REM),
			hhl(PATCH_REP),
			hhl(PATCH_MOV),
			hhl(PATCH_COP),
			hhl(PATCH_TES),
			hhl(PATCH_INC),
			hhl(PATCH_DEC),
		});
		for (const auto& elem : patch) {
			try {
				const auto& op = elem.at(PATCH_OP);
				auto op_str = op.str_view();
				switch (_.fhhl(op_str)) {
					case _.fhhl(PATCH_ADD):
						patch_add(elem, object);
						break;
					case _.fhhl(PATCH_REM):
						patch_remove(elem, object);
						break;
					case _.fhhl(PATCH_REP):
						patch_replace(elem, object);
						break;
					case _.fhhl(PATCH_MOV):
						patch_move(elem, object);
						break;
					case _.fhhl(PATCH_COP):
						patch_copy(elem, object);
						break;
					case _.fhhl(PATCH_TES):
						patch_test(elem, object);
						break;
					case _.fhhl(PATCH_INC):
						patch_incr(elem, object);
						break;
					case _.fhhl(PATCH_DEC):
						patch_decr(elem, object);
						break;
					default:
						THROW(ClientError, "In patch op: %s is not a valid value", repr(op_str));
				}
			} catch (const std::out_of_range&) {
				THROW(ClientError, "Patch Object MUST have exactly one '%s' member", PATCH_OP);
			} catch (const msgpack::type_error&) {
				THROW(ClientError, "'%s' must be string", PATCH_OP);
			}
		}
	} else {
		THROW(ClientError, "A JSON Patch document MUST be an array of objects");
	}
}


void patch_add(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH, PATCH_ADD);
		if (!path_split.empty()) {
			const auto target = path_split.back();
			path_split.pop_back();
			auto& o = object.path(path_split);
			const auto& val = get_patch_value(obj_patch, PATCH_ADD);
			_add(o, val, target);
		} else {
			THROW(ClientError, "Is not allowed path: ''");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "In patch add: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		THROW(ClientError, "In patch add: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		THROW(ClientError, "In patch add: %s", exc.what());
	} catch (const std::exception& exc) {
		THROW(Error, "In patch add: %s", exc.what());
	}
}


void patch_remove(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH, PATCH_REM);
		if (!path_split.empty()) {
			const auto target = path_split.back();
			path_split.pop_back();
			auto& o = object.path(path_split);
			_erase(o, target);
		} else {
			THROW(ClientError, "Is not allowed path: ''");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "In patch remove: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		THROW(ClientError, "In patch remove: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		THROW(ClientError, "In patch remove: %s", exc.what());
	} catch (const std::exception& exc) {
		THROW(Error, "In patch remove: %s", exc.what());
	}
}


void patch_replace(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH, PATCH_REP);
		auto& o = object.path(path_split);
		o = get_patch_value(obj_patch, PATCH_REP);
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "In patch replace: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		THROW(ClientError, "In patch replace: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		THROW(ClientError, "In patch replace: %s", exc.what());
	} catch (const std::exception& exc) {
		THROW(Error, "In patch replace: %s", exc.what());
	}
}


void patch_move(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH, PATCH_MOV);
		if (!path_split.empty()) {
			std::vector<std::string> from_split;
			_tokenizer(obj_patch, from_split, PATCH_FROM, PATCH_MOV);
			if (!from_split.empty()) {
				const auto target = path_split.back();
				path_split.pop_back();
				auto& to = object.path(path_split);
				const auto& from = object.path(from_split);
				_add(to, from, target);

				const auto target_from = from_split.back();
				from_split.pop_back();
				auto& from_parent = object.path(from_split);
				_erase(from_parent, target_from);
			} else {
				THROW(ClientError, "Is not allowed from: ''");
			}
		} else {
			THROW(ClientError, "Is not allowed path: ''");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "In patch move: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		THROW(ClientError, "In patch move: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		THROW(ClientError, "In patch move: %s", exc.what());
	} catch (const std::exception& exc) {
		THROW(Error, "In patch move: %s", exc.what());
	}
}


void patch_copy(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH, PATCH_COP);
		if (!path_split.empty()) {
			std::vector<std::string> from_split;
			_tokenizer(obj_patch, from_split, PATCH_FROM, PATCH_COP);
			if (!from_split.empty()) {
				const auto target = path_split.back();
				path_split.pop_back();
				auto& to = object.path(path_split);
				const auto& from = object.path(from_split);
				_add(to, from, target);
			} else {
				THROW(ClientError, "Is not allowed from: ''");
			}
		} else {
			THROW(ClientError, "Is not allowed path: ''");
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "In patch 'copy': Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		THROW(ClientError, "In patch 'copy': %s", exc.what());
	} catch (const std::out_of_range& exc) {
		THROW(ClientError, "In patch 'copy': %s", exc.what());
	} catch (const std::exception& exc) {
		THROW(Error, "In patch 'copy': %s", exc.what());
	}
}


void patch_test(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH, PATCH_TES);
		const auto& o = object.path(path_split);
		const auto& val = get_patch_value(obj_patch, PATCH_TES);
		if (val != o) {
			THROW(ClientError, "In patch test: Objects are not equals. Expected: %s Result: %s", repr(val.to_string()), repr(o.to_string()));
		}
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "In patch test: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		THROW(ClientError, "In patch test: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		THROW(ClientError, "In patch test: %s", exc.what());
	} catch (const std::exception& exc) {
		THROW(Error, "In patch test: %s", exc.what());
	}
}


void patch_incr(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH, PATCH_INC);
		auto& o = object.path(path_split);
		const auto& val = get_patch_value(obj_patch, PATCH_INC);
		double val_num = get_patch_double(val, PATCH_INC);
		try {
			const auto& o_limit = obj_patch.at(PATCH_LIMIT);
			_incr(o, val_num, get_patch_double(o_limit, PATCH_LIMIT));
		} catch (const std::out_of_range&) {
			_incr(o, val_num);
		}
	} catch (const LimitError& exc){
		THROW(ClientError, "In patch increment: %s", exc.what());
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "In patch increment: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		THROW(ClientError, "In patch increment: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		THROW(ClientError, "In patch increment: %s", exc.what());
	} catch (const std::exception& exc) {
		THROW(Error, "In patch increment: %s", exc.what());
	}
}


void patch_decr(const MsgPack& obj_patch, MsgPack& object) {
	try {
		std::vector<std::string> path_split;
		_tokenizer(obj_patch, path_split, PATCH_PATH, PATCH_DEC);
		auto& o = object.path(path_split);
		const auto& val = get_patch_value(obj_patch, PATCH_DEC);
		double val_num = get_patch_double(val, PATCH_DEC);
		try {
			const auto& o_limit = obj_patch.at(PATCH_LIMIT);
			_incr(o, -val_num, get_patch_double(o_limit, PATCH_LIMIT));
		} catch (const std::out_of_range&) {
			_incr(o, -val_num);
		}
	} catch (const LimitError& exc){
		THROW(ClientError, "In patch decrement: %s", exc.what());
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "In patch decrement: Inconsistent data");
	} catch (const std::invalid_argument& exc) {
		THROW(ClientError, "In patch decrement: %s", exc.what());
	} catch (const std::out_of_range& exc) {
		THROW(ClientError, "In patch decrement: %s", exc.what());
	} catch (const std::exception& exc) {
		THROW(Error, "In patch decrement: %s", exc.what());
	}
}


const MsgPack& get_patch_value(const MsgPack& obj_patch, const char* patch_op) {
	try {
		return obj_patch.at(PATCH_VALUE);
	} catch (const std::out_of_range&) {
		THROW(ClientError, "'%s' must be defined for patch operation: '%s'", PATCH_VALUE, patch_op);
	}
}


double get_patch_double(const MsgPack& val, const char* patch_op) {
	if (val.is_string()) {
		return strict_stod(val.str_view());
	}
	try {
		return val.f64();
	} catch (const msgpack::type_error&) {
		THROW(ClientError, "'%s' must be string or numeric", patch_op);
	}
}
