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
		auto pivot = partition(left, right);
		quicksort(left, pivot);
		quicksort(pivot + 1, right);
	}
}


/***********************************************************************
 * Integer Hash Functions
 */

// Use fast_hasher for smaller sets.
template <typename T>
struct fast_hasher {};
template <>
struct fast_hasher<std::uint32_t> {
	constexpr fast_hasher() { }
	constexpr std::uint32_t hash(std::uint32_t key, std::uint32_t seed) const {
		key = key ^ seed;
		return key;
	}
};
template <>
struct fast_hasher<std::uint64_t> {
	constexpr fast_hasher() { }
	constexpr std::uint32_t hash(std::uint64_t key, std::uint32_t seed) const {
		key = key ^ seed;
		return static_cast<std::uint32_t>(key);
	}
};

// Use strong_hasher for larger sets or if table cannot be found.
template <typename T>
struct strong_hasher {};
template <>
struct strong_hasher<std::uint32_t> {
	constexpr strong_hasher() { }
	constexpr std::uint32_t hash(std::uint32_t key, std::uint32_t seed) const {
		key = key ^ seed;
		key = ~key + (key << 15); // key = (key << 15) - key - 1;
		key = key ^ (key >> 12);
		key = key + (key << 2);
		key = key ^ (key >> 4);
		key = key * 2057; // key = (key + (key << 3)) + (key << 11);
		key = key ^ (key >> 16);
		return key;
	}
};
template <>
struct strong_hasher<std::uint64_t> {
	constexpr strong_hasher() { }
	constexpr std::uint32_t hash(std::uint64_t key, std::uint32_t seed) const {
		key = key ^ seed;
		key = (~key) + (key << 18); // key = (key << 18) - key - 1;
		key = key ^ (key >> 31);
		key = key * 21; // key = (key + (key << 2)) + (key << 4);
		key = key ^ (key >> 11);
		key = key + (key << 6);
		key = key ^ (key >> 22);
		return static_cast<std::uint32_t>(key);
	}
};


/***********************************************************************
 * Prime functions
 */

constexpr bool any_factors(std::size_t target, std::size_t start, std::size_t step) {
	return (
		!(start * start * 36 > target) &&
		(
			( (step == 1)
				&& ((target % (start * 6 + 1) == 0) ||
					(target % (start * 6 + 5) == 0))
			) ||
			( (step > 1)
				&& (any_factors(target, start, step / 2) ||
					any_factors(target, start + step / 2, step / 2))
			)
		)
	);
}

constexpr bool is_prime(std::size_t target) {
	return (
		(target == 2 || target == 3 || target == 5) ||
		(
			target != 0
			&& target != 1
			&& target % 2 != 0
			&& target % 3 != 0
			&& target % 5 != 0
			&& !any_factors(target, 1, target / 6 + 1)
		)
	);
}

constexpr std::size_t next_prime(std::size_t x) {
	while (true) {
		if (is_prime(x)) {
			return x;
		}
		++x;
	}
}


/***********************************************************************
 * Computes a constexpr perfect hash function
 */

constexpr static auto npos = std::numeric_limits<std::size_t>::max();

/*
 * For minimal perfect hash, use index_size = N
 * For smaller tables, use displacement_size = N / 5
 * For faster (more reliable) construction use displacement_size = N
 */
template <typename T, std::size_t N,
	typename Hasher = fast_hasher<T>,
	std::size_t displacement_size = N / 5,
	std::size_t index_size = next_prime(N + N / 4)
>
class phf {
	using displacement_type = std::uint32_t;

	static_assert(N > 0, "Must have at least one element");
	static_assert(index_size >= N, "index_size must be at least N");
	static_assert(displacement_size > 0, "Must have at least one element");
	static_assert(std::is_unsigned<T>::value, "Only supports unsigned integral types");

	struct index_type {
		using item_type = T;

		item_type item;
		std::size_t pos;

		constexpr index_type() : item{0}, pos{npos} { }
	};

	Hasher _hasher{};

	std::size_t _size;
	displacement_type _displacement[displacement_size];
	index_type _index[index_size];

public:
	constexpr phf() : _size{0}, _displacement{}, _index{} { }

	template <typename... Args>
	constexpr phf(Args&&... args) : phf() {
		assign(std::forward<Args>(args)...);
	}

	constexpr void assign(const T (&items)[N]) {
		assign(items, N);
	}

	constexpr void assign(std::initializer_list<T> ilist) {
		assign(ilist.begin(), ilist.end());
	}

	template <typename Iterator>
	constexpr void assign(Iterator first, Iterator last) {
		assign(first, last - first);
	}

	constexpr void assign(const T* items, std::size_t size) {
		if (size > N) {
			throw std::invalid_argument("PHF failed: too many items received");
		}

		clear();
		_size = size;

		std::size_t cnt[displacement_size]{};
		struct bucket_type {
			std::size_t* cnt;
			std::size_t slot;
			std::size_t pos;
			T item;
			constexpr bucket_type() : cnt{nullptr}, slot{0}, pos{0}, item{0} { }
			constexpr bool operator<(const bucket_type& other) const {
				return (*cnt == *other.cnt) ? slot < other.slot : *cnt > *other.cnt;
			}
		} buckets[N]{};

		// Step 1: Mapping.
		for (std::size_t pos = 0; pos < size; ++pos) {
			auto& bucket = buckets[pos];
			auto& item = items[pos];
			auto hashed = item;
			auto slot = static_cast<std::size_t>(hashed % displacement_size);
			bucket.cnt = &cnt[slot];
			bucket.slot = slot;
			bucket.pos = pos;
			bucket.item = item;
			++*bucket.cnt;
		}

		// Step 2: Sort in descending order and process.
		quicksort(&buckets[0], &buckets[size - 1]);

		// Step 3: Search for suitable displacement.
		auto frm = &buckets[0];
		auto to = frm;
		auto end = &buckets[size];

		do {
			++to;
			if (to == end || frm->slot != to->slot) {
				auto& index = _displacement[frm->slot];
				for (displacement_type displacement = 1; displacement > 0; ++displacement) {
					auto frm_ = frm;
					for (; frm_ != to; ++frm_) {
						auto slot = static_cast<std::size_t>(_hasher.hash(frm_->item, displacement) % index_size);
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
						index = displacement;
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
				}
				if (frm != to) {
					throw std::invalid_argument("PHF failed: cannot find suitable table");
				}
			}
		} while (to != end);
	}

	constexpr void clear() noexcept {
		if (_size) {
			for (std::size_t i = 0; i < index_size; ++i) {
				_index[i].pos = npos;
#ifndef NDEBUG
				_index[i].item = 0;
#endif
			}
#ifndef NDEBUG
			for (std::size_t i = 0; i < displacement_size; ++i) {
				_displacement[i] = 0;
			}
#endif
		}
	}

	constexpr std::size_t lookup(const T& item) const noexcept {
		const auto& elem = _index[static_cast<std::size_t>(_hasher.hash(item, _displacement[item % displacement_size]) % index_size)];
		return elem.pos;
	}

	constexpr std::size_t find(const T& item) const noexcept {
		const auto& elem = _index[static_cast<std::size_t>(_hasher.hash(item, _displacement[item % displacement_size]) % index_size)];
		if (elem.item == item) {
			return elem.pos;
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

	constexpr bool empty() const noexcept {
		return !!_size;
	}

	constexpr auto size() const noexcept {
		return _size;
	}

	constexpr auto max_size() const noexcept {
		return N;
	}

	/*
	void dump() const {
		auto displacement_type_bits = sizeof(displacement_type) * 8;
		auto index_type_item_type_bits = sizeof(typename index_type::item_type) * 8;
		printf(
		"///////////////////////////////////////////////////////////////////////\n"
		"// Autogenerated by phf::dump()\n"
		"using displacement_type = std::uint%lu_t;\n"
		"displacement_type _displacement[%zu] = {\n", displacement_type_bits, displacement_size);
		for (std::size_t i = 0; i < displacement_size; ++i) {
			printf("\t%uu,\n", _displacement[i]);
		}
		printf(
		"};\n\n"
		"struct index_type {\n"
			"\tusing item_type = std::uint%lu_t;\n"
			"\titem_type item;\n"
			"\tstd::size_t pos;\n"
		"} _index[%zu] = {\n", index_type_item_type_bits, index_size);
		for (std::size_t i = 0; i < index_size; ++i) {
			printf("\t{ %lluu, %zuu },\n", static_cast<unsigned long long>(_index[i].item), _index[i].pos);
		}
		printf(
		"};\n\n"
		"constexpr std::size_t displacement_size = sizeof(_displacement) / sizeof(displacement_type);\n"
		"constexpr std::size_t index_size = sizeof(_index) / sizeof(index_type);\n"
		"using hash_type = fnv1ah%lu;\n", index_type_item_type_bits);
	}
	*/
};


/*
 * Make a perfect hash function
 */
template <typename T, std::size_t N,
	typename Hasher = fast_hasher<T>,
	std::size_t displacement_size = N / 5,
	std::size_t index_size = next_prime(N + N / 4)
>
constexpr static auto
make_phf(const T (&items)[N]) {
	return phf<T, N, Hasher, displacement_size, index_size>(items);
}

/*
 * Make a minimal perfect hash function
 */
template <typename T, std::size_t N,
	typename Hasher = fast_hasher<T>,
	std::size_t displacement_size = N / 5
>
constexpr static auto
make_mphf(const T (&items)[N]) {
	return phf<T, N, Hasher, displacement_size, N>(items);
}

} // namespace phf


#define PHF_HASH(arg) fnv1ah32::hash(arg)
#define PHF_VAR(name) phf_##name
#define PHF_FIND(arg, name) PHF_VAR(name).find(PHF_HASH(arg))

#define PHF_INIT_BEGIN(name) static constexpr auto PHF_VAR(name) = phf::make_phf({
#define PHF_OPTION_INIT(option, arg) PHF_HASH(#option),
#define PHF_INIT_END(name) });

#define PHF_SWITCH_BEGIN(arg, name) switch (PHF_FIND(arg, name)) {
#define PHF_OPTION_CASE(option, name) case PHF_FIND(#option, name)
#define PHF_OPTION_CASE_RETURN_STRING(option, name) PHF_OPTION_CASE(option, name): { static const std::string _(#option); return _; }
#define PHF_OPTION_CASE_DISPATCH(option, name, ...) PHF_OPTION_CASE(option, name): return _##name##_dispatcher_ ##option(__VA_ARGS__);
#define PHF_SWITCH_END(arg) }

#endif // PHF_HH
