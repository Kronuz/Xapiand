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

#include "config.h"                // for HAVE_CLOCK_GETTIME

#include <sys/time.h>
#include <time.h>                  // for clock_gettime, CLOCK_REALTIME


#ifdef __linux__

#include <time.h>                  // for clock_gettime, CLOCK_REALTIME
#include <unistd.h>

#endif /*__linux__*/

#ifdef __APPLE__

#ifndef HAVE_CLOCK_GETTIME

#include <errno.h>
#include <mach/clock.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <sched.h>
#include <sys/resource.h>
#include <unistd.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 4
#define CLOCK_THREAD_CPUTIME_ID 14
#define CLOCK_PROCESS_CPUTIME_ID 15


using clockid_t = int;

int clock_gettime(clockid_t clk_id, struct timespec *tp);

#endif /* HAVE_CLOCK_GETTIME */

#endif /* __APPLE__ */


#define _timespec_cmp(tsp0, cmp, tsp1) \
	(((tsp0)->tv_sec == (tsp1)->tv_sec) ? \
		((tsp0)->tv_nsec cmp (tsp1)->tv_nsec) : \
		((tsp0)->tv_sec cmp (tsp1)->tv_sec));


struct timespec_t : public timespec {
	timespec_t() {
		clear();
	}

	explicit timespec_t(double other) {
		tv_nsec = (int)((other - (int)other) * 1e9L);
		tv_sec = (int)other;
	}

	inline void clear() noexcept {
		tv_nsec = 0;
		tv_sec = 0;
	}

	inline void set_now() {
		clock_gettime(CLOCK_REALTIME, this);
	}

	inline double as_double() const noexcept {
		return (double)tv_sec + ((double)tv_nsec / 1e9L);
	}

	inline timespec_t& operator=(const timespec_t& other) {
		tv_nsec = other.tv_nsec;
		tv_sec = other.tv_sec;
		return *this;
	}

	inline void operator-=(const timespec_t& other) {
		tv_nsec -= other.tv_nsec;
		tv_sec -= other.tv_sec;
		_adjust();
	}

	inline timespec_t operator-(const timespec_t& other) const {
		timespec_t ts = *this;
		ts -= other;
		return ts;
	}

	inline void operator+=(const timespec_t& other) {
		tv_nsec += other.tv_nsec;
		tv_sec += other.tv_sec;
		_adjust();
	}

	inline timespec_t operator+(const timespec_t& other) const {
		timespec_t ts = *this;
		ts += other;
		return ts;
	}

	inline void operator*=(const timespec_t& other) {
		tv_nsec *= other.tv_nsec;
		tv_sec *= other.tv_sec;
		_adjust();
	}

	inline timespec_t operator*(const timespec_t& other) const {
		timespec_t ts = *this;
		ts *= other;
		return ts;
	}

	inline void operator/=(const timespec_t& other) {
		tv_nsec *= other.tv_nsec;
		tv_sec *= other.tv_sec;
		_adjust();
	}

	inline timespec_t operator/(const timespec_t& other) const {
		timespec_t ts = *this;
		ts *= other;
		return ts;
	}

	inline bool operator<(const timespec_t& other) const {
		return _timespec_cmp(this, <, &other);
	}

	inline bool operator<=(const timespec_t& other) const {
		return _timespec_cmp(this, <=, &other);
	}

	inline bool operator>(const timespec_t& other) const {
		return _timespec_cmp(this, >, &other);
	}

	inline bool operator>=(const timespec_t& other) const {
		return _timespec_cmp(this, >=, &other);
	}

	inline bool operator==(const timespec_t& other) const {
		return _timespec_cmp(this, ==, &other);
	}

	inline bool operator!=(const timespec_t& other) const {
		return _timespec_cmp(this, !=, &other);
	}

private:
	inline void _adjust() {
		while (tv_nsec < 0) {
			tv_nsec += 1e9L;
			tv_sec--;
		}
		while (tv_nsec  >= 1e9L) {
			tv_nsec -= 1e9L;
			tv_sec++;
		}
	}
};


inline timespec_t now() {
	timespec_t ts;
	ts.set_now();
	return ts;
}


inline bool operator<(double dt0, timespec_t& ts1) {
	return timespec_t(dt0) < ts1;
}


inline bool operator<=(double dt0, timespec_t& ts1) {
	return timespec_t(dt0) <= ts1;
}


inline bool operator>(double dt0, timespec_t& ts1) {
	return timespec_t(dt0) > ts1;
}


inline bool operator>=(double dt0, timespec_t& ts1) {
	return timespec_t(dt0) >= ts1;
}


inline bool operator==(double dt0, timespec_t& ts1) {
	return timespec_t(dt0) == ts1;
}


inline bool operator!=(double dt0, timespec_t& ts1) {
	return timespec_t(dt0) != ts1;
}
