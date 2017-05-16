#include "v8pp.h"


#include "../lru.h"


class ScriptLRU : public lru::LRU<size_t, v8pp::Processor> {
public:
	ScriptLRU(ssize_t max_size=-1) : LRU(max_size) { };
};


// void run() {
// 	auto p = v8pp::Processor("unnamed",
// 		"function test_object(old, nn) {"\
// 			"print ('Old: ', old);"\
// 			"nn = {key:'old key', value:'old value'};"\
// 			"print('nn:', nn);"\
// 			"nn.key = 'new key';"\
// 			"nn.value = { a:'new value', b:'value2' };"\
// 			"print ('nn:', nn);"\
// 			"return nn;"\
// 		"}"
// 		"function test_array(old, nn) {"\
// 			"print('old:', old);"\
// 			"nn = ['key', 'value'];"\
// 			"print('nn:', nn);"\
// 			"nn[0] = 'newkey';"\
// 			"nn[1] = 'newvalue';"\
// 			"print ('nn:', nn);"\
// 			"return nn;"\
// 		"}"
// 		"function test_array2(old, nn) {"\
// 			"print('old:', old);"\
// 			"nn = ['key', 'value'];"\
// 			"print('nn:', nn);"\
// 			"nn[0] = 'newkey';"\
// 			"nn[1] = 'newvalue';"\
// 			"print ('nn:', nn);"\
// 			"return old;"\
// 		"}"
// 		"function first(old) {"\
// 			"print ('old:', old);"\
// 			"return 1000;"
// 		"}"
// 		"function test_cycle() {"\
// 			"var map = { a:-110 };"\
// 			"var sub_map = { x:2, y: map };"\
// 			"map.b = sub_map;"\
// 			"return sub_map;"\
// 		"}"
// 		"function test_cycle2() {"\
// 			"var map = { a:{ aa:'AA', ab:'AB' },  b:{ ba:{ baa: 'BAA' }, c:'C' } };"\
// 			"var sub_map = { x:[map.b ,'XXY'], y:'Y' };"\
// 			"map.b.ba.bab = sub_map.x;"\
// 			"return sub_map;"\
// 		"}"
// 	);


// 	MsgPack old_array = { 100, 200, 300, 400, 500 };
// 	MsgPack old_map = {
// 		{ "one", 1 },
// 		{ "two", 2 },
// 		{ "three",
// 			{
// 				{ "value", 30 },
// 				{ "person",
// 					{
// 						{ "name", "José" },
// 						{ "last", "Perez" },
// 					}
// 				}
// 			}
// 		},
// 		{ "four", 4 },
// 		{ "five", 5 }
// 	};

// 	MsgPack new_map;

// 	new_map = p["test_array"](old_array, new_map);
// 	std::cout << "new_array:" << new_map << std::endl;

// 	new_map = p["test_object"](old_map, new_map);
// 	std::cout << "new_map:" << new_map << std::endl;

// 	try {
// 		p["test_cycle"]();
// 	} catch (const v8pp::CycleDetectionError&) {
// 		fprintf(stderr, "ERROR: Cycle Detection\n");
// 	}

// 	try {
// 		p["test_cycle2"]();
// 	} catch (const v8pp::CycleDetectionError&) {
// 		fprintf(stderr, "ERROR: Cycle Detection\n");
// 	}
// }


void run2() {
	try {
		ScriptLRU script_lru;

		std::string script(
			"function tons_to_kg(old) {"\
				"print('old: ', old);"\
				"var nn = [];"\
				"for (var key in old) {"\
					"print('key: ', key);"\
					"nn[key] = old[key] * 1000;"\
					"print('new: ', nn);"\
				"}"\
				"for (var val in nn) {"\
					"print('val: ', nn[val]);"\
				"}"\
				"return nn;"\
			"}"
			"function time_out() {"\
				"while(true);"\
			"}"
			"function void_ret() {"\
				"var i = 0;"\
				"while(i++ < 1000);"\
			"}"
			"function set_get(old) {"\
				"print('Old: ', old.algo);"\
				"old.algo = -100;"\
				"old.algo._value = -1000;"\
				"var x = old.two = 20000;"\
				"print('x: ', x);"\
				"print('Old to string: ', old.toString());"\
				"old.three = 'New Value';"\
				"old.three._value = 'New New Value';"\
				"print('New Old: ', old);"\
				"print('Sum: ', old.algo + old.four[2]);"\
			"}"
		);
		// "function set_get(old) {"\
		// 		"print('Old: ', old.algo);"\
		// 		"old.algo = -100;"\
		// 		"old.two._value = 20000;"\
		// 		"old.three = 'New Value';"\
		// 		"print('New Old: ', old);"\
		// 	"}"


		MsgPack old_map = {
			{ "algo",    {
					{ "_value", 100 }, { "_type", "integer" }
				}
			},
			{ "two",     10000 },
			{ "three",  {
					{ "_value", "My Value" },
					{ "_type", "string" }
				}
			},
			{ "four",     {100, 1000, 10000 } },
		};

		std::cout << "Start Map: " << old_map.to_string(true) << std::endl;

		// MsgPack old_map = { 1 , 2, 3, 4 };

		// auto src_hash = v8pp::hash(script);

		// MsgPack new_map;
		// try {
		// 	auto& processor = script_lru.at(src_hash);
		// 	new_map = processor["tons_to_kg"](old_map);
		// } catch (const std::range_error&) {
		// 	auto& processor = script_lru.insert(std::make_pair(src_hash, v8pp::Processor("new", script)));
		// 	new_map = processor["tons_to_kg"](old_map);
		// }

		// std::cout << "Final Map: " << new_map << std::endl;
		// auto& processor = script_lru.insert(std::make_pair(src_hash, v8pp::Processor("SetFunctions", script)));
		//auto& processor = script_lru.at(src_hash);
		v8pp::Processor processor("SetFunctions", script);
		auto res = processor["set_get"](old_map);
		std::cout << "End Map: " << old_map.to_string(true) << std::endl;
		std::cout << "Return: " << res.to_string() << std::endl;
		fprintf(stderr, "++++ FINISH 1\n");
	} catch (const std::range_error&) {
		fprintf(stderr, "ERROR: Must be in lru\n");
	} catch (const v8pp::Error& e) {
		fprintf(stderr, "\n\nERROR: %s\n", e.what());
	}
}


int
main(int argc, char* argv[]) {
	run2();

	return 0;
}
