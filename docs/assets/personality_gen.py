#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import absolute_import, unicode_literals

import json
import random
from collections import OrderedDict

import names

nm1 = ["A lot can be said of", "Many things can be said of", "There's a lot to say about", "It takes a while to get to know", "It's easy to get to know an open person like", "A lot can be assumed when you first see", "There's more than meets the eye in the case of", "Looks can be deceiving when it comes", "It's hard to describe a complex person like", "Few know the true"]
nm2 = ["above else know that", "if nothing else you should know", "at the very least", "two things you'll never forget are that", "the biggest two things to know are that", "the two traits most people enjoy the most are that", "perhaps most important is that", "most know that above all else", "if there's anything you should know it's that", "at the very least you'll find out"]
nm3 = ["those are often overshadowed by tendencies of", "these are in a way balance by", "they're often slightly tainted by a mindset of", "they're less prominent and often intertwined with", "they're far less prominent, especially compared to impulses of", "in smaller doses and they're often spoiled by habits of", "far less strongly and often mixed with", "in a way they're lesser traits and tained by behaviors of", "they're in shorter supply, especially considering they're mixed with", "they're tainted by and mixed with habits of"]
nm4 = ["most well-liked for", "pretty much known for", "pretty much loved for", "often adored for", "often admired for", "kind of cherished for", "most popular for", "so well-liked for"]
nm5 = ["People often", "Friends usually", "Friends tend to", "Oftentimes people will", "There are many times when friends", "People regularly", "Friends frequently", "On many occasions people will"]
nm6 = ["when they're feeling down", "whenever they need cheering up", "in times of need", "whenever they need help", "when they're in need of support", "whenever they need assistance or help", "especially when they need comforting or support"]
nm7 = ["plenty of less favorable traits", "a fair share of lesser days", "plenty of rainy days", "rotten moods and days", "less pleasant traits", "plenty of less favorable characteristics", "a share of darker sides to deal with", "plenty of lesser desired aspects", "a range of flaws to deal with", "plenty of character faults"]
nm8 = ["tend to get in the way", "aren't exactly fun to deal with", "do cause plenty of friction", "pose plenty of problems", "sour the mood many a time", "risk ruining pleasant moods", "can cause things to heat up", "don't make for the greatest company", "cause plenty of grievances", "are far from ideal"]
nm9 = [", both personally and for others", " on often personal levels", ", though more on a personal level than for others", " and reach all around", ", much to the annoyance of others", " and could ruin plenty of evenings", " and just affect all around negatively", " even at the best of times", " and beyond what people are willing to deal with", " and make things uncomfortable to say the least"]
nm10 = ["helps make sure these days are few and far between", "assures this isn't the case very often", "is usually there to soften the blows", "usually softens the worst of it", "shines brighter on most days", "helps prevent most of these grievances", "is usually there to help mends things when needed", "helps keep them in check for at least a little", "helps lighten the blows and moods when needed", "is there to relift spirits when needed"]
nm11 = ["There's plenty people hate about", "A whole lot can be said about", "There's plenty to say about", "It doesn't take long to dislike", "Plenty of people will dislike", "Many hateful words can be said about", "Unfortunately very few people like", "Looks can be deceiving when dealing with", "You may not notice this when first meeting", "There's more than meets the eye in the case of"]
nm12 = ["To make things worse", "Never mind the fact", "Let alone the fact", "That's without even mentioning", "To top it off", "To make matters worse", "As if that's not enough", "There there's the fact", "On top of that", "That's without even stating"]
nm13 = ["at least those are kept somewhat in check by habits of being", "fortunately they're balanced out slightly by being", "their effects are at least lessened by impulses of being", "at the very least they're countered somewhat by being", "their worst is softened by behaviors of being", "in an odd way they're balanced by habits of being", "at least they're not as bad due to intertwined habits of being", "fortunately they're mixed with behaviors of being", "they're not as prominent and counteracted by habits of being", "at least they only show in smaller doses and mixed with behaviors of being"]
nm14 = ["pretty much º", "notorious", "most disliked", "pretty much hated", "purposely avoided", "almost feared", "most condemned", "loathed", "often despised", "often scorned"]
nm15 = ["Plenty of days have been ruined", "Multiple enemies have been made", "Plenty of relationships came to a halt", "Multiple nights have ended in disaster", "Even careful encounters have been ruined", "There's no sugar coating the effects caused", "There's a great deal of pain left on all sides", "Any healthy relationship is made incredibly difficult", "Even the best intentions have been soured", "There's not much meaningful that can be created"]
nm16 = ["much to the annoyance of others", "which is unfortunate in and of itself", "but there's not much you can really do about it", "which is too bad for all sides", "but different strokes for different folks I guess", "which is far from desired, but so be it", "which plenty have been a witness to", "which is a true shame for both sides", "but that may just be the nature of the beast", "as sad of a reality as this may be"]
nm17 = ["does gave some redeeming qualities", "does have some rays of light", "does have some lighter sides", "isn't completely rotten", "has better sides too", "doesn't turn everything to dust", "does have some brighter sides within the darkness", "is still a complex being with good sides as well", "is complex and grey like the rest of us", "does have some endearing sides"]
nm18 = ["at the very least", "if nothing else", "to name a few", "for a start", "among others", "in good amounts", "among true friends", "when around good friends", "if you look for it", "even if in small doses"]
nm19 = ["it's not like we're dealing with pure evil here", "it's not like we've encountered the devil", "all considered it could be much worse", "there's still a beacon of hope", "it isn't all doom and gloom", "who knows what could happen", "that's at least some form of redemption", "there's at least that to look for and appeal to", "it could be worth to take a chance just once", "there is still some hope yet"]
nm20 = ["tends to ruin whatever good comes from those traits", "often spoils the fun that may have come from those traits", "is always there to ruin everything again", "can always be counted on to turn things back for the worse", "is usually lurking and ready to spoil the fun", "is almost ever present and ready to sour the mood", "often pops up fast enough to ruin the chances of something good", "is always there and ready to strike at the worst moments", "will forever be something to deal with", "will probably never truly go away"]
nm21 = [["active", "active nature"], ["adaptable", "adaptability"], ["adventurous", "adventurous nature"], ["ambitious", "ambitions"], ["amusing", "amusing nature"], ["anticipative", "anticipative nature"], ["appreciative", "appreciative nature"], ["aspiring", "aspirations"], ["athletic", "athleticism"], ["balanced", "sense of balance"], ["brilliant", "brilliance"], ["calm", "calm nature"], ["capable", "capabilities"], ["captivating", "captivating nature"], ["caring", "caring nature"], ["charismatic", "charisma"], ["charming", "charm"], ["cheerful", "cheerfulness"], ["clear-headed", "clear-headedness"], ["clever", "clever nature"], ["companionably", "companionship"], ["compassionate", "compassion"], ["confident", "confidence"], ["considerate", "considerate nature"], ["contemplative", "contemplative nature"], ["cooperative", "cooperation"], ["courageous", "courage"], ["courteous", "courtesy"], ["creative", "creativity"], ["curious", "curiosity"], ["daring", "daring nature"], ["decisive", "decisive nature"], ["dedicated", "dedication"], ["determined", "determination"], ["disciplined", "discipline"], ["discreet", "discretion"], ["dutiful", "dutiful nature"], ["dynamic", "dynamic nature"], ["earnest", "earnestness"], ["efficient", "efficiency"], ["elegant", "elegance"], ["empathetic", "empathy"], ["energetic", "energy"], ["enthusiastic", "enthusiasm"], ["exciting", "excitement"], ["faithful", "faithfulness"], ["farsighted", "farsightedness"], ["flexible", "flexibility"], ["focused", "focus"], ["forgiving", "forgiving nature"], ["forthright", "forthright nature"], ["freethinking", "freethinking nature"], ["friendly", "friendliness"], ["fun-loving", "fun-loving nature"], ["generous", "generosity"], ["gentle", "gentleness"], ["good-natured", "good nature"], ["gracious", "grace"], ["hardworking", "hardworking nature"], ["helpful", "helping hand"], ["heroic", "heroic nature"], ["honest", "honesty"], ["honorable", "honor"], ["humble", "humbleness"], ["humorous", "sense of humor"], ["idealistic", "idealism"], ["imaginative", "imagination"], ["incisive", "incisive nature"], ["incorruptible", "incorruptible nature"], ["independent", "independence"], ["individualistic", "individualism"], ["innovative", "innovative nature"], ["insightful", "insight"], ["intelligent", "intelligence"], ["intuitive", "intuition"], ["kind", "kindness"], ["leaderly", "leadership"], ["lovable", "loving nature"], ["loyal", "loyalty"], ["methodical", "methodical nature"], ["modest", "modesty"], ["objective", "objectivity"], ["observant", "observant nature"], ["open", "openness"], ["optimistic", "optimism"], ["orderly", "sense of order"], ["organized", "organization skills"], ["outspoken", "outspoken nature"], ["passionate", "passion"], ["patient", "patience"], ["perceptive", "perceptive nature"], ["persuasive", "persuasive nature"], ["planful", "planning"], ["playful", "playfulness"], ["practical", "practical thinking"], ["precise", "precision"], ["protective", "protective nature"], ["punctual", "punctuality"], ["rational", "rational thinking"], ["realistic", "realism"], ["reflective", "reflective thinking"], ["relaxed", "relaxed nature"], ["reliable", "reliability"], ["resourceful", "resourcefulness"], ["respectful", "respect"], ["responsible", "responsibility"], ["responsive", "responsiveness"], ["romantic", "romantic nature"], ["selfless", "selflessness"], ["sensitive", "sensitive nature"], ["sentimental", "sentimentality"], ["sharing", "sharing nature"], ["sociable", "sociable nature"], ["spontaneous", "spontaneity"], ["stable", "stable nature"], ["surprising", "surprising nature"], ["sweet", "sweet nature"], ["sympathetic", "sympathetic nature"], ["tolerant", "tolerance"], ["trusting", "trusting nature"], ["understanding", "understanding nature"], ["upright", "upright nature"], ["warm", "warmness"], ["wise", "wisdom"], ["witty", "wit"]]
nm22 = [["abrasive", "abrasiveness"], ["aggressive", "aggression"], ["agonizing", "agonizing nature"], ["amoral", "amoral nature"], ["angry", "anger"], ["apathetic", "apathetic nature"], ["argumentative", "argumentativeness"], ["arrogant", "arrogant nature"], ["barbaric", "barbaric nature"], ["blunt", "blunt nature"], ["brutal", "brutish ways"], ["callous", "callous nature"], ["careless", "carelessness"], ["childish", "childish nature"], ["coarse", "coarseness"], ["cold", "coldness"], ["conceited", "conceited nature"], ["crass", "crassness"], ["crazy", "craziness"], ["criminal", "criminal nature"], ["crude", "crude ways"], ["cruel", "cruelty"], ["cynical", "cynical nature"], ["deceitful", "deceitful ways"], ["demanding", "demanding nature"], ["desperate", "desperation"], ["destructive", "destructive nature"], ["devious", "deviousness"], ["difficult", "difficult nature"], ["disconcerting", "disconcerting nature"], ["dishonest", "dishonesty"], ["disloyal", "disloyalty"], ["disorderly", "disorderliness"], ["disrespectful", "disrespectful nature"], ["disruptive", "disruptive nature"], ["disturbing", "disturbing nature"], ["dominating", "dominating nature"], ["egocentric", "ego"], ["envious", "envy"], ["extreme", "extreme nature"], ["frightening", "frightening nature"], ["greedy", "greed"], ["grim", "grim ways"], ["hateful", "hatred"], ["hostile", "hostility"], ["ignorant", "ignorance"], ["impatient", "impatience"], ["imprudent", "imprudence"], ["inconsiderate", "inconsideration"], ["insensitive", "insensitivity"], ["insincere", "insincerity"], ["insulting", "insulting nature"], ["intolerant", "intolerance"], ["irrational", "irrational nature"], ["irresponsible", "irresponsibility"], ["irritable", "irritable nature"], ["lazy", "laziness"], ["malicious", "maliciousness"], ["miserable", "miserable nature"], ["monstrous", "monstrous nature"], ["morbid", "morbid nature"], ["narcissistic", "narcissistic nature"], ["narrow-minded", "narrow-mindedness"], ["negativistic", "negativity"], ["neglectful", "neglectful nature"], ["obnoxious", "obnoxious nature"], ["obsessive", "obsessive nature"], ["opportunistic", "opportunistic ways"], ["pedantic", "pedantic nature"], ["perverse", "perversions"], ["petty", "petty nature"], ["pompous", "pompous nature"], ["possessive", "possessive nature"], ["power-hungry", "power-hungry ways"], ["predatory", "predatory nature"], ["prejudiced", "prejudices"], ["pretentious", "pretentiousness"], ["provocative", "provocative nature"], ["sadistic", "sadistic ways"], ["scornful", "scornful nature"], ["self-indulgent", "self-indulgence"], ["selfish", "selfishness"], ["shallow", "shallowness"], ["sly", "slyness"], ["superficial", "superficial nature"], ["tactless", "tactlessness"], ["thievish", "thievish nature"], ["thoughtless", "thoughtlessness"], ["treacherous", "treachery"], ["troublesome", "troublesome nature"], ["uncaring", "uncaring nature"], ["unfriendly", "unfriendliness"], ["unstable", "instability"], ["venomous", "venomous nature"], ["vindictive", "vindictive nature"]]
nm23 = ["Aaron", "Adam", "Aidan", "Aiden", "Alex", "Alexander", "Alfie", "Andrew", "Anthony", "Archie", "Arthur", "Ashton", "Bailey", "Ben", "Benjamin", "Billy", "Blake", "Bobby", "Bradley", "Brandon", "Caleb", "Callum", "Cameron", "Charles", "Charlie", "Christopher", "Cody", "Connor", "Corey", "Daniel", "David", "Declan", "Dexter", "Dominic", "Dylan", "Edward", "Elliot", "Ellis", "Ethan", "Evan", "Ewan", "Finlay", "Finley", "Frankie", "Freddie", "Frederick", "Gabriel", "George", "Harley", "Harrison", "Harry", "Harvey", "Hayden", "Henry", "Isaac", "Jack", "Jackson", "Jacob", "Jake", "James", "Jamie", "Jay", "Jayden", "Jenson", "Joe", "Joel", "John", "Jonathan", "Jordan", "Joseph", "Josh", "Joshua", "Jude", "Kai", "Kayden", "Kian", "Kieran", "Kyle", "Leo", "Leon", "Lewis", "Liam", "Logan", "Louie", "Louis", "Luca", "Lucas", "Luke", "Mason", "Matthew", "Max", "Michael", "Morgan", "Nathan", "Nicholas", "Noah", "Oliver", "Ollie", "Oscar", "Owen", "Abbie", "Abby", "Abigail", "Aimee", "Alexandra", "Alice", "Alicia", "Alisha", "Amber", "Amelia", "Amelie", "Amy", "Anna", "Ava", "Bella", "Bethany", "Brooke", "Caitlin", "Cerys", "Charlie", "Charlotte", "Chelsea", "Chloe", "Courtney", "Daisy", "Danielle", "Demi", "Eleanor", "Eliza", "Elizabeth", "Ella", "Ellie", "Eloise", "Elsie", "Emilia", "Emily", "Emma", "Erin", "Esme", "Eva", "Eve", "Evelyn", "Evie", "Faith", "Freya", "Georgia", "Georgina", "Grace", "Gracie", "Hannah", "Harriet", "Heidi", "Hollie", "Holly", "Imogen", "Isabel", "Isabella", "Isabelle", "Isla", "Isobel", "Jade", "Jasmine", "Jennifer", "Jessica", "Jodie", "Julia", "Kate", "Katherine", "Katie", "Kayla", "Kayleigh", "Keira", "Lacey", "Lara", "Laura", "Lauren", "Layla", "Leah", "Lexi", "Lexie", "Libby", "Lilly", "Lily", "Lola", "Louise", "Lucy", "Lydia", "Maddison", "Madeleine", "Madison", "Maisie", "Maisy", "Maria", "Martha", "Matilda", "Maya", "Megan", "Melissa", "Mia", "Mollie"]
nm24 = ["Adams", "Allen", "Anderson", "Andrews", "Armstrong", "Atkinson", "Austin", "Bailey", "Baker", "Ball", "Barker", "Barnes", "Barrett", "Bates", "Baxter", "Bell", "Bennett", "Berry", "Black", "Booth", "Bradley", "Brooks", "Brown", "Burke", "Burns", "Burton", "Butler", "Byrne", "Campbell", "Carr", "Carter", "Chambers", "Chapman", "Clark", "Clarke", "Cole", "Collins", "Cook", "Cooke", "Cooper", "Cox", "Cunningham", "Davidson", "Davies", "Davis", "Dawson", "Day", "Dean", "Dixon", "Doyle", "Duncan", "Edwards", "Elliott", "Ellis", "Evans", "Fisher", "Fletcher", "Foster", "Fox", "Francis", "Fraser", "Gallagher", "Gardner", "George", "Gibson", "Gill", "Gordon", "Graham", "Grant", "Gray", "Green", "Griffiths", "Hall", "Hamilton", "Harper", "Harris", "Harrison", "Hart", "Harvey", "Hawkins", "Hayes", "Henderson", "Hill", "Holland", "Holmes", "Hopkins", "Houghton", "Howard", "Hudson", "Hughes", "Hunt", "Hunter", "Hussain", "Jackson", "James", "Jenkins", "John", "Johnson", "Johnston", "Jones", "Jordan", "Kaur", "Kelly", "Kennedy", "Khan", "King", "Knight", "Lane", "Lawrence", "Lawson", "Lee", "Lewis", "Lloyd", "Lowe", "Macdonald", "Marsh", "Marshall", "Martin", "Mason", "Matthews", "May", "Mccarthy", "Mcdonald", "Miller", "Mills", "Mitchell", "Moore", "Morgan", "Morris", "Moss", "Murphy", "Murray", "Newman", "Nicholson", "Owen", "Palmer", "Parker", "Parry", "Patel", "Pearce", "Pearson", "Perry", "Phillips", "Poole", "Porter", "Powell", "Price", "Read", "Rees", "Reid", "Reynolds", "Richards", "Richardson", "Riley", "Roberts", "Robertson", "Robinson", "Rogers", "Rose", "Ross", "Russell", "Ryan", "Saunders", "Scott", "Sharp", "Shaw", "Simpson", "Smith", "Spencer", "Stevens", "Stewart", "Stone", "Sutton", "Taylor", "Thomas", "Thompson", "Thomson", "Turner", "Walker", "Wallace", "Walsh", "Ward", "Watson", "Watts", "Webb", "Wells", "West", "White", "Wilkinson", "Williams", "Williamson", "Willis", "Wilson", "Wood", "Woods", "Wright", "Young"]
nm25 = ["he's", "his", "His", "He's"]


def nameGen(type_person, first_name, last_name, gender):
    rnd13b = first_name
    rnd14b = last_name
    if gender == 'female':
        nm25 = ["she's", "her", "Her", "She's"]
    else:
        nm25 = ["he's", "his", "His", "He's"]
    if type_person == 1:
        rnd1 = random.randrange(0, len(nm11))
        rnd2 = random.randrange(0, len(nm12))
        rnd3 = random.randrange(0, len(nm13))
        rnd4 = random.randrange(0, len(nm14))
        rnd5 = random.randrange(0, len(nm15))
        rnd6 = random.randrange(0, len(nm16))
        rnd7 = random.randrange(0, len(nm17))
        rnd8 = random.randrange(0, len(nm18))
        rnd9 = random.randrange(0, len(nm19))
        rnd10 = random.randrange(0, len(nm20))
        rnd11 = random.randrange(0, len(nm22))
        rnd12 = random.randrange(0, len(nm22))

        while rnd11 == rnd12:
            rnd12 = random.randrange(0, len(nm22))

        rnd13 = random.randrange(0, len(nm22))
        while rnd11 == rnd13 or rnd12 == rnd13:
            rnd13 = random.randrange(0, len(nm22))

        rnd14 = random.randrange(0, len(nm22))
        while rnd11 == rnd14 or rnd12 == rnd14 or rnd13 == rnd14:
            rnd14 = random.randrange(0, len(nm22))

        rnd15 = random.randrange(0, len(nm22))
        while rnd11 == rnd15 or rnd12 == rnd15 or rnd13 == rnd15 or rnd14 == rnd15:
            rnd15 = random.randrange(0, len(nm22))

        rnd16 = random.randrange(0, len(nm21))
        rnd17 = random.randrange(0, len(nm22))
        rnd18 = random.randrange(0, len(nm21))
        rnd19 = random.randrange(0, len(nm21))
        while rnd18 == rnd19:
            rnd19 = random.randrange(0, len(nm21))

        name = nm11[rnd1] + " " + rnd13b + " " + rnd14b + ", but the fact " + nm25[0] + " " + nm22[rnd11][0] + " and " + nm22[rnd12][0] + " is just the tip of the iceberg. " + nm12[rnd2] + " " + nm25[0] + " also " + nm22[rnd13][0] + ", " + nm22[rnd14][0] + " and " + nm22[rnd15][0] + ", but " + nm13[rnd2] + " " + nm21[rnd16][0] + " as well."
        name2 = "But focus on " + nm25[1] + " as this is what " + nm25[0] + " " + nm14[rnd4] + ". " + nm15[rnd5] + " because of this and " + nm25[1] + " " + nm22[rnd17][1] + ", " + nm16[rnd6] + "."
        name3 = "Fair is fair though, " + rnd13b + " " + nm17[rnd7] + ". " + nm25[3] + " " + nm21[rnd18][0] + " and " + nm21[rnd19][0] + " " + nm18[rnd8] + ", " + nm19[rnd9] + "."
        name4 = "Unfortunately " + nm25[1] + " " + nm22[rnd12][1] + " " + nm20[rnd10] + "."
    else:
        rnd1 = random.randrange(0, len(nm1))
        rnd2 = random.randrange(0, len(nm2))
        rnd3 = random.randrange(0, len(nm3))
        rnd4 = random.randrange(0, len(nm4))
        rnd5 = random.randrange(0, len(nm5))
        rnd6 = random.randrange(0, len(nm6))
        rnd7 = random.randrange(0, len(nm7))
        rnd8 = random.randrange(0, len(nm8))
        rnd9 = random.randrange(0, len(nm9))
        rnd10 = random.randrange(0, len(nm10))
        rnd11 = random.randrange(0, len(nm21))
        rnd12 = random.randrange(0, len(nm21))

        while rnd11 == rnd12:
            rnd12 = random.randrange(0, len(nm21))

        rnd13 = random.randrange(0, len(nm21))
        while rnd11 == rnd13 or rnd12 == rnd13:
            rnd13 = random.randrange(0, len(nm21))

        rnd14 = random.randrange(0, len(nm21))
        while rnd11 == rnd14 or rnd12 == rnd14 or rnd13 == rnd14:
            rnd14 = random.randrange(0, len(nm21))

        rnd15 = random.randrange(0, len(nm21))
        while rnd11 == rnd15 or rnd12 == rnd15 or rnd13 == rnd15 or rnd14 == rnd15:
            rnd15 = random.randrange(0, len(nm21))

        rnd16 = random.randrange(0, len(nm22))
        rnd17 = random.randrange(0, len(nm21))
        rnd18 = random.randrange(0, len(nm22))
        rnd19 = random.randrange(0, len(nm22))
        while rnd18 == rnd19:
            rnd19 = random.randrange(0, len(nm22))

        name = nm1[rnd1] + " " + rnd13b + " " + rnd14b + ", but " + nm2[rnd2] + " " + nm25[0] + " " + nm21[rnd11][0] + " and " + nm21[rnd12][0] + ". Of course " + nm25[0] + " also " + nm21[rnd13][0] + ", " + nm21[rnd14][0] + " and " + nm21[rnd15][0] + ", but " + nm3[rnd3] + " being " + nm22[rnd16][0] + " as well."
        name2 = nm25[2] + " " + nm21[rnd11][1] + " though, this is what " + nm25[0] + " " + nm4[rnd4] + ". " + nm5[rnd5] + " count on this and " + nm25[1] + " " + nm21[rnd17][1] + " " + nm6[rnd6] + "."
        name3 = "Nobody's perfect of course and " + rnd13b + " has " + nm7[rnd7] + " too. " + nm25[2] + " " + nm22[rnd18][1] + " and " + nm22[rnd19][1] + " " + nm8[rnd8] + nm9[rnd9] + "."
        name4 = "Fortunately " + nm25[1] + " " + nm21[rnd12][1] + " " + nm10[rnd10] + "."

    s = " "
    seq = (name, name2, name3, name4)
    return s.join(seq)


STYLE_PANTS = {
    'male': [
        "jeans", "jeans", "jeans", "jeans",
        "shorts", "shorts",
        "sweat pants",
        "khakis", "khakis", "khakis",
        "pleated pants",
        "cargo pants",
        "chinos",
        None, None, None, None, None,
    ],
    'female': [
        "jeans", "jeans", "jeans", "jeans", "jeans",
        "shorts", "shorts", "shorts", "shorts",
        "sweat pants",
        "skirt", "skirt", "skirt",
        "mini-skirt", "mini-skirt",
        "trousers",
        "capris",
        "leggings", "leggings", "leggings", "leggings",
        "yoga pants",
        "palazzos",
        "sailor pants",
        "harem pants",
        None, None, None, None,
    ],
}

STYLE_SHIRT = {
    'male': [
        "dress", "dress",
        "casual", "casual", "casual", "casual",
        "polo", "polo", "polo", "polo",
        "sweatshirt", "sweatshirt",
        "t-shirt", "t-shirt", "t-shirt",
        "epaulette",
        "lumberjack",
        "jersey", "jersey",
        None, None, None, None, None, None,
    ],
    'female': [
        "dress", "dress", "dress",
        "casual", "casual", "casual", "casual",
        "polo",
        "sweatshirt",
        "t-shirt", "t-shirt",
        "epaulette",
        "lumberjack",
        "jersey",
        "tunic", "tunic",
        None, None, None, None,
    ],
}

STYLE_FOOTWEAR = {
    'male': [
        "sneakers", "sneakers", "sneakers",
        "boots", "boots",
        "slippers",
        "casual shoes", "casual shoes", "casual shoes", "casual shoes",
        "designer shoes",
        "dress shoes", "dress shoes", "dress shoes",
        None, None, None, None, None,
    ],
    'female': [
        "sneakers", "sneakers",
        "boots",
        "slippers",
        "casual shoes", "casual shoes",
        "designer shoes", "designer shoes", "designer shoes",
        "high heels", "high heels", "high heels",
        "stiletto heels", "stiletto heels",
        "platforms", "platforms", "platforms", "platforms",
        None, None, None, None,
    ],
}

STYLE_HAIRSTYLE = {
    'male': [
        "spiky", "spiky", "spiky",
        "afro", "afro",
        "bob cut",
        "taper", "taper",
        "undercut",
        "slick back", "slick back",
        "shaggy", "shaggy", "shaggy", "shaggy",
        "bald", "bald", "bald",
        None, None, None, None, None, None, None,
    ],
    'female': [
        "spiky", "spiky", "spiky",
        "afro", "afro",
        "bob cut", "bob cut", "bob cut",
        "taper",
        "shaggy", "shaggy", "shaggy",
        "bald",
        None, None, None, None,
    ],
}

FRUITS = (
    "banana", "banana", "banana", "banana", "banana", "banana", "banana",
    "apple", "apple", "apple", "apple", "apple", "apple", "apple",
    "grape", "grape", "grape", "grape", "grape", "grape",
    "strawberry", "strawberry", "strawberry", "strawberry", "strawberry", "strawberry",
    "orange", "orange", "orange", "orange", "orange", "orange",
    "watermelon", "watermelon", "watermelon", "watermelon", "watermelon",
    "lemon", "lemon", "lemon", "lemon", "lemon",
    "blueberry", "blueberry", "blueberry", "blueberry",
    "peach", "peach", "peach", "peach",
    "cantaloupe", "cantaloupe", "cantaloupe", "cantaloupe",
    "avocado", "avocado", "avocado", "avocado",
    "pineapple", "pineapple", "pineapple", "pineapple",
    "cherry", "cherry", "cherry",
    "pear", "pear", "pear",
    "lime", "lime", "lime",
    "raspberry", "raspberry", "raspberry",
    "blackberry", "blackberry", "blackberry",
    "plum", "plum",
    "nectarine", "nectarine",
    "grapefruit", "grapefruit",
)


def main():
    with open('accounts.json') as f:
        docs = json.load(f, object_pairs_hook=OrderedDict)

    for x in docs:
        _id = x.pop('_id')

        accountNumber = x.pop('account_number')

        balance = x.pop('balance')

        employer = x.pop('employer')

        age = x.pop('age')

        gender_field = 'gender' if 'gender' in x else '#gender'
        gender = 'female' if x.pop(gender_field) in ('F', 'female') else 'male'

        del x['firstname']
        del x['lastname']
        firstname = names.get_first_name(gender=gender)
        lastname = names.get_last_name()

        address = x.pop('address')
        city = x.pop('city')
        state = x.pop('state')
        phone = x.pop('phone')
        email = x.pop('email')

        _, _, domain = email.partition('@')
        email = "{}.{}@{}".format(firstname.lower(), lastname.lower(), domain)

        del x['favoriteFruit']
        favoriteFruit = random.choice(FRUITS)

        eyeColor = x.pop('eyeColor')

        pants = random.choice(STYLE_PANTS[gender])
        shirt = random.choice(STYLE_SHIRT[gender])
        footwear = random.choice(STYLE_FOOTWEAR[gender])
        hairstyle = random.choice(STYLE_HAIRSTYLE[gender])

        personality = nameGen(random.randrange(1, 3), firstname, lastname, gender)

        ###

        x['accountNumber'] = accountNumber

        x['balance'] = balance

        x['employer'] = employer

        x['name'] = OrderedDict()
        x['name']['firstName'] = firstname
        x['name']['lastName'] = lastname

        x['age'] = age
        x[gender_field] = gender

        x['contact'] = OrderedDict()
        x['contact']['address'] = address
        x['contact']['city'] = city
        x['contact']['state'] = state
        x['contact']['phone'] = phone
        x['contact']['email'] = email

        x['favoriteFruit'] = favoriteFruit

        x['eyeColor'] = eyeColor

        if pants or shirt or footwear or hairstyle:
            x['style'] = OrderedDict()
            if pants or shirt or footwear:
                x['style']['clothing'] = OrderedDict()
                if pants:
                    x['style']['clothing']['pants'] = pants
                if shirt:
                    x['style']['clothing']['shirt'] = shirt
                if footwear:
                    x['style']['clothing']['footwear'] = footwear
            if hairstyle:
                x['style']['hairstyle'] = hairstyle

        x['personality'] = personality

        x['_id'] = _id

    with open('accounts_mix.json', 'w') as f:
        json.dump(docs, f, indent=2)


if __name__ == '__main__':
    main()
