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

#include "io_utils.h"


namespace io {

#if defined HAVE_FDATASYNC
#define __FSYNC ::fdatasync
#elif defined HAVE_FSYNC
#define __FSYNC ::fsync
#else
inline int __fsync(int fd) { return 0; }
#define __FSYNC __fsync
#endif


ssize_t write(int fd, const void* buf, size_t nbyte) {
	const char* p = static_cast<const char*>(buf);
	while (nbyte) {
		ssize_t c = ::write(fd, p, nbyte);
		if unlikely(c < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		p += c;
		if likely(c == static_cast<ssize_t>(nbyte)) {
			break;
		}
		nbyte -= c;
	}
	return p - static_cast<const char*>(buf);
}


ssize_t pwrite(int fd, const void* buf, size_t nbyte, off_t offset) {
	const char* p = static_cast<const char*>(buf);
#ifndef HAVE_PWRITE
	if unlikely(io::lseek(fd, offset, SEEK_SET) == -1) {
		return -1;
	}
#endif
	while (nbyte) {
#ifndef HAVE_PWRITE
		ssize_t c = ::write(fd, p, nbyte);
#else
		ssize_t c = ::pwrite(fd, p, nbyte, offset);
#endif
		if unlikely(c < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		p += c;
		if likely(c == static_cast<ssize_t>(nbyte)) {
			break;
		}
		nbyte -= c;
		offset += c;
	}
	return p - static_cast<const char*>(buf);
}


ssize_t read(int fd, void* buf, size_t nbyte) {
	char* p = static_cast<char*>(buf);
	while (true) {
		ssize_t c = ::read(fd, p, nbyte);
		if unlikely(c < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		return c;
	}
}


ssize_t pread(int fd, void* buf, size_t nbyte, off_t offset) {
	char* p = static_cast<char*>(buf);
#ifndef HAVE_PWRITE
	if unlikely(io::lseek(fd, offset, SEEK_SET) == -1) {
		return -1;
	}
#endif
	while (true) {
#ifndef HAVE_PWRITE
		ssize_t c = ::read(fd, p, nbyte);
#else
		ssize_t c = ::pread(fd, p, nbyte, offset);
#endif
		if unlikely(c < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		return c;
	}
}


int fsync(int fd) {
	while (true) {
		int r = __FSYNC(fd);
		if unlikely(r < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		return r;
	}
}


int full_fsync(int fd) {
#ifdef F_FULLFSYNC
	while (true) {
		int r = fcntl(fd, F_FULLFSYNC, 0);
		if unlikely(r < 0) {
			if (errno == EINTR) continue;
			return -1;
		}
		return r;
	}
#else
	return fsync(fd);
#endif
}


#ifndef HAVE_FALLOCATE
int fallocate(int fd, int /* mode */, off_t offset, off_t len) {
#if defined(HAVE_POSIX_FALLOCATE)
	return posix_fallocate(fd, offset, len);
#elif defined(F_PREALLOCATE)
	// Try to get a continous chunk of disk space
	fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, offset, len, 0};
	int ret = fcntl(fd, F_PREALLOCATE, &store);
	if (ret < 0) {
		// OK, perhaps we are too fragmented, allocate non-continuous
		store.fst_flags = F_ALLOCATEALL;
		ret = fcntl(fd, F_PREALLOCATE, &store);
	}

	ftruncate(fd, len);
	return ret;
#else
	// The following is copied from fcntlSizeHint in sqlite
	/* If the OS does not have posix_fallocate(), fake it. First use
	 ** ftruncate() to set the file size, then write a single byte to
	 ** the last byte in each block within the extended region. This
	 ** is the same technique used by glibc to implement posix_fallocate()
	 ** on systems that do not have a real fallocate() system call.
	 */

	struct stat buf;
	if (fstat(fd, &buf))
		return -1;

	if (buf.st_size >= len)
		return -1;

	const int nBlk = buf.st_blksize;

	if (!nBlk)
		return -1;

	if (ftruncate(fd, len))
		return -1;

	int nWrite;
	off_t iWrite = ((buf.st_size + 2 * nBlk - 1) / nBlk) * nBlk - 1; // Next offset to write to
	do {
		nWrite = 0;
		if (io::lseek(fd, iWrite, SEEK_SET) == iWrite) {
			nWrite = ::write(fd, "", 1);
		}
		iWrite += nBlk;
	} while (nWrite == 1 && iWrite < len);
	return 0;
#endif
}
#endif

} /* namespace io */
