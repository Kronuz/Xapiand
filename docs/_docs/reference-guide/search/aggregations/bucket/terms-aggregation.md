---
title: Terms Aggregation
short_title: Terms
---

{: .note .construction }
_This section is a **work in progress**..._


A _multi-bucket_ value source based aggregation where buckets are dynamically
built - one per unique **term**.

{: .note .caution }
**_Performance_**<br>
Whenever is possible, prefer [Values Aggregation](../values-aggregation) to this type as
it's more efficient.


## Structuring

The following snippet captures the structure of range aggregations:

```json
"<aggregation_name>": {
  "_terms": {
    "_field": "<field_name>"
  },
  ...
}
```

Also supports all other functionality as explained in [Bucket Aggregations](..#structuring).

### Field

The `<field_name>` in the `_field` parameter defines the field on which the
aggregation will act upon.

Assuming the data consists of documents representing bank accounts, as shown in
the sample dataset of [Data Exploration]({{ '/docs/exploration' | relative_url }}#sample-dataset)
section:

{% capture req %}

```json
SEARCH /bank/

{
  "_query": "*",
  "_limit": 0,
  "_check_at_least": 1000,
  "_aggregations": {
    "most_used_terms": {
      "_terms": {
        "_field": "personality"
      }
    }
  }
}
```
{% endcapture %}
{% include curl.html req=req %}

{: .test }

```js
pm.test("response is ok", function() {
  pm.response.to.be.ok;
});
```

{: .test }

```js
pm.test("response is aggregation", function() {
  var jsonData = pm.response.json();
  var expected_doc_count = [
    1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 1000, 900, 894, 894, 883, 870, 839, 803,
    785, 785, 768, 741, 741, 735, 646, 642, 641, 626, 616, 604, 602, 585, 519, 519, 507, 501, 481, 481,
    481, 477, 473, 461, 456, 451, 439, 435, 390, 383, 382, 370, 370, 358, 354, 345, 344, 332, 330, 323,
    320, 301, 292, 288, 286, 277, 273, 269, 263, 259, 259, 259, 259, 257, 253, 251, 248, 247, 247, 242,
    237, 236, 234, 233, 229, 224, 224, 222, 219, 218, 217, 217, 214, 211, 210, 208, 208, 206, 202, 201,
    190, 183, 182, 182, 181, 175, 168, 167, 167, 166, 164, 164, 161, 161, 160, 160, 159, 158, 156, 153,
    153, 152, 151, 148, 148, 147, 147, 144, 143, 143, 141, 140, 140, 140, 140, 137, 136, 134, 133,
    132, 126, 126, 125, 125, 125, 120, 119, 119, 116, 111, 108, 107, 104, 104, 103, 103, 103, 103,
    101, 101, 100, 100, 99, 99, 98, 97, 97, 97, 96, 96, 95, 95, 95, 94, 94, 94, 93, 92, 91, 90, 90,
    90, 90, 89, 89, 89, 88, 87, 86, 86, 86, 86, 85, 85, 84, 84, 83, 83, 83, 83, 82, 82, 80, 79, 79,
    79, 79, 79, 79, 79, 79, 78, 78, 78, 78, 78, 77, 77, 77, 77, 76, 75, 74, 74, 74, 74, 74, 74, 74,
    74, 74, 73, 73, 73, 73, 73, 72, 72, 72, 72, 71, 71, 71, 71, 69, 69, 69, 68, 68, 68, 67, 67, 66,
    65, 64, 64, 63, 63, 62, 62, 62, 61, 61, 61, 61, 61, 57, 56, 56, 56, 56, 55, 53, 52, 52, 52, 51,
    51, 51, 50, 50, 50, 49, 49, 49, 49, 49, 49, 49, 48, 48, 48, 48, 48, 47, 47, 46, 46, 46, 46, 46,
    46, 46, 46, 46, 46, 46, 46, 45, 45, 45, 45, 45, 45, 45, 45, 45, 45, 44, 44, 44, 43, 43, 43, 43,
    43, 43, 42, 42, 42, 42, 42, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 41, 40,
    40, 40, 40, 40, 40, 40, 40, 40, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39, 39,
    39, 38, 38, 38, 38, 38, 38, 38, 37, 37, 37, 37, 37, 37, 37, 36, 36, 36, 36, 36, 36, 36, 36, 36,
    36, 36, 36, 36, 36, 36, 36, 36, 36, 35, 35, 35, 35, 35, 35, 35, 35, 35, 34, 34, 34, 34, 34, 34,
    34, 34, 34, 34, 34, 34, 34, 34, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 33, 32, 32, 32, 32, 32,
    32, 32, 32, 32, 32, 32, 31, 31, 31, 31, 31, 31, 31, 31, 31, 31, 30, 30, 30, 30, 30, 30, 30, 30,
    30, 30, 30, 30, 30, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 29, 28,
    28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27, 27,
    27, 27, 27, 27, 27, 27, 27, 27, 27, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26, 26,
    26, 26, 26, 26, 26, 26, 26, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
    25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24, 24,
    24, 24, 24, 24, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 23, 22, 22, 22, 22, 22,
    22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 22, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21, 21,
    21, 21, 21, 21, 21, 21, 21, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 19, 19,
    19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 19, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18, 18,
    18, 18, 18, 18, 18, 18, 18, 18, 18, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17, 17,
    17, 17, 17, 17, 17, 17, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 15, 15, 15, 15, 15, 15,
    15, 15, 15, 15, 15, 15, 15, 15, 15, 14, 14, 14, 14, 14, 14, 14, 14, 14, 14, 13, 13, 13, 13, 13,
    13, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 12, 11, 11, 11, 11, 11, 11, 11, 11, 11, 10, 10,
    10, 10, 10, 10, 9, 9, 9, 9, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7,
    7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,
    5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,
    4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,
    3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,
    2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
    1, 1];
  var expected_key = [
    "what", "well", "this", "on", "of", "is", "but", "being", "as", "and", "also", "to", "the", "nature",
    "for", "though", "in", "a", "too", "fortunately", "has", "course", "count", "they're", "often", "need",
    "all", "plenty", "by", "when", "that", "with", "his", "he's", "are", "people", "she's", "most", "her",
    "be", "know", "they", "can", "at", "least", "friends", "things", "less", "it", "perfect", "nobody's",
    "there's", "there", "many", "much", "days", "whenever", "traits", "usually", "helps", "very", "deal",
    "help", "focus", "mixed", "far", "you", "unfortunately", "others", "fact", "because", "will", "it's",
    "lot", "habits", "way", "cause", "else", "sides", "like", "times", "just", "pretty", "needed", "if",
    "or", "two", "moods", "make", "especially", "lesser", "fair", "liked", "have", "get", "said", "these",
    "support", "about", "case", "up", "around", "even", "few", "prominent", "than", "more", "behaviors",
    "you'll", "favorable", "say", "fun", "personal", "grievances", "ruin", "could", "good", "not", "from",
    "some", "person", "pleasant", "blows", "tainted", "share", "above", "worst", "so", "ways", "tip",
    "those", "tend", "iceberg", "complex", "should", "only", "never", "true", "occasions", "isn't",
    "which", "kind", "does", "intertwined", "begining", "assistance", "doses", "both", "been", "an",
    "out", "open", "meets", "eye", "desired", "comforting", "do", "balance", "first", "comes", "feeling",
    "down", "nothing", "spoiled", "smaller", "frequently", "check", "best", "that's", "slightly", "rotten",
    "worse", "sour", "mood", "known", "cherished", "oftentimes", "popular", "looks", "deceiving", "admired",
    "levels", "balanced", "annoyance", "cheering", "adored", "while", "takes", "softens", "assures",
    "lighten", "find", "may", "brighter", "ruining", "risk", "regularly", "impulses", "little",
    "characteristics", "friction", "willing", "them", "tained", "ready", "loved", "keep", "forget",
    "beyond", "supply", "shorter", "range", "flaws", "considering", "personally", "hard", "evenings",
    "describe", "mindset", "prevent", "still", "reach", "perhaps", "negatively", "important", "heat",
    "easy", "always", "affect", "see", "rainy", "ideal", "enjoy", "assumed", "spirits", "relift", "exactly",
    "aren't", "strongly", "level", "difficult", "aspects", "time", "tendencies", "overshadowed", "greatest",
    "don't", "company", "mends", "biggest", "shines", "darker", "faults", "character", "uncomfortable",
    "soften", "sure", "compared", "between", "top", "problems", "pose", "loving", "anything", "without",
    "superficial", "sense", "destructive", "among", "their", "dynamic", "thievish", "multiple", "bad",
    "something", "made", "anticipative", "dutiful", "dealing", "adventurous", "take", "petty", "hope",
    "enough", "dislike", "captivating", "almost", "sympathetic", "sociable", "romantic", "clever",
    "apathetic", "turn", "outspoken", "witty", "tolerant", "sensitive", "sadistic", "ruined", "persuasive",
    "helpful", "effects", "doesn't", "disturbing", "crude", "barbaric", "responsible", "perceptive",
    "observant", "irritable", "forthright", "everything", "dominating", "decisive", "deceitful", "callous",
    "possessive", "freethinking", "contemplative", "sentimental", "resourceful", "obnoxious", "look",
    "disciplined", "clear", "uncaring", "troublesome", "hateful", "exciting", "disrespectful", "trusting",
    "somewhat", "self", "practical", "intelligent", "incisive", "humorous", "heroic", "generous",
    "farsighted", "extreme", "creative", "confident", "athletic", "appreciative", "ambitious", "sharing",
    "rational", "predatory", "honorable", "hardworking", "faithful", "curious", "companionably", "active",
    "vindictive", "understanding", "scornful", "relaxed", "planful", "neglectful", "narrow", "monstrous",
    "irrational", "innovative", "honest", "gentle", "forgiving", "compassionate", "charming", "calm",
    "adaptable", "provocative", "optimistic", "methodical", "intuitive", "frightening", "determined",
    "amoral", "surprising", "spontaneous", "miserable", "elegant", "demanding", "blunt", "amusing",
    "upright", "thinking", "responsive", "realistic", "playful", "patient", "passionate", "opportunistic",
    "narcissistic", "loyal", "headed", "friendly", "due", "disruptive", "daring", "considerate", "childish",
    "aspiring", "stable", "purposely", "punctual", "obsessive", "dedicated", "cooperative", "charismatic",
    "caring", "avoided", "sweet", "softened", "shame", "reliable", "pedantic", "orderly", "name", "morbid",
    "modest", "lovable", "grim", "enthusiastic", "energetic", "courageous", "whatever", "warm", "tends",
    "selfless", "reflective", "odd", "humble", "flexible", "endearing", "discreet", "disconcerting",
    "venomous", "power", "pompous", "lighter", "leaderly", "insightful", "individualistic", "imaginative",
    "hungry", "focused", "agonizing", "wise", "respectful", "organized", "objective", "mind", "idealistic",
    "gracious", "earnest", "devious", "arrogant", "º", "we're", "pure", "protective", "natured", "michael",
    "incorruptible", "here", "evil", "efficient", "craziness", "considered", "cheerful", "yet", "words",
    "us", "strokes", "rest", "mentioning", "insulting", "independent", "impatient", "idealism", "i",
    "guess", "grey", "folks", "egocentric", "different", "criminal", "capable", "were", "strike", "stating",
    "show", "pretentious", "precise", "pops", "moments", "insincere", "insensitive", "fast", "discipline",
    "courteous", "chances", "we've", "sly", "notice", "mindedness", "meeting", "long", "loathed",
    "laziness", "ignorant", "hostile", "hatred", "hated", "generosity", "feared", "enemies", "encountered",
    "devil", "desperate", "confidence", "better", "worth", "sugar", "start", "spoil", "soured", "once",
    "notorious", "no", "maliciousness", "lurking", "intentions", "hostility", "greed", "encounters",
    "ego", "dust", "conceited", "coating", "chance", "caused", "careful", "appeal", "who", "unfriendly",
    "unfortunate", "truly", "tolerance", "spoils", "probably", "present", "off", "nights", "negativistic",
    "knows", "itself", "indulgent", "happen", "go", "ever", "ended", "disorderly", "disloyal", "disaster",
    "deviousness", "counted", "come", "carelessness", "back", "away", "small", "slyness", "relationship",
    "redemption", "rays", "light", "insincerity", "incredibly", "healthy", "form", "excitement", "cynical",
    "crassness", "countered", "brilliant", "argumentative", "any", "amounts", "tactless", "meaningful",
    "matters", "ignorance", "hate", "forever", "disliked", "dishonesty", "despised", "created", "crazy",
    "courage", "brutal", "argumentativeness", "abrasiveness", "unfriendliness", "shallowness", "scorned",
    "responsibility", "prejudiced", "negativity", "modesty", "inconsideration", "inconsiderate", "gloom",
    "faithfulness", "doom", "creativity", "counteracted", "brutish", "beast", "thoughtless", "shallow",
    "reliability", "really", "perversions", "lessened", "irresponsible", "irresponsibility", "intelligence",
    "imagination", "greedy", "gentleness", "energy", "empathetic", "elegance", "cruelty", "completely",
    "careless", "again", "abrasive", "whole", "tactlessness", "skills", "sentimentality", "pretentiousness",
    "organization", "lazy", "humor", "honor", "disloyalty", "dishonest", "discretion", "cooperation",
    "coarse", "beacon", "unstable", "relationships", "loyalty", "instability", "helping", "hand", "halt",
    "envious", "condemned", "compassion", "coldness", "came", "athleticism", "aspirations", "adaptability",
    "within", "redeeming", "qualities", "perverse", "malicious", "insensitivity", "independence",
    "imprudence", "humbleness", "gave", "flexibility", "farsightedness", "darkness", "cruel", "crass",
    "cheerfulness", "charm", "brilliance", "ambitions", "aggression", "witness", "warmness", "treachery",
    "sad", "respect", "resourcefulness", "reality", "precision", "playfulness", "patience", "kept",
    "jennifer", "intolerant", "indulgence", "honesty", "headedness", "enthusiasm", "earnestness",
    "disorderliness", "christopher", "anger", "thoughtlessness", "spontaneity", "selfish", "order",
    "openness", "minded", "leadership", "grace", "friendliness", "envy", "david", "charisma", "wit",
    "treacherous", "selfishness", "punctuality", "planning", "pain", "optimism", "let", "left", "imprudent",
    "great", "curiosity", "coarseness", "angry", "alone", "smith", "selflessness", "prejudices",
    "intuition", "impatience", "empathy", "desperation", "dedication", "capabilities", "aggressive",
    "miller", "matthew", "jones", "james", "insight", "cold", "wisdom", "william", "thomas", "taylor",
    "realism", "passion", "michelle", "johnson", "jason", "determination", "companionship", "anderson",
    "robert", "responsiveness", "objectivity", "mark", "kindness", "intolerance", "garcia", "efficiency",
    "brown", "wilson", "williams", "steven", "martin", "davis", "courtesy", "ryan", "robinson", "rivera",
    "nicholas", "kimberly", "kevin", "jessica", "individualism", "amanda", "travis", "timothy", "susan",
    "stephanie", "moore", "mary", "martinez", "lisa", "kenneth", "kelly", "eric", "daniel", "christina",
    "thompson", "stone", "reed", "justin", "julie", "john", "jeffrey", "hill", "baker", "ashley", "walker",
    "tracy", "richard", "melissa", "megan", "maria", "keith", "jackson", "hernandez", "elizabeth", "douglas",
    "christine", "brenda", "bailey", "amy", "aaron", "wright", "white", "scott", "sarah", "rachel",
    "perez", "patrick", "nelson", "lee", "karen", "joseph", "jordan", "jensen", "harris", "gregory",
    "gardner", "emily", "courtney", "collins", "chelsea", "brian", "anthony", "anna", "allen", "wood",
    "wagner", "tina", "theresa", "teresa", "tammy", "sullivan", "stewart", "simmons", "sanchez", "samantha",
    "russell", "ronald", "rodriguez", "perry", "paul", "pamela", "obrien", "nicole", "nancy", "murphy",
    "moreno", "mitchell", "michele", "mcdonald", "marc", "laura", "kathleen", "joshua", "jill", "jamie",
    "jacob", "henry", "hall", "gonzalez", "gomez", "gary", "figueroa", "donald", "dawn", "darrell", "charles",
    "carr", "brandon", "bradley", "adams", "zachary", "young", "woods", "weaver", "veronica", "tyler",
    "turner", "tonya", "tiffany", "thornton", "tara", "tanner", "stevens", "stephen", "shannon", "sara",
    "rubio", "rogers", "rodney", "roberts", "reyes", "rebecca", "randall", "ramos", "ramirez", "price",
    "pierce", "phillips", "peterson", "peters", "peter", "patricia", "olivia", "norton", "nichols", "nguyen",
    "nathan", "myers", "morgan", "mason", "lopez", "linda", "lewis", "latoya", "kyle", "kristina", "kim",
    "kathy", "juan", "jose", "jim", "jeffery", "isaac", "hodge", "heather", "hayes", "hansen", "hannah",
    "gordon", "gonzales", "glenn", "gina", "gilbert", "gibson", "george", "garrett", "frederick", "frank",
    "fisher", "erika", "edward", "dylan", "durham", "dunn", "desiree", "dennis", "cynthia", "cunningham",
    "craig", "clark", "catherine", "casey", "carter", "carrie", "caldwell", "brooks", "brandy", "blake",
    "black", "bishop", "berg", "benjamin", "bell", "avila", "austin", "andrew", "alvarez", "zimmerman",
    "zamora", "york", "yang", "williamson", "whitney", "whitehead", "wheeler", "webb", "watts",
    "waters", "washington", "ward", "vega", "valerie", "trujillo", "trevor", "tracey", "torres", "todd",
    "terry", "sweeney", "swanson", "stuart", "steve", "stacy", "soto", "sosa", "solis", "singleton", "sims",
    "shelly", "shawn", "shaw", "sharon", "seth", "sean", "schroeder", "santiago", "sandra", "sandoval",
    "samuel", "salazar", "russo", "roger", "rios", "riley", "ricky", "richardson", "rhodes", "reynolds",
    "renee", "reilly", "raymond", "phelps", "peggy", "patterson", "parker", "pace", "olson", "oconnell",
    "newman", "neal", "murray", "montgomery", "monica", "molina", "mendoza", "melanie", "mckinney", "lucas",
    "lowery", "logan", "leon", "lawson", "lauren", "kristine", "kristen", "king", "kelli", "kayla",
    "katherine", "juarez", "jonathan", "jimenez", "jesus", "jeremy", "jay", "javier", "jack", "hughes",
    "hines", "hickman", "hensley", "hawkins", "hamilton", "griffin", "gray", "garza", "garrison", "fritz",
    "frey", "freeman", "fowler", "fields", "farmer", "esparza", "duran", "donovan", "donna", "diane",
    "diana", "derrick", "derek", "denise", "debra", "deborah", "dana", "cruz", "corey", "connie", "colin",
    "coleman", "cole", "clinton", "claire", "cathy", "castillo", "carlos", "campbell", "camacho", "caitlin",
    "burnett", "buck", "brittany", "brent", "bowers", "billy", "barnett", "barbara", "banks", "atkins",
    "armstrong", "angela", "andrea", "alicia", "alice", "alexander", "abigail", "wolf", "winters",
    "willie", "wilkinson", "wilkins", "wilkerson", "west", "wesley", "wendy", "welch", "weiss", "weeks",
    "weber", "watson", "watkins", "warren", "warner", "walton", "walter", "waller", "wallace", "vincent",
    "villa", "victoria", "victor", "vernon", "velazquez", "velasquez", "vasquez", "vargas", "vanessa",
    "valdez", "tucker", "trevino", "tracie", "tony", "tommy", "tim", "theodore", "terri", "tanya",
    "tami", "tamara", "tabitha", "stokes", "stevenson", "stephens", "steele", "stark", "stanley", "stacey",
    "spencer", "snyder", "simon", "silva", "sherry", "sheri", "shelby", "sheila", "sheena", "shaun",
    "shaffer", "schwartz", "schneider", "schmidt", "sawyer", "sanford", "sanders", "sally", "ruiz",
    "roy", "rowe", "ross", "rose", "rosales", "roman", "rodgers", "robin", "robertson", "roberto", "rivers",
    "rivas", "rickey", "richmond", "richards", "rhonda", "reid", "reginald", "reeves", "ray", "rangel",
    "randy", "randolph", "ralph", "quinn", "pugh", "pruitt", "potts", "pope", "pineda", "philip", "pham",
    "perkins", "penny", "pennington", "pearson", "paula", "parsons", "page", "padilla", "owens", "owen",
    "ortiz", "ortega", "oliver", "oconnor", "ochoa", "noah", "nicholson", "navarro", "natalie", "munoz",
    "moyer", "morse", "morrison", "morris", "montoya", "miranda", "mills", "miles", "miguel", "meza",
    "melody", "mejia", "meghan", "medina", "mcmillan", "mcmahon", "mckee", "mcgrath", "mcgee", "mccoy",
    "mcclain", "mccann", "mccall", "mcbride", "mayo", "mayer", "maxwell", "mathew", "massey", "martha",
    "marshall", "marsh", "margaret", "marcus", "manning", "mahoney", "madison", "mack", "luna", "luke",
    "lowe", "lori", "lloyd", "liu", "lindsey", "li", "leroy", "leblanc", "le", "lawrence", "larson",
    "larsen", "lance", "kristy", "kristin", "krause", "kramer", "knapp", "klein", "kirby", "kidd",
    "kerr", "kent", "kennedy", "kendra", "kemp", "kelsey", "katrina", "katelyn", "kara", "kane", "kaitlyn",
    "julia", "judith", "jorge", "jonathon", "johnston", "johns", "johnny", "joel", "joe", "jody", "jodi",
    "joanne", "jenna", "jenkins", "jefferson", "jasmine", "jared", "janet", "jaime", "jacqueline",
    "jacobson", "jacobs", "isabella", "ingram", "ibarra", "ian", "hull", "huff", "huber", "hubbard",
    "howell", "howard", "houston", "horton", "horn", "hooper", "hood", "holmes", "holly", "hicks",
    "herrera", "heidi", "hebert", "haynes", "hartman", "hart", "harrell", "harper", "hardy", "hanna",
    "hampton", "hammond", "hale", "gwendolyn", "guzman", "gutierrez", "griffith", "gregg", "greer",
    "green", "graves", "grant", "goodman", "gloria", "geoffrey", "gentry", "gay", "gamble", "galvan",
    "gabriella", "gabriela", "fuentes", "friedman", "frazier", "franco", "francisco", "frances", "forbes",
    "flynn", "flores", "fleming", "ferguson", "felicia", "faulkner", "farrell", "ewing", "evelyn",
    "evans", "estrada", "espinoza", "escobar", "ernest", "erik", "erickson", "ellison", "ellis", "edwin",
    "edwards", "earl", "dyer", "dwayne", "dustin", "dunlap", "duncan", "duarte", "duane", "doyle",
    "dorsey", "dodson", "dixon", "dillon", "diaz", "destiny", "delgado", "decker", "debbie", "deanna",
    "dean", "day", "davidson", "daugherty", "daryl", "danny", "daniels", "dan", "dakota", "curtis",
    "crystal", "crosby", "crawford", "crane", "cox", "costa", "cory", "cortez", "cooley", "cooke",
    "cook", "conway", "contreras", "conrad", "connor", "colton", "colon", "cochran", "clayton", "clarke",
    "cisneros", "cindy", "chung", "christian", "christensen", "chris", "chloe", "chen", "chelsey",
    "chavez", "chad", "castro", "cassandra", "carol", "carney", "carlson", "carl", "cannon", "candice",
    "candace", "cameron", "caleb", "caitlyn", "cain", "byrd", "bush", "burns", "bullock", "buckley",
    "buchanan", "bryant", "bryan", "brooke", "brittney", "bridget", "brianna", "brewer", "brett",
    "brendan", "brandi", "branch", "bradshaw", "brad", "boyer", "boyd", "bowman", "booth", "bonilla",
    "blackwell", "blackburn", "beverly", "betty", "beth", "berry", "bernard", "berger", "bender",
    "becker", "beck", "beasley", "bauer", "barron", "barnes", "barker", "barber", "ball", "atkinson",
    "arroyo", "arnold", "ariel", "arellano", "april", "anne", "anita", "angelica", "angel", "andrews",
    "andre", "andrade", "amber", "alyssa", "allison", "alisha", "alexandria", "alexa", "albert",
    "alan", "aguilar", "adriana", "adam", "abbott"];
  for (var i = 0; i < expected_doc_count.length; ++i) {
    pm.expect(jsonData.aggregations.most_used_terms[i]._doc_count).to.equal(expected_doc_count[i]);
    pm.expect(jsonData.aggregations.most_used_terms[i]._key).to.equal(expected_key[i]);
  }
});
```

Response:

```json
{
  "aggregations": {
    "_doc_count": 1000,
    "most_used_terms": [
      ...
    ]
  }, ...
}
```

### Ordering

By default, the returned buckets are sorted by their `_doc_count` descending,
though the order behaviour can be controlled using the `_sort` setting. Supports
the same order functionality as explained in [Bucket Ordering](..#ordering).
