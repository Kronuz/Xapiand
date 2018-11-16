/*
 * Copyright (C) 2018 Dubalu LLC. All rights reserved.
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

#include <cstdint>          // for uint64_t

// Affinity mapping for threads.
// For 64 cores, each bit represents one CPU
// For 4 cores, sixteen bits together represent one CPU

constexpr uint64_t cpu_affinity_wal_writer      = 0b0000000000000000000000000000000000000000000000000000000000001111;
constexpr uint64_t cpu_affinity_logging         = 0b0000000000000000000000000000000000000000000000000000000011111111;
constexpr uint64_t cpu_affinity_replication     = 0b0000000000000000000000000000000000000000000000000000000000000000;
constexpr uint64_t cpu_affinity_committers      = 0b1111111111111111111100000000000000000000000000000000000000000000;
constexpr uint64_t cpu_affinity_fsynchers       = 0b0000000000000000000000000000000000000000000000001111111111111111;
constexpr uint64_t cpu_affinity_updaters        = 0b0000000000000000000000000000000000000000000000000000000000000001;
constexpr uint64_t cpu_affinity_servers         = 0b0000000000000000000000000000000000000000000000001111111111111111;
constexpr uint64_t cpu_affinity_http_clients    = 0b0000000000000000111111111111111111111111111111110000000000000000;
constexpr uint64_t cpu_affinity_binary_clients  = 0b0000000000000000000000000000000011111111111111111111111111111111;
