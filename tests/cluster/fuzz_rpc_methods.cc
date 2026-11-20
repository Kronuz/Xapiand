/*
 * Copyright © 2026 Germán Méndez Bravo (Kronuz)
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

// libFuzzer target for the shard RPC's envelope decoders (src/cluster/rpc_methods.h). The first
// byte picks the decoder, the rest is the envelope. A decoder returns a value or throws Malformed,
// nothing else escapes; a value that decodes must encode to bytes that decode to the same value.
//   fuzz_rpc_methods corpus/ (seeds from fuzz_rpc_seed)

#include <cstddef>      // for std::size_t
#include <cstdint>      // for std::uint8_t
#include <string_view>  // for std::string_view

#include "cluster/rpc_methods.h"
#include "rpc_samples.h"  // for rpc_samples::decoders

namespace rpc = cluster::rpc;

namespace {

// A decoder that has no operator== (MsgPack holds NaN, which never equals itself): re-decode only.
template <class T> void check(std::string_view bytes) {
	try {
		const auto value = T::decode(bytes);
		const auto again = T::decode(value.encode());
		if constexpr (requires { value == again; })
			if (!(value == again))
				__builtin_trap();
	} catch (const rpc::Malformed&) {
	}
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
	if (!size)
		return 0;
	const std::string_view bytes(reinterpret_cast<const char*>(data) + 1, size - 1);
	switch (data[0] % rpc_samples::decoders) {
		case 0: check<rpc::Relayed>(bytes); break;
		case 1: check<rpc::Answer>(bytes); break;
		case 2: check<rpc::StatsRequest>(bytes); break;
		case 3: check<rpc::StatsAnswer>(bytes); break;
		case 4: check<rpc::SearchRequest>(bytes); break;
		case 5: check<rpc::SearchAnswer>(bytes); break;
		case 6: check<rpc::CountAnswer>(bytes); break;
		case 7: check<rpc::InfoRequest>(bytes); break;
		case 8: check<rpc::InfoAnswer>(bytes); break;
		case 9: check<rpc::Keepalive>(bytes); break;
		default: check<rpc::KeepaliveAnswer>(bytes); break;
	}
	return 0;
}
