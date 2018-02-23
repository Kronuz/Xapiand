/*
 * Copyright (C) 2015-2018 dubalu.com LLC. All rights reserved.
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

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>


namespace pmh {

template <typename T>
auto constexpr log(T v) {
	std::size_t n = 0;
	while (v > 1) {
		n += 1;
		v >>= 1;
	}
	return n;
}

constexpr std::size_t bit_weight(std::size_t n) {
	return (
		(n <= 8 * sizeof(unsigned int)) +
		(n <= 8 * sizeof(unsigned long)) +
		(n <= 8 * sizeof(unsigned long long)) +
		(n <= 128)
	);
}

unsigned int select_uint_least(std::integral_constant<std::size_t, 4>);
unsigned long select_uint_least(std::integral_constant<std::size_t, 3>);
unsigned long long select_uint_least(std::integral_constant<std::size_t, 2>);
template<std::size_t N>
unsigned long long select_uint_least(std::integral_constant<std::size_t, N>) {
	static_assert(N < 2, "unsupported type size");
	return {};
}
template<std::size_t N>
using select_uint_least_t = decltype(select_uint_least(std::integral_constant<std::size_t, bit_weight(N)>()));

template <typename T, T a, T c, T m>
class linear_congruential_engine {
	static_assert(std::is_unsigned<T>::value, "Only supports unsigned integral types");

public:
	using result_type = T;
	static constexpr result_type multiplier = a;
	static constexpr result_type increment = c;
	static constexpr result_type modulus = m;
	static constexpr result_type default_seed = 1u;

	linear_congruential_engine() = default;
	constexpr linear_congruential_engine(result_type s) { seed(s); }

	void seed(result_type s = default_seed) { state_ = s; }
	constexpr result_type operator()() {
		using uint_least_t = select_uint_least_t<log(a) + log(m) + 4>;
		uint_least_t tmp = static_cast<uint_least_t>(multiplier) * state_ + increment;
		if(modulus)
			state_ = tmp % modulus;
		else
			state_ = tmp;
		return state_;
	}
	constexpr void discard(unsigned long long n) {
		while (n--)
			operator()();
	}
	static constexpr result_type min() { return increment == 0u ? 1u : 0u; };
	static constexpr result_type max() { return modulus - 1u; };
	friend constexpr bool operator==(const linear_congruential_engine& self,
									 const linear_congruential_engine& other) {
		return self.state_ == other.state_;
	}
	friend constexpr bool operator!=(const linear_congruential_engine& self,
									 const linear_congruential_engine& other) {
		return !(self == other);
	}

private:
	result_type state_ = default_seed;
};

using minstd_rand0 = linear_congruential_engine<std::uint_fast32_t, 16807, 0, 2147483647>;
using minstd_rand = linear_congruential_engine<std::uint_fast32_t, 48271, 0, 2147483647>;
using default_prg_t = minstd_rand;

////////////////////////////////////////////////////////////////////////////////

template <class T>
constexpr void swap(T& a, T& b) {
	auto tmp = a;
	a = b;
	b = tmp;
}

template <typename Iterator>
constexpr Iterator partition(Iterator left, Iterator right) {
	auto pivot = left + (right - left) / 2;
	auto value = *pivot;
	swap(*right, *pivot);
	for (auto it = left; it < right; ++it) {
		if (*it < value) {
			swap(*it, *left);
			++left;
		}
	}
	swap(*right, *left);
	return left;
}

template <typename Iterator>
constexpr void quicksort(Iterator left, Iterator right) {
	if (left < right) {
		auto new_pivot = partition(left, right);
		quicksort(left, new_pivot);
		quicksort(new_pivot + 1, right);
	}
}

#define PMH_RESORT

template <typename T, std::size_t N, typename RNG = default_prg_t>
class pmh {
	static_assert(N > 0, "Must have at least one element");
	static_assert(std::is_unsigned<T>::value, "Only supports unsigned integral types");

public:
	constexpr static int max_first_tries = 100;
	constexpr static int max_second_tries = 100;
	constexpr static int max_clashes = 2 * (1u << (log(N) / 2));
	constexpr static auto npos = std::numeric_limits<std::size_t>::max();

	struct bucket_t {
		T item;
		std::size_t pos;

		constexpr bucket_t() : item(0), pos{npos} { }
	};

private:

	struct hashed_item_t {
		std::size_t cnt;
		T slot1;
		T slot2;
		T hash;
		T item;
		std::size_t pos;

		constexpr hashed_item_t() : cnt{0}, slot1{0}, slot2{0}, hash{0}, item{0}, pos{npos} { }

		constexpr hashed_item_t(const hashed_item_t& other) :
			cnt{other.cnt},
			slot1{other.slot1},
			slot2{other.slot2},
			hash{other.hash},
			item{other.item},
			pos{other.pos} { }

		constexpr hashed_item_t(hashed_item_t&& other) noexcept :
			cnt{std::move(other.cnt)},
			slot1{std::move(other.slot1)},
			slot2{std::move(other.slot2)},
			hash{std::move(other.hash)},
			item{std::move(other.item)},
			pos{std::move(other.pos)} { }

		constexpr hashed_item_t& operator=(const hashed_item_t& other) {
			cnt = other.cnt;
			slot1 = other.slot1;
			slot2 = other.slot2;
			hash = other.hash;
			item = other.item;
			pos = other.pos;
			return *this;
		}

		constexpr hashed_item_t& operator=(hashed_item_t&& other) noexcept {
			cnt = std::move(other.cnt);
			slot1 = std::move(other.slot1);
			slot2 = std::move(other.slot2);
			hash = std::move(other.hash);
			item = std::move(other.item);
			pos = std::move(other.pos);
			return *this;
		}

		constexpr bool operator<(const hashed_item_t& other) const {
			if (cnt == other.cnt) {
				return slot1 < other.slot1;
			}
			return cnt > other.cnt;
		}
	};

	constexpr static std::size_t hash(const T &value, std::size_t seed) {
		std::size_t key = seed ^ value;
		key = (~key) + (key << 21); // key = (key << 21) - key - 1;
		key = key ^ (key >> 24);
		key = (key + (key << 3)) + (key << 8); // key * 265
		key = key ^ (key >> 14);
		key = (key + (key << 2)) + (key << 4); // key * 21
		key = key ^ (key >> 28);
		key = key + (key << 31);
		return key;
	}

	std::size_t _seed;
	bucket_t _first[N];
	bucket_t _second[N];

public:
	constexpr pmh(const T (&items)[N]) {
		RNG prg;
		hashed_item_t hashed_items[N];

		for (int try_first = 0; try_first < max_first_tries; ++try_first) {
			_seed = prg();
			for (std::size_t pos = 0; pos < N; ++pos) {
				auto& item = items[pos];
				auto& hashed_item = hashed_items[pos];
				auto hashed = hash(item, _seed);
				hashed_item.item = item;
				hashed_item.hash = hashed;
				hashed_item.slot1 = hashed % N;
				hashed_item.pos = pos;
				hashed_item.cnt = 0;
			}

			quicksort(&hashed_items[0], &hashed_items[N - 1]);

			auto end = &hashed_items[N];

			///

			auto frm = &hashed_items[0];
			auto to = frm;
			bool last = false;

#ifdef PMH_RESORT
			while (!last) {
				++to;
				last = to == end;
				if (last || frm->slot1 != to->slot1) {
					auto cnt = to - frm;
					if (cnt > max_clashes) {
						break;
					}
					for (; frm != to; ++frm) {
						frm->cnt = cnt;
					}
				}
			}
			if (!last) {
				continue;
			}
			quicksort(&hashed_items[0], &hashed_items[N - 1]);

			///
			frm = &hashed_items[0];
			to = frm;
			last = false;
#endif

			while (!last) {
				++to;
				last = to == end;

				if (last || frm->slot1 != to->slot1) {
					auto& first_bucket = _first[frm->slot1];
					auto cnt = to - frm;
					if (cnt > 1) {
						// slot clash
#ifndef PMH_RESORT
						if (cnt > max_clashes) {
							break;
						}
#endif
						int try_second = 0;
						for (; try_second < max_second_tries; ++try_second) {
							auto seed = prg();
							first_bucket.item = seed;

							auto frm_ = frm;
							for (; frm_ != to; ++frm_) {
								auto hashed = hash(frm_->item, seed);
								frm_->slot2 = hashed % N;
								auto& second_bucket = _second[frm_->slot2];
								if (second_bucket.pos != npos) {
									break;
								}
								second_bucket.item = frm_->item;
								second_bucket.pos = frm_->pos;
							}
							if (frm_ == to) {
								frm = frm_;
								break;
							}
							// rollback, it failed to place all items in empty slots
							for (auto frm__ = frm; frm__ != frm_; ++frm__) {
								auto& second_bucket = _second[frm__->slot2];
								second_bucket.pos = npos;
							}
						}
						if (try_second == max_second_tries) {
							break;
						}
					} else {
						// no slot clash
						first_bucket.item = frm->item;
						first_bucket.pos = frm->pos;
						++frm;
					}
				}
			}
			if (last) {
				return;
			}
		}
		throw std::invalid_argument("Cannot figure out a suitable PMH table");
	}

	constexpr std::size_t operator[](const T& item) const {
		auto pos = find(item);
		if (pos == npos) {
			throw std::out_of_range("Item not found");
		}
		return pos;
	}

	constexpr auto size() const {
		return N;
	}

	constexpr std::size_t find(const T& item) const {
		auto hashed = hash(item, _seed);
		const auto& first_bucket = _first[hashed % N];
		if (first_bucket.pos != npos) {
			if (first_bucket.item != item) {
				return npos;
			}
			return first_bucket.pos;
		}
		hashed = hash(item, first_bucket.item);
		const auto& second_bucket = _second[hashed % N];
		if (second_bucket.pos != npos) {
			if (hashed)
			if (second_bucket.item != item) {
				return npos;
			}
			return second_bucket.pos;
		}
		return npos;
	}

};


template <typename T, std::size_t N>
constexpr static auto
init(const T (&items)[N]) {
	return pmh<T, N>(items);
}

} // namespace pmh


#pragma GCC diagnostic ignored "-Wvariadic-macros"
#pragma GCC diagnostic ignored "-Wgnu-zero-variadic-macro-arguments"

#define PMH_INIT_BEGIN(name) static constexpr auto pmh_##name = pmh::init({
#define PMH_OPTION_INIT(option, arg) fnv1ah32::hash(#option),
#define PMH_INIT_END(name) });

#define PMH_SWITCH_BEGIN(arg, name) switch (pmh_##name.find(fnv1ah32::hash(arg))) {
#define PMH_OPTION_CASE(option, name) case pmh_##name.find(fnv1ah32::hash(#option))
#define PMH_OPTION_CASE_RETURN_STRING(option, name) PMH_OPTION_CASE(option, name): { static const std::string _(#option); return _; }
#define PMH_OPTION_CASE_DISPATCH(option, name, args...) PMH_OPTION_CASE(option, name): return _##name##_dispatcher_ ##option(args);
#define PMH_SWITCH_END(arg) }


#ifdef PMH_EXAMPLE_MAIN
/*
 * This is an eample of usage for pmh
 */

#define EXAMPLE_OPTIONS(args...) \
  OPTION(abate, args) \
  OPTION(justicehood, args) \
  OPTION(sign, args) \
  OPTION(unfunny, args) \
  OPTION(zoanthropy, args)

PMH_INIT_BEGIN(example)
	#define OPTION PMH_OPTION_INIT
	EXAMPLE_OPTIONS(name)
	#undef OPTION
PMH_INIT_END()

// to_string returns a reference to a static std::string
const std::string& to_string(std::string_view name) {
	PMH_SWITCH_BEGIN(name, example)
		#define OPTION PMH_OPTION_CASE_RETURN_STRING
		EXAMPLE_OPTIONS(example)
		#undef OPTION
		default: {
			static const std::string _;
			return _;
		}
	PMH_SWITCH_END()
}

void _xxx_dispatcher_abate() { std::cerr << "dispatcher -> " << "YOUVE" << std::endl; }
void _xxx_dispatcher_justicehood() { std::cerr << "dispatcher -> " << "JUSTICEHOOD" << std::endl; }
void _xxx_dispatcher_sign() { std::cerr << "dispatcher -> " << "SIGN" << std::endl; }
void _xxx_dispatcher_unfunny() { std::cerr << "dispatcher -> " << "UNFUNNY" << std::endl; }
void _xxx_dispatcher_zoanthropy() { std::cerr << "dispatcher -> " << "ZOANTHROPY" << std::endl; }

void dispatch(std::string_view name) {
	PMH_SWITCH_BEGIN(name, example)
		#define OPTION PMH_OPTION_CASE_DISPATCH
		EXAMPLE_OPTIONS(example)
		#undef OPTION
	PMH_SWITCH_END()
}

int main(int argc, char const *argv[])
{
	const char* arg = argc > 1 ? argv[1] : "zoanthropy";

	auto pos = pmh_xxx.find(fnv1ah32::hash(arg));
	if (pos != decltype(pmh_xxx)::npos) {
		std::cerr << arg << " found at " << pos << " / " << pmh_xxx.size() - 1 << std::endl;
	} else {
		std::cerr << arg << " not found." << std::endl;
	}

	std::cerr << "to_string -> " <<  to_string(arg) << std::endl;
	dispatch(arg);
}

#endif
