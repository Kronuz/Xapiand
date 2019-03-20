/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include "package.h"
#include "package_config.h"

#ifndef PACKAGE_REVISION
#define PACKAGE_REVISION ""
#endif

#ifndef PACKAGE_HASH
#define PACKAGE_HASH ""
#endif

std::string Package::STRING = PACKAGE_STRING;
std::string Package::NAME = PACKAGE_NAME;
std::string Package::VERSION = PACKAGE_VERSION;
std::string Package::REVISION = PACKAGE_REVISION;
std::string Package::VERSION_STRING = PACKAGE_VERSION_STRING;
std::string Package::REVISION_STRING = PACKAGE_REVISION_STRING;
std::string Package::HASH = PACKAGE_HASH;
std::string Package::URL = PACKAGE_URL;
std::string Package::BUGREPORT = PACKAGE_BUGREPORT;
std::string Package::TARNAME = PACKAGE_TARNAME;
