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

#ifndef TCLAP_VERSION_VISITOR_H
#define TCLAP_VERSION_VISITOR_H

#include "CmdLineInterface.h"
#include "CmdLineOutput.h"
#include "Visitor.h"

namespace TCLAP {

/**
 * A Vistor that will call the version method of the given CmdLineOutput
 * for the specified CmdLine object and then exit.
 */
class VersionVisitor: public Visitor
{
	private:
		/**
		 * Prevent accidental copying
		 */
		VersionVisitor(const VersionVisitor& rhs);
		VersionVisitor& operator=(const VersionVisitor& rhs);

	protected:

		/**
		 * The CmdLine of interest.
		 */
		CmdLineInterface* _cmd;

		/**
		 * The output object. 
		 */
		CmdLineOutput** _out;

	public:

		/**
		 * Constructor.
		 * \param cmd - The CmdLine the output is generated for. 
		 * \param out - The type of output. 
		 */
		VersionVisitor( CmdLineInterface* cmd, CmdLineOutput** out ) 
				: Visitor(), _cmd( cmd ), _out( out ) { }

		/**
		 * Calls the version method of the output object using the
		 * specified CmdLine.
		 */
		void visit() { 
		    (*_out)->version(*_cmd); 
		    throw ExitException(0); 
		}

};

}

#endif
