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

#include "msgpack_wrapper.h"
#include "serialise.h"
#include "database_utils.h"
#include "utils.h"


enum class unitTime {
	SECOND,
	MINUTE,
	HOUR,
	DAY,
	MONTH,
	YEAR,
};


enum class Index {
	ALL,
	TERM,
	VALUE
};


const std::vector<std::string> str_time({ "second", "minute", "hour", "day", "month", "year" });
const std::vector<std::string> str_analyzer({ "STEM_NONE", "STEM_SOME", "STEM_ALL", "STEM_ALL_Z" });
const std::vector<std::string> str_index({ "ALL", "TERM", "VALUE" });


const std::vector<double> def_accuracy_geo  { 1, 0.2, 0, 5, 10, 15, 20, 25 }; // { partials, error, accuracy levels }
const std::vector<double> def_accuracy_num  { 100, 1000, 10000, 100000 };
const std::vector<double> def_acc_date      { toUType(unitTime::HOUR), toUType(unitTime::DAY), toUType(unitTime::MONTH), toUType(unitTime::YEAR) };


struct specification_t {
	std::vector<unsigned> position;
	std::vector<unsigned> weight;
	std::vector<std::string> language;
	std::vector<bool> spelling;
	std::vector<bool> positions;
	std::vector<unsigned> analyzer;
	std::vector<double> accuracy;
	std::vector<std::string> acc_prefix;
	unsigned int slot;
	std::vector<char> sep_types;
	std::string prefix;
	Index index;
	bool store;
	bool dynamic;
	bool date_detection;
	bool numeric_detection;
	bool geo_detection;
	bool bool_detection;
	bool string_detection;
	bool bool_term;

	specification_t();

	std::string to_string() const;
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
	void update_root(MsgPack& properties, const MsgPack& item_doc);

	/*
	 * Updates properties of attr using item.
	 * Returns properties of attr updated.
	 */
	MsgPack get_subproperties(MsgPack& properties, const std::string& item_key, const MsgPack& item_doc);

	/*
	 * Stores schema only if needed.
	 */
	void store();

	/*
	 * Updates only specification struct using item_doc.
	 */
	void update_specification(const MsgPack& item_doc);

	/*
	 * Sets the type of field and updates properties.
	 */
	void set_type(MsgPack& properties, const std::string& item_key, const MsgPack& item_doc);

	/*
	 * Set type to array in schema.
	 */
	void set_type_to_array(MsgPack& properties);

	/*
	 * Set type to object in schema.
	 */
	void set_type_to_object(MsgPack& properties);

	/*
	 * Transforms schema into string.
	 */
	std::string to_string(bool prettify);

	/*
	 * Getters and Setters.
	 */

	void setDatabase(Database* _db);

	inline MsgPack getProperties() const {
		return schema.at(RESERVED_SCHEMA);
	}

	inline std::string to_string() const {
		return schema.to_string();
	}

	inline MsgPack getSchema() const {
		return schema.duplicate();
	}

	inline bool getStore() const {
		return to_store;
	}

	template<typename... Args>
	inline void setSchema(Args&&... args) {
		schema = MsgPack(std::forward<Args>(args)...);
	}

	inline void setStore(bool _to_store) {
		to_store = _to_store;
	}

private:
	Database* db;

	MsgPack schema;
	bool to_store;

	/*
	 * All the reserved word found into item_doc are added in properties.
	 */
	void insert(MsgPack& properties, const std::string& item_key, const MsgPack& item_doc, bool is_root=false);

	/*
	 * Updates properties, first we check whether the document contains reserved words that can be
	 * modified, otherwise we check in properties and if reserved words do not exist, we take the values
	 * of the parent if they are inheritable.
	 */
	void update(MsgPack& properties, const std::string& item_key, const MsgPack& item_doc, bool is_root=false);

	/*
	 * It inserts properties that are not inheritable. Only it is called when _type has not been fixed.
	 */
	void insert_noninheritable_data(MsgPack& properties, const MsgPack& item_doc);

	/*
	 * For updating required data in properties. When a new item is inserted in the schema, it is
	 * necessary verify that all the required reserved words are defined, otherwise they will be defined.
	 */
	void update_required_data(MsgPack& properties, const std::string& item_key);

	/*
	 * Recursively transforms item_schema into a readable form.
	 */
	void readable(MsgPack&& item_schema);

	/*
	 * Return the item_doc's type
	 */
	char get_type(const MsgPack& item_doc);
};
