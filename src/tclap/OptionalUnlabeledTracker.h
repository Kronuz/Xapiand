/*
 * Copyright (c) 2005, Michael E. Smoot.
 * All rights reverved.
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

#ifndef TCLAP_OPTIONAL_UNLABELED_TRACKER_H
#define TCLAP_OPTIONAL_UNLABELED_TRACKER_H

#include <string>

namespace TCLAP {

class OptionalUnlabeledTracker
{

	public:

		static void check( bool req, const std::string& argName );

		static void gotOptional() { alreadyOptionalRef() = true; }

		static bool& alreadyOptional() { return alreadyOptionalRef(); }

	private:

		static bool& alreadyOptionalRef() { static bool ct = false; return ct; }
};


inline void OptionalUnlabeledTracker::check( bool req, const std::string& argName )
{
    if ( OptionalUnlabeledTracker::alreadyOptional() )
        throw( SpecificationException(
	"You can't specify ANY Unlabeled Arg following an optional Unlabeled Arg",
	                argName ) );

    if ( !req )
        OptionalUnlabeledTracker::gotOptional();
}


} // namespace TCLAP

#endif
