# Copyright (c) 2015-2019 Dubalu LLC
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
#

#     ____        __
#    / __ \____ _/ /____  _____
#   / / / / __ `/ __/ _ \/ ___/
#  / /_/ / /_/ / /_/  __(__  )
# /_____/\__,_/\__/\___/____/


HOUR_TERM = "hour"
DAY_TERM = "day"
MONTH_TERM = "month"
YEAR_TERM = "year"
DECADE_TERM = "decade"
CENTURY_TERM = "century"
MILLENIUM_TERM = "millenium"

# Typical ranges
DAY_TO_YEAR_ACCURACY = [
    DAY_TERM,
    MONTH_TERM,
    YEAR_TERM,
]

YEAR_ACCURACY = [
    YEAR_TERM,
]

HOUR_TO_YEAR_ACCURACY = [
    HOUR_TERM,
    DAY_TERM,
    MONTH_TERM,
    YEAR_TERM,
]

#    ______                       _       __
#   / ____/__  ____  ____  ____  (_)___  / /______
#  / / __/ _ \/ __ \/ __ \/ __ \/ / __ \/ __/ ___/
# / /_/ /  __/ /_/ / /_/ / /_/ / / / / / /_(__  )
# \____/\___/\____/ .___/\____/_/_/ /_/\__/____/
#                /_/

# HTM terms (Hierarchical Triangular Mesh)
# Any integer value in the range 0-25 can be used to specify an HTM level
# An approximation of the accuracy obtained by a level X can be estimated as:
#    0.30 * 2 ** (25 - X)
LEVEL_0_TERM = 0  # Approx. accuracy: 10,066,329.6 m ...  10,066 Km approx
LEVEL_5_TERM = 5  # Approx. accuracy: 314,572.8 m ...  314 Km approx
LEVEL_10_TERM = 10  # Approx. accuracy: 9,830.4 m ...  10 Km approx
LEVEL_15_TERM = 15  # Approx. accuracy: 307.2 m
LEVEL_20_TERM = 20  # Approx. accuracy: 9.6 m

# Typical ranges
STATE_TO_BLOCK_ACCURACY = [
    LEVEL_5_TERM,
    LEVEL_10_TERM,
    LEVEL_15_TERM,
]

AREA_TO_BLOCK_ACCURACY = [
    LEVEL_10_TERM,
    LEVEL_15_TERM,
]


#     _   __                          _
#    / | / /_  ______ ___  ___  _____(_)____
#   /  |/ / / / / __ `__ \/ _ \/ ___/ / ___/
#  / /|  / /_/ / / / / / /  __/ /  / / /__
# /_/ |_/\__,_/_/ /_/ /_/\___/_/  /_/\___/

LEVEL_10_TERM = 10
LEVEL_100_TERM = 100
LEVEL_1000_TERM = 1000
LEVEL_10000_TERM = 10000
LEVEL_100000_TERM = 100000
LEVEL_1000000_TERM = 1000000
LEVEL_10000000_TERM = 10000000

# Typical ranges
TENS_TO_TEN_THOUSANDS_ACCURACY = [
    LEVEL_10_TERM,
    LEVEL_100_TERM,
    LEVEL_1000_TERM,
    LEVEL_10000_TERM,
]

TENS_ACCURACY = [
    LEVEL_10_TERM,
]

HUDREDS_TO_MILLIONS_ACCURACY = [
    LEVEL_100_TERM,
    LEVEL_1000_TERM,
    LEVEL_10000_TERM,
    LEVEL_100000_TERM,
    LEVEL_1000000_TERM,
]

HUDREDS_ACCURACY = [
    LEVEL_100_TERM,
]

HUDREDS_TO_THOUSANDS_ACCURACY = [
    LEVEL_100_TERM,
    LEVEL_1000_TERM,
]

THOUSANDS_ACCURACY = [
    LEVEL_1000_TERM,
]

HUDREDS_TO_TEN_THOUSANDS_ACCURACY = [
    LEVEL_100_TERM,
    LEVEL_1000_TERM,
    LEVEL_10000_TERM,
]
