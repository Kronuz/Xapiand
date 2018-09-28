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

#include "io_utils.h"

#include <cstring>     // for strerror, size_t
#include <errno.h>      // for __error, errno, EINTR

#include "config.h"     // for HAVE_PWRITE, HAVE_FSYNC
#include "log.h"        // for L_CALL, L_ERRNO

namespace io {

#if defined HAVE_FDATASYNC
#define __FSYNC ::fdatasync
#elif defined HAVE_FSYNC
#define __FSYNC ::fsync
#else
inline int __fsync(int /*unused*/) { return 0; }
#define __FSYNC __fsync
#endif


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


ssize_t write(int fd, const void* buf, size_t nbyte) {
	L_CALL("io::write(%d, <buf>, %lu)", fd, nbyte);

	const auto* p = static_cast<const char*>(buf);
	while (nbyte != 0u) {
		ssize_t c = ::write(fd, p, nbyte);
		if unlikely(c < 0) {
			L_ERRNO("io::write() -> %s (%d): %s [%llu]", strerrno(errno), errno, strerror(errno), p - static_cast<const char*>(buf));
			if (errno == EINTR) { continue; }
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
		if unlikely(c < 0) {
			L_ERRNO("io::pwrite() -> %s (%d): %s [%llu]", strerrno(errno), errno, strerror(errno), p - static_cast<const char*>(buf));
			if (errno == EINTR) { continue; }
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

	auto* p = static_cast<char*>(buf);
	while (nbyte != 0u) {
		ssize_t c = ::read(fd, p, nbyte);
		if unlikely(c < 0) {
			L_ERRNO("io::read() -> %s (%d): %s [%llu]", strerrno(errno), errno, strerror(errno), p - static_cast<const char*>(buf));
			if (errno == EINTR) { continue; }
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
		if unlikely(c < 0) {
			L_ERRNO("io::pread() -> %s (%d): %s [%llu]", strerrno(errno), errno, strerror(errno), p - static_cast<const char*>(buf));
			if (errno == EINTR) { continue; }
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


int fsync(int fd) {
	L_CALL("io::fsync(%d)", fd);

	while (true) {
		int r = __FSYNC(fd);
		if unlikely(r < 0) {
			L_ERRNO("io::fsync() -> %s (%d): %s", strerrno(errno), errno, strerror(errno));
			if (errno == EINTR) { continue; }
			return -1;
		}
		return r;
	}
}


int full_fsync(int fd) {
#ifdef F_FULLFSYNC
	L_CALL("io::full_fsync(%d)", fd);

	while (true) {
		int r = fcntl(fd, F_FULLFSYNC, 0);
		if unlikely(r < 0) {
			L_ERRNO("io::full_fsync() -> %s (%d): %s", strerrno(errno), errno, strerror(errno));
			if (errno == EINTR) { continue; }
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
	// {fst_flags, fst_posmode, fst_offset, fst_length, fst_bytesalloc}
	auto eof = ::lseek(fd, 0, SEEK_END);
	if (eof == -1) {
		return -1;
	}
	fstore_t store = {F_ALLOCATECONTIG, F_PEOFPOSMODE, 0, offset + len - eof, 0};
	int res = fcntl(fd, F_PREALLOCATE, &store);
	if (res == -1) {
		// Try and allocate space with fragments
		store.fst_flags = F_ALLOCATEALL;
		res = fcntl(fd, F_PREALLOCATE, &store);
	}
	if (res != -1) {
		ftruncate(fd, offset + len);
	}
	return res;
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
		if (::lseek(fd, iWrite, SEEK_SET) == iWrite) {
			nWrite = ::write(fd, "", 1);
		}
		iWrite += nBlk;
	} while (nWrite == 1 && iWrite < len);
	return 0;
#endif
}
#endif


} /* namespace io */
