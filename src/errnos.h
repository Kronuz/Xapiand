// Linux only:
#ifdef EADV
	__ERRNO(EADV)
#endif
#ifdef EBADE
	__ERRNO(EBADE)
#endif
#ifdef EBADFD
	__ERRNO(EBADFD)
#endif
#ifdef EBADR
	__ERRNO(EBADR)
#endif
#ifdef EBADRQC
	__ERRNO(EBADRQC)
#endif
#ifdef EBADSLT
	__ERRNO(EBADSLT)
#endif
#ifdef EBFONT
	__ERRNO(EBFONT)
#endif
#ifdef ECHRNG
	__ERRNO(ECHRNG)
#endif
#ifdef ECOMM
	__ERRNO(ECOMM)
#endif
#ifdef EDEADLOCK
	__ERRNO(EDEADLOCK)
#endif
#ifdef EDOTDOT
	__ERRNO(EDOTDOT)
#endif
#ifdef EISNAM
	__ERRNO(EISNAM)
#endif
#ifdef EKEYEXPIRED
	__ERRNO(EKEYEXPIRED)
#endif
#ifdef EKEYREJECTED
	__ERRNO(EKEYREJECTED)
#endif
#ifdef EKEYREVOKED
	__ERRNO(EKEYREVOKED)
#endif
#ifdef EL2HLT
	__ERRNO(EL2HLT)
#endif
#ifdef EL2NSYNC
	__ERRNO(EL2NSYNC)
#endif
#ifdef EL3HLT
	__ERRNO(EL3HLT)
#endif
#ifdef EL3RST
	__ERRNO(EL3RST)
#endif
#ifdef ELIBACC
	__ERRNO(ELIBACC)
#endif
#ifdef ELIBBAD
	__ERRNO(ELIBBAD)
#endif
#ifdef ELIBEXEC
	__ERRNO(ELIBEXEC)
#endif
#ifdef ELIBMAX
	__ERRNO(ELIBMAX)
#endif
#ifdef ELIBSCN
	__ERRNO(ELIBSCN)
#endif
#ifdef ELNRNG
	__ERRNO(ELNRNG)
#endif
#ifdef EMEDIUMTYPE
	__ERRNO(EMEDIUMTYPE)
#endif
#ifdef ENAVAIL
	__ERRNO(ENAVAIL)
#endif
#ifdef ENOANO
	__ERRNO(ENOANO)
#endif
#ifdef ENOCSI
	__ERRNO(ENOCSI)
#endif
#ifdef ENOKEY
	__ERRNO(ENOKEY)
#endif
#ifdef ENOMEDIUM
	__ERRNO(ENOMEDIUM)
#endif
#ifdef ENONET
	__ERRNO(ENONET)
#endif
#ifdef ENOPKG
	__ERRNO(ENOPKG)
#endif
#ifdef ENOTNAM
	__ERRNO(ENOTNAM)
#endif
#ifdef ENOTUNIQ
	__ERRNO(ENOTUNIQ)
#endif
#ifdef EREMCHG
	__ERRNO(EREMCHG)
#endif
#ifdef EREMOTEIO
	__ERRNO(EREMOTEIO)
#endif
#ifdef ERESTART
	__ERRNO(ERESTART)
#endif
#ifdef ESRMNT
	__ERRNO(ESRMNT)
#endif
#ifdef ESTRPIPE
	__ERRNO(ESTRPIPE)
#endif
#ifdef EUCLEAN
	__ERRNO(EUCLEAN)
#endif
#ifdef EUNATCH
	__ERRNO(EUNATCH)
#endif
#ifdef EWOULDBLOCK
	__ERRNO(EWOULDBLOCK)
#endif
#ifdef EXFULL
	__ERRNO(EXFULL)
#endif

// macOS only:
#ifdef EQFULL
	__ERRNO(EQFULL)
#endif

// both:
#ifdef E2BIG
	__ERRNO(E2BIG)
#endif
#ifdef EACCES
	__ERRNO(EACCES)
#endif
#ifdef EADDRINUSE
	__ERRNO(EADDRINUSE)
#endif
#ifdef EADDRNOTAVAIL
	__ERRNO(EADDRNOTAVAIL)
#endif
#ifdef EAFNOSUPPORT
	__ERRNO(EAFNOSUPPORT)
#endif
#ifdef EAGAIN
	__ERRNO(EAGAIN)
#endif
#ifdef EALREADY
	__ERRNO(EALREADY)
#endif
#ifdef EAUTH
	__ERRNO(EAUTH)
#endif
#ifdef EBADARCH
	__ERRNO(EBADARCH)
#endif
#ifdef EBADEXEC
	__ERRNO(EBADEXEC)
#endif
#ifdef EBADF
	__ERRNO(EBADF)
#endif
#ifdef EBADMACHO
	__ERRNO(EBADMACHO)
#endif
#ifdef EBADMSG
	__ERRNO(EBADMSG)
#endif
#ifdef EBADRPC
	__ERRNO(EBADRPC)
#endif
#ifdef EBUSY
	__ERRNO(EBUSY)
#endif
#ifdef ECANCELED
	__ERRNO(ECANCELED)
#endif
#ifdef ECHILD
	__ERRNO(ECHILD)
#endif
#ifdef ECONNABORTED
	__ERRNO(ECONNABORTED)
#endif
#ifdef ECONNREFUSED
	__ERRNO(ECONNREFUSED)
#endif
#ifdef ECONNRESET
	__ERRNO(ECONNRESET)
#endif
#ifdef EDEADLK
	__ERRNO(EDEADLK)
#endif
#ifdef EDESTADDRREQ
	__ERRNO(EDESTADDRREQ)
#endif
#ifdef EDEVERR
	__ERRNO(EDEVERR)
#endif
#ifdef EDOM
	__ERRNO(EDOM)
#endif
#ifdef EDQUOT
	__ERRNO(EDQUOT)
#endif
#ifdef EEXIST
	__ERRNO(EEXIST)
#endif
#ifdef EFAULT
	__ERRNO(EFAULT)
#endif
#ifdef EFBIG
	__ERRNO(EFBIG)
#endif
#ifdef EFTYPE
	__ERRNO(EFTYPE)
#endif
#ifdef EHOSTDOWN
	__ERRNO(EHOSTDOWN)
#endif
#ifdef EHOSTUNREACH
	__ERRNO(EHOSTUNREACH)
#endif
#ifdef EIDRM
	__ERRNO(EIDRM)
#endif
#ifdef EILSEQ
	__ERRNO(EILSEQ)
#endif
#ifdef EINPROGRESS
	__ERRNO(EINPROGRESS)
#endif
#ifdef EINTR
	__ERRNO(EINTR)
#endif
#ifdef EINVAL
	__ERRNO(EINVAL)
#endif
#ifdef EIO
	__ERRNO(EIO)
#endif
#ifdef EISCONN
	__ERRNO(EISCONN)
#endif
#ifdef EISDIR
	__ERRNO(EISDIR)
#endif
#ifdef ELOOP
	__ERRNO(ELOOP)
#endif
#ifdef EMFILE
	__ERRNO(EMFILE)
#endif
#ifdef EMLINK
	__ERRNO(EMLINK)
#endif
#ifdef EMSGSIZE
	__ERRNO(EMSGSIZE)
#endif
#ifdef EMULTIHOP
	__ERRNO(EMULTIHOP)
#endif
#ifdef ENAMETOOLONG
	__ERRNO(ENAMETOOLONG)
#endif
#ifdef ENEEDAUTH
	__ERRNO(ENEEDAUTH)
#endif
#ifdef ENETDOWN
	__ERRNO(ENETDOWN)
#endif
#ifdef ENETRESET
	__ERRNO(ENETRESET)
#endif
#ifdef ENETUNREACH
	__ERRNO(ENETUNREACH)
#endif
#ifdef ENFILE
	__ERRNO(ENFILE)
#endif
#ifdef ENOATTR
	__ERRNO(ENOATTR)
#endif
#ifdef ENOBUFS
	__ERRNO(ENOBUFS)
#endif
#ifdef ENODATA
	__ERRNO(ENODATA)
#endif
#ifdef ENODEV
	__ERRNO(ENODEV)
#endif
#ifdef ENOENT
	__ERRNO(ENOENT)
#endif
#ifdef ENOEXEC
	__ERRNO(ENOEXEC)
#endif
#ifdef ENOLCK
	__ERRNO(ENOLCK)
#endif
#ifdef ENOLINK
	__ERRNO(ENOLINK)
#endif
#ifdef ENOMEM
	__ERRNO(ENOMEM)
#endif
#ifdef ENOMSG
	__ERRNO(ENOMSG)
#endif
#ifdef ENOPOLICY
	__ERRNO(ENOPOLICY)
#endif
#ifdef ENOPROTOOPT
	__ERRNO(ENOPROTOOPT)
#endif
#ifdef ENOSPC
	__ERRNO(ENOSPC)
#endif
#ifdef ENOSR
	__ERRNO(ENOSR)
#endif
#ifdef ENOSTR
	__ERRNO(ENOSTR)
#endif
#ifdef ENOSYS
	__ERRNO(ENOSYS)
#endif
#ifdef ENOTBLK
	__ERRNO(ENOTBLK)
#endif
#ifdef ENOTCONN
	__ERRNO(ENOTCONN)
#endif
#ifdef ENOTDIR
	__ERRNO(ENOTDIR)
#endif
#ifdef ENOTEMPTY
	__ERRNO(ENOTEMPTY)
#endif
#ifdef ENOTRECOVERABLE
	__ERRNO(ENOTRECOVERABLE)
#endif
#ifdef ENOTSOCK
	__ERRNO(ENOTSOCK)
#endif
#ifdef ENOTSUP
	__ERRNO(ENOTSUP)
#endif
#ifdef ENOTTY
	__ERRNO(ENOTTY)
#endif
#ifdef ENXIO
	__ERRNO(ENXIO)
#endif
#ifdef EOPNOTSUPP
	__ERRNO(EOPNOTSUPP)
#endif
#ifdef EOVERFLOW
	__ERRNO(EOVERFLOW)
#endif
#ifdef EOWNERDEAD
	__ERRNO(EOWNERDEAD)
#endif
#ifdef EPERM
	__ERRNO(EPERM)
#endif
#ifdef EPFNOSUPPORT
	__ERRNO(EPFNOSUPPORT)
#endif
#ifdef EPIPE
	__ERRNO(EPIPE)
#endif
#ifdef EPROCLIM
	__ERRNO(EPROCLIM)
#endif
#ifdef EPROCUNAVAIL
	__ERRNO(EPROCUNAVAIL)
#endif
#ifdef EPROGMISMATCH
	__ERRNO(EPROGMISMATCH)
#endif
#ifdef EPROGUNAVAIL
	__ERRNO(EPROGUNAVAIL)
#endif
#ifdef EPROTO
	__ERRNO(EPROTO)
#endif
#ifdef EPROTONOSUPPORT
	__ERRNO(EPROTONOSUPPORT)
#endif
#ifdef EPROTOTYPE
	__ERRNO(EPROTOTYPE)
#endif
#ifdef EPWROFF
	__ERRNO(EPWROFF)
#endif
#ifdef ERANGE
	__ERRNO(ERANGE)
#endif
#ifdef EREMOTE
	__ERRNO(EREMOTE)
#endif
#ifdef EROFS
	__ERRNO(EROFS)
#endif
#ifdef ERPCMISMATCH
	__ERRNO(ERPCMISMATCH)
#endif
#ifdef ESHLIBVERS
	__ERRNO(ESHLIBVERS)
#endif
#ifdef ESHUTDOWN
	__ERRNO(ESHUTDOWN)
#endif
#ifdef ESOCKTNOSUPPORT
	__ERRNO(ESOCKTNOSUPPORT)
#endif
#ifdef ESPIPE
	__ERRNO(ESPIPE)
#endif
#ifdef ESRCH
	__ERRNO(ESRCH)
#endif
#ifdef ESTALE
	__ERRNO(ESTALE)
#endif
#ifdef ETIME
	__ERRNO(ETIME)
#endif
#ifdef ETIMEDOUT
	__ERRNO(ETIMEDOUT)
#endif
#ifdef ETOOMANYREFS
	__ERRNO(ETOOMANYREFS)
#endif
#ifdef ETXTBSY
	__ERRNO(ETXTBSY)
#endif
#ifdef EUSERS
	__ERRNO(EUSERS)
#endif
#ifdef EXDEV
	__ERRNO(EXDEV)
#endif
