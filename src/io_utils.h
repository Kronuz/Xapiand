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

#ifndef IO_UTILS_H
#define IO_UTILS_H

#include "xapiand.h"

#include <errno.h>      // for errno
#include <fcntl.h>      // for fchmod, open, O_RDONLY
#include <stddef.h>     // for size_t
#include <sys/stat.h>   // for fstat
#include <sys/socket.h> // for send
#include <unistd.h>     // for off_t, ssize_t, close, lseek, unlink


// Do not accept any file descriptor less than this value, in order to avoid
// opening database file using file descriptors that are commonly used for
// standard input, output, and error.
#ifndef XAPIAND_MINIMUM_FILE_DESCRIPTOR
#define XAPIAND_MINIMUM_FILE_DESCRIPTOR STDERR_FILENO + 1
#endif


namespace io {

inline int unlink(const char* path) {
	return ::unlink(path);
}


inline int open(const char* path, int oflag=O_RDONLY, int mode=0644) {
	int fd;
	while (true) {
#ifdef O_CLOEXEC
		fd = ::open(path, oflag | O_CLOEXEC, mode);
#else
		fd = ::open(path, oflag, mode);
#endif
		if (fd == -1) {
			if (errno == EINTR) {
				continue;
			}
			break;
		}
		if (fd >= XAPIAND_MINIMUM_FILE_DESCRIPTOR) {
			break;
		}
		::close(fd);
		fd = -1;
		if (::open("/dev/null", oflag, mode) == -1) {
			break;
		}
	}
	if (fd != -1) {
		if (mode != 0) {
			struct stat statbuf;
			if (::fstat(fd, &statbuf) == 0 && statbuf.st_size == 0 && (statbuf.st_mode & 0777) != mode) {
				::fchmod(fd, mode);
			}
		}
#if defined(FD_CLOEXEC) && (!defined(O_CLOEXEC) || O_CLOEXEC == 0)
		::fcntl(fd, F_SETFD, ::fcntl(fd, F_GETFD, 0) | FD_CLOEXEC);
#endif
	}
	return fd;
}


inline int close(int fd) {
	// Make sure we don't ever close 0, 1 or 2 file descriptors
	assert(fd != -1 && fd >= XAPIAND_MINIMUM_FILE_DESCRIPTOR);
	if (fd >= XAPIAND_MINIMUM_FILE_DESCRIPTOR) {
		return ::close(fd);
	}
	return -1;
}


inline off_t lseek(int fd, off_t offset, int whence) {
	return ::lseek(fd, offset, whence);
}


inline int fcntl(int fd, int cmd, int arg) {
	return ::fcntl(fd, cmd, arg);
}


inline int fstat(int fd, struct stat* buf) {
	return ::fstat(fd, buf);
}


inline int dup(int fd) {
	return ::dup(fd);
}


inline int dup2(int fd, int fildes2) {
	return ::dup2(fd, fildes2);
}


inline int shutdown(int socket, int how) {
	return ::shutdown(socket, how);
}


inline ssize_t send(int socket, const void* buffer, size_t length, int flags) {
	return ::send(socket, buffer, length, flags);
}


inline ssize_t sendto(int socket, const void* buffer, size_t length, int flags, const struct sockaddr* dest_addr, socklen_t dest_len) {
	return ::sendto(socket, buffer, length, flags, dest_addr, dest_len);
}


inline ssize_t recv(int socket, void* buffer, size_t length, int flags) {
	return ::recv(socket, buffer, length, flags);
}


inline ssize_t recvfrom(int socket, void* buffer, size_t length, int flags, struct sockaddr* address, socklen_t* address_len) {
	return ::recvfrom(socket, buffer, length, flags, address, address_len);
}


inline int getsockopt(int socket, int level, int option_name, void* option_value, socklen_t* option_len){
	return ::getsockopt(socket, level, option_name, option_value, option_len);
}


inline int setsockopt(int socket, int level, int option_name, const void* option_value, socklen_t option_len){
	return ::setsockopt(socket, level, option_name, option_value, option_len);
}


inline int listen(int socket, int backlog) {
	return ::listen(socket, backlog);
}


inline int accept(int socket, struct sockaddr* address, socklen_t* address_len) {
	return ::accept(socket, address, address_len);
}


inline int bind(int socket, const struct sockaddr *address, socklen_t address_len) {
	return ::bind(socket, address, address_len);
}


inline int connect(int socket, const struct sockaddr* address, socklen_t address_len) {
	return ::connect(socket, address, address_len);
}


const char* strerrno(int errnum);

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

#endif // IO_UTILS_H