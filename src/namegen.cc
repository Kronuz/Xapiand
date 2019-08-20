/**
 *
 * @file A fantasy name generator library.
 * @version 1.1.0
 * @license Public Domain
 * @authors:
 *   German Mendez Bravo (Kronuz)
 *
 */

#include "namegen.h"

#include <algorithm>     // for srd::reverse
#include <cwchar>        // for std::size_t, std::mbsrtowcs, std::wcsrtombs
#include <cwctype>       // for std::towupper
#include <memory>        // for std::make_unique
#include <stdexcept>     // for std::invalid_argument, std::out_of_range
#include <utility>       // for std::move

#include "random.hh"     // for random_real


using namespace NameGen;


// https://isocpp.org/wiki/faq/ctors#static-init-order
// Avoid the "static initialization order fiasco"
const std::unordered_map<std::string, const std::vector<std::string>>&
Generator::SymbolMap()
{
	static auto* const symbols = new std::unordered_map<std::string, const std::vector<std::string>>({
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
			"S", {
				"ba", "bai", "bau", "be", "bi", "bia", "bio", "bla", "blo", "blu", "bo", "bra",
				"brau", "bri", "bria", "brie", "bro", "bru", "bue", "ca",
				"ce", "cha", "che", "chi", "cho", "chu", "ci", "cia", "cie", "cio", "cla",
				"cle", "cli", "clo", "co", "cra", "cre", "cri", "cu", "cua", "da", "dai",
				"daia", "de", "dei", "di", "dia", "die", "dio",
				"do", "doi", "doia", "dou", "dra", "dre", "dri", "dria", "dro", "du", "dua",
				"dui", "fa", "fe", "fi", "fia", "fie", "fio", "fla", "flo", "fo", "fra",
				"fre", "fri", "fu", "ga", "gau", "ge", "gei", "gi", "gia", "gie", "gio",
				"giu", "gla", "glai", "gle", "glei", "glo", "go", "gra", "gre", "gri", "gu",
				"gua", "gue", "ja", "jai", "je",
				"ji", "jo", "ju", "jua", "la", "lai", "le", "lei",
				"leu", "li", "lia", "lie", "lio", "liu",
				"lo", "loi", "lu", "lua", "lui", "ma", "mai",
				"maia", "mau", "me", "mei", "mi", "mia", "mla", "mo", "moi",
				"mu", "na", "nai", "naia", "ne", "nei", "neu", "ni", "nia", "nie",
				"nio", "no", "nu", "nue", "pa", "pau", "pe", "peyo",
				"pi", "pia", "pie", "po", "pra", "pri", "pria", "pu", "que",
				"qui", "ra", "rai", "raya", "re", "rei", "rey", "ri", "ria",
				"rie", "rio", "rla", "rle", "rli", "rlo", "ro",
				"ru", "sa", "sai", "sau", "sca", "se", "si", "sia", "sio", "so",
				"ste", "su", "sue", "ta",
				"tai", "taua", "te", "ti", "tia", "tla", "to",
				"tra", "tre", "tri", "trio", "tro", "troi", "tru", "tu",
				"va", "vai", "ve", "vei", "vi", "via", "vie", "vio", "vo",
				"za", "zai", "ze", "zi", "zia", "zie", "zio", "zo",
			}
		},
		{
			"b", {
				"bal", "ban", "bar", "bas", "bed", "bel", "ben", "ber", "bet", "bian", "bil",
				"bin", "blan", "blas", "bles", "bob", "bon", "bran", "bras", "brec", "bren", "bril",
				"bur", "can", "car", "cas", "cat", "cay", "ced", "cel", "cen", "cep", "ces", "chan",
				"ches", "chor", "cial", "cin", "cion", "cir", "cis", "claud", "cles", "clif", "cob",
				"com", "con", "cons", "cor", "cos", "cosm", "cost", "cris", "cual", "cun", "cus", "dad",
				"daf", "dal", "dam", "dan", "dar", "das", "del", "den", "der", "des", "det",
				"dieg" "dic", "diel", "dier", "dil", "din", "dios", "dir", "dis", "dit", "dol",
				"don", "dor", "dox", "dras", "dred", "dres", "duar", "dul", "dun",
				"fal", "fan", "faus", "fer", "fet", "fin", "flor", "fon", "fran", "fred",
				"fren", "fris", "ful", "gail", "gan", "gar", "gas", "gel", "gels", "gem", "gen",
				"ger", "ges", "gian", "gib", "gil", "gin", "gior", "giot", "gis", "git", "glas",
				"glen", "gon", "gor", "gos", "got", "gret", "grid", "gros", "gual", "guel",
				"guer", "gun", "gus", "guz", "jac",
				"jan", "jas", "jauc", "jaz", "jef", "jen", "jes", "jis", "jor", "jos", "juan",
				"jur", "jus", "lain", "lais", "lam", "lan", "lar", "las", "laur", "leb", "led",
				"lem", "len", "ler", "les", "let", "liam", "lian", "liel", "liet", "lil",
				"lim", "lin", "lins", "lip", "lir", "lis", "lius", "liz", "lon", "lor", "lot",
				"luc", "lud", "luis", "lum", "lus", "lux", "luz", "mad", "mag", "mal", "man",
				"mar", "mas", "mat", "max", "med", "mel", "men", "mer", "mes",
				"mian", "mic", "mig", "mil", "mim", "min", "mir", "mis", "mit", "mon", "mor",
				"mos", "muel", "mun", "mus", "nac", "nal", "nan", "nar",
				"nas", "nat", "nef", "nel", "nep", "ner", "nes", "net", "neus", "nic", "nid",
				"niel", "nif", "nil", "nin", "nis", "noc", "nol", "non", "nor", "nos",
				"nuar", "nuel", "nuem", "nul", "nun", "nur", "nus", "pan", "par", "pas",
				"paz", "peb", "peg", "per", "pol", "pom", "pris", "prit", "quar", "quel",
				"ques", "quet", "quin", "ral", "ram", "ran", "rap", "rar", "ras", "raz",
				"rel", "rem", "ren", "res", "ret", "ric", "rid", "riel",
				"riet", "ril", "rim", "rin", "riol", "ris", "rit", "sac"
				"sal", "sam", "san", "sar", "sas", "sef", "sel", "sen", "sep", "ser", "ses",
				"set", "siel", "sier", "sig", "sil", "sin", "sion", "sis", "six",
				"sol", "son", "sop", "sual", "sun", "sus", "tac", "tad", "tal",
				"tan", "tap", "tas", "tel", "ten", "ter", "tes", "tian", "tif",
				"til", "tin", "tir", "tis", "ton", "top", "tor", "tos", "tr", "tran", "trid",
				"tris", "triz", "val", "van", "var", "vas", "vec", "ven", "ver", "ves",
				"vian", "vic", "vid", "vier", "vil", "vin", "vir", "vis", "von", "vor", "vril",
				"z", "zac", "zaid", "zan", "zar", "zel", "zen", "zer", "zid", "ziel",
				"zul",
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
		},
	});

	return *symbols;
}


Generator::Generator(std::vector<std::unique_ptr<Generator>>&& generators_)
	: generators(std::move(generators_)) { }


size_t
Generator::combinations() const
{
	size_t total = 1;
	for (const auto& g : generators) {
		total *= g->combinations();
	}
	return total;
}


size_t
Generator::min() const
{
	size_t final = 0;
	for (const auto& g : generators) {
		final += g->min();
	}
	return final;
}


size_t
Generator::max() const
{
	size_t final = 0;
	for (const auto& g : generators) {
		final += g->max();
	}
	return final;
}


std::string
Generator::toString() const
{
	std::string str;
	for (const auto& g : generators) {
		str.append(g->toString());
	}
	return str;
}


void
Generator::add(std::unique_ptr<Generator>&& g)
{
	generators.push_back(std::move(g));
}


Random::Random(std::vector<std::unique_ptr<Generator>>&& generators_)
	: Generator(std::move(generators_)) { }


size_t
Random::combinations() const
{
	size_t total = 0;
	for (const auto& g : generators) {
		total += g->combinations();
	}
	return total != 0u ? total : 1;
}


size_t
Random::min() const
{
	size_t final = -1;
	for (const auto& g : generators) {
		size_t current = g->min();
		if (current < final) {
			final = current;
		}
	}
	return final;
}


size_t
Random::max() const
{
	size_t final = 0;
	for (const auto& g : generators) {
		size_t current = g->max();
		if (current > final) {
			final = current;
		}
	}
	return final;
}


std::string
Random::toString() const
{
	if (generators.empty()) {
		return std::string();
	}

	int rnd = random_real(0, generators.size() - 1) + 0.5;

	return generators[rnd]->toString();
}


Sequence::Sequence(std::vector<std::unique_ptr<Generator>>&& generators_)
	: Generator(std::move(generators_)) { }


Literal::Literal(std::string value_)
	: value(std::move(value_)) { }


size_t
Literal::combinations() const
{
	return 1;
}


size_t
Literal::min() const
{
	return value.size();
}


size_t
Literal::max() const
{
	return value.size();
}


std::string
Literal::toString() const
{
	return value;
}


Reverser::Reverser(std::unique_ptr<Generator>&& g)
{
	add(std::move(g));
}


std::string
Reverser::toString() const
{
	std::wstring str = towstring(Generator::toString());
	std::reverse(str.begin(), str.end());
	return tostring(str);
}


Capitalizer::Capitalizer(std::unique_ptr<Generator>&& g)
{
	add(std::move(g));
}


std::string
Capitalizer::toString() const
{
	std::wstring str = towstring(Generator::toString());
	str[0] = std::towupper(str[0]);
	return tostring(str);
}


Collapser::Collapser(std::unique_ptr<Generator>&& g)
{
	add(std::move(g));
}


std::string
Collapser::toString() const
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
		int mch = 2;
		switch(ch) {
			case 'a':
			case 'h':
			case 'i':
			case 'j':
			case 'q':
			case 'u':
			case 'v':
			case 'w':
			case 'x':
			case 'y':
				mch = 1;
		}
		if (cnt < mch) {
			out.push_back(ch);
		}
		pch = ch;
	}
	return tostring(out);
}


Generator::Generator(const std::string& pattern, bool collapse_triples)
{
	std::unique_ptr<Generator> last;

	std::stack<std::unique_ptr<Group>> stack;
	std::unique_ptr<Group> top = std::make_unique<GroupSymbol>();

	for (auto c : pattern) {
		switch (c) {
			case '<':
				stack.push(std::move(top));
				top = std::make_unique<GroupSymbol>();
				break;
			case '(':
				stack.push(std::move(top));
				top = std::make_unique<GroupLiteral>();
				break;
			case '>':
			case ')':
				if (stack.empty()) {
					throw std::invalid_argument("Unbalanced brackets");
				} else if (c == '>' && top->type != GroupType::Symbol) {
					throw std::invalid_argument("Unexpected '>' in pattern");
				} else if (c == ')' && top->type != GroupType::Literal) {
					throw std::invalid_argument("Unexpected ')' in pattern");
				}
				last = top->emit();
				top = std::move(stack.top());
				stack.pop();
				top->add(std::move(last));
				break;
			case '|':
				top->split();
				break;
			case '!':
				if (top->type == GroupType::Symbol) {
					top->wrap(Wrapper::Capitalizer);
				} else {
					top->add(c);
				}
				break;
			case '~':
				if (top->type == GroupType::Symbol) {
					top->wrap(Wrapper::Reverser);
				} else {
					top->add(c);
				}
				break;
			default:
				top->add(c);
				break;
		}
	}

	if (!stack.empty()) {
		throw std::invalid_argument("Missing closing bracket");
	}

	std::unique_ptr<Generator> g = top->emit();
	if (collapse_triples) {
		g = std::make_unique<Collapser>(std::move(g));
	}
	add(std::move(g));
}


Generator::Group::Group(GroupType type_)
	: type(type_) { }


void
Generator::Group::add(std::unique_ptr<Generator>&& g)
{
	while (!wrappers.empty()) {
		switch (wrappers.top()) {
			case Wrapper::Reverser:
				g = std::make_unique<Reverser>(std::move(g));
				break;
			case Wrapper::Capitalizer:
				g = std::make_unique<Capitalizer>(std::move(g));
				break;
		}
		wrappers.pop();
	}
	if (set.empty()) {
		set.push_back(std::make_unique<Sequence>());
	}
	set.back()->add(std::move(g));
}


void
Generator::Group::add(char c)
{
	std::string value(1, c);
	std::unique_ptr<Generator> g = std::make_unique<Random>();
	g->add(std::make_unique<Literal>(value));
	Group::add(std::move(g));
}


std::unique_ptr<Generator>
Generator::Group::emit()
{
	switch (set.size()) {
		case 0:
			return std::make_unique<Literal>("");
		case 1:
			return std::move(*set.begin());
		default:
			return std::make_unique<Random>(std::move(set));
	}
}


void
Generator::Group::split()
{
	if (set.empty()) {
		set.push_back(std::make_unique<Sequence>());
	}
	set.push_back(std::make_unique<Sequence>());
}


void
Generator::Group::wrap(Wrapper _type)
{
	wrappers.push(_type);
}


Generator::GroupSymbol::GroupSymbol()
	: Group(GroupType::Symbol) { }


void
Generator::GroupSymbol::add(char c)
{
	std::string value(1, c);
	std::unique_ptr<Generator> g = std::make_unique<Random>();
	try {
		static const auto& symbols = SymbolMap();
		for (const auto& s : symbols.at(value)) {
			g->add(std::make_unique<Literal>(s));
		}
	} catch (const std::out_of_range&) {
		g->add(std::make_unique<Literal>(value));
	}
	Group::add(std::move(g));
}


Generator::GroupLiteral::GroupLiteral()
	: Group(GroupType::Literal) { }


std::wstring towstring(const std::string& s) {
	const char *cs = s.c_str();
	const size_t wn = std::mbsrtowcs(nullptr, &cs, 0, nullptr);

	if (wn == static_cast<size_t>(-1)) {
		return L"";
	}

	std::vector<wchar_t> buf(wn);
	const size_t wn_again = std::mbsrtowcs(buf.data(), &cs, wn, nullptr);

	if (wn_again == static_cast<size_t>(-1)) {
		return L"";
	}

	return std::wstring(buf.data(), wn);
}


std::string tostring(const std::wstring& s) {
	const wchar_t *cs = s.c_str();
	const size_t wn = std::wcsrtombs(nullptr, &cs, 0, nullptr);

	if (wn == static_cast<size_t>(-1)) {
		return "";
	}

	std::vector<char> buf(wn);
	const size_t wn_again = std::wcsrtombs(buf.data(), &cs, wn, nullptr);

	if (wn_again == static_cast<size_t>(-1)) {
		return "";
	}

	return std::string(buf.data(), wn);
}
