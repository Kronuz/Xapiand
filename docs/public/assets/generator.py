#!/usr/bin/env python
# -*- coding: utf-8 -*-
# Copyright © 2026 Germán Méndez Bravo (Kronuz)
# Copyright © 2015-2020 Dubalu LLC
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

from __future__ import absolute_import, unicode_literals

import json
import random
import sys
from collections import OrderedDict

from faker import Faker
from faker.providers import BaseProvider


class Personality(BaseProvider):
    """
    Personality description generator

    This description generator will generate a fairly random description of
    either an overall positive or overall negative personality based on your
    choice.
    """
    # Positive personality type
    Good_things_can_be_said_of = ["Plenty can be said about", "Not much is hidden about", "People are curious about", "Everyone has an opinion on", "Getting to know a person like", "A first impression of", "There is a lot to learn about", "A short summary of", "Stories are told about", "Few have seen the full picture of"]
    above_all = ["first of all remember that", "what matters most is that", "the short version is that", "you should know that", "most agree that", "the main thing is that", "to put it plainly", "the headline is that", "before anything else note that", "it is widely felt that"]
    negatively = ["these come alongside a habit of", "these share the stage with a tendency toward", "these are balanced by a streak of", "these sit beside moments of", "these are sometimes cancelled out by spells of", "these rarely arrive without a touch of", "these are shadowed by a knack for", "these come with an occasional bout of", "these live next door to a pattern of", "these are weighed down by episodes of"]
    loved_for = ["best known for", "widely praised for", "warmly remembered for", "valued most for", "respected for", "appreciated for", "recognized for", "thanked for"]
    people_often = ["Friends often", "People regularly", "Colleagues tend to", "Neighbors will often", "Family members frequently", "Acquaintances usually", "Plenty of people", "Those who know them"]
    when_most_needed = ["when spirits are low", "during hard times", "whenever a hand is needed", "when things go wrong", "on difficult days", "when a shoulder to lean on is needed", "when life gets heavy"]
    Nobody_s_perfect_and = ["Then again,", "Naturally,", "To be fair,", "Of course,"]
    less_favorable_traits = ["a few rough edges", "some off days", "a handful of bad habits", "some stormy moods", "quite a few shortcomings", "a number of sharp corners", "traits worth working on", "several blind spots", "some weak spots", "more than one fault"]
    pose_problems = ["get in the way", "wear on people", "cause some friction", "make for tense moments", "are hard to live with", "spoil more than one good mood", "test everyone's patience", "leave a mark", "complicate simple things", "are not easy on anyone"]
    personally_and_for_others = [", at times", ", mostly in private", ", for everyone around", " now and then", " more often than anyone would like", ", sometimes without noticing", " in the worst moments", ", even on quiet days", ", without warning", " and everyone feels it"]
    Fortunately = ["Luckily"]
    helps_a_lot = ["makes up for most of it", "smooths things over", "keeps the worst in check", "tends to save the day", "usually wins people back", "rarely lets things go too far", "turns most bad days around", "patches things up quickly", "restores the peace", "balances the scales"]

    # Negative personality type
    Many_will_dislike = ["Few people enjoy being around", "Not everyone gets along with", "First impressions rarely do justice to", "It takes little to get annoyed by", "Plenty of folks steer clear of", "There is no shortage of complaints about", "Most people are wary of", "Nobody is surprised to hear criticism of", "The mood tends to sour around", "Reviews are mixed, at best, for"]
    just_the_tip = ["only the beginning", "merely the surface", "a small part of the story"]
    To_make_things_worse = ["And that's not all, since", "Worse yet,", "In addition,", "What's more,", "It gets worse:", "Besides all that,"]
    at_least_being = ["some of that is offset by a streak of being", "the damage is limited by moments of being", "it helps that now and then they manage to be", "a few redeeming moments of being", "things are tempered by times of being", "it is partly cushioned by a knack for being", "it is held back by days of being", "it is softened by spells of being", "there is a counterweight in a habit of being", "a little comfort comes from a flair for being"]
    most_disliked = ["widely disliked for", "quietly resented for", "openly criticized for", "routinely avoided for", "mostly dodged for", "rarely forgiven for", "often blamed for", "seldom welcomed for"]
    Many_occasions_were_spoiled = ["Gatherings have been spoiled", "Friendships have ended", "Good plans have fallen apart", "More than one evening has gone wrong", "Trust has been worn thin", "Bridges have been burned", "Plenty of goodwill has been lost", "Hard feelings linger on every side", "Even simple talks turn sour", "Nothing lasting is easy to build"]
    which_is_unfortunate = ["a pity really", "and that is a shame", "which nobody enjoys", "though that's just how it is", "which many have seen firsthand", "regrettably", "and there is little anyone can do", "which is hard on everyone involved", "as unfortunate as that is", "but that is how things stand"]
    All_in_all = ["Still,", "In fairness,", "Even so,"]
    does_have_some_qualities = ["has a few redeeming sides", "isn't beyond saving", "shows flashes of warmth", "isn't all bad", "has a softer side too", "is not without merit", "has more to offer than it seems", "is a mix of light and shadow", "is as complicated as anyone", "has a few endearing habits"]
    if_nothing_else = ["in the right company", "on a good day", "when it counts", "now and then", "in small measure", "when relaxed", "around trusted people", "if given the chance", "at the right moments", "to some degree"]
    there_s_hope = ["so things could be worse", "so all is not lost", "which counts for something", "so there is room for hope", "and that gives people something to build on", "so a second chance seems fair", "which keeps the door open", "so it is not a lost cause", "so a little optimism is justified", "and that is something"]
    Unfortunately = ["Sadly"]
    tends_to_prevale = ["usually wins out in the end", "tends to resurface at the worst time", "never stays hidden for long", "keeps undoing the good", "casts a long shadow over the rest", "returns whenever things go well", "will likely follow them around", "overshadows most of what is good", "rarely stays quiet", "outlasts the better moments"]

    good = [["active", "active nature"], ["adaptable", "adaptability"], ["adventurous", "adventurous nature"], ["ambitious", "ambitions"], ["amusing", "amusing nature"], ["anticipative", "anticipative nature"], ["appreciative", "appreciative nature"], ["aspiring", "aspirations"], ["athletic", "athleticism"], ["balanced", "sense of balance"], ["brilliant", "brilliance"], ["calm", "calm nature"], ["capable", "capabilities"], ["captivating", "captivating nature"], ["caring", "caring nature"], ["charismatic", "charisma"], ["charming", "charm"], ["cheerful", "cheerfulness"], ["clear-headed", "clear-headedness"], ["clever", "clever nature"], ["companionably", "companionship"], ["compassionate", "compassion"], ["confident", "confidence"], ["considerate", "considerate nature"], ["contemplative", "contemplative nature"], ["cooperative", "cooperation"], ["courageous", "courage"], ["courteous", "courtesy"], ["creative", "creativity"], ["curious", "curiosity"], ["daring", "daring nature"], ["decisive", "decisive nature"], ["dedicated", "dedication"], ["determined", "determination"], ["disciplined", "discipline"], ["discreet", "discretion"], ["dutiful", "dutiful nature"], ["dynamic", "dynamic nature"], ["earnest", "earnestness"], ["efficient", "efficiency"], ["elegant", "elegance"], ["empathetic", "empathy"], ["energetic", "energy"], ["enthusiastic", "enthusiasm"], ["exciting", "excitement"], ["faithful", "faithfulness"], ["farsighted", "farsightedness"], ["flexible", "flexibility"], ["focused", "focus"], ["forgiving", "forgiving nature"], ["forthright", "forthright nature"], ["freethinking", "freethinking nature"], ["friendly", "friendliness"], ["fun-loving", "fun-loving nature"], ["generous", "generosity"], ["gentle", "gentleness"], ["good-natured", "good nature"], ["gracious", "grace"], ["hardworking", "hardworking nature"], ["helpful", "helping hand"], ["heroic", "heroic nature"], ["honest", "honesty"], ["honorable", "honor"], ["humble", "humbleness"], ["humorous", "sense of humor"], ["idealistic", "idealism"], ["imaginative", "imagination"], ["incisive", "incisive nature"], ["incorruptible", "incorruptible nature"], ["independent", "independence"], ["individualistic", "individualism"], ["innovative", "innovative nature"], ["insightful", "insight"], ["intelligent", "intelligence"], ["intuitive", "intuition"], ["kind", "kindness"], ["leaderly", "leadership"], ["lovable", "loving nature"], ["loyal", "loyalty"], ["methodical", "methodical nature"], ["modest", "modesty"], ["objective", "objectivity"], ["observant", "observant nature"], ["open", "openness"], ["optimistic", "optimism"], ["orderly", "sense of order"], ["organized", "organization skills"], ["outspoken", "outspoken nature"], ["passionate", "passion"], ["patient", "patience"], ["perceptive", "perceptive nature"], ["persuasive", "persuasive nature"], ["planful", "planning"], ["playful", "playfulness"], ["practical", "practical thinking"], ["precise", "precision"], ["protective", "protective nature"], ["punctual", "punctuality"], ["rational", "rational thinking"], ["realistic", "realism"], ["reflective", "reflective thinking"], ["relaxed", "relaxed nature"], ["reliable", "reliability"], ["resourceful", "resourcefulness"], ["respectful", "respect"], ["responsible", "responsibility"], ["responsive", "responsiveness"], ["romantic", "romantic nature"], ["selfless", "selflessness"], ["sensitive", "sensitive nature"], ["sentimental", "sentimentality"], ["sharing", "sharing nature"], ["sociable", "sociable nature"], ["spontaneous", "spontaneity"], ["stable", "stable nature"], ["surprising", "surprising nature"], ["sweet", "sweet nature"], ["sympathetic", "sympathetic nature"], ["tolerant", "tolerance"], ["trusting", "trusting nature"], ["understanding", "understanding nature"], ["upright", "upright nature"], ["warm", "warmness"], ["wise", "wisdom"], ["witty", "wit"]]
    bad = [["abrasive", "abrasiveness"], ["aggressive", "aggression"], ["agonizing", "agonizing nature"], ["amoral", "amoral nature"], ["angry", "anger"], ["apathetic", "apathetic nature"], ["argumentative", "argumentativeness"], ["arrogant", "arrogant nature"], ["barbaric", "barbaric nature"], ["blunt", "blunt nature"], ["brutal", "brutish ways"], ["callous", "callous nature"], ["careless", "carelessness"], ["childish", "childish nature"], ["coarse", "coarseness"], ["cold", "coldness"], ["conceited", "conceited nature"], ["crass", "crassness"], ["crazy", "craziness"], ["criminal", "criminal nature"], ["crude", "crude ways"], ["cruel", "cruelty"], ["cynical", "cynical nature"], ["deceitful", "deceitful ways"], ["demanding", "demanding nature"], ["desperate", "desperation"], ["destructive", "destructive nature"], ["devious", "deviousness"], ["difficult", "difficult nature"], ["disconcerting", "disconcerting nature"], ["dishonest", "dishonesty"], ["disloyal", "disloyalty"], ["disorderly", "disorderliness"], ["disrespectful", "disrespectful nature"], ["disruptive", "disruptive nature"], ["disturbing", "disturbing nature"], ["dominating", "dominating nature"], ["egocentric", "ego"], ["envious", "envy"], ["extreme", "extreme nature"], ["frightening", "frightening nature"], ["greedy", "greed"], ["grim", "grim ways"], ["hateful", "hatred"], ["hostile", "hostility"], ["ignorant", "ignorance"], ["impatient", "impatience"], ["imprudent", "imprudence"], ["inconsiderate", "inconsideration"], ["insensitive", "insensitivity"], ["insincere", "insincerity"], ["insulting", "insulting nature"], ["intolerant", "intolerance"], ["irrational", "irrational nature"], ["irresponsible", "irresponsibility"], ["irritable", "irritable nature"], ["lazy", "laziness"], ["malicious", "maliciousness"], ["miserable", "miserable nature"], ["monstrous", "monstrous nature"], ["morbid", "morbid nature"], ["narcissistic", "narcissistic nature"], ["narrow-minded", "narrow-mindedness"], ["negativistic", "negativity"], ["neglectful", "neglectful nature"], ["obnoxious", "obnoxious nature"], ["obsessive", "obsessive nature"], ["opportunistic", "opportunistic ways"], ["pedantic", "pedantic nature"], ["perverse", "perversions"], ["petty", "petty nature"], ["pompous", "pompous nature"], ["possessive", "possessive nature"], ["power-hungry", "power-hungry ways"], ["predatory", "predatory nature"], ["prejudiced", "prejudices"], ["pretentious", "pretentiousness"], ["provocative", "provocative nature"], ["sadistic", "sadistic ways"], ["scornful", "scornful nature"], ["self-indulgent", "self-indulgence"], ["selfish", "selfishness"], ["shallow", "shallowness"], ["sly", "slyness"], ["superficial", "superficial nature"], ["tactless", "tactlessness"], ["thievish", "thievish nature"], ["thoughtless", "thoughtlessness"], ["treacherous", "treachery"], ["troublesome", "troublesome nature"], ["uncaring", "uncaring nature"], ["unfriendly", "unfriendliness"], ["unstable", "instability"], ["venomous", "venomous nature"], ["vindictive", "vindictive nature"]]

    firstnames_male = ["Aaron", "Adam", "Aidan", "Aiden", "Alex", "Alexander", "Alfie", "Andrew", "Anthony", "Archie", "Arthur", "Ashton", "Bailey", "Ben", "Benjamin", "Billy", "Blake", "Bobby", "Bradley", "Brandon", "Caleb", "Callum", "Cameron", "Charles", "Charlie", "Christopher", "Cody", "Connor", "Corey", "Daniel", "David", "Declan", "Dexter", "Dominic", "Dylan", "Edward", "Elliot", "Ellis", "Ethan", "Evan", "Ewan", "Finlay", "Finley", "Frankie", "Freddie", "Frederick", "Gabriel", "George", "Harley", "Harrison", "Harry", "Harvey", "Hayden", "Henry", "Isaac", "Jack", "Jackson", "Jacob", "Jake", "James", "Jamie", "Jay", "Jayden", "Jenson", "Joe", "Joel", "John", "Jonathan", "Jordan", "Joseph", "Josh", "Joshua", "Jude", "Kai", "Kayden", "Kian", "Kieran", "Kyle", "Leo", "Leon", "Lewis", "Liam", "Logan", "Louie", "Louis", "Luca", "Lucas", "Luke", "Mason", "Matthew", "Max", "Michael", "Morgan", "Nathan", "Nicholas", "Noah", "Oliver", "Ollie", "Oscar", "Owen"]
    firstnames_female = ["Abbie", "Abby", "Abigail", "Aimee", "Alexandra", "Alice", "Alicia", "Alisha", "Amber", "Amelia", "Amelie", "Amy", "Anna", "Ava", "Bella", "Bethany", "Brooke", "Caitlin", "Cerys", "Charlie", "Charlotte", "Chelsea", "Chloe", "Courtney", "Daisy", "Danielle", "Demi", "Eleanor", "Eliza", "Elizabeth", "Ella", "Ellie", "Eloise", "Elsie", "Emilia", "Emily", "Emma", "Erin", "Esme", "Eva", "Eve", "Evelyn", "Evie", "Faith", "Freya", "Georgia", "Georgina", "Grace", "Gracie", "Hannah", "Harriet", "Heidi", "Hollie", "Holly", "Imogen", "Isabel", "Isabella", "Isabelle", "Isla", "Isobel", "Jade", "Jasmine", "Jennifer", "Jessica", "Jodie", "Julia", "Kate", "Katherine", "Katie", "Kayla", "Kayleigh", "Keira", "Lacey", "Lara", "Laura", "Lauren", "Layla", "Leah", "Lexi", "Lexie", "Libby", "Lilly", "Lily", "Lola", "Louise", "Lucy", "Lydia", "Maddison", "Madeleine", "Madison", "Maisie", "Maisy", "Maria", "Martha", "Matilda", "Maya", "Megan", "Melissa", "Mia", "Mollie"]
    lastnames = ["Adams", "Allen", "Anderson", "Andrews", "Armstrong", "Atkinson", "Austin", "Bailey", "Baker", "Ball", "Barker", "Barnes", "Barrett", "Bates", "Baxter", "Bell", "Bennett", "Berry", "Black", "Booth", "Bradley", "Brooks", "Brown", "Burke", "Burns", "Burton", "Butler", "Byrne", "Campbell", "Carr", "Carter", "Chambers", "Chapman", "Clark", "Clarke", "Cole", "Collins", "Cook", "Cooke", "Cooper", "Cox", "Cunningham", "Davidson", "Davies", "Davis", "Dawson", "Day", "Dean", "Dixon", "Doyle", "Duncan", "Edwards", "Elliott", "Ellis", "Evans", "Fisher", "Fletcher", "Foster", "Fox", "Francis", "Fraser", "Gallagher", "Gardner", "George", "Gibson", "Gill", "Gordon", "Graham", "Grant", "Gray", "Green", "Griffiths", "Hall", "Hamilton", "Harper", "Harris", "Harrison", "Hart", "Harvey", "Hawkins", "Hayes", "Henderson", "Hill", "Holland", "Holmes", "Hopkins", "Houghton", "Howard", "Hudson", "Hughes", "Hunt", "Hunter", "Hussain", "Jackson", "James", "Jenkins", "John", "Johnson", "Johnston", "Jones", "Jordan", "Kaur", "Kelly", "Kennedy", "Khan", "King", "Knight", "Lane", "Lawrence", "Lawson", "Lee", "Lewis", "Lloyd", "Lowe", "Macdonald", "Marsh", "Marshall", "Martin", "Mason", "Matthews", "May", "Mccarthy", "Mcdonald", "Miller", "Mills", "Mitchell", "Moore", "Morgan", "Morris", "Moss", "Murphy", "Murray", "Newman", "Nicholson", "Owen", "Palmer", "Parker", "Parry", "Patel", "Pearce", "Pearson", "Perry", "Phillips", "Poole", "Porter", "Powell", "Price", "Read", "Rees", "Reid", "Reynolds", "Richards", "Richardson", "Riley", "Roberts", "Robertson", "Robinson", "Rogers", "Rose", "Ross", "Russell", "Ryan", "Saunders", "Scott", "Sharp", "Shaw", "Simpson", "Smith", "Spencer", "Stevens", "Stewart", "Stone", "Sutton", "Taylor", "Thomas", "Thompson", "Thomson", "Turner", "Walker", "Wallace", "Walsh", "Ward", "Watson", "Watts", "Webb", "Wells", "West", "White", "Wilkinson", "Williams", "Williamson", "Willis", "Wilson", "Wood", "Woods", "Wright", "Young"]

    def personality(self, first_name=None, last_name=None, gender=None):
        personality_type = self.random_element(OrderedDict((('positive', 0.75), ('negative', 0.25))))
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


SEED = 20261116


def refresh_personalities(path):
    """Rewrites every personality text of accounts.json in place (same records,
    names and fields), from the fixed SEED, so the output is reproducible."""
    fake = Faker()
    fake.add_provider(Personality)
    random.seed(SEED)
    Faker.seed(SEED)
    with open(path) as f:
        docs = json.load(f, object_pairs_hook=OrderedDict)
    for x in docs:
        gender = 'female' if x.get('gender', x.get('#gender')) in ('F', 'female') else 'male'
        x['personality']['_value'] = fake.personality(
            first_name=x['name']['firstName'], last_name=x['name']['lastName'], gender=gender)
    with open(path, 'w') as f:
        json.dump(docs, f, indent=2, separators=(', ', ': '))


def main():
    if sys.argv[1:2] == ['--personality']:
        refresh_personalities(sys.argv[2])
        return
    fake = Faker()
    fake.add_provider(Personality)
    fake.add_provider(Fruit)
    random.seed(SEED)
    Faker.seed(SEED)

    with open('accounts.json') as f:
        docs = json.load(f, object_pairs_hook=OrderedDict)

    for idx, x in enumerate(docs):
        _id = x.pop('_id')

        x.pop('_schema', None)

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

        x.pop('checkin', None)

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
        x.clear()

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
            x['style']['_namespace'] = True
            x['style']['_partial_paths'] = True
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

        x['personality'] = {
            '_type': 'text',
            '_language': 'en',
            '_value': personality,
        }

        x['_id'] = _id

    with open('accounts_mix.json', 'w') as f:
        json.dump(docs, f, indent=2)


if __name__ == '__main__':
    main()
