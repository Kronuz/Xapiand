/*
 * Copyright (c) 2003, Michael E. Smoot.
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

#ifndef TCLAP_HELP_VISITOR_H
#define TCLAP_HELP_VISITOR_H

#include "tclap/CmdLineInterface.h"
#include "tclap/CmdLineOutput.h"
#include "tclap/Visitor.h"

namespace TCLAP {

/**
 * A Visitor object that calls the usage method of the given CmdLineOutput
 * object for the specified CmdLine object.
 */
class HelpVisitor: public Visitor
{
	private:
		/**
		 * Prevent accidental copying.
		 */
		HelpVisitor(const HelpVisitor& rhs);
		HelpVisitor& operator=(const HelpVisitor& rhs);

	protected:

		/**
		 * The CmdLine the output will be generated for.
		 */
		CmdLineInterface* _cmd;

		/**
		 * The output object.
		 */
		CmdLineOutput** _out;

	public:

		/**
		 * Constructor.
		 * \param cmd - The CmdLine the output will be generated for.
		 * \param out - The type of output.
		 */
		HelpVisitor(CmdLineInterface* cmd, CmdLineOutput** out)
				: Visitor(), _cmd( cmd ), _out( out ) { }

		/**
		 * Calls the usage method of the CmdLineOutput for the
		 * specified CmdLine.
		 */
		void visit() { (*_out)->usage(*_cmd); throw ExitException(0); }

};

}

#endif
