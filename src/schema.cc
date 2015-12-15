/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include "database.h"
#include "log.h"


Schema::Schema(Database* _db)
	: db(_db)
{
	std::string s_schema;
	db->get_metadata(RESERVED_SCHEMA, s_schema);

	if (s_schema.empty()) {
		to_store = true;
		find = false;
		schema = unique_cJSON(cJSON_CreateObject());
	} else {
		to_store = false;
		find = true;
		schema = unique_cJSON(cJSON_Parse(s_schema.c_str()));
		if (!schema) {
			schema.reset();
			throw MSG_Error("Schema is corrupt, you need provide a new one. JSON Before: [%s]", cJSON_GetErrorPtr());
		}

		cJSON *version = cJSON_GetObjectItem(schema.get(), RESERVED_VERSION);
		if (version == nullptr || version->valuedouble != DB_VERSION_SCHEMA) {
			schema.reset();
			throw MSG_Error("Different database's version schemas, the current version is %1.1f", DB_VERSION_SCHEMA);
		}
	}
}


cJSON*
Schema::get_properties_schema()
{
	cJSON* properties = cJSON_GetObjectItem(schema.get(), RESERVED_SCHEMA);
	if (!properties) {
		to_store = true;
		find = false;
		properties = cJSON_CreateObject(); // It is managed by schema.
		cJSON_AddItemToObject(schema.get(), RESERVED_VERSION, cJSON_CreateNumber(DB_VERSION_SCHEMA));
		cJSON_AddItemToObject(schema.get(), RESERVED_SCHEMA, properties);
	} else {
		to_store = false;
		find = true;
	}

	return properties;
}


cJSON*
Schema::get_properties_id(cJSON* item)
{
	cJSON* properties = cJSON_GetObjectItem(item, RESERVED_ID);
	if (!properties) {
		to_store = true;
		find = false;
		properties = cJSON_CreateObject(); // It is managed by item.
		cJSON *type = cJSON_CreateArray(); // Managed by shema
		cJSON_AddItemToArray(type, cJSON_CreateNumber(NO_TYPE));
		cJSON_AddItemToArray(type, cJSON_CreateNumber(NO_TYPE));
		cJSON_AddItemToArray(type, cJSON_CreateNumber(STRING_TYPE));
		cJSON_AddItemToObject(properties, RESERVED_TYPE, type);
		cJSON_AddItemToObject(properties, RESERVED_INDEX, cJSON_CreateNumber(ALL));
		cJSON_AddItemToObject(properties, RESERVED_SLOT, cJSON_CreateNumber(DB_SLOT_ID));
		cJSON_AddItemToObject(properties, RESERVED_PREFIX, cJSON_CreateString(DOCUMENT_ID_TERM_PREFIX));
		cJSON_AddItemToObject(properties, RESERVED_BOOL_TERM, cJSON_CreateTrue());
		cJSON_AddItemToObject(item, RESERVED_ID, properties);
	} else {
		to_store = false;
		find = true;
	}

	return properties;
}


cJSON*
Schema::get_properties(cJSON* item, const char* attr)
{
	cJSON* properties = cJSON_GetObjectItem(item, attr);
	if (!properties) {
		to_store = true;
		find = false;
		properties = cJSON_CreateObject(); // It is managed by item.
		cJSON_AddItemToObject(item, attr, properties);
	} else {
		to_store = false;
		find = true;
	}

	return properties;
}


void
Schema::store()
{
	if (to_store) {
		unique_char_ptr _cprint(cJSON_Print(schema.get()));
		db->set_metadata(RESERVED_SCHEMA, _cprint.get());
	}
}
