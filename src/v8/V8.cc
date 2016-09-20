// brew install v8-315
// c++ -std=c++14 -fsanitize=address -g -O2 -o test tst.cpp -lv8 -L/usr/local/opt/v8-315/lib -I/usr/local/opt/v8-315/include && ./test

#include <iostream>
#include <cassert>
#include <string>
#include <map>
#include <array>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <v8.h>

#include "../msgpack.h"

namespace WV8 {
	struct Function {
		Processor* that;
		v8::Persistent<v8::Function> function;

		Function(Processor* that_, v8::Persistent<v8::Function> function_)
			: that(that_), function(function_) {
		}

		~Function() {
			function.Dispose();
		}

		template<typename... Args>
		void operator()(Args&&... args) {
			fprintf(stderr, "+++ Function::operator()\n");
			that->invoke(function, std::forward<Args>(args)...);
		}
	};
};

