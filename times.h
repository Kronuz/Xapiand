/*
 * Copyright (C) 2015 deipi.com LLC and contributors. All rights reserved.
 * Copyright (C), MM Weiss. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 *   * Neither the name of the MM Weiss nor the names of its contributors
 *     may be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef XAPIAND_TIME_H
#define XAPIAND_TIME_H

#include <sys/time.h>


#ifdef __APPLE__

#include <sys/resource.h>
#include <mach/mach.h>
#include <mach/clock.h>
#include <mach/mach_time.h>
#include <errno.h>
#include <unistd.h>
#include <sched.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 4
#define CLOCK_THREAD_CPUTIME_ID 14
#define CLOCK_PROCESS_CPUTIME_ID 15

typedef	int	clockid_t;

int clock_gettime(clockid_t clk_id, struct timespec *tp);

#endif /* __APPLE__ */


#define _timespec_cmp(tsp0, cmp, tsp1) \
	(((tsp0)->tv_sec == (tsp1)->tv_sec) ? \
		((tsp0)->tv_nsec cmp (tsp1)->tv_nsec) : \
		((tsp0)->tv_sec cmp (tsp1)->tv_sec))
;

typedef struct timespec_s : public timespec {
	timespec_s() {
		clear();
	}

	timespec_s(double other) {
		tv_nsec = (int)((other - (int)other) * 1e9L);
		tv_sec = (int)other;
	}

	inline void clear() {
		tv_nsec = 0;
		tv_sec = 0;
	}

	inline void set_now() {
		clock_gettime(CLOCK_REALTIME, this);
	}

	inline double as_double() const {
		return (double)tv_sec + ((double)tv_nsec / 1e9L);
	}

	inline timespec_s & operator=(const timespec_s &other) {
		tv_nsec = other.tv_nsec;
		tv_sec = other.tv_sec;
		return *this;
	}

	inline void operator-=(const timespec_s &other) {
		tv_nsec -= other.tv_nsec;
		tv_sec -= other.tv_sec;
		_adjust();
	}

	inline timespec_s operator-(const timespec_s &other) const {
		timespec_s ts = *this;
		ts -= other;
		return ts;
	}

	inline void operator+=(const timespec_s &other) {
		tv_nsec += other.tv_nsec;
		tv_sec += other.tv_sec;
		_adjust();
	}

	inline timespec_s operator+(const timespec_s &other) const {
		timespec_s ts = *this;
		ts += other;
		return ts;
	}

	inline void operator*=(const timespec_s &other) {
		tv_nsec *= other.tv_nsec;
		tv_sec *= other.tv_sec;
		_adjust();
	}

	inline timespec_s operator*(const timespec_s &other) const {
		timespec_s ts = *this;
		ts *= other;
		return ts;
	}

	inline void operator/=(const timespec_s &other) {
		tv_nsec *= other.tv_nsec;
		tv_sec *= other.tv_sec;
		_adjust();
	}

	inline timespec_s operator/(const timespec_s &other) const {
		timespec_s ts = *this;
		ts *= other;
		return ts;
	}

	inline bool operator<(const timespec_s &other) const {
		return _timespec_cmp(this, <, &other);
	}

	inline bool operator<=(const timespec_s &other) const {
		return _timespec_cmp(this, <=, &other);
	}

	inline bool operator>(const timespec_s &other) const {
		return _timespec_cmp(this, >, &other);
	}

	inline bool operator>=(const timespec_s &other) const {
		return _timespec_cmp(this, >=, &other);
	}

	inline bool operator==(const timespec_s &other) const {
		return _timespec_cmp(this, ==, &other);
	}

	inline bool operator!=(const timespec_s &other) const {
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
} timespec_t;


inline timespec_t now() {
	timespec_t ts;
	ts.set_now();
	return ts;
}


inline bool operator<(double dt0, timespec_t &ts1) {
	return timespec_t(dt0) < ts1;
}


inline bool operator<=(double dt0, timespec_t &ts1) {
	return timespec_t(dt0) <= ts1;
}


inline bool operator>(double dt0, timespec_t &ts1) {
	return timespec_t(dt0) > ts1;
}


inline bool operator>=(double dt0, timespec_t &ts1) {
	return timespec_t(dt0) >= ts1;
}


inline bool operator==(double dt0, timespec_t &ts1) {
	return timespec_t(dt0) == ts1;
}


inline bool operator!=(double dt0, timespec_t &ts1) {
	return timespec_t(dt0) != ts1;
}


#endif /* XAPIAND_TIME_H */
