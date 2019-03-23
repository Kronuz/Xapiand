/*
 * Copyright (c) 2015-2018 Dubalu LLC
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

#include <cstring>                  // for strerror_r
#include <string>                   // for std::string
#include <array>


namespace error {

namespace detail {
template<typename T, std::size_t N>
inline constexpr std::size_t size(T (&)[N]) noexcept {
	return N;
}

inline const auto& errnos() {
	static const auto _errnos = []{
		#pragma clang diagnostic push
		#pragma clang diagnostic ignored "-Wc99-extensions"
		#pragma clang diagnostic ignored "-Winitializer-overrides"
		#define __ERRNO_ASSIGN(name) [name] = #name
		constexpr const char* __errnos[] = {
			[0] = "UNDEFINED",
			// Linux only:
			#ifdef EADV
				__ERRNO_ASSIGN(EADV),
			#endif
			#ifdef EBADE
				__ERRNO_ASSIGN(EBADE),
			#endif
			#ifdef EBADFD
				__ERRNO_ASSIGN(EBADFD),
			#endif
			#ifdef EBADR
				__ERRNO_ASSIGN(EBADR),
			#endif
			#ifdef EBADRQC
				__ERRNO_ASSIGN(EBADRQC),
			#endif
			#ifdef EBADSLT
				__ERRNO_ASSIGN(EBADSLT),
			#endif
			#ifdef EBFONT
				__ERRNO_ASSIGN(EBFONT),
			#endif
			#ifdef ECHRNG
				__ERRNO_ASSIGN(ECHRNG),
			#endif
			#ifdef ECOMM
				__ERRNO_ASSIGN(ECOMM),
			#endif
			#ifdef EDEADLOCK
				__ERRNO_ASSIGN(EDEADLOCK),
			#endif
			#ifdef EDOTDOT
				__ERRNO_ASSIGN(EDOTDOT),
			#endif
			#ifdef EISNAM
				__ERRNO_ASSIGN(EISNAM),
			#endif
			#ifdef EKEYEXPIRED
				__ERRNO_ASSIGN(EKEYEXPIRED),
			#endif
			#ifdef EKEYREJECTED
				__ERRNO_ASSIGN(EKEYREJECTED),
			#endif
			#ifdef EKEYREVOKED
				__ERRNO_ASSIGN(EKEYREVOKED),
			#endif
			#ifdef EL2HLT
				__ERRNO_ASSIGN(EL2HLT),
			#endif
			#ifdef EL2NSYNC
				__ERRNO_ASSIGN(EL2NSYNC),
			#endif
			#ifdef EL3HLT
				__ERRNO_ASSIGN(EL3HLT),
			#endif
			#ifdef EL3RST
				__ERRNO_ASSIGN(EL3RST),
			#endif
			#ifdef ELIBACC
				__ERRNO_ASSIGN(ELIBACC),
			#endif
			#ifdef ELIBBAD
				__ERRNO_ASSIGN(ELIBBAD),
			#endif
			#ifdef ELIBEXEC
				__ERRNO_ASSIGN(ELIBEXEC),
			#endif
			#ifdef ELIBMAX
				__ERRNO_ASSIGN(ELIBMAX),
			#endif
			#ifdef ELIBSCN
				__ERRNO_ASSIGN(ELIBSCN),
			#endif
			#ifdef ELNRNG
				__ERRNO_ASSIGN(ELNRNG),
			#endif
			#ifdef EMEDIUMTYPE
				__ERRNO_ASSIGN(EMEDIUMTYPE),
			#endif
			#ifdef ENAVAIL
				__ERRNO_ASSIGN(ENAVAIL),
			#endif
			#ifdef ENOANO
				__ERRNO_ASSIGN(ENOANO),
			#endif
			#ifdef ENOCSI
				__ERRNO_ASSIGN(ENOCSI),
			#endif
			#ifdef ENOKEY
				__ERRNO_ASSIGN(ENOKEY),
			#endif
			#ifdef ENOMEDIUM
				__ERRNO_ASSIGN(ENOMEDIUM),
			#endif
			#ifdef ENONET
				__ERRNO_ASSIGN(ENONET),
			#endif
			#ifdef ENOPKG
				__ERRNO_ASSIGN(ENOPKG),
			#endif
			#ifdef ENOTNAM
				__ERRNO_ASSIGN(ENOTNAM),
			#endif
			#ifdef ENOTUNIQ
				__ERRNO_ASSIGN(ENOTUNIQ),
			#endif
			#ifdef EREMCHG
				__ERRNO_ASSIGN(EREMCHG),
			#endif
			#ifdef EREMOTEIO
				__ERRNO_ASSIGN(EREMOTEIO),
			#endif
			#ifdef ERESTART
				__ERRNO_ASSIGN(ERESTART),
			#endif
			#ifdef ESRMNT
				__ERRNO_ASSIGN(ESRMNT),
			#endif
			#ifdef ESTRPIPE
				__ERRNO_ASSIGN(ESTRPIPE),
			#endif
			#ifdef EUCLEAN
				__ERRNO_ASSIGN(EUCLEAN),
			#endif
			#ifdef EUNATCH
				__ERRNO_ASSIGN(EUNATCH),
			#endif
			#ifdef EWOULDBLOCK
				__ERRNO_ASSIGN(EWOULDBLOCK),
			#endif
			#ifdef EXFULL
				__ERRNO_ASSIGN(EXFULL),
			#endif

			// macOS only:
			#ifdef EQFULL
				__ERRNO_ASSIGN(EQFULL),
			#endif

			// both:
			#ifdef E2BIG
				__ERRNO_ASSIGN(E2BIG),
			#endif
			#ifdef EACCES
				__ERRNO_ASSIGN(EACCES),
			#endif
			#ifdef EADDRINUSE
				__ERRNO_ASSIGN(EADDRINUSE),
			#endif
			#ifdef EADDRNOTAVAIL
				__ERRNO_ASSIGN(EADDRNOTAVAIL),
			#endif
			#ifdef EAFNOSUPPORT
				__ERRNO_ASSIGN(EAFNOSUPPORT),
			#endif
			#ifdef EAGAIN
				__ERRNO_ASSIGN(EAGAIN),
			#endif
			#ifdef EALREADY
				__ERRNO_ASSIGN(EALREADY),
			#endif
			#ifdef EAUTH
				__ERRNO_ASSIGN(EAUTH),
			#endif
			#ifdef EBADARCH
				__ERRNO_ASSIGN(EBADARCH),
			#endif
			#ifdef EBADEXEC
				__ERRNO_ASSIGN(EBADEXEC),
			#endif
			#ifdef EBADF
				__ERRNO_ASSIGN(EBADF),
			#endif
			#ifdef EBADMACHO
				__ERRNO_ASSIGN(EBADMACHO),
			#endif
			#ifdef EBADMSG
				__ERRNO_ASSIGN(EBADMSG),
			#endif
			#ifdef EBADRPC
				__ERRNO_ASSIGN(EBADRPC),
			#endif
			#ifdef EBUSY
				__ERRNO_ASSIGN(EBUSY),
			#endif
			#ifdef ECANCELED
				__ERRNO_ASSIGN(ECANCELED),
			#endif
			#ifdef ECHILD
				__ERRNO_ASSIGN(ECHILD),
			#endif
			#ifdef ECONNABORTED
				__ERRNO_ASSIGN(ECONNABORTED),
			#endif
			#ifdef ECONNREFUSED
				__ERRNO_ASSIGN(ECONNREFUSED),
			#endif
			#ifdef ECONNRESET
				__ERRNO_ASSIGN(ECONNRESET),
			#endif
			#ifdef EDEADLK
				__ERRNO_ASSIGN(EDEADLK),
			#endif
			#ifdef EDESTADDRREQ
				__ERRNO_ASSIGN(EDESTADDRREQ),
			#endif
			#ifdef EDEVERR
				__ERRNO_ASSIGN(EDEVERR),
			#endif
			#ifdef EDOM
				__ERRNO_ASSIGN(EDOM),
			#endif
			#ifdef EDQUOT
				__ERRNO_ASSIGN(EDQUOT),
			#endif
			#ifdef EEXIST
				__ERRNO_ASSIGN(EEXIST),
			#endif
			#ifdef EFAULT
				__ERRNO_ASSIGN(EFAULT),
			#endif
			#ifdef EFBIG
				__ERRNO_ASSIGN(EFBIG),
			#endif
			#ifdef EFTYPE
				__ERRNO_ASSIGN(EFTYPE),
			#endif
			#ifdef EHOSTDOWN
				__ERRNO_ASSIGN(EHOSTDOWN),
			#endif
			#ifdef EHOSTUNREACH
				__ERRNO_ASSIGN(EHOSTUNREACH),
			#endif
			#ifdef EIDRM
				__ERRNO_ASSIGN(EIDRM),
			#endif
			#ifdef EILSEQ
				__ERRNO_ASSIGN(EILSEQ),
			#endif
			#ifdef EINPROGRESS
				__ERRNO_ASSIGN(EINPROGRESS),
			#endif
			#ifdef EINTR
				__ERRNO_ASSIGN(EINTR),
			#endif
			#ifdef EINVAL
				__ERRNO_ASSIGN(EINVAL),
			#endif
			#ifdef EIO
				__ERRNO_ASSIGN(EIO),
			#endif
			#ifdef EISCONN
				__ERRNO_ASSIGN(EISCONN),
			#endif
			#ifdef EISDIR
				__ERRNO_ASSIGN(EISDIR),
			#endif
			#ifdef ELOOP
				__ERRNO_ASSIGN(ELOOP),
			#endif
			#ifdef EMFILE
				__ERRNO_ASSIGN(EMFILE),
			#endif
			#ifdef EMLINK
				__ERRNO_ASSIGN(EMLINK),
			#endif
			#ifdef EMSGSIZE
				__ERRNO_ASSIGN(EMSGSIZE),
			#endif
			#ifdef EMULTIHOP
				__ERRNO_ASSIGN(EMULTIHOP),
			#endif
			#ifdef ENAMETOOLONG
				__ERRNO_ASSIGN(ENAMETOOLONG),
			#endif
			#ifdef ENEEDAUTH
				__ERRNO_ASSIGN(ENEEDAUTH),
			#endif
			#ifdef ENETDOWN
				__ERRNO_ASSIGN(ENETDOWN),
			#endif
			#ifdef ENETRESET
				__ERRNO_ASSIGN(ENETRESET),
			#endif
			#ifdef ENETUNREACH
				__ERRNO_ASSIGN(ENETUNREACH),
			#endif
			#ifdef ENFILE
				__ERRNO_ASSIGN(ENFILE),
			#endif
			#ifdef ENOATTR
				__ERRNO_ASSIGN(ENOATTR),
			#endif
			#ifdef ENOBUFS
				__ERRNO_ASSIGN(ENOBUFS),
			#endif
			#ifdef ENODATA
				__ERRNO_ASSIGN(ENODATA),
			#endif
			#ifdef ENODEV
				__ERRNO_ASSIGN(ENODEV),
			#endif
			#ifdef ENOENT
				__ERRNO_ASSIGN(ENOENT),
			#endif
			#ifdef ENOEXEC
				__ERRNO_ASSIGN(ENOEXEC),
			#endif
			#ifdef ENOLCK
				__ERRNO_ASSIGN(ENOLCK),
			#endif
			#ifdef ENOLINK
				__ERRNO_ASSIGN(ENOLINK),
			#endif
			#ifdef ENOMEM
				__ERRNO_ASSIGN(ENOMEM),
			#endif
			#ifdef ENOMSG
				__ERRNO_ASSIGN(ENOMSG),
			#endif
			#ifdef ENOPOLICY
				__ERRNO_ASSIGN(ENOPOLICY),
			#endif
			#ifdef ENOPROTOOPT
				__ERRNO_ASSIGN(ENOPROTOOPT),
			#endif
			#ifdef ENOSPC
				__ERRNO_ASSIGN(ENOSPC),
			#endif
			#ifdef ENOSR
				__ERRNO_ASSIGN(ENOSR),
			#endif
			#ifdef ENOSTR
				__ERRNO_ASSIGN(ENOSTR),
			#endif
			#ifdef ENOSYS
				__ERRNO_ASSIGN(ENOSYS),
			#endif
			#ifdef ENOTBLK
				__ERRNO_ASSIGN(ENOTBLK),
			#endif
			#ifdef ENOTCONN
				__ERRNO_ASSIGN(ENOTCONN),
			#endif
			#ifdef ENOTDIR
				__ERRNO_ASSIGN(ENOTDIR),
			#endif
			#ifdef ENOTEMPTY
				__ERRNO_ASSIGN(ENOTEMPTY),
			#endif
			#ifdef ENOTRECOVERABLE
				__ERRNO_ASSIGN(ENOTRECOVERABLE),
			#endif
			#ifdef ENOTSOCK
				__ERRNO_ASSIGN(ENOTSOCK),
			#endif
			#ifdef ENOTSUP
				__ERRNO_ASSIGN(ENOTSUP),
			#endif
			#ifdef ENOTTY
				__ERRNO_ASSIGN(ENOTTY),
			#endif
			#ifdef ENXIO
				__ERRNO_ASSIGN(ENXIO),
			#endif
			#ifdef EOPNOTSUPP
				__ERRNO_ASSIGN(EOPNOTSUPP),
			#endif
			#ifdef EOVERFLOW
				__ERRNO_ASSIGN(EOVERFLOW),
			#endif
			#ifdef EOWNERDEAD
				__ERRNO_ASSIGN(EOWNERDEAD),
			#endif
			#ifdef EPERM
				__ERRNO_ASSIGN(EPERM),
			#endif
			#ifdef EPFNOSUPPORT
				__ERRNO_ASSIGN(EPFNOSUPPORT),
			#endif
			#ifdef EPIPE
				__ERRNO_ASSIGN(EPIPE),
			#endif
			#ifdef EPROCLIM
				__ERRNO_ASSIGN(EPROCLIM),
			#endif
			#ifdef EPROCUNAVAIL
				__ERRNO_ASSIGN(EPROCUNAVAIL),
			#endif
			#ifdef EPROGMISMATCH
				__ERRNO_ASSIGN(EPROGMISMATCH),
			#endif
			#ifdef EPROGUNAVAIL
				__ERRNO_ASSIGN(EPROGUNAVAIL),
			#endif
			#ifdef EPROTO
				__ERRNO_ASSIGN(EPROTO),
			#endif
			#ifdef EPROTONOSUPPORT
				__ERRNO_ASSIGN(EPROTONOSUPPORT),
			#endif
			#ifdef EPROTOTYPE
				__ERRNO_ASSIGN(EPROTOTYPE),
			#endif
			#ifdef EPWROFF
				__ERRNO_ASSIGN(EPWROFF),
			#endif
			#ifdef ERANGE
				__ERRNO_ASSIGN(ERANGE),
			#endif
			#ifdef EREMOTE
				__ERRNO_ASSIGN(EREMOTE),
			#endif
			#ifdef EROFS
				__ERRNO_ASSIGN(EROFS),
			#endif
			#ifdef ERPCMISMATCH
				__ERRNO_ASSIGN(ERPCMISMATCH),
			#endif
			#ifdef ESHLIBVERS
				__ERRNO_ASSIGN(ESHLIBVERS),
			#endif
			#ifdef ESHUTDOWN
				__ERRNO_ASSIGN(ESHUTDOWN),
			#endif
			#ifdef ESOCKTNOSUPPORT
				__ERRNO_ASSIGN(ESOCKTNOSUPPORT),
			#endif
			#ifdef ESPIPE
				__ERRNO_ASSIGN(ESPIPE),
			#endif
			#ifdef ESRCH
				__ERRNO_ASSIGN(ESRCH),
			#endif
			#ifdef ESTALE
				__ERRNO_ASSIGN(ESTALE),
			#endif
			#ifdef ETIME
				__ERRNO_ASSIGN(ETIME),
			#endif
			#ifdef ETIMEDOUT
				__ERRNO_ASSIGN(ETIMEDOUT),
			#endif
			#ifdef ETOOMANYREFS
				__ERRNO_ASSIGN(ETOOMANYREFS),
			#endif
			#ifdef ETXTBSY
				__ERRNO_ASSIGN(ETXTBSY),
			#endif
			#ifdef EUSERS
				__ERRNO_ASSIGN(EUSERS),
			#endif
			#ifdef EXDEV
				__ERRNO_ASSIGN(EXDEV),
			#endif
		};
		#undef __ERRNO_ASSIGN
		#pragma clang diagnostic pop
		constexpr size_t num_errnos = size(__errnos);
		constexpr size_t num_errors = num_errnos < 256 ? 256 : num_errnos;
		std::array<std::string, num_errors> _names{};
		std::array<std::string, num_errors> _descriptions{};
		for (size_t i = 0; i < num_errors; ++i) {
			if (i < num_errnos && __errnos[i]) {
				_names[i] = __errnos[i];
			}
			char description[100];
			int errnum = static_cast<int>(i);
			if (strerror_r(errnum, description, sizeof(description)) == 0) {
				_descriptions[i] = description;
			} else {
				snprintf(description, sizeof(description), "Unknown error: %d", errnum);
				_descriptions[i] = description;
			}
		}
		return std::make_pair(_names, _descriptions);
	}();
	return _errnos;
}
}


inline const auto&
name(int errnum)
{
	auto& _errnos = detail::errnos().first;
	if (errnum >= 0 && errnum < static_cast<int>(_errnos.size())) {
		auto& name = _errnos[errnum];
		if (!name.empty()) {
			return name;
		}
	}
	static const std::string _unknown = "UNKNOWN";
	return _unknown;
}


inline const auto&
description(int errnum)
{
	auto& _errnos = detail::errnos().second;
	if (errnum >= 0 && errnum < static_cast<int>(_errnos.size())) {
		auto& name = _errnos[errnum];
		if (!name.empty()) {
			return name;
		}
	}
	static const std::string _unknown = "Unknown error";
	return _unknown;
}

}
