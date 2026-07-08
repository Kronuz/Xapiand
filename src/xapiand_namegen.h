/*
 * The fantasy name generator now lives in the standalone fork
 * github.com/Kronuz/fantasyname (German's C++ implementation, based on and
 * upstreamed to skeeto/fantasyname; Public Domain). This file is a thin Xapiand
 * wrapper, named xapiand_namegen.h (not namegen.h) so it never shadows the library's
 * namegen.h on the include path. It reaches the library's NameGen::Generator (and
 * towstring/tostring) with a plain #include "namegen.h" off the path (no absolute-path
 * macro) and re-adds Xapiand's name_generator().
 */

#pragma once

#include "namegen.h"          // NameGen::Generator + towstring/tostring (fork c++/namegen.h)

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
