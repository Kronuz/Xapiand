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

#include "test_patcher.h"

#include "../src/msgpack_patcher.h"
#include "../src/rapidjson/document.h"
#include "../src/rapidjson/rapidjson.h"
#include "utils.h"


const std::string path_patcher_test = std::string(FIXTURES_PATH) + "/examples/json/";


int test_patcher_mix() {
	INIT_LOG
	std::string obj_str;
	std::string filename(path_patcher_test + "object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string patch_str;
	filename = path_patcher_test + "patch_mix.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string expected;
	filename = path_patcher_test + "patch_result.txt";
	if (!read_file_contents(filename, &expected)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	rapidjson::Document doc_patch;
	rapidjson::Document doc_obj;
	json_load(doc_patch, patch_str);
	json_load(doc_obj, obj_str);

	MsgPack patch(doc_patch);
	MsgPack obj(doc_obj);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR("ERROR: Patch is not working.\nResult:\n{}\nExpected:\n{}", result, expected);
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}


int test_patcher_add() {
	INIT_LOG
	std::string obj_str;
	std::string filename(path_patcher_test + "object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string patch_str;
	filename = path_patcher_test + "patch_add.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\",\"name\":\"Bruce Wayne\",\"super_power\":\"High-tech equipment and weapons\",\"enemy\":\"Joker\",\"creation\":\"1939\",\"partnerships\":\"Robin\"},{\"hero\":\"Superman\",\"name\":\"Clark Kent\",\"super_power\":\"too many\",\"enemy\":\"Lex Luthor\",\"creation\":\"1933\"},{\"hero\":\"Flash\",\"name\":\"Bart Allen\",\"super_power\":\"fast\",\"enemy\":\"Zoom\",\"creation\":\"1940\"},{\"hero\":\"Green Lantern\",\"name\":\"Hal Jordan\",\"super_power\":\"Use of power ring\",\"enemy\":\"The Gambler\",\"creation\":\"1940\"}],\"villains\":[{\"villain\":\"Joker\",\"name\":\"unknown\",\"super_power\":\"Genius-level intellect\",\"enemy\":\"Batman\",\"creation\":\"1940\"},{\"villain\":\"Mr. Freeze\",\"name\":\"Dr. Victor Fries\",\"super_power\":\"Sub-zero physiology\",\"enemy\":\"Batman\",\"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR("ERROR: Patch is not working.\nResult:\n{}\nExpected:\n{}", result, expected);
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}


int test_patcher_remove() {
	INIT_LOG
	std::string obj_str;
	std::string filename(path_patcher_test + "object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string patch_str;
	filename = path_patcher_test + "patch_remove.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\",\"name\":\"Bruce Wayne\",\"super_power\":\"High-tech equipment and weapons\",\"enemy\":\"Joker\"},{\"hero\":\"Superman\",\"name\":\"Clark Kent\",\"super_power\":\"too many\",\"enemy\":\"Lex Luthor\",\"creation\":\"1933\"},{\"hero\":\"Flash\",\"name\":\"Bart Allen\",\"super_power\":\"fast\",\"enemy\":\"Zoom\",\"creation\":\"1940\"}],\"villains\":[{\"villain\":\"Joker\",\"name\":\"unknown\",\"super_power\":\"Genius-level intellect\",\"enemy\":\"Batman\",\"creation\":\"1940\"},{\"villain\":\"Mr. Freeze\",\"name\":\"Dr. Victor Fries\",\"super_power\":\"Sub-zero physiology\",\"enemy\":\"Batman\",\"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR("ERROR: Patch is not working.\nResult:\n{}\nExpected:\n{}", result, expected);
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}


int test_patcher_replace() {
	INIT_LOG
	std::string obj_str;
	std::string filename(path_patcher_test + "object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string patch_str;
	filename = path_patcher_test + "patch_replace.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\",\"name\":\"Bruce Wayne\",\"super_power\":\"High-tech equipment and weapons\",\"enemy\":\"Riddler\",\"creation\":\"1939\"},{\"hero\":\"Superman\",\"name\":\"Clark Kent\",\"super_power\":\"too many\",\"enemy\":\"Lex Luthor\",\"creation\":\"1933\"},{\"hero\":\"Flash\",\"name\":\"Bart Allen\",\"super_power\":\"fast\",\"enemy\":\"Zoom\",\"creation\":\"1940\"}],\"villains\":[{\"villain\":\"Joker\",\"name\":\"unknown\",\"super_power\":\"Genius-level intellect\",\"enemy\":\"Batman\",\"creation\":\"1940\"},{\"villain\":\"Mr. Freeze\",\"name\":\"Dr. Victor Fries\",\"super_power\":\"Sub-zero physiology\",\"enemy\":\"Batman\",\"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR("ERROR: Patch is not working.\nResult:\n{}\nExpected:\n{}", result, expected);
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}


int test_patcher_move() {
	INIT_LOG
	std::string obj_str;
	std::string filename(path_patcher_test + "object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string patch_str;
	filename = path_patcher_test + "patch_move.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\",\"name\":\"Bruce Wayne\",\"super_power\":\"High-tech equipment and weapons\",\"creation\":\"1939\"},{\"hero\":\"Superman\",\"name\":\"Clark Kent\",\"super_power\":\"too many\",\"enemy\":\"Joker\",\"creation\":\"1933\"},{\"hero\":\"Flash\",\"name\":\"Bart Allen\",\"super_power\":\"fast\",\"enemy\":\"Zoom\",\"creation\":\"1940\"}],\"villains\":[{\"villain\":\"Joker\",\"name\":\"unknown\",\"super_power\":\"Genius-level intellect\",\"enemy\":\"Batman\",\"creation\":\"1940\"},{\"villain\":\"Mr. Freeze\",\"name\":\"Dr. Victor Fries\",\"super_power\":\"Sub-zero physiology\",\"enemy\":\"Batman\",\"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR("ERROR: Patch is not working.\nResult:\n{}\nExpected:\n{}", result, expected);
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}


int test_patcher_copy() {
	INIT_LOG
	std::string obj_str;
	std::string filename(path_patcher_test + "object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string patch_str;
	filename = path_patcher_test + "patch_copy.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string expected("{\"heroes\":[{\"hero\":\"Batman\",\"name\":\"Bruce Wayne\",\"super_power\":\"High-tech equipment and weapons\",\"enemy\":\"Joker\",\"creation\":\"1939\"},{\"hero\":\"Superman\",\"name\":\"Clark Kent\",\"super_power\":\"too many\",\"enemy\":\"Lex Luthor\",\"creation\":\"1933\",\"other_enemy\":\"Joker\"},{\"hero\":\"Flash\",\"name\":\"Bart Allen\",\"super_power\":\"fast\",\"enemy\":\"Zoom\",\"creation\":\"1940\"}],\"villains\":[{\"villain\":\"Joker\",\"name\":\"unknown\",\"super_power\":\"Genius-level intellect\",\"enemy\":\"Batman\",\"creation\":\"1940\"},{\"villain\":\"Mr. Freeze\",\"name\":\"Dr. Victor Fries\",\"super_power\":\"Sub-zero physiology\",\"enemy\":\"Batman\",\"creation\":\"1956\"}]}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR("ERROR: Patch is not working.\nResult:\n{}\nExpected:\n{}", result, expected);
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}


int test_patcher_test() {
	INIT_LOG
	std::string obj_str;
	std::string filename(path_patcher_test + "object_to_patch.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string patch_str;
	filename = path_patcher_test + "patch_test.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		RETURN(0);
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}


int test_patcher_incr() {
	INIT_LOG
	std::string obj_str("{ \"age\" : 24 }");
	std::string patch_str("[ { \"op\":\"incr\", \"path\":\"/age\", \"value\": \"1\", \"limit\": \"26\"} ]");
	std::string expected("{\"age\":25}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);

	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		L_DEBUG("RESULT FOR TEST_INCR {}", result);
		if (expected.compare(result) != 0) {
			L_ERR("ERROR: Patch is not working.\nResult:\n{}\nExpected:\n{}", result, expected);
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}


int test_patcher_decr() {
	INIT_LOG
	std::string obj_str("{ \"age\" : 24 }");
	std::string patch_str("[ { \"op\":\"decr\", \"path\":\"/age\", \"value\": 1, \"limit\": 22} ]");
	std::string expected("{\"age\":23}");

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);

	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		auto result = obj.to_string();
		if (expected.compare(result) != 0) {
			L_ERR("ERROR: Patch is not working.\nResult:\n{}\nExpected:\n{}", result, expected);
			RETURN(1);
		} else {
			RETURN(0);
		}
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}


int test_patcher_rfc6901() {
	INIT_LOG
	std::string obj_str;
	std::string filename(path_patcher_test + "rfc6901.txt");
	if (!read_file_contents(filename, &obj_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	std::string patch_str;
	filename = path_patcher_test + "patch_rfc6901.txt";
	if (!read_file_contents(filename, &patch_str)) {
		L_ERR("Can not read the file {}", filename);
		RETURN(1);
	}

	rapidjson::Document doc_obj;
	rapidjson::Document doc_patch;
	json_load(doc_obj, obj_str);
	json_load(doc_patch, patch_str);
	MsgPack obj(doc_obj);
	MsgPack patch(doc_patch);

	try {
		apply_patch(patch, obj);
		RETURN(0);
	} catch (const BaseException& exc) {
		L_EXC("ERROR: {}", exc.get_context());
		RETURN(1);
	}
}
