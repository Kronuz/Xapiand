/*
 * Copyright (c) 2015 xpol xpolife@gmail.com
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
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

#include <string>

#include "../rapidjson/document.h"
#include "../rapidjson/stringbuffer.h"
#include "../rapidjson/writer.h"

#include "../msgpack.hpp"
#include "rapidjson.hpp"


struct Msgpack {

    struct Document {
        std::string buffer;
        msgpack::unpacked unpacked;
    };

public:
    typedef Document document_type;
    static void load(Document& doc, const std::string& str);
    static std::string save(const rapidjson::Document& doc);
};

struct RapidJSON {
    typedef rapidjson::Document document_type;

public:
	static void load(rapidjson::Document& doc, const std::string& str);
	static std::string save(const Msgpack::Document& sdoc);
};

template <typename Src, typename Dest>
std::string convert(std::string &s_str)
{
    typename Src::document_type doc;
    Src::load(doc, s_str);
    return Dest::save(doc);
}
