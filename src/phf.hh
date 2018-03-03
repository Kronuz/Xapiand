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

#ifndef PHF_HH
#define PHF_HH

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <stdexcept>


namespace phf {

/***********************************************************************
 * Constexpr quicksort
 */

template <class T>
constexpr static void swap(T& a, T& b)
{
	auto tmp = a;
	a = b;
	b = tmp;
}

template <typename Iterator>
constexpr static Iterator partition(Iterator left, Iterator right)
{
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
constexpr static void quicksort(Iterator left, Iterator right)
{
	if (left < right) {
		auto pivot = partition(left, right);
		quicksort(left, pivot);
		quicksort(pivot + 1, right);
	}
}


/***********************************************************************
 * Primality test
 */

template <typename T>
constexpr static auto power(T x, T y, T m)
{
	T p = 1;
	x = x % m;
	while (y > 0) {
		if (y % 2) p = (p * x) % m;
		y /= 2;
		x = (x * x) % m;
	}
	return p;
}

template <typename T>
constexpr static auto miller_rabin_test(T a, T d, T n, T s)
{
	auto x = power(a, d, n);
	if (x == 1 || x == n - 1) return false;
	for (T i = 1; i < s; ++i) {
		x = (x * x) % n;
		if (x == n - 1) return false;
	}
	return true;
}

template <typename T>
constexpr static bool is_prime(T n)
{
	if (!(n % 2)) return false;
	if (!(n % 3)) return false;
	if (!(n % 5)) return false;
	if (!(n % 7)) return false;

	// Miller-Rabin Primality Test
	auto d = n - 1;
	T s = 0;
	do {
		++s;
		d /= 2;
	} while (!(d % 2));
	return !(
		miller_rabin_test(static_cast<T>(2), d, n, s) ||
		miller_rabin_test(static_cast<T>(7), d, n, s) ||
		miller_rabin_test(static_cast<T>(61), d, n, s)
	);
}

constexpr static std::size_t next_prime(std::size_t x)
{
	x += (x % 2) ? 0 : 1;
	while (!is_prime(x)) {
		x += 2;
	}
	return x;
}


/***********************************************************************
 * Log2
 */

template<class T>
auto constexpr log(T v) {
	std::size_t n = 0;
	while (v) {
		v /= 2;
		++n;
	}
	return n;
}


/***********************************************************************
 * Computes a constexpr perfect hash function
 * Based on:
 *   Djamal Belazzougui, Fabiano C. Botelho, and Martin Dietzfelbinger,
 *   "Hash, displace, and compress" paper ESA2009, page 13
 * Also a good reference:
 *   Edward A. Fox, Lenwood S. Heath, Qi Fan Chen and Amjad M. Daoud,
 *   "Practical minimal perfect hash functions for large databases",
 *   Algorithm II, CACM1992, 35(1):105-121
 */

constexpr static auto npos = std::numeric_limits<std::size_t>::max();

template <template<typename TT, std::size_t NN> class Impl, typename T, std::size_t M>
class phf {
	using I = Impl<T, M>;
	using displacement_type = std::uint32_t;

	static_assert(M > 0, "Must have at least one element");
	static_assert(I::index_size <= std::numeric_limits<displacement_type>::max(), "Too many elements");
	static_assert(I::index_size >= M, "index_size must be at least M");
	static_assert(I::buckets_size > 0, "Must have at least one element");
	static_assert(std::is_unsigned<T>::value, "Only supports unsigned integral types");

	struct bucket_type {
		displacement_type d0;
		displacement_type d1;

		constexpr bucket_type() : d0{0}, d1{0} { }
	};

	struct index_type {
		using item_type = T;

		item_type item;
		std::size_t pos;

		constexpr index_type() : item{0}, pos{npos} { }
	};

	std::size_t _size;
	bucket_type _buckets[I::buckets_size];
	index_type _index[I::index_size];

	constexpr const auto& _lookup(const T& item) const noexcept {
		const auto& bucket =_buckets[I::g(item) % I::buckets_size];
		return _index[static_cast<std::size_t>((I::f1(item) + I::f2(item) * bucket.d0 + bucket.d1) % I::index_size)];
	}

public:
	constexpr phf() : _size{0}, _buckets{}, _index{} { }

	template <typename... Args>
	constexpr phf(Args&&... args) : phf() {
		assign(std::forward<Args>(args)...);
	}

	constexpr void assign(const T (&items)[M]) {
		assign(items, M);
	}

	constexpr void assign(std::initializer_list<T> ilist) {
		assign(ilist.begin(), ilist.end());
	}

	template <typename Iterator>
	constexpr void assign(Iterator first, Iterator last) {
		assign(first, last - first);
	}

	constexpr void assign(const T* items, std::size_t size) {
		if (size > M) {
			throw std::invalid_argument("PHF failed: too many items received");
		}

		clear();
		_size = size;

		std::size_t cnt[I::buckets_size]{};
		struct bucket_mapping_type {
			std::size_t* cnt;
			std::size_t slot;
			std::size_t pos;
			T item;
			T f1;
			T f2;
			constexpr bucket_mapping_type() : cnt{nullptr}, slot{0}, pos{0}, item{0}, f1{0}, f2{0} { }
			constexpr bool operator<(const bucket_mapping_type& other) const {
				return (*cnt == *other.cnt) ? slot < other.slot : *cnt > *other.cnt;
			}
		} bucket_mapping[M]{};

		// Step 1: Mapping.
		for (std::size_t pos = 0; pos < size; ++pos) {
			auto& bucket = bucket_mapping[pos];
			auto& item = items[pos];
			auto slot = static_cast<std::size_t>(I::g(item) % I::buckets_size);
			bucket.cnt = &cnt[slot];
			bucket.slot = slot;
			bucket.pos = pos;
			bucket.item = item;
			bucket.f1 = I::f1(item);
			bucket.f2 = I::f2(item);
			++*bucket.cnt;
		}

		// Step 2: Sort in descending order and process.
		quicksort(&bucket_mapping[0], &bucket_mapping[size - 1]);

		// Step 3: Search for suitable displacement pair.
		auto frm = &bucket_mapping[0];
		auto to = frm;
		auto end = &bucket_mapping[size];

		do {
			++to;
			if (to == end || frm->slot != to->slot) {
				auto& bucket = _buckets[frm->slot];
				for (displacement_type d0{0}, d1{0};;) {
					auto frm_ = frm;
					for (; frm_ != to; ++frm_) {
						auto slot = static_cast<std::size_t>((frm_->f1 + frm_->f2 * d0 + d1) % I::index_size);
						if (_index[slot].pos != npos) {
							if (_index[slot].item == frm_->item) {
								throw std::invalid_argument("PHF failed: duplicate items found");
							}
							break;
						}
						_index[slot].item = frm_->item;
						_index[slot].pos = frm_->pos;
						frm_->slot = slot;
					}
					if (frm_ == to) {
						bucket.d0 = d0;
						bucket.d1 = d1;
						frm = frm_;
						break;
					}
					// it failed to place all items in empty slots, rollback
					for (auto frm__ = frm; frm__ != frm_; ++frm__) {
						_index[frm__->slot].pos = npos;
#ifndef NDEBUG
						_index[frm__->slot].item = 0;
#endif
					}
					if (++d0 == I::index_size) {
						d0 = 0;
						if (++d1 == I::index_size) {
							throw std::invalid_argument("PHF failed: cannot find suitable table");
						}
					};
				}
			}
		} while (to != end);
	}

	constexpr void clear() noexcept {
		if (_size) {
			for (std::size_t i = 0; i < I::index_size; ++i) {
				_index[i].pos = npos;
#ifndef NDEBUG
				_index[i].item = 0;
#endif
			}
#ifndef NDEBUG
			for (std::size_t i = 0; i < I::buckets_size; ++i) {
				_buckets[i].d0 = 0;
				_buckets[i].d1 = 0;
			}
#endif
		}
	}

	constexpr std::size_t lookup(const T& item) const noexcept {
		const auto& elem = _lookup(item);
		return elem.pos;
	}

	constexpr std::size_t find(const T& item) const noexcept {
		const auto& elem = _lookup(item);
		return (elem.item == item ? 0 : npos) | elem.pos;
	}

	constexpr std::size_t count(const T& item) const noexcept {
		const auto& elem = _lookup(item);
		return elem.item == item;
	}

	constexpr std::size_t at(const T& item) const {
		const auto& elem = _lookup(item);
		if (elem.item == item) {
			return elem.pos;
		}
		throw std::out_of_range("Item not found");
	}

	constexpr std::size_t operator[](const T& item) const {
		return at(item);
	}

	constexpr bool empty() const noexcept {
		return !!_size;
	}

	constexpr auto size() const noexcept {
		return _size;
	}

	constexpr auto max_size() const noexcept {
		return M;
	}
};


/***********************************************************************/

template <typename T, std::size_t M>
struct fast_phf {
	constexpr static std::size_t buckets_size{1 << log(M / 5)};
	constexpr static std::size_t index_size{next_prime(M)};

	constexpr static T g(T key) {
		return key;
	}
	constexpr static T f1(T key) {
		return key;
	}
	constexpr static T f2(T key) {
		return key;
	}
};


template <typename T, std::size_t M>
struct strict_phf {
	constexpr static std::size_t buckets_size{1 << log(M / 5)};
	constexpr static std::size_t index_size{next_prime(M)};

	constexpr static T g(T key) {
		return key;
	}
	constexpr static T f1(T key) {
		key = key * 48271u;
		key = key % index_size;
		return key;
	}
	constexpr static T f2(T key) {
		key = key * 25214903917u;
		key = key % (index_size - 1) + 1;
		return key;
	}
};


/***********************************************************************/

/*
 * Make a perfect hash function
 */
template <template<typename TT, std::size_t NN> class Impl = fast_phf, typename T, std::size_t M>
constexpr static auto
make_phf(const T (&items)[M]) {
	return phf<Impl, T, M>(items);
}

} // namespace phf

#endif // PHF_HH
