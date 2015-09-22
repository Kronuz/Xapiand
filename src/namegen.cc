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

/*
 *  s    Generic syllable
 *  v    Single vowel
 *  V    Single vowel or vowel combination
 *  c    Single consonant
 *  B    Single consonant or consonant combination suitable for beginning a word
 *  C    Single consonant or consonant combination suitable anywhere in a word
 *  i    Unit for an insult
 *  m    Unit for a mushy name
 *  M    Unit for a mushy name ending
 *  D    Consonant or consonant suited for a stupid person's name
 *  d    Syllable suited for a stupid person's name (always begins with a vowel)
 *
 *  '    Literal apostrophe
 *  -    Literal hyphen
 *
 *  ( )  Denotes that anything inside is literal
 *  < >  Denotes that anything inside is a special symbol
 *  |    Logical OR operator for use in ( ) and < >
 */

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
	},
	{
		"K", {
			// Drow Prefixes ("Female", "Male")
			"akor", "alak",         // Beloved, best, first
			"alaun", "alton",       // Lightning, powerful
			"aly", "kel",           // Legendary, singing, song
			"ang", "adin",          // Beast, monstrous, savage
			"ardul", "amal",        // Blessed, divine, godly
			"aun", "ant",           // Crypt, dead, deadly, death
			"bae", "bar",           // Fate, fated, luck, lucky
			"bal", "bel",           // Burned, burning, fire, flame
			"belar", "bruh",        // Arrow, lance, pierced
			"briz", "berg",         // Graceful, fluid, water, wet
			"bur", "bhin",          // Craft, crafty, sly
			"chal", "chasz",        // Earth, stable
			"char", "kron",         // Sick, venom, venomed
			"chess", "cal",         // Noble, lady", "lord
			"dhaun",                // Infested, plague
			"dil", "dur",           // Cold, ice, still
			"dirz", "div",          // Dream, dreaming, fantasy
			"dris", "riz",          // Ash, dawn, east, eastern
			"eclav", "elk",         // Chaos, mad, madness
			"elvan", "kalan",       // Elf, elven, far, lost
			"elv", "elaug",         // Drow, mage, power
			"erel", "rhyl",         // Eye, moon, spy
			"ethe", "erth",         // Mithril, resolute
			"faer", "selds",        // Oath, sworn, vow
			"felyn", "fil",         // Pale, thin, weak, white
			"filf", "phar",         // Dwarf, dwarven, treacherous
			"gauss", "orgoll",      // Dread, fear, feared, vile
			"g’eld",                // Friend, spider
			"ghuan",                // Accursed, curse, unlucky
			"gin", "din",           // Berserk, berserker, orc, wild
			"grey", "gul",          // Ghost, pale, unliving
			"hael", "hatch",        // Marked, trail, way
			"hal", "sol",           // Deft, nimble, spider
			"houn", "rik",          // Magic, ring, staff
			"iiv", "dip",           // Liege, war, warrior
			"iim",                  // Life, living, spirit, soul
			"illiam", "im",         // Devoted, heart, love
			"in", "sorn",           // Enchanted, spell
			"ilph",                 // Emerald, green, lush, tree
			"irae", "ilzt",         // Arcane, mystic, wizard
			"in", "izz",            // Hidden, mask, masked
			"iym", "ist",           // Endless, immortal
			"jan", "duag",          // Shield, warded
			"jhael", "gel",         // Ambitious, clan, kin, family
			"jhul", "jar",          // Charmed, rune, symbol
			"jys", "driz",          // Hard, steel, unyielding
			"lael", "llt",          // Iron, west, western
			"lar", "les",           // Binding, bound, law, lawful
			"lineer", "mourn",      // Legend, legendary, mythical
			"lird", "ryld",         // Brand, branded, owned, slave
			"lua", "lyme",          // Bright, crystal, light
			"mal", "malag",         // Mystery, secret
			"may", "mas",           // Beautiful, beauty, silver
			"micar",                // Lost, poison, widow
			"min", "ran",           // Lesser, minor, second
			"mol", "go",            // Blue, storm, thunder, wind
			"myr", "nym",           // Lost, skeleton, skull
			"nath", "mer",          // Doom, doomed, fate
			"ned", "nad",           // Cunning, genius, mind, thought
			"nhil", "nal",          // Fear, horrible, horror, outraged
			"neer",                 // Core, root, strong
			"null", "nil",          // Sad, tear, weeping
			"olor", "omar",         // Skin, tattoo, tattooed
			"pellan", "relon",      // North, platinum, wind
			"phaer", "vorn",        // Honor, honored
			"phyr", "phyx",         // Bless, blessed, blessing
			"qualn", "quil",        // Mighty, ocean, sea
			"quar",                 // Aged, eternal, time
			"quav", "quev",         // Charmed, docile, friend
			"qil", "quil",          // Foe, goblin, slave
			"rauv", "welv",         // Cave, rock, stone
			"ril", "ryl",           // Foretold, omen
			"sbat", "szor",         // Amber, yellow
			"sab", "tsab",          // Abyss, empty, void
			"shi’n", "kren",        // Fool, foolish, young
			"shri", "ssz",          // Silk, silent
			"shur", "shar",         // Dagger, edge, stiletto
			"shynt",                // Invisible, skilled, unseen
			"sin", "szin",          // Festival, joy, pleasure
			"ssap", "tath",         // Blue, midnight, nigh
			"susp", "spir",         // Learned, skilled, wise
			"talab", "tluth",       // Burn, burning, fire
			"tal", "tar",           // Love, pain, wound, wounded
			"triel", "taz",         // Bat, winged
			"t’riss", "teb",        // Blade, sharp, sword
			"ulvir", "uhls",        // Gold, golden, treasure
			"umrae", "hurz",        // Faith, faithful, true
			"vas", "vesz",          // Blood, body, flesh
			"vic",                  // Abyss, deep, profound
			"vier", "val",          // Black, dark, darkness
			"vlon", "wod",          // Bold, hero, heroic
			"waer", "wehl",         // Deep, hidden, south, southern
			"wuyon", "wruz",        // Humble, third, trivial
			"xull", "url",          // Blooded, crimson, ruby
			"xun",                  // Demon, fiend, fiendish
			"yas", "yaz",           // Riddle, spinning, thread, web
			"zar", "zakn",          // Dusk, haunted, shadow
			"zebey", "zek",         // Dragon, lithe, rage, wyrm
			"zes", "zsz",           // Ancient, elder, respected
			"zilv", "vuz",          // Forgotten, old, unknown
		}
	},
	{
		"L", {
			// Drow House Names Prefix
			"alean",      // The noble line of
			"ale",        // Traders in
			"arab",       // Daughters of
			"arken",      // Mages of
			"auvry",      // Blood of the
			"baen",       // Blessed by
			"barri",      // Spawn of
			"cladd",      // Warriors from
			"desp",       // Victors of
			"de",         // Champions of
			"do’",        // Walkers in
			"eils",       // Lands of
			"everh",      // The caverns of
			"fre",        // Friends to
			"gode",       // Clan of
			"helvi",      // Those above
			"hla",        // Seers of
			"hun’",       // The sisterhood of
			"ken",        // Sworn to
			"kil",        // People of
			"mae",        // Raiders from
			"mel",        // Mothers of
			"my",         // Honored of
			"noqu",       // Sacred to
			"orly",       // Guild of
			"ouss",       // Heirs to
			"rilyn",      // House of
			"teken’",     // Delvers in
			"tor",        // Mistresses of
			"zau",        // Children of
		}
	},
	{
		"E", {
			// Elven Prefixes
			"ael",       // knight
			"aer",       // law, order
			"af",        // ring
			"ah",        // crafty, sly
			"al",        // sea
			"am",        // swan
			"ama",       // beauty, beautiful
			"an",        // hand
			"ang",       // glitter
			"ansr",      // rune
			"ar",        // gold, golden
			"ari",       // silver
			"arn",       // south
			"aza",       // life, lives
			"bael",      // guardian
			"bes",       // oath
			"cael",      // archer, arrow
			"cal",       // faith
			"cas",       // herald
			"cla",       // rose
			"cor",       // legend, legendary
			"cy",        // onyx
			"dae",       // white
			"dho",       // falcon
			"dre",       // hound
			"du",        // crescent
			"eil",       // azure, blue
			"eir",       // sharp
			"el",        // green
			"er",        // boar
			"ev",        // stag
			"fera",      // champion
			"fi",        // rain
			"fir",       // dark
			"fis",       // light
			"gael",      // pegasus
			"gar",       // owl
			"gil",       // griffin
			"ha",        // free, freedom
			"hu",        // horse
			"ia",        // day
			"il",        // mist
			"ja",        // staff
			"jar",       // dove
			"ka",        // dragon
			"kan",       // eagle
			"ker",       // spell
			"keth",      // wind
			"koeh",      // earth
			"kor",       // black
			"ky",        // ruby
			"la",        // night
			"laf",       // moon
			"lam",       // east
			"lue",       // riddle
			"ly",        // wolf
			"mai",       // death, slayer
			"mal",       // war
			"mara",      // priest
			"my",        // emerald
			"na",        // ancient
			"nai",       // oak
			"nim",       // deep
			"nu",        // hope, hopeful
			"ny",        // diamond
			"py",        // sapphire
			"raer",      // unicorn
			"re",        // bear
			"ren",       // west
			"rhy", "ry", // jade
			"ru",        // dream
			"rua",       // star
			"rum",       // meadow
			"rid",       // spear
			"sae",       // wood
			"seh",       // soft
			"sel",       // high
			"sha",       // sun
			"she",       // age, time
			"si",        // cat, feline
			"sim",       // north
			"sol",       // history, memory
			"sum",       // water
			"syl",       // faerie
			"ta",        // fox
			"tahl",      // blade
			"tha",       // vigil, vigilance
			"tho",       // true, truth
			"ther",      // sky
			"thro",      // lore, sage
			"tia",       // magic
			"tra",       // tree
			"ty", "try", // crystal
			"uth",       // wizard
			"ver",       // peace
			"vil",       // finger, point
			"von",       // ice
			"ya",        // bridge, path, way
			"za",        // royal
			"zy",        // ivory
		}
	},
	{
		"F", {
			// Elven House Name Prefixes
			"alean",      // The noble line of
			"alea",       // Traders in
			"arabi",      // Daughters of
			"arkenea",    // Mages of
			"auvrea",     // Blood of the
			"baequi",     // Blessed by
			"banni",      // Holder's of
			"cyred",      // Warriors from
			"dirth",      // Victors of
			"dryear",     // Champions of
			"dwin’",      // Walkers in
			"eyllis",     // Lands of
			"eyther",     // The Forests of
			"freani",     // Friends to
			"gysse",      // Clan of
			"heasi",      // Those above
			"hlae",       // Seers of
			"hunith",     // The sisterhood of
			"kennyr",     // Sworn to
			"kille",      // People of
			"maern",      // Defenders from
			"melith",     // Mothers of
			"myrth",      // Honoured of
			"norre",      // Sacred to
			"orle",       // Guild of
			"oussea",     // Heirs to
			"rilynn",     // House of
			"teasen’",    // Trackers of
			"tyr",        // Mistresses of
			"tyrnea",     // Children of
		}
	},
	{
		"k", {
			// Drow Suffixes ("Female", "Male")
			"a", "agh",          // Breaker, destruction, end, omega
			"ace", "as",         // Savant, scholar, wizard
			"ae", "aun",         // Dance, dancer, life, player
			"aer", "d",          // Blood, blood of, heir
			"afae", "afein",     // Bane, executioner, slayer
			"afay", "aufein",    // Eyes, eyes of, seeress", "seer
			"ala", "launim",     // Healer, priestess", "priest
			"anna", "erin",      // Advisor, counselor to
			"arra", "atar",      // Queen", "prince, queen of", "prince of
			"aste",              // Bearer, keeper, slaver
			"avin", "aonar",     // Guardian, guard, shield
			"ayne", "al",        // Lunatic, maniac, manic, rage
			"baste", "gloth",    // Path, walker
			"breena", "antar",   // Matriarch", "patriarch, ruler
			"bryn", "lyn",       // Agent, assassin, killer
			"cice", "roos",      // Born of, child, young
			"cyrl", "axle",      // Ally, companion, friend
			"da", "daer",        // Illusionist, trickster
			"dia", "drin",       // Rogue, stealer
			"diira", "diirn",    // Initiate, sister", "brother
			"dra", "zar",        // Lover, match, mate
			"driira", "driirn",  // Mother", "father, teacher
			"dril", "dorl",      // Knight, sword, warrior
			"e",                 // Servant, slave, vassal
			"eari", "erd",       // Giver, god, patron
			"eyl",               // Archer, arrow, flight, flyer
			"ffyn", "fein",      // Minstrel, singer, song
			"fryn",              // Champion, victor, weapon, weapon of
			"iara", "ica",       // Baron, duke, lady", "lord
			"ice", "eth",        // Obsession, taker, taken
			"idil", "imar",      // Alpha, beginning, creator of, maker
			"iira", "inid",      // Harbinger, herald
			"inidia",            // Secret, wall, warder
			"inil", "in",        // Lady", "lord, rider, steed
			"intra",             // Envoy, messenger, prophet
			"isstra", "atlab",   // Acolyte, apprentice, student
			"ithra", "irahc",    // Dragon, serpent, wyrm
			"jra", "gos",        // Beast, biter, stinger
			"jss",               // Scout, stalker
			"kacha", "kah",      // Beauty, hair, style
			"kiira", "raen",     // Apostle, disciple
			"lay", "dyn",        // Flight, flyer, wing, wings
			"lara", "aghar",     // Cynic, death, end, victim
			"lin",               // Arm, armor, commander
			"lochar",            // Messenger, spider
			"mice", "myr",       // Bone, bones, necromancer, witch
			"mur’ss",            // Shadow, spy, witness
			"na", "nar",         // Adept, ghost, spirit
			"nilee", "olil",     // Corpse, disease, ravager
			"niss", "nozz",      // Chance, gambler, game
			"nitra", "net",      // Kicker, returned, risen
			"nolu",              // Art, artist, expert, treasure
			"olin",              // Ascension, love, lover, lust
			"onia", "onim",      // Rod, staff, token, wand
			"oyss", "omph",      // Binder, judge, law, prison
			"qualyn",            // Ally, caller, kin
			"quarra", "net",     // Horde, host, legion
			"quiri", "oj",       // Aura, cloak, hide, skin
			"ra", "or",          // Fool, game, prey, quarry
			"rae", "rar",        // Secret, seeker, quest
			"raema", "orvir",    // Crafter, fist, hand
			"raena", "olvir",    // Center, haven, home
			"riia", "rak",       // Chaos, storm, tempest
			"ril",               // Bandit, enemy, raider, outlaw
			"riina", "ree",      // Enchanter, mage, spellcaster
			"ryna", "oyn",       // Follower, hired, mercenary
			"ryne", "ryn",       // Blooded, elder, experienced
			"shalee", "ral",     // Abjurer, gaze, watch, watcher
			"ssysn", "rysn",     // Artifact, dweomer, sorcerer, spell
			"stin", "trin",      // Clan, house, merchant, of the house
			"stra", "tran",      // Spider, spinner, weaver
			"tana", "ton",       // Darkness, lurker, prowler
			"thara", "tar",      // Glyph, marker, rune
			"thrae", "olg",      // Charmer, leader, seducer
			"tree", "tel",       // Exile, loner, outcast, pariah
			"tyrr",              // Dagger, poison, poisoner, scorpion
			"ual", "dan",        // Speed, strider
			"ue", "dor",         // Arm, artisan, fingers
			"uit", "dar",        // Breath, voice, word
			"une", "diin",       // Diviner, fate, future, oracle
			"uque",              // Cavern, digger, mole, tunnel
			"urra", "dax",       // Nomad, renegade, wanderer
			"va", "ven",         // Comrade, honor, honored
			"vayas",             // Forge, forger, hammer, smith
			"vyll",              // Punishment, scourge, whip, zealot
			"vyrae", "vyr",      // Mistress", "master, overseer
			"wae", "hrae",       // Heir, inheritor, princess
			"wiira", "hriir",    // Seneschal of, steward
			"wyss", "hrys",      // Best, creator, starter
			"xae", "zaer",       // Orb, rank, ruler, scepter
			"xena", "zen",       // Cutter, gem, jewel, jeweler
			"xyra", "zyr",       // Sage, teller
			"yl",                // Drow, woman", "man
			"ylene", "yln",      // Handmaiden", "squire, maiden", "youth
			"ymma", "inyon",     // Drider, feet, foot, runner
			"ynda", "yrd",       // Captain, custodian, marshal, ranger
			"ynrae", "yraen",    // Heretic, rebel, riot, void
			"vrae",              // Architect, founder, mason
			"yrr",               // Protector, rival, wielder
			"zyne", "zt",        // Finder, hunter
		}
	},
	{
		"l", {
			// Drow House Names Suffixes
			"afin",       // The web
			"ana",        // The night
			"ani",        // The widow
			"ar",         // Poison
			"arn",        // Fire
			"ate",        // The way
			"ath",        // The dragons
			"duis",       // The whip
			"ervs",       // The depths
			"ep",         // The Underdark
			"ett",        // Magic
			"ghym",       // The forgotten ways
			"iryn",       // History
			"lyl",        // The blade
			"mtor",       // The abyss
			"ndar",       // Black hearts
			"neld",       // The arcane
			"rae",        // Fell powers
			"rahel",      // The gods
			"rret",       // The void
			"sek",        // Adamantite
			"th",         // Challenges
			"tlar",       // Mysteries
			"t’tar",      // Victory
			"tyl",        // The pits
			"und",        // The spider's kiss
			"urden",      // The darkness
			"val",        // Silken weaver
			"viir",       // Dominance
			"zynge",      // The ruins
		}
	},
	{
		"e", {
			// Elven Suffixes
			"ae", "nae",                              // whisper
			"ael",                                    // great
			"aer", "aera",                            // singer, song
			"aias", "aia",                            // mate, husband, wife
			"ah", "aha",                              // wand
			"aith", "aira",                           // home
			"al", "ala", "la", "lae", "llae",         // harmony
			"ali",                                    // shadow
			"am", "ama",                              // strider
			"an", "ana", "a", "ani", "uanna",         // make, maker
			"ar", "ara", "ra",                        // man, woman
			"ari", "ri",                              // spring
			"aro", "ro",                              // summer
			"as", "ash", "sah",                       // bow, fletcher
			"ath",                                    // by, of, with
			"avel",                                   // sword
			"brar", "abrar", "ibrar",                 // craft, crafter
			"dar", "adar", "odar",                    // world
			"deth", "eath", "eth",                    // eternal
			"dre",                                    // charm, charming
			"drim", "drimme", "udrim",                // flight, flyer
			"dul",                                    // glade
			"ean",                                    // ride, rider
			"el", "ele", "ela",                       // hawk
			"emar",                                   // honor
			"en",                                     // autumn
			"er", "erl", "ern",                       // winter
			"ess", "esti",                            // elves, elvin
			"evar",                                   // flute
			"fel", "afel", "efel",                    // lake
			"hal", "ahal", "ihal",                    // pale, weak
			"har", "ihar", "uhar",                    // wisdom, wise
			"hel", "ahel", "ihel",                    // sadness, tears
			"ian", "ianna", "ia", "ii", "ion",        // lord, lady
			"iat",                                    // fire
			"ik",                                     // might, mighty
			"il", "iel", "ila", "lie",                // gift, giver
			"im",                                     // duty
			"in", "inar", "ine",                      // sibling, brother, sister
			"ir", "ira", "ire",                       // dusk
			"is", "iss", "ist",                       // scribe, scroll
			"ith", "lath", "lith", "lyth",            // child, young
			"kash", "ashk", "okash",                  // fate
			"ki",                                     // void
			"lan", "lanna", "lean", "olan", "ola",    // son, daughter
			"lam", "ilam", "ulam",                    // fair
			"lar", "lirr",                            // shine
			"las",                                    // wild
			"lian", "lia",                            // master, mistress
			"lis", "elis", "lys",                     // breeze
			"lon", "ellon",                           // chief
			"lyn", "llinn", "lihn",                   // bolt, ray
			"mah", "ma", "mahs",                      // mage
			"mil", "imil", "umil",                    // bond, promise
			"mus",                                    // ally, companion
			"nal", "inal", "onal",                    // distant, far
			"nes",                                    // heart
			"nin", "nine", "nyn",                     // rite, ritual
			"nis", "anis",                            // dawn
			"on", "onna",                             // Keep, Keeper
			"or", "oro",                              // Flower
			"oth", "othi",                            // gate
			"que",                                    // forgotten, lost
			"quis",                                   // branch, limb
			"rah", "rae", "raee",                     // beast
			"rad", "rahd",                            // leaf
			"rail", "ria", "aral", "ral", "ryl",      // hunt, hunter
			"ran", "re", "reen",                      // binding, shackles
			"reth", "rath",                           // arcane
			"ro", "ri", "ron",                        // walker, walks
			"ruil", "aruil", "eruil",                 // noble
			"sal", "isal", "sali",                    // honey, sweet
			"san",                                    // drink, wine
			"sar", "asar", "isar",                    // quest, seeker
			"sel", "asel", "isel",                    // mountain
			"sha", "she", "shor",                     // ocean
			"spar",                                   // fist
			"tae", "itae",                            // beloved, love
			"tas", "itas",                            // wall, ward
			"ten", "iten",                            // spinner
			"thal", "tha", "ethal", "etha",           // heal, healer, healing
			"thar", "ethar", "ithar",                 // friend
			"ther", "ather", "thir",                  // armor, protection
			"thi", "ethil", "thil",                   // wing
			"thus", "thas", "aethus", "aethas",       // harp, harper
			"ti", "eti", "il",                        // eye, sight
			"tril", "tria", "atri", "atril", "atria", // dance, dancer
			"ual", "lua",                             // holy
			"uath", "luth", "uth",                    // lance
			"us", "ua",                               // cousin, kin
			"van", "vanna",                           // forest
			"var", "vara", "avar", "avara",           // father, mother
			"vain", "avain",                          // spirit
			"via", "avia",                            // good fortune, luck
			"vin", "avin",                            // storm
			"wyn",                                    // music, muscian
			"ya",                                     // helm
			"yr", "yn",                               // bringer
			"yth",                                    // folk, people
			"zair", "zara", "azair", "ezara",         // lightning
		}
	},
	{
		"f", {
			// Elven House Name Suffixes
			"altin",      // The branch
			"anea",       // The night
			"annia",      // The willow
			"aear",       // Water
			"arnith",     // Fire
			"atear",      // The way
			"athem",      // The dragons
			"dlues",      // The bow
			"elrvis",     // The leaves
			"eplith",     // The forest
			"ettln",      // Magic
			"ghymn",      // The forgotten ways
			"itryn",      // History
			"lylth",      // The blade
			"mitore",     // The moon
			"nddare",     // The winds
			"neldth",     // The arcane
			"rae",        // Powers of Light
			"raheal",     // The gods
			"rretyn",     // The heavens
			"sithek",     // Adamantite
			"thym",       // Challenges
			"tlarn",      // Mysteries
			"tlithar",    // Victory
			"tylar",      // The healers
			"undlin",     // The lover’s kiss
			"urdrenn",    // The light
			"valsa",      // Silken weaver
			"virrea",     // Success
			"zea",        // The crystal growth
		}
	},

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
