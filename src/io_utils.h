/*
 * Copyright (C) 2015,2016 deipi.com LLC and contributors. All rights reserved.
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

#include "xapiand.h"

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

namespace io {

inline int unlink(const char *path) {
	return ::unlink(path);
}

inline int close(int fd) {
	return ::close(fd);
}

inline int open(const char *path, int oflag=O_RDONLY, int mode=0644) {
	return ::open(path, oflag, mode);
}

inline off_t lseek(int fd, off_t offset, int whence) {
	return ::lseek(fd, offset, whence);
}

ssize_t write(int fd, const void* buf, size_t nbyte);
ssize_t pwrite(int fd, const void* buf, size_t nbyte, off_t offset);

ssize_t read(int fd, void* buf, size_t nbyte);
ssize_t pread(int fd, void* buf, size_t nbyte, off_t offset);

int fsync(int fd);
int full_fsync(int fd);

#ifdef HAVE_FALLOCATE
inline int fallocate(int fd, int mode, off_t offset, off_t len) {
	return ::fallocate(fd, mode, offset, len);
}
#else
int fallocate(int fd, int mode, off_t offset, off_t len);
#endif


#ifdef HAVE_POSIX_FADVISE
inline int fadvise(int fd, off_t offset, off_t len, int advice) {
	return posix_fadvise(fd, offset, len, advice) == 0;
}
#else
#define POSIX_FADV_NORMAL     0
#define POSIX_FADV_SEQUENTIAL 1
#define POSIX_FADV_RANDOM     2
#define POSIX_FADV_WILLNEED   3
#define POSIX_FADV_DONTNEED   4
#define POSIX_FADV_NOREUSE    5

inline int fadvise(int, off_t, off_t, int) {
	return 0;
}
#endif

} /* namespace io */
