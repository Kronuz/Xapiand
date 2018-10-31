/*
 * Copyright (C) 2015-2018 Dubalu LLC. All rights reserved.
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

#include "fs.hh"

#include <fnmatch.h>             // for fnmatch
#include <stdio.h>               // for rename
#include <sys/stat.h>            // for stat, mkdir
#include <unistd.h>              // for rmdir
#include <vector>                // for std::vector

#include "io.hh"                 // for io::*
#include "log.h"                 // for L_ERR, L_WARNING, L_INFO
#include "repr.hh"               // for repr
#include "split.h"               // for Split
#include "string.hh"             // for string::startswith, string::endswith
#include "stringified.hh"        // for stringified

#define L_FS L_NOTHING


// #undef L_DEBUG
// #define L_DEBUG L_GREY
// #undef L_CALL
// #define L_CALL L_STACKED_DIM_GREY
// #undef L_FS
// #define L_FS L_WHITE


void delete_files(std::string_view path, const std::string& include_pattern, const std::string& exclude_pattern) {
	L_CALL("delete_files(%s, %s)", repr(path), repr(include_pattern), repr(exclude_pattern));

	stringified path_string(path);

	DIR *dirp = ::opendir(path_string.c_str());
	if (dirp == nullptr) {
		return;
	}

	bool empty = true;
	struct dirent *ent;
	while ((ent = ::readdir(dirp)) != nullptr) {
		const char *n = ent->d_name;
		L_FS("delete_files -> Entry %s (0x%x)", ent->d_name, static_cast<int>(ent->d_type));
		switch (ent->d_type) {
			case DT_DIR:  // This is a directory.
				if (n[0] == '.' && (n[1] == '\0' || (n[1] == '.' && n[2] == '\0'))) {
					continue;
				}
				empty = false;
				break;
			case DT_REG:  //  This is a regular file.
				if (!exclude_pattern.empty() && ::fnmatch(exclude_pattern.c_str(), n, 0) == 0) {
					L_FS("File %s was excluded by %s", n, repr(exclude_pattern));
				} else if (::fnmatch(include_pattern.c_str(), n, 0) == 0) {
					std::string file(path);
					file.push_back('/');
					file.append(n);
					if (::remove(file.c_str()) != 0) {
						L_ERR("File %s could not be deleted", n);
					} else {
						L_FS("File %s deleted", n);
					}
				} else {
					L_FS("File %s did not match pattern %s", n, repr(include_pattern));
				}
				/* FALLTHROUGH */
			case DT_LNK:  // This is a symbolic link.
			default:
				empty = false;
		}
	}

	::closedir(dirp);

	if (empty) {
		if (::rmdir(path_string.c_str()) != 0) {
			L_ERR("Directory %s could not be deleted", path_string);
		}
	}
}


void move_files(std::string_view src, std::string_view dst) {
	L_CALL("move_files(%s, %s)", repr(src), repr(dst));

	stringified src_string(src);
	DIR *dirp = ::opendir(src_string.c_str());
	if (dirp == nullptr) {
		return;
	}

	struct dirent *ent;
	while ((ent = ::readdir(dirp)) != nullptr) {
		if (ent->d_type == DT_REG) {
			std::string old_name(src);
			old_name.push_back('/');
			old_name.append(ent->d_name);
			std::string new_name(dst);
			new_name.push_back('/');
			new_name.append(ent->d_name);
			if (::rename(old_name.c_str(), new_name.c_str()) != 0) {
				L_ERR("Couldn't rename %s to %s", old_name, new_name);
			}
		}
	}

	::closedir(dirp);
	if (::rmdir(src_string.c_str()) != 0) {
		L_ERR("Directory %s could not be deleted", src_string);
	}
}


bool exists(std::string_view path) {
	L_CALL("exists(%s)", repr(path));

	struct stat buf;
	return ::stat(stringified(path).c_str(), &buf) == 0;
}


bool build_path(std::string_view path) {
	L_CALL("build_path(%s)", repr(path));

	if (exists(path)) {
		return true;
	}
	Split<char> directories(path, '/');
	std::string dir;
	dir.reserve(path.size());
	if (path.front() == '/') {
		dir.push_back('/');
	}
	for (const auto& _dir : directories) {
		dir.append(_dir).push_back('/');
		if (::mkdir(dir.c_str(),  S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1 && errno != EEXIST) {
			return false;
		}
	}
	return true;
}


bool build_path_index(std::string_view path_index) {
	L_CALL("build_path_index(%s)", repr(path_index));

	size_t found = path_index.find_last_of('/');
	if (found == std::string_view::npos) {
		return build_path(path_index);
	}
	return build_path(path_index.substr(0, found));
}


DIR* opendir(std::string_view path, bool create) {
	L_CALL("opendir(%s, %s)", repr(path), create ? "true" : "false");

	stringified path_string(path);
	DIR* dirp = ::opendir(path_string.c_str());
	if (dirp == nullptr && errno == ENOENT && create) {
		if (::mkdir(path_string.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == 0) {
			dirp = ::opendir(path_string.c_str());
		}
	}
	return dirp;
}


void find_file_dir(DIR* dir, File_ptr& fptr, std::string_view pattern, bool pre_suf_fix) {
	bool(*match_pattern)(std::string_view, std::string_view);
	if (pre_suf_fix) {
		match_pattern = string::startswith;
	} else {
		match_pattern = string::endswith;
	}

	if (fptr.ent != nullptr) {
#if defined(__APPLE__) && defined(__MACH__)
		seekdir(dir, fptr.ent->d_seekoff);
#elif defined(__FreeBSD__)
		seekdir(dir, telldir(dir));
#else
		seekdir(dir, fptr.ent->d_off);
#endif
	}

	while ((fptr.ent = ::readdir(dir)) != nullptr) {
		if (fptr.ent->d_type == DT_REG) {
			std::string_view filename(fptr.ent->d_name);
			if (match_pattern(filename, pattern)) {
				return;
			}
		}
	}
}


int copy_file(std::string_view src, std::string_view dst, bool create, std::string_view file_name, std::string_view new_name) {
	stringified src_string(src);
	DIR* dir_src = ::opendir(src_string.c_str());
	if (dir_src == nullptr) {
		L_ERR("ERROR: couldn't open directory %s: %s", strerror(errno));
		return -1;
	}

	struct stat buf;
	stringified dst_string(dst);
	int err = ::stat(dst_string.c_str(), &buf);

	if (err == -1) {
		if (ENOENT == errno && create) {
			if (::mkdir(dst_string.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
				L_ERR("ERROR: couldn't create directory %s: %s", dst_string, strerror(errno));
				return -1;
			}
		} else {
			L_ERR("ERROR: couldn't obtain directory information %s: %s", dst_string, strerror(errno));
			return -1;
		}
	}

	bool ended = false;
	struct dirent *ent;
	unsigned char buffer[4096];

	while ((ent = ::readdir(dir_src)) != nullptr and not ended) {
		if (ent->d_type == DT_REG) {

			if (not file_name.empty()) {
				if (file_name == ent->d_name) {
					ended = true;
				} else {
					continue;
				}
			}

			std::string src_path(src);
			src_path.push_back('/');
			src_path.append(ent->d_name);
			std::string dst_path(dst);
			dst_path.push_back('/');
			if (new_name.empty()) {
				dst_path.append(ent->d_name);
			} else {
				dst_path.append(new_name.data(), new_name.size());
			}

			int src_fd = io::open(src_path.c_str(), O_RDONLY);
			if (src_fd == -1) {
				L_ERR("ERROR: opening file. %s\n", src_path);
				return -1;
			}

			int dst_fd = io::open(dst_path.c_str(), O_CREAT | O_WRONLY, 0644);
			if (src_fd == -1) {
				L_ERR("ERROR: opening file. %s\n", dst_path);
				return -1;
			}

			while (true) {
				ssize_t bytes = io::read(src_fd, buffer, 4096);
				if (bytes == -1) {
					L_ERR("ERROR: reading file. %s: %s\n", src_path, strerror(errno));
					return -1;
				}

				if (bytes == 0) { break; }

				bytes = io::write(dst_fd, buffer, bytes);
				if (bytes == -1) {
					L_ERR("ERROR: writing file. %s: %s\n", dst_path, strerror(errno));
					return -1;
				}
			}
			io::close(src_fd);
			io::close(dst_fd);
		}
	}
	::closedir(dir_src);
	return 0;
}


char* normalize_path(const char* src, const char* end, char* dst, bool slashed) {
	int levels = 0;
	char* ret = dst;
	char ch = '\0';
	while (*src != '\0' && src < end) {
		ch = *src++;
		if (ch == '.' && (levels != 0 || dst == ret || *(dst - 1) == '/' )) {
			*dst++ = ch;
			++levels;
		} else if (ch == '/') {
			while (levels != 0 && dst > ret) {
				if (*--dst == '/') {
					levels -= 1;
				}
			}
			if (dst == ret || *(dst - 1) != '/') {
				*dst++ = ch;
			}
		} else {
			*dst++ = ch;
			levels = 0;
		}
	}
	if (slashed && ch != '/') {
		*dst++ = '/';
	}
	*dst++ = '\0';
	return ret;
}


char* normalize_path(std::string_view src, char* dst, bool slashed) {
	size_t src_size = src.size();
	const char* src_str = src.data();
	return normalize_path(src_str, src_str + src_size, dst, slashed);
}


std::string normalize_path(std::string_view src, bool slashed) {
	size_t src_size = src.size();
	const char* src_str = src.data();
	std::vector<char> dst;
	dst.resize(src_size + 2);
	return normalize_path(src_str, src_str + src_size, &dst[0], slashed);
}
