/*
 * Copyright (C) 2018 German Mendez Bravo (Kronuz)
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

#ifndef MPH_HH
#define MPH_HH

#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>


namespace mph {

////////////////////////////////////////////////////////////////////////////////
// Constexpr linear congruential random number generator

template <typename T, T multiplier, T increment, T modulus>
class linear_congruential_generator {
	static_assert(std::is_unsigned<T>::value, "Only supports unsigned integral types");

	T seed = 1u;

public:
	constexpr T operator()() {
		seed = (seed * multiplier + increment) % modulus;
		return seed;
	}
};


////////////////////////////////////////////////////////////////////////////////
// Constexpr quicksort

template <class T>
constexpr void swap(T& a, T& b) {
	auto tmp = a;
	a = b;
	b = tmp;
}

template <typename It>
constexpr It partition(It left, It right) {
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

template <typename It>
constexpr void quicksort(It left, It right) {
	if (left < right) {
		auto pivot = partition(left, right);
		quicksort(left, pivot);
		quicksort(pivot + 1, right);
	}
}


////////////////////////////////////////////////////////////////////////////////
// Computes a constexpr minimal perfect hash table

constexpr static auto npos = std::numeric_limits<std::size_t>::max();

#define MPH_SORT_CLASHES

template <typename T, std::size_t N, typename RNG = linear_congruential_generator<T, 48271, 0, 2147483647>>
class mph {
	static_assert(N > 0, "Must have at least one element");
	static_assert(std::is_unsigned<T>::value, "Only supports unsigned integral types");

	struct hashed_item_t {
		T item;
		T slot;
		std::size_t cnt;
		std::size_t pos;

		constexpr hashed_item_t() : item{0}, slot{0}, cnt{0}, pos{npos} { }

		constexpr hashed_item_t(const hashed_item_t& other) :
			item{other.item},
			slot{other.slot},
			cnt{other.cnt},
			pos{other.pos} { }

		constexpr hashed_item_t(hashed_item_t&& other) noexcept :
			item{std::move(other.item)},
			slot{std::move(other.slot)},
			cnt{std::move(other.cnt)},
			pos{std::move(other.pos)} { }

		constexpr hashed_item_t& operator=(const hashed_item_t& other) {
			item = other.item;
			slot = other.slot;
			cnt = other.cnt;
			pos = other.pos;
			return *this;
		}

		constexpr hashed_item_t& operator=(hashed_item_t&& other) noexcept {
			item = std::move(other.item);
			slot = std::move(other.slot);
			cnt = std::move(other.cnt);
			pos = std::move(other.pos);
			return *this;
		}

		constexpr bool operator<(const hashed_item_t& other) const {
			if (cnt == other.cnt) {
				return slot < other.slot;
			}
			return cnt > other.cnt;
		}
	};

	T _index[N];
	T _items[N];

public:
	constexpr mph(const T (&items)[N]) : _index{}, _items{} {
		RNG rng;
		hashed_item_t hashed_items[N];

		for (std::size_t pos = 0; pos < N; ++pos) {
			auto& item = items[pos];
			auto& hashed_item = hashed_items[pos];
			auto hashed = item;
			hashed_item.item = item;
			hashed_item.slot = hashed % N;
			hashed_item.pos = pos;
		}

		quicksort(&hashed_items[0], &hashed_items[N - 1]);

		auto end = &hashed_items[N];
		auto frm = &hashed_items[0];
		auto to = frm;

#ifdef MPH_SORT_CLASHES
		do {
			++to;
			if (to == end || (frm->item % N) != (to->item % N)) {
				auto cnt = to - frm;
				for (; frm != to; ++frm) {
					frm->cnt = cnt;
				}
			}
		} while (to != end);

		quicksort(&hashed_items[0], &hashed_items[N - 1]);

		frm = &hashed_items[0];
		to = frm;
#endif

		do {
			++to;
			if (to == end || frm->slot != to->slot) {
				auto& index = _index[frm->slot];
				while (true) {
					auto rnd = rng();
					auto frm_ = frm;
					std::size_t used_zero = npos;
					for (; frm_ != to; ++frm_) {
						auto slot = (frm_->item ^ rnd) % N;
						if (_items[slot] || slot == used_zero) {
							break;
						}
						if (frm_->item) {
							_items[slot] = frm_->item;
						} else {
							used_zero = slot;
						}
						frm_->slot = slot;
					}
					if (frm_ == to) {
						index = rnd;
						frm = frm_;
						break;
					}
					// it failed to place all items in empty slots, rollback
					for (auto frm__ = frm; frm__ != frm_; ++frm__) {
						_items[frm__->slot] = 0;
					}
				}
			}
		} while (to != end);
	}

	constexpr std::size_t find(const T& item) const {
		auto slot = (item ^ _index[item % N]) % N;
		if (_items[slot] == item) {
			return slot;
		}
		return npos;
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
};


template <typename T, std::size_t N>
constexpr static auto
init(const T (&items)[N]) {
	return mph<T, N>(items);
}

} // namespace mph


#define MPH_HASH(arg) fnv1ah32::hash(arg)
#define MPH_VAR(name) mph_##name
#define MPH_FIND(arg, name) MPH_VAR(name).find(MPH_HASH(arg))

#define MPH_INIT_BEGIN(name) static constexpr auto MPH_VAR(name) = mph::init({
#define MPH_OPTION_INIT(option, arg) MPH_HASH(#option),
#define MPH_INIT_END(name) });

#define MPH_SWITCH_BEGIN(arg, name) switch (MPH_FIND(arg, name)) {
#define MPH_OPTION_CASE(option, name) case MPH_FIND(#option, name)
#define MPH_OPTION_CASE_RETURN_STRING(option, name) MPH_OPTION_CASE(option, name): { static const std::string _(#option); return _; }
#define MPH_OPTION_CASE_DISPATCH(option, name, ...) MPH_OPTION_CASE(option, name): return _##name##_dispatcher_ ##option(__VA_ARGS__);
#define MPH_SWITCH_END(arg) }

#endif // MPH_HH
