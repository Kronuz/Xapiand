/*
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

#include "haystack.h"


Haystack::Haystack(const std::string &path_) :
	path(path_)
{
	for (volumes = 0; read_volume(volumes); volumes++);
}

Haystack::~Haystack()
{
}

bool Haystack::read_volume(unsigned int volume)
{
	NeedleIndex buffer[1024];
	char filename[PATH_MAX];

	snprintf(filename, sizeof(filename), "%s/%d.index", path.c_str(), volume);
	FILE *f = fopen(filename, "r");
	if (!f) return false;

	size_t items;
	do {
		items = fread(buffer, sizeof(NeedleIndex), 1024, f);
		for (size_t i = 0; i < items; i++) {
			NeedleIndex& ni = buffer[i];
			index[ni.key] = std::pair<uint32_t, uint32_t>(ni.offset, ni.size);
		}
	} while (items);

	fclose(f);
	return true;
}
