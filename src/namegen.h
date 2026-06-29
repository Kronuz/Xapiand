/*
 * The fantasy name generator now lives in the standalone fork
 * github.com/Kronuz/fantasyname (German's C++ implementation, based on and
 * upstreamed to skeeto/fantasyname; Public Domain). This file is a thin Xapiand
 * wrapper: it pulls the library's NameGen::Generator (and towstring/tostring) in by
 * absolute path (FANTASYNAME_HEADER, set in CMakeLists.txt) to avoid the same-name
 * "namegen.h" collision with this wrapper, and re-adds Xapiand's name_generator().
 */

#pragma once

#include FANTASYNAME_HEADER   // NameGen::Generator + towstring/tostring (fork c++/namegen.h)

#include <strings.h>          // for strcasecmp
#include <string>             // for std::string


// A node-name generator: a capitalized name from the "S"/"b" syllable sets,
// skipping "nan"/"inf" (which could be mistaken for float literals elsewhere).
inline std::string name_generator() {
	static NameGen::Generator generator("!<b<||v|V><||s|S|b>>");
	std::string nm;
	do {
		nm = generator.toString();
	} while (strcasecmp(nm.c_str(), "nan") == 0 || strcasecmp(nm.c_str(), "inf") == 0);
	return nm;
}
