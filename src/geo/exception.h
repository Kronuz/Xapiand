/*
 * Copyright (C) 2017 deipi.com LLC and contributors. All rights reserved.
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

#pragma once

#include "../exception.h"


class GeoError : public ClientError {
public:
	template<typename... Args>
	GeoError(Args&&... args) : ClientError(std::forward<Args>(args)...) { }
};


class CartesianError : public GeoError {
public:
	template<typename... Args>
	CartesianError(Args&&... args) : GeoError(std::forward<Args>(args)...) { }
};


class GeometryError : public GeoError {
public:
	template<typename... Args>
	GeometryError(Args&&... args) : GeoError(std::forward<Args>(args)...) { }
};


class NullConvex : public GeometryError {
public:
	template<typename... Args>
	NullConvex(Args&&... args) : GeometryError(std::forward<Args>(args)...) { }
};


class HTMError : public GeoError {
public:
	template<typename... Args>
	HTMError(Args&&... args) : GeoError(std::forward<Args>(args)...) { }
};


class EWKTError : public GeoError {
public:
	template<typename... Args>
	EWKTError(Args&&... args) : GeoError(std::forward<Args>(args)...) { }
};
