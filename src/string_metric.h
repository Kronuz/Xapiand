/*
 * Copyright (c) 2015-2018 Dubalu LLC
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#pragma once

// The string-metric algorithms now come from the standalone string-similarity
// library (github.com/Kronuz/string-similarity), whose headers sit flat at its
// root (no metrics/ prefix). The serialise()/unserialise() persistence the
// library drops is re-attached below via metric_serialise.h (the Serialisable*
// wrappers), and double_metaphone.h adds the Double Metaphone encoder selectable
// for PhoneticMetric alongside the soundex family.
#include "basic_string_metric.h"
#include "jaccard.h"
#include "jaro.h"
#include "jaro_winkler.h"
#include "lcsubsequence.h"
#include "lcsubstr.h"
#include "levenshtein.h"
#include "sorensen_dice.h"
#include "phonetic_metric.h"

#include "metric_serialise.h"
#include "double_metaphone.h"
