/**
 * Copyright (C) 2015 German M. Bravo (Kronuz). All rights reserved.
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

#include "namegen.h"

#include <random>
#include <cstdlib>


using namespace NameGen;


static std::random_device rd;  // Random device engine, usually based on /dev/random on UNIX-like systems
static std::mt19937 rng(rd()); // Initialize Mersennes' twister using rd to generate the seed

const std::unordered_map<std::string, const std::vector<const std::string> >
Generator::symbols = {
	{
		"s", {
			"ach", "ack", "ad", "age", "ald", "ale", "an", "ang", "ar", "ard",
			"as", "ash", "at", "ath", "augh", "aw", "ban", "bel", "bur", "cer",
			"cha", "che", "dan", "dar", "del", "den", "dra", "dyn", "ech", "eld",
			"elm", "em", "en", "end", "eng", "enth", "er", "ess", "est", "et",
			"gar", "gha", "hat", "hin", "hon", "ia", "ight", "ild", "im", "ina",
			"ine", "ing", "ir", "is", "iss", "it", "kal", "kel", "kim", "kin",
			"ler", "lor", "lye", "mor", "mos", "nal", "ny", "nys", "old", "om",
			"on", "or", "orm", "os", "ough", "per", "pol", "qua", "que", "rad",
			"rak", "ran", "ray", "ril", "ris", "rod", "roth", "ryn", "sam",
			"say", "ser", "shy", "skel", "sul", "tai", "tan", "tas", "ther",
			"tia", "tin", "ton", "tor", "tur", "um", "und", "unt", "urn", "usk",
			"ust", "ver", "ves", "vor", "war", "wor", "yer"
		}
	},
	{
		"v", {
			"a", "e", "i", "o", "u", "y"
		}
	},
	{
		"V", {
			"a", "e", "i", "o", "u", "y", "ae", "ai", "au", "ay", "ea", "ee",
			"ei", "eu", "ey", "ia", "ie", "oe", "oi", "oo", "ou", "ui"
		}
	},
	{
		"c", {
			"b", "c", "d", "f", "g", "h", "j", "k", "l", "m", "n", "p", "q", "r",
			"s", "t", "v", "w", "x", "y", "z"
		}
	},
	{
		"B", {
			"b", "bl", "br", "c", "ch", "chr", "cl", "cr", "d", "dr", "f", "g",
			"h", "j", "k", "l", "ll", "m", "n", "p", "ph", "qu", "r", "rh", "s",
			"sch", "sh", "sl", "sm", "sn", "st", "str", "sw", "t", "th", "thr",
			"tr", "v", "w", "wh", "y", "z", "zh"
		}
	},
	{
		"C", {
			"b", "c", "ch", "ck", "d", "f", "g", "gh", "h", "k", "l", "ld", "ll",
			"lt", "m", "n", "nd", "nn", "nt", "p", "ph", "q", "r", "rd", "rr",
			"rt", "s", "sh", "ss", "st", "t", "th", "v", "w", "y", "z"
		}
	},
	{
		"i", {
			"air", "ankle", "ball", "beef", "bone", "bum", "bumble", "bump",
			"cheese", "clod", "clot", "clown", "corn", "dip", "dolt", "doof",
			"dork", "dumb", "face", "finger", "foot", "fumble", "goof",
			"grumble", "head", "knock", "knocker", "knuckle", "loaf", "lump",
			"lunk", "meat", "muck", "munch", "nit", "numb", "pin", "puff",
			"skull", "snark", "sneeze", "thimble", "twerp", "twit", "wad",
			"wimp", "wipe"
		}
	},
	{
		"m", {
			"baby", "booble", "bunker", "cuddle", "cuddly", "cutie", "doodle",
			"foofie", "gooble", "honey", "kissie", "lover", "lovey", "moofie",
			"mooglie", "moopie", "moopsie", "nookum", "poochie", "poof",
			"poofie", "pookie", "schmoopie", "schnoogle", "schnookie",
			"schnookum", "smooch", "smoochie", "smoosh", "snoogle", "snoogy",
			"snookie", "snookum", "snuggy", "sweetie", "woogle", "woogy",
			"wookie", "wookum", "wuddle", "wuddly", "wuggy", "wunny"
		}
	},
	{
		"M", {
			"boo", "bunch", "bunny", "cake", "cakes", "cute", "darling",
			"dumpling", "dumplings", "face", "foof", "goo", "head", "kin",
			"kins", "lips", "love", "mush", "pie", "poo", "pooh", "pook", "pums"
		}
	},
	{
		"D", {
			"b", "bl", "br", "cl", "d", "f", "fl", "fr", "g", "gh", "gl", "gr",
			"h", "j", "k", "kl", "m", "n", "p", "th", "w"
		}
	},
	{
		"d", {
			"elch", "idiot", "ob", "og", "ok", "olph", "olt", "omph", "ong",
			"onk", "oo", "oob", "oof", "oog", "ook", "ooz", "org", "ork", "orm",
			"oron", "ub", "uck", "ug", "ulf", "ult", "um", "umb", "ump", "umph",
			"un", "unb", "ung", "unk", "unph", "unt", "uzz"
		}
	}
};


Generator::Generator()
{
}


Generator::Generator(const std::vector<Generator *> & generators_) :
	generators(generators_)
{
}

Generator::~Generator() {
	for (auto g : generators) {
		delete g;
	}
}


size_t Generator::combinations()
{
	size_t total = 1;
	for (auto g : generators) {
		total *= g->combinations();
	}
	return total;
}


size_t Generator::min()
{
	size_t final = 0;
	for (auto g : generators) {
		final += g->min();
	}
	return final;
}


size_t Generator::max()
{
	size_t final = 0;
	for (auto g : generators) {
		final += g->max();
	}
	return final;
}

std::string Generator::toString() {
	std::string str;
	for (auto g : generators) {
		str.append(g->toString());
	}
	return str;
}


void Generator::add(Generator *g)
{
	generators.push_back(g);
}


Random::Random()
{
}

Random::Random(const std::vector<Generator *> & generators_) :
	Generator(generators_)
{
}

size_t Random::combinations()
{
	size_t total = 0;
	for (auto g : generators) {
		total += g->combinations();
	}
	return total ? total : 1;
}

size_t Random::min()
{
	size_t final = -1;
	for (auto g : generators) {
		size_t current = g->min();
		if (current < final) {
			final = current;
		}
	}
	return final;
}

size_t Random::max()
{
	size_t final = 0;
	for (auto g : generators) {
		size_t current = g->max();
		if (current > final) {
			final = current;
		}
	}
	return final;
}


std::string Random::toString()
{
	if (!generators.size()) {
		return "";
	}
	std::uniform_real_distribution<double> distribution(0, generators.size() - 1);
	int rnd = distribution(rng) + 0.5;
	return generators[rnd]->toString();
}


Sequence::Sequence()
{
}

Sequence::Sequence(const std::vector<Generator *> & generators_) :
	Generator(generators_)
{
}

Literal::Literal(const std::string &value_) :
	value(value_)
{
}

size_t Literal::combinations()
{
	return 1;
}
size_t Literal::min()
{
	return value.size();
}
size_t Literal::max()
{
	return value.size();
}

std::string Literal::toString()
{
	return value;
}

Reverser::Reverser(Generator *generator_) :
	Generator(std::vector<Generator *>({generator_}))
{
}


std::string Reverser::toString()
{
	std::wstring str = towstring(Generator::toString());
	std::reverse(str.begin(), str.end());
	return tostring(str);
}

Capitalizer::Capitalizer(Generator *generator_) :
	Generator(std::vector<Generator *>({generator_}))
{
}

std::string Capitalizer::toString()
{
	std::wstring str = towstring(Generator::toString());
	str[0] = towupper(str[0]);
	return tostring(str);
}


Collapser::Collapser(Generator *generator_) :
	Generator(std::vector<Generator *>({generator_}))
{
}

std::string Collapser::toString()
{
	std::wstring str = towstring(Generator::toString());
	std::wstring out;
	int cnt = 0;
	wchar_t pch = L'\0';
	for (auto ch : str) {
		if (ch == pch) {
			cnt++;
		} else {
			cnt = 0;
		}
		if (cnt < ((ch == 'i') ? 1 : 2)) {
			out.push_back(ch);
		}
		pch = ch;
	}
	return tostring(out);
}


Generator::Generator(const std::string &pattern, bool collapse_triples) {
	Group *top;
	Generator *last;

	std::stack<Group *> stack;

	stack.push(new GroupSymbol());

	for (auto c : pattern) {
		top = stack.top();
		switch (c) {
			case '<':
				stack.push(new GroupSymbol());
				break;
			case '(':
				stack.push(new GroupLiteral());
				break;
			case '>':
			case ')':
				if (stack.size() == 1) {
					throw std::invalid_argument("Unbalanced brackets");
				} else if (c == '>' && top->type != group_types::symbol) {
					throw std::invalid_argument("Unexpected '>' in pattern");
				} else if (c == ')' && top->type != group_types::literal) {
					throw std::invalid_argument("Unexpected ')' in pattern");
				}

				last = top->emit();
				stack.pop();
				delete top;

				top = stack.top();
				top->add(last);
				break;
			case '|':
				top->split();
				break;
			case '!':
				if (top->type == group_types::symbol) {
					top->wrap(wrappers::capitalizer);
				} else {
					top->add(c);
				}
				break;
			case '~':
				if (top->type == group_types::symbol) {
					top->wrap(wrappers::reverser);
				} else {
					top->add(c);
				}
				break;
			default:
				top->add(c);
				break;
		}
	}

	if (stack.size() != 1) {
		throw std::invalid_argument("Missing closing bracket");
	}

	top = stack.top();
	Generator *g = top->emit();
	if (collapse_triples) {
		g = new Collapser(g);
	}
	add(g);

	while (!stack.empty()) {
		delete stack.top();
		stack.pop();
	}
}


Generator::Group::Group(group_types_t type_) :
	type(type_)
{
}

void Generator::Group::add(Generator *g)
{
	while (!wrappers.empty()) {
		switch (wrappers.top()) {
			case reverser:
				g = new Reverser(g);
				break;
			case capitalizer:
				g = new Capitalizer(g);
				break;
		}
		wrappers.pop();
	}
	if (set.size() == 0) {
		set.push_back(new Sequence());
	}
	set.back()->add(g);
}

void Generator::Group::add(char c)
{
	std::string value(&c, 1);
	Generator * g = new Random();
	g->add(new Literal(value));
	Group::add(g);
}

Generator * Generator::Group::emit()
{
	switch(set.size()) {
		case 0:
			return new Literal("");
		case 1:
			return *(set.begin());
		default:
			return new Random(set);
	}
}

void Generator::Group::split()
{
	if (set.size() == 0) {
		set.push_back(new Sequence());
	}
	set.push_back(new Sequence());
}

void Generator::Group::wrap(wrappers_t type)
{
	wrappers.push(type);
}

Generator::GroupSymbol::GroupSymbol() :
	Group(group_types::symbol)
{
}

void Generator::GroupSymbol::add(char c)
{
	std::string value(&c, 1);
	Generator * g = new Random();
	try {
		for (auto s : Generator::symbols.at(value)) {
			g->add(new Literal(s));
		}
	} catch (std::out_of_range) {
		g->add(new Literal(value));
	}
	Group::add(g);
}

Generator::GroupLiteral::GroupLiteral() :
	Group(group_types::literal)
{
}

std::wstring towstring(const std::string & s)
{
	const char *cs = s.c_str();
	const size_t wn = std::mbsrtowcs(NULL, &cs, 0, NULL);

	if (wn == -1) {
		return L"";
	}

	std::vector<wchar_t> buf(wn);
	const size_t wn_again = std::mbsrtowcs(buf.data(), &cs, wn, NULL);

	if (wn_again == -1) {
		return L"";
	}

	return std::wstring(buf.data(), wn);
}

std::string tostring(const std::wstring & s)
{
	const wchar_t *cs = s.c_str();
	const size_t wn = std::wcsrtombs(NULL, &cs, 0, NULL);

	if (wn == -1) {
		return "";
	}

	std::vector<char> buf(wn);
	const size_t wn_again = std::wcsrtombs(buf.data(), &cs, wn, NULL);

	if (wn_again == -1) {
		return "";
	}

	return std::string(buf.data(), wn);
}
