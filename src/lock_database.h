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

#pragma once

#include "manager.h"

#include <type_traits>


/*
*	Note: HasX checks for any data or function member called x, with arbitrary type.
*	The sole purpose of introducing the member name is to have a possible ambiguity for member-name lookup
*	the type of the member isn't important.
*/

template<typename T> struct HasEndpoints {
    struct Fallback { int endpoints; };
    struct Derived : T, Fallback { };

    template<typename C, C> struct ChT;

    template<typename C> static char (&f(ChT<int Fallback::*, &C::endpoints>*))[1];
    template<typename C> static char (&f(...))[2];

    static bool const value = sizeof(f<Derived>(0)) == 2;
};


template<typename T> struct Hasflags {
    struct Fallback { int flags; };
    struct Derived : T, Fallback { };

    template<typename C, C> struct ChT;

    template<typename C> static char (&f(ChT<int Fallback::*, &C::flags>*))[1];
    template<typename C> static char (&f(...))[2];

    static bool const value = sizeof(f<Derived>(0)) == 2;
};


template<typename T> struct HasDatabase {
    struct Fallback { int database; };
    struct Derived : T, Fallback { };

    template<typename C, C> struct ChT;

    template<typename C> static char (&f(ChT<int Fallback::*, &C::database>*))[1];
    template<typename C> static char (&f(...))[2];

    static bool const value = sizeof(f<Derived>(0)) == 2;
};


template<typename T> struct HasDBLocks {
    struct Fallback { int database_locks; };
    struct Derived : T, Fallback { };

    template<typename C, C> struct ChT;

    template<typename C> static char (&f(ChT<int Fallback::*, &C::database_locks>*))[1];
    template<typename C> static char (&f(...))[2];

    static bool const value = sizeof(f<Derived>(0)) == 2;
};


template <typename T, typename = std::enable_if_t<HasEndpoints<T>::value && Hasflags<T>::value && HasDatabase<T>::value && HasDBLocks<T>::value>>
class lock_database {
	T* db_handler;

	lock_database(const lock_database&) = delete;
	lock_database& operator=(const lock_database&) = delete;

	template <bool internal>
	void _lock() {
		if (db_handler != nullptr) {
			if (db_handler->database) {
				if constexpr (internal) {
					// internal always increments number of locks
					++db_handler->database_locks;
				}
			} else {
				XapiandManager::manager->database_pool.checkout(db_handler->database, db_handler->endpoints, db_handler->flags);
				assert(db_handler->database);
				++db_handler->database_locks;
			}
		}
	}

	template <bool internal>
	void _unlock() {
		if (db_handler != nullptr) {
			if (db_handler->database && db_handler->database_locks > 0) {
				if (--db_handler->database_locks == 0) {
					XapiandManager::manager->database_pool.checkin(db_handler->database);
				}
			} else if constexpr (!internal) {
				// internal never throws, just ignores
				THROW(Error, "lock_database is not locked: %s", repr(db_handler->endpoints.to_string()));
			}
		}
	}

public:
	lock_database(T* db_handler_)
		: db_handler(db_handler_) {
			_lock<true>();
		}

	~lock_database() {
		_unlock<true>();
	}

	void lock() {
		_lock<false>();
	}
	void unlock() {
		_unlock<false>();
	}
	void unsafe_unlock() {
		_unlock<true>();
	}
};