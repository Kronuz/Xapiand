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

#ifndef TCLAP_CONSTRAINT_H
#define TCLAP_CONSTRAINT_H

#include <string>
#include <vector>
#include <list>
#include <iostream>
#include <iomanip>
#include <algorithm>

namespace TCLAP {

/**
 * The interface that defines the interaction between the Arg and Constraint.
 */
template<class T>
class Constraint
{

	public:
		/**
		 * Returns a description of the Constraint.
		 */
		virtual std::string description() const =0;

		/**
		 * Returns the short ID for the Constraint.
		 */
		virtual std::string shortID() const =0;

		/**
		 * The method used to verify that the value parsed from the command
		 * line meets the constraint.
		 * \param value - The value that will be checked.
		 */
		virtual bool check(const T& value) const =0;

		/**
		 * Destructor.
		 * Silences warnings about Constraint being a base class with virtual
		 * functions but without a virtual destructor.
		 */
		virtual ~Constraint() { ; }
};

} //namespace TCLAP
#endif
