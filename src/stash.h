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


enum class StashState : uint8_t {
	Ok,
	EmptyBin,
	EmptyChunk,
	OutOfRange,
	EmptyStash,
};


template <typename _Tp, size_t _Size>
class Stash {
protected:
	struct Bin {
		_Tp val;

		Bin() = default;

		Bin(unsigned long long key)
			: val(_Tp(key)) { }

		Bin(const _Tp& val_)
			: val(val_) { }

		Bin(_Tp&& val_) noexcept
			: val(std::move(val_)) { }
	};

	class Data {
		using Chunks = std::array<std::atomic<Bin*>, _Size>;
		std::atomic<Chunks*> atom_chunk;
		std::atomic<Data*> atom_next;

	public:
		Data()
			: atom_chunk(nullptr),
			  atom_next(nullptr) { }

		Data(Data&& o) noexcept
			: atom_chunk(o.atom_chunk.load()),
			  atom_next(o.atom_next.load()) { }

		~Data() {
			clear();
		}

		void clear() {
			Data* next;
			do {
				next = atom_next.load();
				if (next) {
					auto next_next = next->atom_next.load();
					while (!atom_next.compare_exchange_weak(next, next_next)) {
						if (next) {
							next_next = next->atom_next.load();
						} else {
							next_next = nullptr;
						}
					}
					if (next) {
						auto old_next_next = next_next;
						while (!next->atom_next.compare_exchange_weak(next_next, nullptr));
						while (!atom_next.compare_exchange_weak(old_next_next, next_next));
						delete next;
					}
				}
			} while (next);

			auto chunk = atom_chunk.load();
			while (!atom_chunk.compare_exchange_weak(chunk, nullptr));
			if (chunk) {
				for (auto& atom_bin : *chunk) {
					auto bin_ptr = atom_bin.load();
					while (!atom_bin.compare_exchange_weak(bin_ptr, nullptr));
					if (bin_ptr) {
						delete bin_ptr;
					}
				}
				delete chunk;
			}
		}

		StashState bin(std::atomic<Bin*>** bin_pptr, size_t slot, bool spawn) {
			if (!spawn && !atom_next && !atom_chunk) {
				return StashState::EmptyStash;
			}

			size_t chunk_num = 0;
			if (slot >= _Size) {
				chunk_num = slot / _Size;
				slot = slot % _Size;
			}

			auto data = this;
			for (size_t c = 0; c < chunk_num; ++c) {
				auto next = data->atom_next.load();
				if (!next) {
					if (!spawn) {
						return StashState::OutOfRange;
					}
					auto tmp = new Data;
					if (data->atom_next.compare_exchange_strong(next, tmp)) {
						next = tmp;
					} else {
						delete tmp;
					}
				}
				data = next;
			}

			auto chunk = data->atom_chunk.load();
			if (!chunk) {
				if (!spawn) {
					return StashState::EmptyChunk;
				}
				auto tmp = new Chunks{ {} };
				if (data->atom_chunk.compare_exchange_strong(chunk, tmp)) {
					chunk = tmp;
				} else {
					delete tmp;
				}
			}

			auto& atomic_bin = (*chunk)[slot];
			if (!spawn && !atomic_bin.load()) {
				return StashState::EmptyBin;
			}

			*bin_pptr = &atomic_bin;
			return StashState::Ok;
		}
	};

	Data data;

public:
	Stash(Stash&& o) noexcept
		: data(std::move(o.data)) { }

	Stash() = default;

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


template <unsigned long long(*_CurrentKey)()>
struct StashContext {
	unsigned long long cur_key;
	unsigned long long current_key;
	std::atomic_ullong atom_cur_key;
	std::atomic_ullong atom_end_key;

	StashContext(StashContext<_CurrentKey>&& o) noexcept
		: cur_key(std::move(o.cur_key)),
		  current_key(std::move(o.current_key)),
		  atom_cur_key(o.atom_cur_key.load()),
		  atom_end_key(o.atom_end_key.load()) { }

	StashContext()
		: cur_key(_CurrentKey()),
		  current_key(cur_key),
		  atom_cur_key(cur_key),
		  atom_end_key(cur_key) { }
};


template <typename _Tp, size_t _Size, unsigned long long(*_CurrentKey)(), unsigned long long _Div, unsigned long long _Mod, bool _Ring>
class StashSlots : public Stash<_Tp, _Size> {
	using Stash_T = Stash<_Tp, _Size>;
	using Bin = typename Stash_T::Bin;

	StashContext<_CurrentKey>& ctx;

	unsigned long long get_base_key(unsigned long long key) {
		return (key / _Div) * _Div;
	}

	unsigned long long get_inc_base_key(unsigned long long key) {
		return get_base_key(key + _Div);
	}

	size_t get_slot(unsigned long long key) {
		return (key / _Div) % _Mod;
	}

	bool check(unsigned long long final_key) {
		if (ctx.current_key && ctx.cur_key >= ctx.current_key) {
			return false;
		}

		if (final_key && ctx.cur_key > final_key) {
			return false;
		}

		if (ctx.cur_key > ctx.atom_end_key.load()) {
			return false;
		}

		return true;
	}

public:
	StashSlots(StashSlots&& o) noexcept
		: Stash_T::Stash(std::move(o)),
		  ctx(o.ctx) { }


	StashSlots(StashContext<_CurrentKey>& pos_)
		: ctx(pos_) { }

	template <typename T>
	bool next(T** value_ptr, unsigned long long final_key=0, bool peep=false) {
		auto loop = check(final_key);

		while (loop) {
			auto new_cur_key = get_inc_base_key(ctx.cur_key);

			auto cur = get_slot(ctx.cur_key);

			L_INFO_HOOK_LOG("StashSlots::LOOP", this, "StashSlots::" CYAN "LOOP" NO_COL " - %s_Mod:%llu, cur_key:%llu, cur:%llu, final_key:%llu, end_key:%llu, peep:%s", peep ? DARK_GREY : NO_COL, _Mod, ctx.cur_key, cur, final_key, ctx.atom_end_key.load(), peep ? "true" : "false");

			Bin* ptr = nullptr;
			std::atomic<Bin*>* bin_ptr;
			switch (Stash_T::get_bin(&bin_ptr, cur)) {
				case StashState::Ok: {
					ptr = (*bin_ptr).load();
					if (ptr) {
						if (ptr->val.next(value_ptr, new_cur_key, peep)) {
							L_INFO_HOOK_LOG("StashSlots::FOUND", this, "StashSlots::" GREEN "FOUND" NO_COL " - %s_Mod:%llu, cur_key:%llu, cur:%llu, final_key:%llu, end_key:%llu, peep:%s", peep ? DARK_GREY : NO_COL, _Mod, ctx.cur_key, cur, final_key, ctx.atom_end_key.load(), peep ? "true" : "false");
							return true;
						}
					}
					break;
				}
				case StashState::OutOfRange:
				case StashState::EmptyStash:
					L_INFO_HOOK_LOG("StashSlots::BREAK", this, "StashSlots::" YELLOW "BREAK" NO_COL " - %s_Mod:%llu, cur_key:%llu, cur:%llu, final_key:%llu, end_key:%llu, peep:%s", peep ? DARK_GREY : NO_COL, _Mod, ctx.cur_key, cur, final_key, ctx.atom_end_key.load(), peep ? "true" : "false");
					return false;
				default:
					break;
			}

			loop = check(final_key);

			if (!peep && ptr) {
				L_INFO_HOOK_LOG("StashSlots::CLEAR", this, "StashSlots::" RED "CLEAR" NO_COL " - %s_Mod:%llu, cur_key:%llu, cur:%llu, final_key:%llu, end_key:%llu, peep:%s", peep ? DARK_GREY : NO_COL, _Mod, ctx.cur_key, cur, final_key, ctx.atom_end_key.load(), peep ? "true" : "false");
				// ptr->val.clear();
			}

			if (peep || ctx.atom_cur_key.compare_exchange_strong(ctx.cur_key, new_cur_key)) {
				ctx.cur_key = new_cur_key;
			}
		}

		L_INFO_HOOK_LOG("StashSlots::MISSING", this, "StashSlots::" YELLOW "MISSING" NO_COL " - %s_Mod:%llu, cur_key:%llu, cur:%llu, final_key:%llu, end_key:%llu, peep:%s", peep ? DARK_GREY : NO_COL, _Mod, ctx.cur_key, get_slot(ctx.cur_key), final_key, ctx.atom_end_key.load(), peep ? "true" : "false");
		return false;
	}

	template <typename T>
	void add(T&& value, unsigned long long key) {
		auto slot = get_slot(key);
		auto cur_key = ctx.atom_cur_key.load();
		auto end_key = ctx.atom_end_key.load();

		auto& bin = Stash_T::spawn_bin(slot);
		while (key < cur_key && !ctx.atom_cur_key.compare_exchange_weak(cur_key, key));
		while (key > end_key && !ctx.atom_end_key.compare_exchange_weak(end_key, key));

		L_INFO_HOOK_LOG("StashSlots::ADD", this, "StashSlots::" MAGENTA "ADD" NO_COL " - _Mod:%llu, key:%llu, slot:%llu, cur_key:%llu, end_key:%llu", _Mod, key, slot, cur_key, end_key);
		Stash_T::_put(bin, _Tp(ctx)).add(std::forward<T>(value), key);
	}
};


template <typename _Tp, size_t _Size, unsigned long long(*_CurrentKey)()>
class StashValues : public Stash<_Tp, _Size> {
	using Stash_T = Stash<_Tp, _Size>;
	using Bin = typename Stash_T::Bin;

	std::atomic_size_t atom_cur;
	std::atomic_size_t atom_end;

public:
	StashValues(StashValues&& o) noexcept
		: Stash_T::Stash(std::move(o)),
		  atom_cur(o.atom_cur.load()),
		  atom_end(o.atom_end.load()) { }

	StashValues(StashContext<_CurrentKey>&)
		: atom_cur(0),
		  atom_end(0) { }

	template <typename T>
	bool is_empty(const std::shared_ptr<T> ptr) {
		return !ptr;
	}

	template <typename T, typename = std::enable_if_t<not std::is_same<_Tp, std::shared_ptr<T>>::value>>
	bool is_empty(_Tp) {
		return false;
	}

	template <typename T>
	bool next(T** value_ptr, unsigned long long, bool peep) {
		auto cur = atom_cur.load();

		do {
			L_INFO_HOOK_LOG("StashValues::LOOP", this, "StashValues::" LIGHT_CYAN "LOOP" NO_COL " - %scur:%llu, peep:%s", peep ? DARK_GREY : NO_COL, cur, peep ? "true" : "false");

			std::atomic<Bin*>* bin_ptr;
			switch (Stash_T::get_bin(&bin_ptr, cur)) {
				case StashState::Ok: {
					auto ptr = (*bin_ptr).load();
					if (ptr) {
						auto new_cur = cur + 1;
						if (peep || atom_cur.compare_exchange_strong(cur, new_cur)) {
							cur = new_cur;
						}
						if (!is_empty(ptr->val)) {
							L_INFO_HOOK_LOG("StashValues::FOUND", this, "StashValues::" LIGHT_GREEN "FOUND" NO_COL " - %scur:%llu, peep:%s", peep ? DARK_GREY : NO_COL, cur, peep ? "true" : "false");
							*value_ptr = &ptr->val;
							return true;
						}
						continue;
					}
				}
				default:
					break;
			}
			return false;
		} while(true);
	}

	template <typename T>
	bool next(T** value_ptr, bool peep=false) {
		return next(value_ptr, 0, peep);
	}

	template <typename T>
	void add(T&& value, unsigned long long) {
		auto slot = atom_end.load();
		while (!atom_end.compare_exchange_weak(slot, slot + 1));
		auto& bin = Stash_T::spawn_bin(slot);

		L_INFO_HOOK_LOG("StashValues::ADD", this, "StashValues::" LIGHT_MAGENTA "ADD" NO_COL " - slot:%llu", slot);
		Stash_T::_put(bin, std::forward<T>(value));
	}
};
