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

#pragma once

#include <dirent.h>              // for DIR, readdir, opendir, closedir
#include <string>                // for std::string
#include <string_view>           // for std::string_view
#include <vector>                // for std::vector


struct File_ptr {
	struct dirent *ent;

	File_ptr()
		: ent(nullptr) { }
};


void delete_files(std::string_view path, const std::vector<std::string>& patterns = {"*"});

void move_files(std::string_view src, std::string_view dst);

bool exists(std::string_view path);

bool mkdir(std::string_view path);

bool mkdirs(std::string_view path);

bool build_path_index(std::string_view path_index);

DIR* opendir(std::string_view path, bool create = false);

void find_file_dir(DIR* dir, File_ptr& fptr, std::string_view pattern, bool pre_suf_fix);

// Copy all directory if file_name and new_name are empty
int copy_file(std::string_view src, std::string_view dst, bool create = true, std::string_view file_name = "", std::string_view new_name = "");

char* normalize_path(const char* src, const char* end, char* dst, bool slashed = false);
char* normalize_path(std::string_view src, char* dst, bool slashed = false);
std::string normalize_path(std::string_view src, bool slashed = false);
