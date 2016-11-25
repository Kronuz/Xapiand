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


template <typename _Tp, size_t _Size, uint64_t(*_CurrentKey)(), uint64_t _Div, uint64_t _Mod, bool _Ring>
class StashSlots : public Stash<_Tp, _Size> {
	using Stash_T = Stash<_Tp, _Size>;
	using Bin = typename Stash_T::Bin;
	using Nil = typename Stash_T::Nil;

	std::atomic<uint64_t> atom_cur_pos_key;
	std::atomic<uint64_t> atom_end_pos_key;

	size_t get_slot(uint64_t key) {
		auto slot = (key / _Div) % _Mod;
		return slot;
	}

	auto& put(uint64_t key) {
		auto slot = get_slot(key);
		auto cur_pos_key = atom_cur_pos_key.load();
		auto end_pos_key = atom_end_pos_key.load();

		L_INFO_HOOK_LOG("StashSlots::put", this, "StashSlots::put() - _Mod:%llu, key:%llu, slot:%llu, cur_pos_key:%llu, end_pos_key:%llu", _Mod, key, slot, cur_pos_key, end_pos_key);

		auto& bin = Stash_T::spawn_bin(slot);
		while (key < cur_pos_key && !atom_cur_pos_key.compare_exchange_weak(cur_pos_key, key));
		while (key > end_pos_key && !atom_end_pos_key.compare_exchange_weak(end_pos_key, key));

		return Stash_T::_put(bin, Nil());
	}

	bool check_pos(uint64_t& cur_pos_key, uint64_t& final_pos_key, bool keep_going, bool peep) {
		auto end_pos_key = atom_end_pos_key.load();
		if ((!peep && cur_pos_key > final_pos_key) || cur_pos_key > end_pos_key) {
			if (!peep && keep_going) {
				final_pos_key = _CurrentKey();
				if ((!peep && cur_pos_key > final_pos_key) || cur_pos_key > end_pos_key) {
					return false;
				}
			} else {
				return false;
			}
		}
		return true;
	}

public:
	StashSlots(StashSlots&& o) noexcept
		: Stash_T::Stash(std::move(o)),
		  atom_cur_pos_key(std::move(o.atom_cur_pos_key)),
		  atom_end_pos_key(std::move(o.atom_end_pos_key)) { }


	StashSlots()
		: atom_cur_pos_key(_CurrentKey()),
		  atom_end_pos_key(_CurrentKey()) { }

	template <typename T>
	bool next(T** value_ptr, uint64_t final_pos_key=0, bool keep_going=true, bool peep=false) {
		auto cur_pos_key = atom_cur_pos_key.load();

		if (!check_pos(cur_pos_key, final_pos_key, keep_going, peep)) {
			return false;
		}

		do {
			auto cur_pos = get_slot(cur_pos_key);

			L_INFO_HOOK_LOG("StashSlots::next::loop", this, "StashSlots::next()::loop - _Mod:%llu, cur_pos_key:%llu, cur_pos:%llu, final_pos_key:%llu, keep_going:%s, peep:%s", _Mod, cur_pos_key, cur_pos, final_pos_key, keep_going ? "true" : "false", peep ? "true" : "false");

			Bin* ptr = nullptr;
			std::atomic<Bin*>* bin_ptr;
			switch (Stash_T::get_bin(&bin_ptr, cur_pos)) {
				case StashState::Ok: {
					ptr = (*bin_ptr).load();
					if (ptr) {
						if (ptr->val.next(value_ptr, final_pos_key, keep_going, peep)) {
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

			auto new_cur_pos_key = ((cur_pos_key + _Div) / _Div) * _Div;

			if (!check_pos(new_cur_pos_key, final_pos_key, keep_going, peep)) {
				return false;
			}

			if (peep || atom_cur_pos_key.compare_exchange_strong(cur_pos_key, new_cur_pos_key)) {
				cur_pos_key = new_cur_pos_key;
			}

			if (_Ring && !ptr && !peep) {
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

	std::atomic<size_t> atom_cur_pos;
	std::atomic<size_t> atom_end_pos;

public:
	StashValues(StashValues&& o) noexcept
		: Stash_T::Stash(std::move(o)),
		  atom_cur_pos(std::move(o.atom_cur_pos)),
		  atom_end_pos(std::move(o.atom_end_pos)) { }

	StashValues()
		: atom_cur_pos(0),
		  atom_end_pos(0) { }

	template <typename T>
	bool next(T** value_ptr, uint64_t, bool, bool peep) {
		auto cur_pos = atom_cur_pos.load();

		std::atomic<Bin*>* bin_ptr;
		switch (Stash_T::get_bin(&bin_ptr, cur_pos)) {
			case StashState::Ok: {
				auto new_cur_pos = cur_pos + 1;
				if (peep || atom_cur_pos.compare_exchange_strong(cur_pos, new_cur_pos)) {
					cur_pos = new_cur_pos;
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
		auto& bin = Stash_T::spawn_bin(atom_end_pos++);
		Stash_T::_put(bin, std::forward<T>(value));
	}
};
