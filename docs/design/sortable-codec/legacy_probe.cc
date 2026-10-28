// Black-box probe of Xapiand's current value codec: calls it and prints its output, for the
// size and compatibility rows of docs/design/sortable-codec.md. Reads "i|u|d VALUE" lines.
#include "sortable_serialise.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

int main() {
	std::string kind, text;
	while (std::cin >> kind >> text) {
		std::string out;
		if (kind == "i") {
			out = sortable_serialise(static_cast<std::int64_t>(std::strtoll(text.c_str(), nullptr, 10)));
		} else if (kind == "u") {
			out = sortable_serialise(static_cast<std::uint64_t>(std::strtoull(text.c_str(), nullptr, 10)));
		} else {
			out = sortable_serialise(static_cast<long double>(std::strtod(text.c_str(), nullptr)));
		}
		std::printf("%s %s ", kind.c_str(), text.c_str());
		for (unsigned char c : out) std::printf("%02x", c);
		std::printf("\n");
	}
}
