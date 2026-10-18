/*
 * Trace hook for the `reactor` library, wired to Xapiand.
 *
 * L_EXC reports a UDP datagram handler that threw; reactor_udp.h drops the
 * datagram and keeps receiving. It is Xapiand's real (async) L_EXC: unlike the
 * scheduler and threadpool seams, log.h is safe to include here, since nothing in
 * the logger's include chain reaches reactor_udp.h (only cluster's bus.h does).
 */

#pragma once

#include "log.h"          // for L_EXC
