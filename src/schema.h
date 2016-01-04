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

#pragma once

#include "cJSON.h"
#include "utils.h"
#include "serialise.h"


enum enum_time {
	DB_SECOND2INT,
	DB_MINUTE2INT,
	DB_HOUR2INT,
	DB_DAY2INT,
	DB_MONTH2INT,
	DB_YEAR2INT,
};


enum enum_index {
	ALL,
	TERM,
	VALUE
};


const std::vector<std::string> str_time({ "second", "minute", "hour", "day", "month", "year" });
const std::vector<std::string> str_analizer({ "STEM_NONE", "STEM_SOME", "STEM_ALL", "STEM_ALL_Z" });
const std::vector<std::string> str_index({ "ALL", "TERM", "VALUE" });


const std::vector<double> def_accuracy_geo  { 1, 0.2, 0, 5, 10, 15, 20, 25 }; // { partials, error, accuracy levels }
const std::vector<double> def_accuracy_num  { 100, 1000, 10000, 100000 };
const std::vector<double> def_acc_date      { DB_HOUR2INT, DB_DAY2INT, DB_MONTH2INT, DB_YEAR2INT };


struct specification_t {
	std::vector<int> position;
	std::vector<int> weight;
	std::vector<std::string> language;
	std::vector<bool> spelling;
	std::vector<bool> positions;
	std::vector<int> analyzer;
	std::vector<double> accuracy;
	std::vector<std::string> acc_prefix;
	unsigned int slot;
	std::vector<char> sep_types;
	std::string prefix;
	int index;
	bool store;
	bool dynamic;
	bool date_detection;
	bool numeric_detection;
	bool geo_detection;
	bool bool_detection;
	bool string_detection;
	bool bool_term;

	specification_t();

	std::string to_string();
};


extern const specification_t default_spc;


class Database;


class Schema {
public:
	specification_t specification;
	bool found_field;

	Schema();

	Schema(Schema&& schema) = delete;
	Schema(const Schema& schema) = delete;

	Schema& operator=(Schema&& schema) = delete;
	Schema& operator=(const Schema& schema) = delete;

	~Schema() = default;

	/*
	 * Updates the properties of schema using root.
	 * Returns properties of schema updated.
	 */
	void update_root(cJSON* properties, cJSON* root);

	/*
	 * Updates properties of attr using item.
	 * Returns properties of attr updated.
	 */
	cJSON* get_subproperties(cJSON* properties, const char* attr, cJSON* item);

	/*
	 * Stores schema only if needed.
	 */
	void store();

	/*
	 * Updates only specification using item.
	 */
	void update_specification(cJSON* item);

	/*
	 * Sets the type of field and updates properties.
	 */
	void set_type(cJSON* field, const std::string &field_name, cJSON* properties);

	/*
	 * Set type to array in schema.
	 */
	void set_type_to_array(cJSON* properties);

	/*
	 * Set type to array in schema.
	 */
	void set_type_to_object(cJSON* properties);

	/*
	 * Accuracy, type, analyzer and index of schema are transformed to readable form.
	 */
	std::string to_string(bool pretty);

	/*
	 * Getters and Setters.
	 */

	void setDatabase(Database* _db);

	cJSON* get_properties_schema();

	inline unique_cJSON getSchema() {
		return unique_cJSON(cJSON_Duplicate(schema.get(), 1));
	}

	inline bool getStore() {
		return to_store;
	}

	inline void setSchema(unique_cJSON&& _schema) {
		schema = std::move(_schema);
	}

	inline void setStore(bool _to_store) {
		to_store = _to_store;
	}

private:
	Database* db;

	unique_cJSON schema;
	bool to_store;

	/*
	 * All the reserved word found in a new field are added in properties.
	 */
	void insert(cJSON* item, cJSON* properties, const std::string &item_name, bool root=false);

	/*
	 * For updating the specifications, first we check whether the document contains reserved words that can be
	 * modified, otherwise we check in properties and if reserved words do not exist, we take the values
	 * of the parent if they are heritable.
	 */
	void update(cJSON* item, cJSON* properties, bool root=false);

	/*
	 * It inserts fields that are not hereditary and if _type has been fixed these can not be modified.
	 */
	void insert_inheritable_specifications(cJSON* item, cJSON* properties);

	/*
	 * For updating required data in properties. When a new field is inserted in the scheme it is
	 * necessary verify that all the required reserved words are defined, otherwise they will be defined.
	 */
	void update_required_data(const std::string &name, cJSON* properties);

	/*
	 * Recursively transforms the field into a readable form.
	 */
	void readable(cJSON* field);

	/*
	 * Return the field's type
	 */
	char get_type(cJSON* field);
};
