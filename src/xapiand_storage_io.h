/*
 * IO policy for the `storage` library, wired to Xapiand's io.cc.
 *
 * The storage engine routes every file operation through its IO template
 * parameter (default storage::DefaultIO). Xapiand instead runs all storage IO
 * through its own instrumented io.cc (EINTR retry, error injection for tests,
 * the unchecked_* variants the fsyncher uses). This header is the engine's
 * STORAGE_IO_HEADER: it declares XapiandStorageIO (a thin policy delegating to
 * io::) and #defines STORAGE_DEFAULT_IO to it, so every Storage<> instantiation
 * picks it up without passing a 4th template argument at each site.
 *
 * The engine includes this BEFORE its Storage template, so XapiandStorageIO is in
 * scope when the template's default IO parameter is resolved. io.hh also supplies
 * the POSIX_FADV_* fallback tokens the engine's read path uses.
 */

#pragma once

#include <fcntl.h>           // for O_RDONLY (open's default oflag)
#include <sys/types.h>       // for off_t, ssize_t

#include "io.hh"             // for io::open/close/read/write/pread/pwrite/lseek/fsync/full_fsync/fallocate/fadvise


struct XapiandStorageIO {
	static int open(const char* path, int oflag = O_RDONLY, int mode = 0644) noexcept { return io::open(path, oflag, mode); }
	static int close(int fd) noexcept { return io::close(fd); }
	static off_t lseek(int fd, off_t offset, int whence) noexcept { return io::lseek(fd, offset, whence); }
	static ssize_t read(int fd, void* buf, size_t nbyte) noexcept { return io::read(fd, buf, nbyte); }
	static ssize_t write(int fd, const void* buf, size_t nbyte) noexcept { return io::write(fd, buf, nbyte); }
	static ssize_t pread(int fd, void* buf, size_t nbyte, off_t offset) noexcept { return io::pread(fd, buf, nbyte, offset); }
	static ssize_t pwrite(int fd, const void* buf, size_t nbyte, off_t offset) noexcept { return io::pwrite(fd, buf, nbyte, offset); }
	static int fsync(int fd) noexcept { return io::fsync(fd); }
	static int full_fsync(int fd) noexcept { return io::full_fsync(fd); }
	static int fallocate(int fd, int mode, off_t offset, off_t len) noexcept { return io::fallocate(fd, mode, offset, len); }
	static int fadvise(int fd, off_t offset, off_t len, int advice) noexcept { return io::fadvise(fd, offset, len, advice); }
};

#define STORAGE_DEFAULT_IO XapiandStorageIO
