/*
 * Copyright (c) 2015-2018 Dubalu LLC
 * Copyright (c) 2014 furan
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

#include "ContentReader.h"


ContentReader::ContentReader(char* _content)
	: currentPosition(0),
	  currentLine(1),
	  currentColumn(1),
	  content(_content) { }


Symbol
ContentReader::NextSymbol()
{
	Symbol ret;
	if (content[currentPosition] != '\0') {
		char c = content[currentPosition++];
		switch (c) {
			case '\n':
				currentColumn++;
				currentLine = 1;
				if (content[currentPosition] == '\r') {
					currentPosition++;
				}
				break;
			case '\r':
				currentColumn++;
				currentLine = 1;
				if (content[currentPosition] == '\n') {
					currentPosition++;
				}
				break;
			default:
				currentColumn++;
				break;
		}
		ret.symbol = c;
	} else {
		ret.symbol = 0;
	}
	ret.line = currentLine;
	ret.column = currentColumn;
	return ret;
}
