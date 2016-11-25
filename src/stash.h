/*
 * Copyright (C) 2016 deipi.com LLC and contributors. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to
 * deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 */

#pragma once

#include <array>     // for array
#include <atomic>    // for atomic

#include "logger_fwd.h"


enum class StashState {
	Ok,
	EmptyBin,
	EmptyChunk,
	OutOfRange,
	EmptyStash,
};


template <typename _Tp, size_t _Size>
class Stash {
protected:
	struct Nil { };

	struct Bin {
		_Tp val;

		Bin() = default;

		Bin(Nil&&) { }

		Bin(uint64_t key)
			: val(_Tp(key)) { }

		Bin(const _Tp& val_)
			: val(val_) { }

		Bin(_Tp&& val_) noexcept
			: val(std::move(val_)) { }
	};

	class Data {
		using Chunks = std::array<std::atomic<Bin*>, _Size>;
		std::atomic<Chunks*> chunk;
		std::atomic<Data*> next;

	public:
		Data()
			: chunk(nullptr),
			  next(nullptr) { }

		Data(Data&& o) noexcept
			: chunk(o.chunk.load()),
			  next(o.next.load()) { }

		~Data() {
			clear();
		}

		void clear() {
			auto s = chunk.load();
			while (!chunk.compare_exchange_weak(s, nullptr));
			if (s) {
				for (auto& a : *s) {
					auto p = a.load();
					while (!a.compare_exchange_weak(p, nullptr));
					if (p) {
						delete p;
					}
				}
				delete s;
			}

			Data* n;
			do {
				n = next.load();
				if (n) {
					while (!next.compare_exchange_weak(n, n->next));
					if (n) {
						n->next = nullptr;
						delete n;
					}
				}
			} while (n);
		}

		StashState bin(std::atomic<Bin*>** bin_pptr, size_t slot, bool spawn) {
			if (!spawn && !next && !chunk) {
				return StashState::EmptyStash;
			}

			size_t chunk = 0;
			if (slot >= _Size) {
				chunk = slot / _Size;
				slot = slot % _Size;
			}

			auto data = this;
			for (size_t c = 0; c < chunk; ++c) {
				auto ptr = data->next.load();
				if (!spawn && !ptr) {
					return StashState::OutOfRange;
				}
				if (!ptr) {
					auto tmp = new Data;
					if (data->next.compare_exchange_strong(ptr, tmp)) {
						ptr = tmp;
					} else {
						delete tmp;
					}
				}
				data = ptr;
			}

			auto ptr = data->chunk.load();
			if (!spawn && !ptr) {
				return StashState::EmptyChunk;
			}
			if (!ptr) {
				auto tmp = new Chunks{ {} };
				if (data->chunk.compare_exchange_strong(ptr, tmp)) {
					ptr = tmp;
				} else {
					delete tmp;
				}
			}

			auto& bin = (*ptr)[slot];
			if (!spawn && !bin.load()) {
				return StashState::EmptyBin;
			}

			*bin_pptr = &bin;
			return StashState::Ok;
		}
	};

	Data data;

public:
	Stash(Stash&& o) noexcept
		: data(std::move(o.data)) { }

	Stash() { }

	~Stash() {
		clear();
	}

	void clear() {
		data.clear();
	}

	StashState get_bin(std::atomic<Bin*>** bin_pptr, size_t slot) {
		/* This could fail with:
		 *   StashState::EmptyStash
		 *   StashState::EmptyBin
		 *   StashState::EmptyChunk
		 *   StashState::OutOfRange
		 */
		return Stash::data.bin(bin_pptr, slot, false);
	}

	auto& spawn_bin(size_t slot) {
		/* This shouldn't fail. */
		std::atomic<Bin*>* bin_ptr;
		if (Stash::data.bin(&bin_ptr, slot, true) != StashState::Ok) {
			throw std::logic_error("spawn_bin should only return Ok!");
		}
		return *bin_ptr;
	}

	template <typename T>
	auto& _put(std::atomic<Bin*>& bin, T&& value) {
		auto ptr = bin.load();
		if (!ptr) {
			auto tmp = new Bin(std::forward<T>(value));
			if (bin.compare_exchange_strong(ptr, tmp)) {
				ptr = tmp;
			} else {
				delete tmp;
			}
		}
		return ptr->val;
	}
};


template <typename _Tp, size_t _Size, uint64_t(*_CurrentKey)(), uint64_t _Sum, uint64_t _Div, uint64_t _Mod, bool _Ring>
class StashSlots : public Stash<_Tp, _Size> {
	using Stash_T = Stash<_Tp, _Size>;
	using Bin = typename Stash_T::Bin;
	using Nil = typename Stash_T::Nil;

	std::atomic_size_t atom_cur_pos;
	std::atomic_size_t atom_end_pos;

	size_t get_slot(uint64_t key) {
		auto slot = ((key + _Sum) / _Div) % _Mod;
		return slot;
	}

	size_t get_slot_size() {
		static const auto size = _Div + _Sum;
		return size;
	}

	auto& put(uint64_t key) {
		auto slot = get_slot(key);
		auto cur_pos = atom_cur_pos.load();
		auto end_pos = atom_end_pos.load();

		L_INFO_HOOK_LOG("StashSlots::put", this, "StashSlots::put() - _Mod:%llu, slot:%llu, cur_pos:%llu, end_pos:%llu", _Mod, slot, cur_pos, end_pos);

		auto& bin = Stash_T::spawn_bin(slot);

		if (!_Ring) {
			// This is not a ring, any new (older) item added could move the position
			while (slot < cur_pos && !atom_cur_pos.compare_exchange_weak(cur_pos, slot));
		}

		return Stash_T::_put(bin, Nil());
	}

	bool check_pos(size_t cur_pos, size_t initial_pos, size_t final_pos, size_t last_pos) {
		/* There are basically two cases where cur_pos is valid (v) based in the
		 * values of final_pos (fp) and initial_pos (ip).
		 *
		 * 1. When final_pos >= initial_pos:
		 *        |       (ip)vvvvvvvvvvvvvvvv(fp)       |
		 *                ^------- valid --------^
		 *
		 * 2. When final_pos < initial_pos:
		 *        |vvvvvvv(fp)                (ip)vvvvvvv|
		 *         ^--valid--^                ^--valid--^
		 *
		 */


		if (!_Ring && cur_pos == 0) {
			return false;
		}

		if (cur_pos == last_pos) {
			return false;
		}

		if (final_pos >= initial_pos) {
			if (cur_pos < initial_pos || cur_pos > final_pos) {
				// We're beyond final_pos, stop
				return false;
			}
		} else {
			if (cur_pos < initial_pos && cur_pos > final_pos) {
				// We're beyond final_pos, stop
				return false;
			}
		}

		return true;
	}

	bool increment_pos(bool peep, size_t& cur_pos, bool keep_going, size_t initial_pos, size_t& final_pos, size_t last_pos) {
		auto new_pos = (cur_pos + 1) % _Mod;

		if (!check_pos(new_pos, initial_pos, final_pos, last_pos)) {
			if (keep_going) {
				final_pos = get_slot(_CurrentKey());
				if (!check_pos(new_pos, initial_pos, final_pos, last_pos)) {
					return false;
				}
			} else {
				return false;
			}
		}

		if (peep || atom_cur_pos.compare_exchange_strong(cur_pos, new_pos)) {
			cur_pos = new_pos;
		}
		return true;
	}

public:
	StashSlots(StashSlots&& o) noexcept
		: Stash_T::Stash(std::move(o)),
		  atom_cur_pos(std::move(o.atom_cur_pos)),
		  atom_end_pos(std::move(o.atom_end_pos)) { }

	StashSlots()
		: atom_cur_pos(0),
		  atom_end_pos(0) { }

	StashSlots(uint64_t key)
		: atom_cur_pos(get_slot(key)),
		  atom_end_pos(get_slot(key)) { }

	template <typename T>
	bool next(T** value_ptr, bool final=true, uint64_t final_key=0, bool keep_going=true, bool peep=false) {
		auto cur_pos = atom_cur_pos.load();

		keep_going = keep_going && final && !final_key;
		if (keep_going) {
			final_key = _CurrentKey();
		}
		auto initial_pos = cur_pos;
		auto last_pos = static_cast<size_t>((initial_pos + _Mod - 1) % _Mod);
		auto final_pos = final ? get_slot(final_key) : last_pos;

		do {
			L_INFO_HOOK_LOG("StashSlots::next::loop", this, "StashSlots::next()::loop - _Mod:%llu, cur_pos:%llu, initial_pos:%llu, final:%s, final_pos:%llu, last_pos:%llu, keep_going:%s, peep:%s", _Mod, cur_pos, initial_pos, final ? "true" : "false", final_pos, last_pos, keep_going ? "true" : "false", peep ? "true" : "false");

			Bin* ptr = nullptr;
			std::atomic<Bin*>* bin_ptr;
			switch (Stash_T::get_bin(&bin_ptr, cur_pos)) {
				case StashState::Ok: {
					ptr = (*bin_ptr).load();
					if (ptr) {
						if (ptr->val.next(value_ptr, final && cur_pos == final_pos, final_key, keep_going, peep)) {
							return true;
						}
					}
					break;
				}
				case StashState::OutOfRange:
				case StashState::EmptyStash:
					return false;
				default:
					break;
			}

			if (!increment_pos(peep, cur_pos, keep_going, initial_pos, final_pos, last_pos)) {
				return false;
			}

			if (!final && ptr && !peep) {
				// Dispose if it's not in the final slice
				ptr->val.clear();
			}
		} while (true);
	}

	template <typename T>
	void add(T&& value, uint64_t key) {
		if (!key) key = _CurrentKey();
		put(key).add(std::forward<T>(value), key);
	}
};


template <typename _Tp, size_t _Size>
class StashValues : public Stash<_Tp, _Size> {
	using Stash_T = Stash<_Tp, _Size>;
	using Bin = typename Stash_T::Bin;
	using Nil = typename Stash_T::Nil;

	std::atomic_size_t atom_cur_pos;
	std::atomic_size_t idx;

public:
	StashValues(StashValues&& o) noexcept
		: Stash_T::Stash(std::move(o)),
		  atom_cur_pos(std::move(o.atom_cur_pos)),
		  idx(std::move(o.idx)) { }

	StashValues()
		: atom_cur_pos(0),
		  idx(0) { }

	StashValues(uint64_t)
		: atom_cur_pos(0),
		  idx(0) { }

	bool increment_pos(bool peep, size_t& cur_pos) {
		auto new_pos = cur_pos + 1;

		if (peep || atom_cur_pos.compare_exchange_strong(cur_pos, new_pos)) {
			cur_pos = new_pos;
		}
		return true;
	}

	template <typename T>
	bool next(T** value_ptr, bool, uint64_t, bool, bool peep) {
		// std::cout << "\t\tLogStash::next()" << std::endl;
		auto cur_pos = atom_cur_pos.load();
		// std::cout << "\t\t\tLogStash cur_pos:" << cur_pos << std::endl;
		std::atomic<Bin*>* bin_ptr;
		switch (Stash_T::get_bin(&bin_ptr, cur_pos)) {
			case StashState::Ok: {
				if (!increment_pos(peep, cur_pos)) {
					return false;
				}
				auto ptr = (*bin_ptr).load();
				if (ptr) {
					*value_ptr = &ptr->val;
					L_INFO_HOOK_LOG("StashValues::next::found", this, "StashValues::next()::found - cur_pos:%llu, peep:%s", cur_pos, peep ? "true" : "false");
					return true;
				}
			}
			default:
				break;
		}
		return false;
	}

	template <typename T>
	bool next(T** value_ptr, bool peep=false) {
		return next(value_ptr, true, 0, true, peep);
	}

	template <typename T>
	void add(T&& value, uint64_t) {
		auto& bin = Stash_T::spawn_bin(idx++);
		Stash_T::_put(bin, std::forward<T>(value));
	}
};
