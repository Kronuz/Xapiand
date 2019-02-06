#!/usr/bin/env python
# -*- coding: utf-8 -*-
from __future__ import absolute_import, unicode_literals

import json
import random
from collections import OrderedDict

from faker import Faker
from faker.providers import BaseProvider


class Personality(BaseProvider):
    """
    Personality description generator

    This description generator will generate a fairly random description of
    either an overall positive or overall negative personality based on your
    choice.

    https://www.fantasynamegenerators.com/personality-descriptions.php
    """
    # Positive personality type
    Good_things_can_be_said_of = ["A lot can be said of", "Many things can be said of", "There's a lot to say about", "It takes a while to get to know", "It's easy to get to know an open person like", "A lot can be assumed when you first see", "There's more than meets the eye in the case of", "Looks can be deceiving when it comes", "It's hard to describe a complex person like", "Few know the true"]
    above_all = ["above else know that", "if nothing else you should know", "at the very least", "two things you'll never forget are that", "the biggest two things to know are that", "the two traits most people enjoy the most are that", "perhaps most important is that", "most know that above all else", "if there's anything you should know it's that", "at the very least you'll find out"]
    negatively = ["those are often overshadowed by tendencies of", "these are in a way balance by", "they're often slightly tainted by a mindset of", "they're less prominent and often intertwined with", "they're far less prominent, especially compared to impulses of", "in smaller doses and they're often spoiled by habits of", "far less strongly and often mixed with", "in a way they're lesser traits and tained by behaviors of", "they're in shorter supply, especially considering they're mixed with", "they're tainted by and mixed with habits of"]
    loved_for = ["most well-liked for", "pretty much known for", "pretty much loved for", "often adored for", "often admired for", "kind of cherished for", "most popular for", "so well-liked for"]
    people_often = ["People often", "Friends usually", "Friends tend to", "Oftentimes people will", "There are many times when friends", "People regularly", "Friends frequently", "On many occasions people will"]
    when_most_needed = ["when they're feeling down", "whenever they need cheering up", "in times of need", "whenever they need help", "when they're in need of support", "whenever they need assistance or help", "especially when they need comforting or support"]
    Nobody_s_perfect_and = ["All and all,", "Nobody's perfect of course and"]
    less_favorable_traits = ["plenty of less favorable traits", "a fair share of lesser days", "plenty of rainy days", "rotten moods and days", "less pleasant traits", "plenty of less favorable characteristics", "a share of darker sides to deal with", "plenty of lesser desired aspects", "a range of flaws to deal with", "plenty of character faults"]
    pose_problems = ["tend to get in the way", "aren't exactly fun to deal with", "do cause plenty of friction", "pose plenty of problems", "sour the mood many a time", "risk ruining pleasant moods", "can cause things to heat up", "don't make for the greatest company", "cause plenty of grievances", "are far from ideal"]
    personally_and_for_others = [", both personally and for others", " on often personal levels", ", though more on a personal level than for others", " and reach all around", ", much to the annoyance of others", " and could ruin plenty of evenings", " and just affect all around negatively", " even at the best of times", " and beyond what people are willing to deal with", " and make things uncomfortable to say the least"]
    Fortunately = ["Fortunately"]
    helps_a_lot = ["helps make sure these days are few and far between", "assures this isn't the case very often", "is usually there to soften the blows", "usually softens the worst of it", "shines brighter on most days", "helps prevent most of these grievances", "is usually there to help mends things when needed", "helps keep them in check for at least a little", "helps lighten the blows and moods when needed", "is there to relift spirits when needed"]

    # Negative personality type
    Many_will_dislike = ["There's plenty people hate about", "A whole lot can be said about", "There's plenty to say about", "It doesn't take long to dislike", "Plenty of people will dislike", "Many hateful words can be said about", "Unfortunately very few people like", "Looks can be deceiving when dealing with", "You may not notice this when first meeting", "There's more than meets the eye in the case of"]
    just_the_tip = ["just the tip of the iceberg", "only the begining"]
    To_make_things_worse = ["To make things worse", "Never mind the fact", "Let alone the fact", "That's without even mentioning", "To top it off", "To make matters worse", "As if that's not enough", "There there's the fact", "On top of that", "That's without even stating"]
    at_least_being = ["at least those are kept somewhat in check by habits of being", "fortunately they're balanced out slightly by being", "their effects are at least lessened by impulses of being", "at the very least they're countered somewhat by being", "their worst is softened by behaviors of being", "in an odd way they're balanced by habits of being", "at least they're not as bad due to intertwined habits of being", "fortunately they're mixed with behaviors of being", "they're not as prominent and counteracted by habits of being", "at least they only show in smaller doses and mixed with behaviors of being"]
    most_disliked = ["pretty much º", "notorious", "most disliked", "pretty much hated", "purposely avoided", "almost feared", "most condemned", "loathed", "often despised", "often scorned"]
    Many_occasions_were_spoiled = ["Many occasions were spoiled", "Plenty of days have been ruined", "Multiple enemies have been made", "Plenty of relationships came to a halt", "Multiple nights have ended in disaster", "Even careful encounters have been ruined", "There's no sugar coating the effects caused", "There's a great deal of pain left on all sides", "Any healthy relationship is made incredibly difficult", "Even the best intentions have been soured", "There's not much meaningful that can be created"]
    which_is_unfortunate = ["unfortunately", "much to the annoyance of others", "which is unfortunate in and of itself", "but there's not much you can really do about it", "which is too bad for all sides", "but different strokes for different folks I guess", "which is far from desired, but so be it", "which plenty have been a witness to", "which is a true shame for both sides", "but that may just be the nature of the beast", "as sad of a reality as this may be"]
    All_in_all = ["All and all,", "Fair is fair though,"]
    does_have_some_qualities = ["does gave some redeeming qualities", "does have some rays of light", "does have some lighter sides", "isn't completely rotten", "has better sides too", "doesn't turn everything to dust", "does have some brighter sides within the darkness", "is still a complex being with good sides as well", "is complex and grey like the rest of us", "does have some endearing sides"]
    if_nothing_else = ["at the very least", "if nothing else", "to name a few", "for a start", "among others", "in good amounts", "among true friends", "when around good friends", "if you look for it", "even if in small doses"]
    there_s_hope = ["it's not like we're dealing with pure evil here", "it's not like we've encountered the devil", "all considered it could be much worse", "there's still a beacon of hope", "it isn't all doom and gloom", "who knows what could happen", "that's at least some form of redemption", "there's at least that to look for and appeal to", "it could be worth to take a chance just once", "there is still some hope yet"]
    Unfortunately = ["Unfortunately"]
    tends_to_prevale = ["tends to ruin whatever good comes from those traits", "often spoils the fun that may have come from those traits", "is always there to ruin everything again", "can always be counted on to turn things back for the worse", "is usually lurking and ready to spoil the fun", "is almost ever present and ready to sour the mood", "often pops up fast enough to ruin the chances of something good", "is always there and ready to strike at the worst moments", "will forever be something to deal with", "will probably never truly go away"]

    good = [["active", "active nature"], ["adaptable", "adaptability"], ["adventurous", "adventurous nature"], ["ambitious", "ambitions"], ["amusing", "amusing nature"], ["anticipative", "anticipative nature"], ["appreciative", "appreciative nature"], ["aspiring", "aspirations"], ["athletic", "athleticism"], ["balanced", "sense of balance"], ["brilliant", "brilliance"], ["calm", "calm nature"], ["capable", "capabilities"], ["captivating", "captivating nature"], ["caring", "caring nature"], ["charismatic", "charisma"], ["charming", "charm"], ["cheerful", "cheerfulness"], ["clear-headed", "clear-headedness"], ["clever", "clever nature"], ["companionably", "companionship"], ["compassionate", "compassion"], ["confident", "confidence"], ["considerate", "considerate nature"], ["contemplative", "contemplative nature"], ["cooperative", "cooperation"], ["courageous", "courage"], ["courteous", "courtesy"], ["creative", "creativity"], ["curious", "curiosity"], ["daring", "daring nature"], ["decisive", "decisive nature"], ["dedicated", "dedication"], ["determined", "determination"], ["disciplined", "discipline"], ["discreet", "discretion"], ["dutiful", "dutiful nature"], ["dynamic", "dynamic nature"], ["earnest", "earnestness"], ["efficient", "efficiency"], ["elegant", "elegance"], ["empathetic", "empathy"], ["energetic", "energy"], ["enthusiastic", "enthusiasm"], ["exciting", "excitement"], ["faithful", "faithfulness"], ["farsighted", "farsightedness"], ["flexible", "flexibility"], ["focused", "focus"], ["forgiving", "forgiving nature"], ["forthright", "forthright nature"], ["freethinking", "freethinking nature"], ["friendly", "friendliness"], ["fun-loving", "fun-loving nature"], ["generous", "generosity"], ["gentle", "gentleness"], ["good-natured", "good nature"], ["gracious", "grace"], ["hardworking", "hardworking nature"], ["helpful", "helping hand"], ["heroic", "heroic nature"], ["honest", "honesty"], ["honorable", "honor"], ["humble", "humbleness"], ["humorous", "sense of humor"], ["idealistic", "idealism"], ["imaginative", "imagination"], ["incisive", "incisive nature"], ["incorruptible", "incorruptible nature"], ["independent", "independence"], ["individualistic", "individualism"], ["innovative", "innovative nature"], ["insightful", "insight"], ["intelligent", "intelligence"], ["intuitive", "intuition"], ["kind", "kindness"], ["leaderly", "leadership"], ["lovable", "loving nature"], ["loyal", "loyalty"], ["methodical", "methodical nature"], ["modest", "modesty"], ["objective", "objectivity"], ["observant", "observant nature"], ["open", "openness"], ["optimistic", "optimism"], ["orderly", "sense of order"], ["organized", "organization skills"], ["outspoken", "outspoken nature"], ["passionate", "passion"], ["patient", "patience"], ["perceptive", "perceptive nature"], ["persuasive", "persuasive nature"], ["planful", "planning"], ["playful", "playfulness"], ["practical", "practical thinking"], ["precise", "precision"], ["protective", "protective nature"], ["punctual", "punctuality"], ["rational", "rational thinking"], ["realistic", "realism"], ["reflective", "reflective thinking"], ["relaxed", "relaxed nature"], ["reliable", "reliability"], ["resourceful", "resourcefulness"], ["respectful", "respect"], ["responsible", "responsibility"], ["responsive", "responsiveness"], ["romantic", "romantic nature"], ["selfless", "selflessness"], ["sensitive", "sensitive nature"], ["sentimental", "sentimentality"], ["sharing", "sharing nature"], ["sociable", "sociable nature"], ["spontaneous", "spontaneity"], ["stable", "stable nature"], ["surprising", "surprising nature"], ["sweet", "sweet nature"], ["sympathetic", "sympathetic nature"], ["tolerant", "tolerance"], ["trusting", "trusting nature"], ["understanding", "understanding nature"], ["upright", "upright nature"], ["warm", "warmness"], ["wise", "wisdom"], ["witty", "wit"]]
    bad = [["abrasive", "abrasiveness"], ["aggressive", "aggression"], ["agonizing", "agonizing nature"], ["amoral", "amoral nature"], ["angry", "anger"], ["apathetic", "apathetic nature"], ["argumentative", "argumentativeness"], ["arrogant", "arrogant nature"], ["barbaric", "barbaric nature"], ["blunt", "blunt nature"], ["brutal", "brutish ways"], ["callous", "callous nature"], ["careless", "carelessness"], ["childish", "childish nature"], ["coarse", "coarseness"], ["cold", "coldness"], ["conceited", "conceited nature"], ["crass", "crassness"], ["crazy", "craziness"], ["criminal", "criminal nature"], ["crude", "crude ways"], ["cruel", "cruelty"], ["cynical", "cynical nature"], ["deceitful", "deceitful ways"], ["demanding", "demanding nature"], ["desperate", "desperation"], ["destructive", "destructive nature"], ["devious", "deviousness"], ["difficult", "difficult nature"], ["disconcerting", "disconcerting nature"], ["dishonest", "dishonesty"], ["disloyal", "disloyalty"], ["disorderly", "disorderliness"], ["disrespectful", "disrespectful nature"], ["disruptive", "disruptive nature"], ["disturbing", "disturbing nature"], ["dominating", "dominating nature"], ["egocentric", "ego"], ["envious", "envy"], ["extreme", "extreme nature"], ["frightening", "frightening nature"], ["greedy", "greed"], ["grim", "grim ways"], ["hateful", "hatred"], ["hostile", "hostility"], ["ignorant", "ignorance"], ["impatient", "impatience"], ["imprudent", "imprudence"], ["inconsiderate", "inconsideration"], ["insensitive", "insensitivity"], ["insincere", "insincerity"], ["insulting", "insulting nature"], ["intolerant", "intolerance"], ["irrational", "irrational nature"], ["irresponsible", "irresponsibility"], ["irritable", "irritable nature"], ["lazy", "laziness"], ["malicious", "maliciousness"], ["miserable", "miserable nature"], ["monstrous", "monstrous nature"], ["morbid", "morbid nature"], ["narcissistic", "narcissistic nature"], ["narrow-minded", "narrow-mindedness"], ["negativistic", "negativity"], ["neglectful", "neglectful nature"], ["obnoxious", "obnoxious nature"], ["obsessive", "obsessive nature"], ["opportunistic", "opportunistic ways"], ["pedantic", "pedantic nature"], ["perverse", "perversions"], ["petty", "petty nature"], ["pompous", "pompous nature"], ["possessive", "possessive nature"], ["power-hungry", "power-hungry ways"], ["predatory", "predatory nature"], ["prejudiced", "prejudices"], ["pretentious", "pretentiousness"], ["provocative", "provocative nature"], ["sadistic", "sadistic ways"], ["scornful", "scornful nature"], ["self-indulgent", "self-indulgence"], ["selfish", "selfishness"], ["shallow", "shallowness"], ["sly", "slyness"], ["superficial", "superficial nature"], ["tactless", "tactlessness"], ["thievish", "thievish nature"], ["thoughtless", "thoughtlessness"], ["treacherous", "treachery"], ["troublesome", "troublesome nature"], ["uncaring", "uncaring nature"], ["unfriendly", "unfriendliness"], ["unstable", "instability"], ["venomous", "venomous nature"], ["vindictive", "vindictive nature"]]

    firstnames_male = ["Aaron", "Adam", "Aidan", "Aiden", "Alex", "Alexander", "Alfie", "Andrew", "Anthony", "Archie", "Arthur", "Ashton", "Bailey", "Ben", "Benjamin", "Billy", "Blake", "Bobby", "Bradley", "Brandon", "Caleb", "Callum", "Cameron", "Charles", "Charlie", "Christopher", "Cody", "Connor", "Corey", "Daniel", "David", "Declan", "Dexter", "Dominic", "Dylan", "Edward", "Elliot", "Ellis", "Ethan", "Evan", "Ewan", "Finlay", "Finley", "Frankie", "Freddie", "Frederick", "Gabriel", "George", "Harley", "Harrison", "Harry", "Harvey", "Hayden", "Henry", "Isaac", "Jack", "Jackson", "Jacob", "Jake", "James", "Jamie", "Jay", "Jayden", "Jenson", "Joe", "Joel", "John", "Jonathan", "Jordan", "Joseph", "Josh", "Joshua", "Jude", "Kai", "Kayden", "Kian", "Kieran", "Kyle", "Leo", "Leon", "Lewis", "Liam", "Logan", "Louie", "Louis", "Luca", "Lucas", "Luke", "Mason", "Matthew", "Max", "Michael", "Morgan", "Nathan", "Nicholas", "Noah", "Oliver", "Ollie", "Oscar", "Owen"]
    firstnames_female = ["Abbie", "Abby", "Abigail", "Aimee", "Alexandra", "Alice", "Alicia", "Alisha", "Amber", "Amelia", "Amelie", "Amy", "Anna", "Ava", "Bella", "Bethany", "Brooke", "Caitlin", "Cerys", "Charlie", "Charlotte", "Chelsea", "Chloe", "Courtney", "Daisy", "Danielle", "Demi", "Eleanor", "Eliza", "Elizabeth", "Ella", "Ellie", "Eloise", "Elsie", "Emilia", "Emily", "Emma", "Erin", "Esme", "Eva", "Eve", "Evelyn", "Evie", "Faith", "Freya", "Georgia", "Georgina", "Grace", "Gracie", "Hannah", "Harriet", "Heidi", "Hollie", "Holly", "Imogen", "Isabel", "Isabella", "Isabelle", "Isla", "Isobel", "Jade", "Jasmine", "Jennifer", "Jessica", "Jodie", "Julia", "Kate", "Katherine", "Katie", "Kayla", "Kayleigh", "Keira", "Lacey", "Lara", "Laura", "Lauren", "Layla", "Leah", "Lexi", "Lexie", "Libby", "Lilly", "Lily", "Lola", "Louise", "Lucy", "Lydia", "Maddison", "Madeleine", "Madison", "Maisie", "Maisy", "Maria", "Martha", "Matilda", "Maya", "Megan", "Melissa", "Mia", "Mollie"]
    lastnames = ["Adams", "Allen", "Anderson", "Andrews", "Armstrong", "Atkinson", "Austin", "Bailey", "Baker", "Ball", "Barker", "Barnes", "Barrett", "Bates", "Baxter", "Bell", "Bennett", "Berry", "Black", "Booth", "Bradley", "Brooks", "Brown", "Burke", "Burns", "Burton", "Butler", "Byrne", "Campbell", "Carr", "Carter", "Chambers", "Chapman", "Clark", "Clarke", "Cole", "Collins", "Cook", "Cooke", "Cooper", "Cox", "Cunningham", "Davidson", "Davies", "Davis", "Dawson", "Day", "Dean", "Dixon", "Doyle", "Duncan", "Edwards", "Elliott", "Ellis", "Evans", "Fisher", "Fletcher", "Foster", "Fox", "Francis", "Fraser", "Gallagher", "Gardner", "George", "Gibson", "Gill", "Gordon", "Graham", "Grant", "Gray", "Green", "Griffiths", "Hall", "Hamilton", "Harper", "Harris", "Harrison", "Hart", "Harvey", "Hawkins", "Hayes", "Henderson", "Hill", "Holland", "Holmes", "Hopkins", "Houghton", "Howard", "Hudson", "Hughes", "Hunt", "Hunter", "Hussain", "Jackson", "James", "Jenkins", "John", "Johnson", "Johnston", "Jones", "Jordan", "Kaur", "Kelly", "Kennedy", "Khan", "King", "Knight", "Lane", "Lawrence", "Lawson", "Lee", "Lewis", "Lloyd", "Lowe", "Macdonald", "Marsh", "Marshall", "Martin", "Mason", "Matthews", "May", "Mccarthy", "Mcdonald", "Miller", "Mills", "Mitchell", "Moore", "Morgan", "Morris", "Moss", "Murphy", "Murray", "Newman", "Nicholson", "Owen", "Palmer", "Parker", "Parry", "Patel", "Pearce", "Pearson", "Perry", "Phillips", "Poole", "Porter", "Powell", "Price", "Read", "Rees", "Reid", "Reynolds", "Richards", "Richardson", "Riley", "Roberts", "Robertson", "Robinson", "Rogers", "Rose", "Ross", "Russell", "Ryan", "Saunders", "Scott", "Sharp", "Shaw", "Simpson", "Smith", "Spencer", "Stevens", "Stewart", "Stone", "Sutton", "Taylor", "Thomas", "Thompson", "Thomson", "Turner", "Walker", "Wallace", "Walsh", "Ward", "Watson", "Watts", "Webb", "Wells", "West", "White", "Wilkinson", "Williams", "Williamson", "Willis", "Wilson", "Wood", "Woods", "Wright", "Young"]

    def personality(self, first_name=None, last_name=None, gender=None):
        personality_type = self.random_element({'positive': 0.75, 'negative': 0.25})
        if personality_type == 'positive':
            return self.positive_personality(first_name=first_name, last_name=last_name, gender=gender)
        else:
            return self.negative_personality(first_name=first_name, last_name=last_name, gender=gender)

    def positive_personality(self, first_name=None, last_name=None, gender=None):
        if gender is None:
            gender = self.random_element(('male', 'female'))
        if gender.lower() in ('male', 'm'):
            he_s, his, His, He_s = ["he's", "his", "His", "He's"]
            if first_name is None:
                first_name = self.random_element(self.firstnames_male)
        else:
            he_s, his, His, He_s = ["she's", "her", "Her", "She's"]
            if first_name is None:
                first_name = self.random_element(self.firstnames_female)
        if last_name is None:
            last_name = self.random_element(self.lastnames)

        Good_things_can_be_said_of = self.random_element(self.Good_things_can_be_said_of)
        above_all = self.random_element(self.above_all)
        negatively = self.random_element(self.negatively)
        loved_for = self.random_element(self.loved_for)
        people_often = self.random_element(self.people_often)
        when_most_needed = self.random_element(self.when_most_needed)
        Nobody_s_perfect_and = self.random_element(self.Nobody_s_perfect_and)
        less_favorable_traits = self.random_element(self.less_favorable_traits)
        pose_problems = self.random_element(self.pose_problems)
        personally_and_for_others = self.random_element(self.personally_and_for_others)
        Fortunately = self.random_element(self.Fortunately)
        helps_a_lot = self.random_element(self.helps_a_lot)

        good1 = self.random_element(self.good)
        good2 = self.random_element(self.good)

        while good1 == good2:
            good2 = self.random_element(self.good)

        good3 = self.random_element(self.good)
        while good1 == good3 or good2 == good3:
            good3 = self.random_element(self.good)

        good4 = self.random_element(self.good)
        while good1 == good4 or good2 == good4 or good3 == good4:
            good4 = self.random_element(self.good)

        good5 = self.random_element(self.good)
        while good1 == good5 or good2 == good5 or good3 == good5 or good4 == good5:
            good5 = self.random_element(self.good)

        bad1 = self.random_element(self.bad)
        good6 = self.random_element(self.good)
        bad2 = self.random_element(self.bad)
        bad3 = self.random_element(self.bad)
        while bad2 == bad3:
            bad3 = self.random_element(self.bad)

        good1_attr, good1_prop = good1
        good2_attr, good2_prop = good2
        good3_attr, good3_prop = good3
        good4_attr, good4_prop = good4
        good5_attr, good5_prop = good5
        good6_attr, good6_prop = good6
        bad1_attr, bad1_prop = bad1
        bad2_attr, bad2_prop = bad2
        bad3_attr, bad3_prop = bad3

        personality = " ".join((
            "{Good_things_can_be_said_of} {first_name} {last_name}, but {above_all} {he_s} {good1_attr} and {good2_attr}. Of course {he_s} also {good3_attr}, {good4_attr} and {good5_attr}, but {negatively} being {bad1_attr} as well.",
            "{His} {good1_prop} though, this is what {he_s} {loved_for}. {people_often} count on this and {his} {good6_prop} {when_most_needed}.",
            "{Nobody_s_perfect_and} {first_name} has {less_favorable_traits} too. {His} {bad2_prop} and {bad3_prop} {pose_problems}{personally_and_for_others}.",
            "{Fortunately} {his} {good2_prop} {helps_a_lot}.",
        ))
        personality = personality.format(**locals())
        return personality

    def negative_personality(self, first_name=None, last_name=None, gender=None):
        if gender is None:
            gender = self.random_element(('male', 'female'))
        if gender.lower() in ('male', 'm'):
            he_s, his, His, He_s = ["he's", "his", "His", "He's"]
            if first_name is None:
                first_name = self.random_element(self.firstnames_male)
        else:
            he_s, his, His, He_s = ["she's", "her", "Her", "She's"]
            if first_name is None:
                first_name = self.random_element(self.firstnames_female)
        if last_name is None:
            last_name = self.random_element(self.lastnames)

        Many_will_dislike = self.random_element(self.Many_will_dislike)
        just_the_tip = self.random_element(self.just_the_tip)
        To_make_things_worse = self.random_element(self.To_make_things_worse)
        at_least_being = self.random_element(self.at_least_being)
        most_disliked = self.random_element(self.most_disliked)
        Many_occasions_were_spoiled = self.random_element(self.Many_occasions_were_spoiled)
        which_is_unfortunate = self.random_element(self.which_is_unfortunate)
        All_in_all = self.random_element(self.All_in_all)
        does_have_some_qualities = self.random_element(self.does_have_some_qualities)
        if_nothing_else = self.random_element(self.if_nothing_else)
        there_s_hope = self.random_element(self.there_s_hope)
        Unfortunately = self.random_element(self.Unfortunately)
        tends_to_prevale = self.random_element(self.tends_to_prevale)
        bad1 = self.random_element(self.bad)
        bad2 = self.random_element(self.bad)

        while bad1 == bad2:
            bad2 = self.random_element(self.bad)

        bad3 = self.random_element(self.bad)
        while bad1 == bad3 or bad2 == bad3:
            bad3 = self.random_element(self.bad)

        bad4 = self.random_element(self.bad)
        while bad1 == bad4 or bad2 == bad4 or bad3 == bad4:
            bad4 = self.random_element(self.bad)

        bad5 = self.random_element(self.bad)
        while bad1 == bad5 or bad2 == bad5 or bad3 == bad5 or bad4 == bad5:
            bad5 = self.random_element(self.bad)

        good1 = self.random_element(self.good)
        bad6 = self.random_element(self.bad)
        good2 = self.random_element(self.good)
        good3 = self.random_element(self.good)
        while good2 == good3:
            good3 = self.random_element(self.good)

        bad1_attr, bad1_prop = bad1
        bad2_attr, bad2_prop = bad2
        bad3_attr, bad3_prop = bad3
        bad4_attr, bad4_prop = bad4
        bad5_attr, bad5_prop = bad5
        bad6_attr, bad6_prop = bad6
        good1_attr, good1_prop = good1
        good2_attr, good2_prop = good2
        good3_attr, good3_prop = good3

        personality = " ".join((
            "{Many_will_dislike} {first_name} {last_name}, but the fact {he_s} {bad1_attr} and {bad2_attr} is {just_the_tip}. {To_make_things_worse} {he_s} also {bad3_attr}, {bad4_attr} and {bad5_attr}, but {at_least_being} {good1_attr} as well.",
            "But focus on {his} as this is what {he_s} {most_disliked}. {Many_occasions_were_spoiled} because of this and {his} {bad6_prop}, {which_is_unfortunate}.",
            "{All_in_all} {first_name} {does_have_some_qualities}. {He_s} {good2_attr} and {good3_attr} {if_nothing_else}, {there_s_hope}.",
            "{Unfortunately} {his} {bad1_prop} {tends_to_prevale}.",
        ))
        personality = personality.format(**locals())
        return personality


class Fruit(BaseProvider):
    fruits = {
        "banana": 75,
        "apple": 73,
        "grape": 65,
        "strawberry": 63,
        "orange": 61,
        "watermelon": 52,
        "lemon": 48,
        "blueberry": 45,
        "peach": 42,
        "cantaloupe": 41,
        "avocado": 40,
        "pineapple": 40,
        "cherry": 37,
        "pear": 36,
        "lime": 33,
        "raspberry": 30,
        "blackberry": 30,
        "plum": 25,
        "nectarine": 25,
        "grapefruit": 23,
        None: 10,
    }

    def fruit(self):
        return self.random_element(self.fruits)


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


def main():
    fake = Faker()
    fake.add_provider(Personality)
    fake.add_provider(Fruit)

    with open('accounts.json') as f:
        docs = json.load(f, object_pairs_hook=OrderedDict)

    for idx, x in enumerate(docs):
        _id = x.pop('_id')

        accountNumber = x.pop('accountNumber')

        balance = x.pop('balance')

        employer = x.pop('employer')

        x.pop('name', None)

        age = x.pop('age')

        gender_field = 'gender' if 'gender' in x else '#gender'
        gender = 'female' if x.pop(gender_field) in ('F', 'female') else 'male'

        if gender == 'female':
            first_name = fake.first_name_female()
        else:
            first_name = fake.first_name_male()
        last_name = fake.last_name()

        contact = x.pop('contact')
        address = contact.pop('address')
        city = contact.pop('city')
        state = contact.pop('state')
        postcode = fake.postalcode_in_state(state_abbr=dict(zip((
            'Alabama', 'Alaska', 'Arizona', 'Arkansas', 'California', 'Colorado',
            'Connecticut', 'Delaware', 'Florida', 'Georgia', 'Hawaii', 'Idaho',
            'Illinois', 'Indiana', 'Iowa', 'Kansas', 'Kentucky', 'Louisiana',
            'Maine', 'Maryland', 'Massachusetts', 'Michigan', 'Minnesota',
            'Mississippi', 'Missouri', 'Montana', 'Nebraska', 'Nevada',
            'New Hampshire', 'New Jersey', 'New Mexico', 'New York',
            'North Carolina', 'North Dakota', 'Ohio', 'Oklahoma', 'Oregon',
            'Pennsylvania', 'Rhode Island', 'South Carolina', 'South Dakota',
            'Tennessee', 'Texas', 'Utah', 'Vermont', 'Virginia', 'Washington',
            'West Virginia', 'Wisconsin', 'Wyoming',
        ), (
            'AL', 'AK', 'AZ', 'AR', 'CA', 'CO', 'CT', 'DE', 'DC', 'FL', 'GA', 'HI',
            'ID', 'IL', 'IN', 'IA', 'KS', 'KY', 'LA', 'ME', 'MD', 'MA', 'MI', 'MN',
            'MS', 'MO', 'MT', 'NE', 'NV', 'NH', 'NJ', 'NM', 'NY', 'NC', 'ND', 'OH',
            'OK', 'OR', 'PA', 'RI', 'SC', 'SD', 'TN', 'TX', 'UT', 'VT', 'VA', 'WA',
            'WV', 'WI', 'WY',
        ))).get(state))

        phone = contact.pop('phone')
        email = contact.pop('email')

        checkin = fake.local_latlng(country_code='US', coords_only=True)

        _, _, domain = email.partition('@')
        email = "{}.{}@{}".format(first_name.lower(), last_name.lower(), domain)

        x.pop('favoriteFruit', None)
        favoriteFruit = fake.fruit()

        eyeColor = x.pop('eyeColor')

        pants = random.choice(STYLE_PANTS[gender])
        shirt = random.choice(STYLE_SHIRT[gender])
        footwear = random.choice(STYLE_FOOTWEAR[gender])
        hairstyle = random.choice(STYLE_HAIRSTYLE[gender])

        x.pop('style', None)

        x.pop('personality', None)
        personality = fake.personality(first_name=first_name, last_name=last_name, gender=gender)

        ###

        if not idx:
            x['_schema'] = {
                'schema': {
                    'checkin': {
                        '_type': 'geospatial',
                    },
                    'personality': {
                        '_type': 'text',
                        '_language': 'en',
                    },
                    'style': {
                        '_namespace': True,
                        '_partial_paths': True,
                    },
                },
            }

        x['accountNumber'] = accountNumber

        x['balance'] = balance

        x['employer'] = employer

        x['name'] = OrderedDict()
        x['name']['firstName'] = first_name
        x['name']['lastName'] = last_name

        x['age'] = age
        x[gender_field] = gender

        x['contact'] = OrderedDict()
        x['contact']['address'] = address
        x['contact']['city'] = city
        x['contact']['state'] = state
        x['contact']['postcode'] = postcode
        x['contact']['phone'] = phone
        x['contact']['email'] = email

        x['checkin'] = {
            '_point': {
                '_latitude': float(checkin[0]),
                '_longitude': float(checkin[1]),
            }
        }

        if favoriteFruit:
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
