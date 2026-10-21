/*
 * A Xapian::Stem backed by the snowball library, so the query parser stems exactly
 * as the term generator does at index time.
 */

#pragma once

#include <stdexcept>               // for std::invalid_argument
#include <string>                  // for std::string

#include "snowball.hh"             // for snowball::stemmer
#include "xapian.h"                // for Xapian::Stem, Xapian::StemImplementation


// Each Stem owns its own stemmer.
class SnowballStem : public Xapian::StemImplementation {
	snowball::stemmer stemmer;

public:
	explicit SnowballStem(const std::string& language) : stemmer(language) { }

	std::string operator()(const std::string& word) override {
		return std::string(stemmer(word));
	}

	std::string get_description() const override {
		return "Xapian::Stem(snowball)";
	}
};


inline Xapian::Stem
makeStem(const std::string& language)
{
	try {
		return Xapian::Stem(new SnowballStem(language));
	} catch (const std::invalid_argument&) {
		throw Xapian::InvalidArgumentError("Language code " + language + " unknown");
	}
}
