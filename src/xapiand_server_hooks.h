/*
 * Injection-seam hooks for the `server` engine, wired to Xapiand.
 *
 * The standalone engine reached three host singletons through seams with safe
 * defaults (server_hooks.h). This is the engine's SERVER_HOOKS_HEADER: it maps
 * those seams back onto Xapiand exactly as the in-tree code did, so the de-vendor
 * is behavior-preserving.
 *
 *   SERVER_FATAL(code)            -> sig_exit(code)  (graceful exit)
 *   SERVER_ON_CLIENT_CREATED()    -> ++XapiandManager::total_clients
 *   SERVER_ON_CLIENT_DESTROYED()  -> the total_clients decrement + last-client
 *                                    try_shutdown the in-tree BaseClient dtor did
 *   SERVER_LENGTH_ERROR           -> Xapian::SerialisationError (Xapiand's length
 *                                    codec throws this on malformed input)
 */

#pragma once

#include <sysexits.h>       // for EX_SOFTWARE

#include "manager.h"        // for XapiandManager, sig_exit
#include "xapian.h"         // for Xapian::SerialisationError

#define SERVER_FATAL(code) sig_exit(code)

#define SERVER_ON_CLIENT_CREATED() \
	do { \
		if (auto _m = XapiandManager::manager()) { \
			++_m->total_clients; \
		} \
	} while (0)

#define SERVER_ON_CLIENT_DESTROYED() \
	do { \
		if (auto _m = XapiandManager::manager()) { \
			if (_m->total_clients.fetch_sub(1) == 0) { \
				sig_exit(-EX_SOFTWARE); \
			} \
		} \
		XapiandManager::try_shutdown(); \
	} while (0)

#define SERVER_LENGTH_ERROR ::Xapian::SerialisationError
