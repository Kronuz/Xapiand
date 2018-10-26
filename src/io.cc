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

#include "io.hh"

#include <cstring>               // for strerror, size_t

#include "cassert.hh"            // for assert

#include "likely.h"              // for likely, unlikely
#include "log.h"                 // for L_CALL, L_ERRNO


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wgnu-statement-expression"

namespace io {

std::atomic_bool& ignore_eintr() {
	static std::atomic_bool ignore_eintr = true;
	return ignore_eintr;
}


// From /usr/include/sys/errno.h:
const char* sys_errnolist[] {
	"",
	"EPERM",            /* 1:   Operation not permitted */
	"ENOENT",           /* 2:   No such file or directory */
	"ESRCH",            /* 3:   No such process */
	"EINTR",            /* 4:   Interrupted system call */
	"EIO",              /* 5:   Input/output error */
	"ENXIO",            /* 6:   Device not configured */
	"E2BIG",            /* 7:   Argument list too long */
	"ENOEXEC",          /* 8:   Exec format error */
	"EBADF",            /* 9:   Bad file descriptor */
	"ECHILD",           /* 10:  No child processes */
	"EDEADLK",          /* 11:  Resource deadlock avoided */
	"ENOMEM",           /* 12:  Cannot allocate memory */
	"EACCES",           /* 13:  Permission denied */
	"EFAULT",           /* 14:  Bad address */
	"ENOTBLK",          /* 15:  Block device required */
	"EBUSY",            /* 16:  Device / Resource busy */
	"EEXIST",           /* 17:  File exists */
	"EXDEV",            /* 18:  Cross-device link */
	"ENODEV",           /* 19:  Operation not supported by device */
	"ENOTDIR",          /* 20:  Not a directory */
	"EISDIR",           /* 21:  Is a directory */
	"EINVAL",           /* 22:  Invalid argument */
	"ENFILE",           /* 23:  Too many open files in system */
	"EMFILE",           /* 24:  Too many open files */
	"ENOTTY",           /* 25:  Inappropriate ioctl for device */
	"ETXTBSY",          /* 26:  Text file busy */
	"EFBIG",            /* 27:  File too large */
	"ENOSPC",           /* 28:  No space left on device */
	"ESPIPE",           /* 29:  Illegal seek */
	"EROFS",            /* 30:  Read-only file system */
	"EMLINK",           /* 31:  Too many links */
	"EPIPE",            /* 32:  Broken pipe */
	"EDOM",             /* 33:  Numerical argument out of domain */
	"ERANGE",           /* 34:  Result too large */
	"EAGAIN",           /* 35:  Resource temporarily unavailable */
	"EINPROGRESS",      /* 36:  Operation now in progress */
	"EALREADY",         /* 37:  Operation already in progress */
	"ENOTSOCK",         /* 38:  Socket operation on non-socket */
	"EDESTADDRREQ",     /* 39:  Destination address required */
	"EMSGSIZE",         /* 40:  Message too long */
	"EPROTOTYPE",       /* 41:  Protocol wrong type for socket */
	"ENOPROTOOPT",      /* 42:  Protocol not available */
	"EPROTONOSUPPORT",  /* 43:  Protocol not supported */
	"ESOCKTNOSUPPORT",  /* 44:  Socket type not supported */
	"ENOTSUP",          /* 45:  Operation not supported */
	"EPFNOSUPPORT",     /* 46:  Protocol family not supported */
	"EAFNOSUPPORT",     /* 47:  Address family not supported by protocol family */
	"EADDRINUSE",       /* 48:  Address already in use */
	"EADDRNOTAVAIL",    /* 49:  Can't assign requested address */
	"ENETDOWN",         /* 50:  Network is down */
	"ENETUNREACH",      /* 51:  Network is unreachable */
	"ENETRESET",        /* 52:  Network dropped connection on reset */
	"ECONNABORTED",     /* 53:  Software caused connection abort */
	"ECONNRESET",       /* 54:  Connection reset by peer */
	"ENOBUFS",          /* 55:  No buffer space available */
	"EISCONN",          /* 56:  Socket is already connected */
	"ENOTCONN",         /* 57:  Socket is not connected */
	"ESHUTDOWN",        /* 58:  Can't send after socket shutdown */
	"ETOOMANYREFS",     /* 59:  Too many references: can't splice */
	"ETIMEDOUT",        /* 60:  Operation timed out */
	"ECONNREFUSED",     /* 61:  Connection refused */
	"ELOOP",            /* 62:  Too many levels of symbolic links */
	"ENAMETOOLONG",     /* 63:  File name too long */
	"EHOSTDOWN",        /* 64:  Host is down */
	"EHOSTUNREACH",     /* 65:  No route to host */
	"ENOTEMPTY",        /* 66:  Directory not empty */
	"EPROCLIM",         /* 67:  Too many processes */
	"EUSERS",           /* 68:  Too many users */
	"EDQUOT",           /* 69:  Disc quota exceeded */
	"ESTALE",           /* 70:  Stale NFS file handle */
	"EREMOTE",          /* 71:  Too many levels of remote in path */
	"EBADRPC",          /* 72:  RPC struct is bad */
	"ERPCMISMATCH",     /* 73:  RPC version wrong */
	"EPROGUNAVAIL",     /* 74:  RPC prog. not avail */
	"EPROGMISMATCH",    /* 75:  Program version wrong */
	"EPROCUNAVAIL",     /* 76:  Bad procedure for program */
	"ENOLCK",           /* 77:  No locks available */
	"ENOSYS",           /* 78:  Function not implemented */
	"EFTYPE",           /* 79:  Inappropriate file type or format */
	"EAUTH",            /* 80:  Authentication error */
	"ENEEDAUTH",        /* 81:  Need authenticator */
	"EPWROFF",          /* 82:  Device power is off */
	"EDEVERR",          /* 83:  Device error, e.g. paper out */
	"EOVERFLOW",        /* 84:  Value too large to be stored in data type */
	"EBADEXEC",         /* 85:  Bad executable */
	"EBADARCH",         /* 86:  Bad CPU type in executable */
	"ESHLIBVERS",       /* 87:  Shared library version mismatch */
	"EBADMACHO",        /* 88:  Malformed Macho file */
	"ECANCELED",        /* 89:  Operation canceled */
	"EIDRM",            /* 90:  Identifier removed */
	"ENOMSG",           /* 91:  No message of desired type */
	"EILSEQ",           /* 92:  Illegal byte sequence */
	"ENOATTR",          /* 93:  Attribute not found */
	"EBADMSG",          /* 94:  Bad message */
	"EMULTIHOP",        /* 95:  Reserved */
	"ENODATA",          /* 96:  No message available on STREAM */
	"ENOLINK",          /* 97:  Reserved */
	"ENOSR",            /* 98:  No STREAM resources */
	"ENOSTR",           /* 99:  Not a STREAM */
	"EPROTO",           /* 100: Protocol error */
	"ETIME",            /* 101: STREAM ioctl timeout */
	"EOPNOTSUPP",       /* 102: Operation not supported on socket */
	"ENOPOLICY",        /* 103: No such policy registered */
	"ENOTRECOVERABLE",  /* 104: State not recoverable */
	"EOWNERDEAD",       /* 105: Previous owner died */
	"EQFULL",           /* 106: Interface output queue is full */
};


const char* strerrno(int errnum) {
	return sys_errnolist[errnum];
}


int open(const char* path, int oflag, int mode) {
	int fd;
	while (true) {
#ifdef O_CLOEXEC
		fd = ::open(path, oflag | O_CLOEXEC, mode);
#else
		fd = ::open(path, oflag, mode);
#endif
		if unlikely(fd == -1) {
			if unlikely(errno == EINTR && ignore_eintr().load()) {
				continue;
			}
			break;
		}
		if (fd >= XAPIAND_MINIMUM_FILE_DESCRIPTOR) {
			break;
		}
		::close(fd);
		fd = -1;
		if unlikely(RetryAfterSignal(::open, "/dev/null", oflag, mode) == -1) {
			break;
		}
	}
	if (fd != -1) {
		if (mode != 0) {
			struct stat statbuf;
			if (::fstat(fd, &statbuf) == 0 && statbuf.st_size == 0 && (statbuf.st_mode & 0777) != mode) {
				RetryAfterSignal(::fchmod, fd, mode);
			}
		}
#if defined(FD_CLOEXEC) && (!defined(O_CLOEXEC) || O_CLOEXEC == 0)
		RetryAfterSignal(::fcntl, fd, F_SETFD, RetryAfterSignal(::fcntl, fd, F_GETFD, 0) | FD_CLOEXEC);
#endif
	}
	CHECK_OPEN(fd);
	return fd;
}


int close(int fd) {
	// Make sure we don't ever close 0, 1 or 2 file descriptors
	assert(fd != -1 && fd >= XAPIAND_MINIMUM_FILE_DESCRIPTOR);
	if unlikely(fd == -1 || fd >= XAPIAND_MINIMUM_FILE_DESCRIPTOR) {
		CHECK_CLOSING(fd);
		return ::close(fd);  // IMPORTANT: don't check EINTR (do not use RetryAfterSignal here)
	}
	errno = EBADF;
	return -1;
}


ssize_t write(int fd, const void* buf, size_t nbyte) {
	L_CALL("io::write(%d, <buf>, %lu)", fd, nbyte);
	CHECK_OPENED("during write()", fd);

	const auto* p = static_cast<const char*>(buf);
	while (nbyte != 0u) {
		ssize_t c = ::write(fd, p, nbyte);
		if unlikely(c == -1) {
			L_ERRNO("io::write() -> %s (%d): %s [%llu]", strerrno(errno), errno, strerror(errno), p - static_cast<const char*>(buf));
			if unlikely(errno == EINTR && ignore_eintr().load()) {
				continue;
			}
			size_t written = p - static_cast<const char*>(buf);
			if (written == 0) {
				return -1;
			}
			return written;
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
	L_CALL("io::pwrite(%d, <buf>, %lu, %lu)", fd, nbyte, offset);
	CHECK_OPENED("during pwrite()", fd);

	const auto* p = static_cast<const char*>(buf);
#ifndef HAVE_PWRITE
	if unlikely(::lseek(fd, offset, SEEK_SET) == -1) {
		return -1;
	}
#endif
	while (nbyte != 0u) {
#ifndef HAVE_PWRITE
		ssize_t c = ::write(fd, p, nbyte);
#else
		ssize_t c = ::pwrite(fd, p, nbyte, offset);
#endif
		if unlikely(c == -1) {
			L_ERRNO("io::pwrite() -> %s (%d): %s [%llu]", strerrno(errno), errno, strerror(errno), p - static_cast<const char*>(buf));
			if unlikely(errno == EINTR && ignore_eintr().load()) {
				continue;
			}
			size_t written = p - static_cast<const char*>(buf);
			if (written == 0) {
				return -1;
			}
			return written;
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
	L_CALL("io::read(%d, <buf>, %lu)", fd, nbyte);
	CHECK_OPENED("during read()", fd);

	auto* p = static_cast<char*>(buf);
	while (nbyte != 0u) {
		ssize_t c = ::read(fd, p, nbyte);
		if unlikely(c == -1) {
			L_ERRNO("io::read() -> %s (%d): %s [%llu]", strerrno(errno), errno, strerror(errno), p - static_cast<const char*>(buf));
			if unlikely(errno == EINTR && ignore_eintr().load()) {
				continue;
			}
			size_t read = p - static_cast<const char*>(buf);
			if (read == 0) {
				return -1;
			}
			return read;
		} else if unlikely(c == 0) {
			break;
		}
		p += c;
		if likely(c == static_cast<ssize_t>(nbyte)) {
			break;
		}
		nbyte -= c;
	}
	return p - static_cast<const char*>(buf);
}


ssize_t pread(int fd, void* buf, size_t nbyte, off_t offset) {
	L_CALL("io::pread(%d, <buf>, %lu, %lu)", fd, nbyte, offset);
	CHECK_OPENED("during pread()", fd);

#ifndef HAVE_PWRITE
	if unlikely(::lseek(fd, offset, SEEK_SET) == -1) {
		return -1;
	}
#endif
	auto* p = static_cast<char*>(buf);
	while (nbyte != 0u) {
#ifndef HAVE_PWRITE
		ssize_t c = ::read(fd, p, nbyte);
#else
		ssize_t c = ::pread(fd, p, nbyte, offset);
#endif
		if unlikely(c == -1) {
			L_ERRNO("io::pread() -> %s (%d): %s [%llu]", strerrno(errno), errno, strerror(errno), p - static_cast<const char*>(buf));
			if unlikely(errno == EINTR && ignore_eintr().load()) {
				continue;
			}
			size_t read = p - static_cast<const char*>(buf);
			if (read == 0) {
				return -1;
			}
			return read;
		}
		p += c;
		break; // read() doesn't have to read the whole nbytes
		// if likely(c == static_cast<ssize_t>(nbyte)) {
		// 	break;
		// }
		// nbyte -= c;
		// offset += c;
	}
	return p - static_cast<const char*>(buf);
}


#ifndef HAVE_FALLOCATE
int fallocate(int fd, int /* mode */, off_t offset, off_t len) {
	CHECK_OPENED("during fallocate()", fd);
#if defined(HAVE_POSIX_FALLOCATE)
	return posix_fallocate(fd, offset, len);
#elif defined(F_PREALLOCATE)
	// Try to get a continous chunk of disk space
	// {fst_flags, fst_posmode, fst_offset, fst_length, fst_bytesalloc}
	fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, offset + len, 0};
	int err = RetryAfterSignal(::fcntl, fd, F_PREALLOCATE, &store);
	if unlikely(err == -1) {
		// Try and allocate space with fragments
		store.fst_flags = F_ALLOCATEALL;
		err = RetryAfterSignal(::fcntl, fd, F_PREALLOCATE, &store);
	}
	if likely(err != -1) {
		RetryAfterSignal(::ftruncate, fd, offset + len);
	}
	return err;
#else
	// The following is copied from fcntlSizeHint in sqlite
	/* If the OS does not have posix_fallocate(), fake it. First use
	 ** ftruncate() to set the file size, then write a single byte to
	 ** the last byte in each block within the extended region. This
	 ** is the same technique used by glibc to implement posix_fallocate()
	 ** on systems that do not have a real fallocate() system call.
	 */

	struct stat buf;
	if (::fstat(fd, &buf)) {
		return -1;
	}

	if (buf.st_size >= offset + len) {
		return -1;
	}

	const int st_blksize = buf.st_blksize;
	if (!st_blksize) {
		return -1;
	}

	if (RetryAfterSignal(::ftruncate, fd, offset + len)) {
		return -1;
	}

	off_t next_offset = ((buf.st_size + 2 * st_blksize - 1) / st_blksize) * st_blksize - 1;  // Next offset to write to
	int written;
	do {
		written = 0;
		if (::lseek(fd, next_offset, SEEK_SET) == next_offset) {
			written = RetryAfterSignal(::write, fd, "", 1);
		}
		next_offset += st_blksize;
	} while (written == 1 && next_offset < offset + len);
	return 0;
#endif
}
#endif

#ifdef XAPIAND_CHECK_IO_FDES
#include <mutex>
#include <bitset>
int check(const char* msg, int fd, int check_set, int check_unset, int set, const char* function, const char* filename, int line) {
	static std::mutex mtx;
	static std::bitset<1024*1024> socket;
	static std::bitset<1024*1024> opened;
	static std::bitset<1024*1024> closed;

	if unlikely(fd == -1) {
		return -1;
	}

	if unlikely(fd >= 1024*1024) {
		L_ERR("fd (%d) is too big to track %s", fd, msg);
		return -1;
	}

	std::lock_guard<std::mutex> lk(mtx);

	int currently = (
		(socket.test(fd) ? SOCKET : 0) |
		(opened.test(fd) ? OPENED : 0) |
		(closed.test(fd) ? CLOSED : 0)
	);

	if (currently & SOCKET) {
		if (check_unset & SOCKET) {
			L_ERR("fd (%d) is a socket %s" + DEBUG_COL + "%s", fd, msg, ::traceback(function, filename, line, 8));
		}
	} else {
		if (check_set & SOCKET) {
			L_ERR("fd (%d) is not a socket %s" + DEBUG_COL + "%s", fd, msg, ::traceback(function, filename, line, 8));
		}
	}
	if (currently & OPENED) {
		if (check_unset & OPENED) {
			L_ERR("fd (%d) is opened %s" + DEBUG_COL + "%s", fd, msg, ::traceback(function, filename, line, 8));
		}
	} else {
		if (check_set & OPENED) {
			L_ERR("fd (%d) is not opened %s" + DEBUG_COL + "%s", fd, msg, ::traceback(function, filename, line, 8));
		}
	}
	if (currently & CLOSED) {
		if (check_unset & CLOSED) {
			L_ERR("fd (%d) is closed %s" + DEBUG_COL + "%s", fd, msg, ::traceback(function, filename, line, 8));
		}
	} else {
		if (check_set & CLOSED) {
			L_ERR("fd (%d) is not closed %s" + DEBUG_COL + "%s", fd, msg, ::traceback(function, filename, line, 8));
		}
	}

	if (set & SOCKET) {
		socket.set(fd);
	}
	if (set & OPENED) {
		opened.set(fd);
	}
	if (set & CLOSED) {
		closed.set(fd);
	}

	return currently;
}
#endif

} /* namespace io */

#ifdef XAPIAND_CHECK_IO_FDES
#include <sysexits.h>                       // for EX_SOFTWARE
int close(int fd) {
	static int honeypot = io::RetryAfterSignal(::open, "/tmp/xapiand.honeypot", O_RDWR | O_CREAT | O_TRUNC, 0644);
	if unlikely(honeypot == -1) {
		L_ERR("honeypot -> %s", io::strerrno(errno));
		exit(EX_SOFTWARE);
	}
	CHECK_CLOSE(fd);
	return io::RetryAfterSignal(::dup2, honeypot, fd);
}
#endif

#pragma GCC diagnostic pop
