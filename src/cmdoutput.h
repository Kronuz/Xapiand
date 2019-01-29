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

#include "config.h"           // for HAVE_SSTREAM, HAVE_STRSTREAM

#include <sysexits.h>         // for EX_USAGE

#include "package.h"          // for Package
#include "tclap/CmdLine.h"    // for CmdLine, ArgException, Arg, CmdL...


namespace TCLAP {

constexpr int LINE_LENGTH = 78;
constexpr int MAX_PADDING_DESC = 30;

/*
 * This exemplifies how the output class can be overridden to provide
 * user defined output.
 */
class CmdOutput : public StdOutput {
	void spacePrint(std::ostream& os, const std::string& s, int maxWidth,
		int indentSpaces, int secondLineOffset, bool endl=true) const {
		int len = static_cast<int>(s.length());

		if ((len + indentSpaces > maxWidth) && maxWidth > 0) {
			int allowedLen = maxWidth - indentSpaces;
			if (allowedLen < 0) allowedLen = 0;
			int start = 0;
			while (start < len) {
				// find the substring length
				// int stringLen = std::min<int>( len - start, allowedLen );
				// doing it this way to support a VisualC++ 2005 bug
				using namespace std;
				int stringLen = min<int>(len - start, allowedLen);

				// trim the length so it doesn't end in middle of a word
				if (stringLen == allowedLen) {
					while (stringLen >= 0 &&
						s[stringLen + start] != ' ' &&
						s[stringLen + start] != ',' &&
						s[stringLen + start] != '|') {
						--stringLen;
					}
				}

				// ok, the word is longer than the line, so just split
				// wherever the line ends
				if (stringLen <= 0) {
					stringLen = allowedLen;
				}

				// check for newlines
				for (int i = 0; i < stringLen; ++i) {
					if (s[start + i] == '\n') {
						stringLen = i + 1;
					}
				}

				if (start != 0) {
					os << std::endl;
				}

				// print the indent
				for (int i = 0; i < indentSpaces; ++i) {
					os << " ";
				}

				if (start == 0) {
					// handle second line offsets
					indentSpaces += secondLineOffset;

					// adjust allowed len
					allowedLen = maxWidth - secondLineOffset;
				}

				os << s.substr(start, stringLen);

				// so we don't start a line with a space
				while (s[stringLen + start] == ' ' && start < len) {
					++start;
				}

				start += stringLen;
			}
		} else {
			for (int i = 0; i < indentSpaces; ++i) {
				os << " ";
			}
			os << s;
		}

		if (endl) {
			os << std::endl;
		}
	}

	void _shortUsage(CmdLineInterface& _cmd, std::ostream& os) const {
		std::list<Arg*> argList = _cmd.getArgList();
		std::string progName = _cmd.getProgramName();
		XorHandler xorHandler = _cmd.getXorHandler();
		std::vector<std::vector<Arg*>> xorList = xorHandler.getXorList();

		std::string s = progName + " ";

		// first the xor
		for (size_t i = 0; i < xorList.size(); ++i) {
			s += " {";
			for (auto it = xorList[i].begin(); it != xorList[i].end(); ++it) {
				s += (*it)->shortID() + "|";
			}

			s[s.length() - 1] = '}';
		}

		// then the rest
		for (auto it = argList.begin(); it != argList.end(); ++it) {
			if (!xorHandler.contains((*it))) {
				s += " " + (*it)->shortID();
			}
		}

		// if the program name is too long, then adjust the second line offset
		int secondLineOffset = static_cast<int>(progName.length()) + 2;
		if (secondLineOffset > LINE_LENGTH / 2) {
			secondLineOffset = static_cast<int>(LINE_LENGTH / 2);
		}

		spacePrint(os, s, LINE_LENGTH, 3, secondLineOffset);
	}

	void _longUsage(CmdLineInterface& _cmd, std::ostream& os) const {
		std::list<Arg*> argList = _cmd.getArgList();
		std::string message = _cmd.getMessage();
		XorHandler xorHandler = _cmd.getXorHandler();
		std::vector< std::vector<Arg*> > xorList = xorHandler.getXorList();

		// Check the right padding for description for xorList.
		size_t max = 0;
		for (size_t i = 0; i < xorList.size() && max != MAX_PADDING_DESC; ++i) {
			for (auto it = xorList[i].begin(); it != xorList[i].end(); ++it) {
				const std::string& id = (*it)->longID();
				if (id.size() > max) {
					max = id.size();
					if (max > MAX_PADDING_DESC) {
						max = MAX_PADDING_DESC;
						break;
					}
				}
			}
		}

		// First the xor
		for (size_t i = 0; i < xorList.size(); ++i) {
			for (auto it = xorList[i].begin(); it != xorList[i].end(); ++it) {
				const std::string& id = (*it)->longID();
				if (id.length() > max) {
					spacePrint(os, id, LINE_LENGTH, 3, 3);
					spacePrint(os, (*it)->getDescription(), LINE_LENGTH, max + 5, 0, false);
				} else {
					spacePrint(os, id, LINE_LENGTH, 3, 3, false);
					spacePrint(os, (*it)->getDescription(), LINE_LENGTH, static_cast<int>((max + 2) - id.length()), id.length() + 3, false);
				}

				if (it + 1 != xorList[i].end()) {
					spacePrint(os, "-- OR --", LINE_LENGTH, 9, 3);
				}
			}
			os << std::endl << std::endl;
		}

		// Check the right padding for description or args.
		max = 0;
		for (auto it = argList.begin(); it != argList.end(); ++it) {
			if (!xorHandler.contains((*it))) {
				const std::string& id = (*it)->longID();
				if (id.size() > max) {
					max = id.size();
					if (max > MAX_PADDING_DESC) {
						max = MAX_PADDING_DESC;
						break;
					}
				}
			}
		}

		// Then the rest
		for (auto it = argList.begin(); it != argList.end(); ++it) {
			if (!xorHandler.contains((*it))) {
				const std::string& id = (*it)->longID();
				if (id.length() > max) {
					spacePrint(os, id, LINE_LENGTH, 3, 3);
					spacePrint(os, (*it)->getDescription(), LINE_LENGTH, max + 5, 0, false);
				} else {
					spacePrint(os, id, LINE_LENGTH, 3, 3, false);
					spacePrint(os, (*it)->getDescription(), LINE_LENGTH, static_cast<int>((max + 2) - id.size()), id.length() + 3, false);
				}
				os << std::endl;
			}
		}
		os << std::endl;

		if (!message.empty()) {
			spacePrint(os, message, LINE_LENGTH, 3, 0);
		}
	}

public:
	void failure(CmdLineInterface& _cmd, ArgException& exc) override {
		std::string progName = _cmd.getProgramName();

		std::cerr << "Error: " << exc.argId() << std::endl;
		spacePrint(std::cerr, exc.error(), LINE_LENGTH, 3, 0);
		std::cerr << std::endl;

		if (_cmd.hasHelpAndVersion()) {
			std::cerr << "Usage: " << std::endl;

			_shortUsage(_cmd, std::cerr);

			std::cerr << std::endl << "For complete usage and help type: "
					  << std::endl << "   " << progName << " "
					  << Arg::nameStartString() << "help"
					  << std::endl << std::endl;
		} else {
			usage(_cmd);
		}

		throw ExitException(EX_USAGE);
	}

	void usage(CmdLineInterface& _cmd) override {
		spacePrint(std::cout, Package::STRING, LINE_LENGTH, 0, 0);
		spacePrint(std::cout, "[" + Package::BUGREPORT + "]", LINE_LENGTH, 0, 0);

		std::cout << "Usage: " << std::endl;
		_shortUsage(_cmd, std::cout);

		std::cout << std::endl << "Where: " << std::endl;
		_longUsage(_cmd, std::cout);
	}

	void version(CmdLineInterface& _cmd) override {
		std::string xversion = _cmd.getVersion();
		std::cout << xversion << std::endl;
	}
};

}
