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


typedef struct timespec timespec_t;


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

int clock_gettime(clockid_t clk_id, timespec_t *tp);

#endif /* __APPLE__ */


inline timespec_t * timespec_clear(timespec_t *tps0) {
	tps0->tv_nsec = 0;
	tps0->tv_sec = 0;
	return tps0;
}


inline double timespec_to_double(const timespec_t *tps) {
	return (double)tps->tv_sec + ((double)tps->tv_nsec / 1e9L);
}


inline timespec_t * double_to_timespec(timespec_t *tps, double dt) {
	tps->tv_nsec = (int)((dt - (int)dt) * 1e9L);
	tps->tv_sec = (int)dt;
	return tps;
}


inline timespec_t * timespec_set(timespec_t *tps0, const timespec_t *tps1) {
	tps0->tv_nsec = tps1->tv_nsec;
	tps0->tv_sec = tps1->tv_sec;
	return tps0;
}


inline timespec_t * timespec_add(timespec_t *tps0, const timespec_t *tps1) {
	tps0->tv_nsec += tps1->tv_nsec;
	tps0->tv_sec += tps1->tv_sec;
	while (tps0->tv_nsec >= 1e9L) {
		tps0->tv_nsec -= 1e9L;
		tps0->tv_sec++;
	}
	return tps0;
}


inline timespec_t * timespec_sub(timespec_t *tps0, const timespec_t *tps1) {
	tps0->tv_nsec -= tps1->tv_nsec;
	tps0->tv_sec -= tps1->tv_sec;
	while (tps0->tv_nsec < 0) {
		tps0->tv_nsec += 1e9L;
		tps0->tv_sec--;
	}
	return tps0;
}


inline timespec_t * timespec_add(timespec_t *tps, double dt) {
	timespec_t ts;
	double_to_timespec(&ts, dt);
	timespec_add(tps, &ts);
	return tps;
}


#define timespec_cmp(tsp0, cmp, tsp1) \
	(((tsp0)->tv_sec == (tsp1)->tv_sec) ? \
		((tsp0)->tv_nsec cmp (tsp1)->tv_nsec) : \
		((tsp0)->tv_sec cmp (tsp1)->tv_sec))
;

inline bool operator<(timespec_t &ts0, timespec_t &ts1) {
	return timespec_cmp(&ts0, <, &ts1);
}

inline bool operator<=(timespec_t &ts0, timespec_t &ts1) {
	return timespec_cmp(&ts0, <=, &ts1);
}

inline bool operator>(timespec_t &ts0, timespec_t &ts1) {
	return timespec_cmp(&ts0, >, &ts1);
}

inline bool operator>=(timespec_t &ts0, timespec_t &ts1) {
	return timespec_cmp(&ts0, >=, &ts1);
}

inline bool operator==(timespec_t &ts0, timespec_t &ts1) {
	return timespec_cmp(&ts0, ==, &ts1);
}

inline bool operator!=(timespec_t &ts0, timespec_t &ts1) {
	return timespec_cmp(&ts0, !=, &ts1);
}

inline bool operator<(timespec_t &ts0, double dt1) {
	timespec_t ts1;
	double_to_timespec(&ts1, dt1);
	return timespec_cmp(&ts0, <, &ts1);
}

inline bool operator<=(timespec_t &ts0, double dt1) {
	timespec_t ts1;
	double_to_timespec(&ts1, dt1);
	return timespec_cmp(&ts0, <=, &ts1);
}

inline bool operator>(timespec_t &ts0, double dt1) {
	timespec_t ts1;
	double_to_timespec(&ts1, dt1);
	return timespec_cmp(&ts0, >, &ts1);
}

inline bool operator>=(timespec_t &ts0, double dt1) {
	timespec_t ts1;
	double_to_timespec(&ts1, dt1);
	return timespec_cmp(&ts0, >=, &ts1);
}

inline bool operator==(timespec_t &ts0, double dt1) {
	timespec_t ts1;
	double_to_timespec(&ts1, dt1);
	return timespec_cmp(&ts0, ==, &ts1);
}

inline bool operator!=(timespec_t &ts0, double dt1) {
	timespec_t ts1;
	double_to_timespec(&ts1, dt1);
	return timespec_cmp(&ts0, !=, &ts1);
}

inline bool operator<(double dt0, timespec_t &ts1) {
	timespec_t ts0;
	double_to_timespec(&ts1, dt0);
	return timespec_cmp(&ts0, <, &ts1);
}

inline bool operator<=(double dt0, timespec_t &ts1) {
	timespec_t ts0;
	double_to_timespec(&ts1, dt0);
	return timespec_cmp(&ts0, <=, &ts1);
}

inline bool operator>(double dt0, timespec_t &ts1) {
	timespec_t ts0;
	double_to_timespec(&ts1, dt0);
	return timespec_cmp(&ts0, >, &ts1);
}

inline bool operator>=(double dt0, timespec_t &ts1) {
	timespec_t ts0;
	double_to_timespec(&ts1, dt0);
	return timespec_cmp(&ts0, >=, &ts1);
}

inline bool operator==(double dt0, timespec_t &ts1) {
	timespec_t ts0;
	double_to_timespec(&ts1, dt0);
	return timespec_cmp(&ts0, ==, &ts1);
}

inline bool operator!=(double dt0, timespec_t &ts1) {
	timespec_t ts0;
	double_to_timespec(&ts1, dt0);
	return timespec_cmp(&ts0, !=, &ts1);
}

#endif /* XAPIAND_TIME_H */
