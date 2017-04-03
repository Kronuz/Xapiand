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


#ifndef L_STASH
#define L_STASH_DEFINED
#define L_STASH L_TEST
#endif


enum class StashState : uint8_t {
	Ok,
	ChunkEmpty,
	StashShort,
	StashEmpty,
};


struct StashContext {
	enum class Operation : uint8_t {
		walk,
		peep,
		clean,
	};

	Operation op;

	unsigned long long cur_key;
	std::atomic_ullong atom_cur_key;
	std::atomic_ullong atom_end_key;

	unsigned long long current_key;

	StashContext(StashContext&& o) noexcept
		: cur_key(std::move(o.cur_key)),
		  atom_cur_key(o.atom_cur_key.load()),
		  atom_end_key(o.atom_end_key.load()),
		  current_key(std::move(o.current_key)) { }

	StashContext(unsigned long long cur_key_)
		: cur_key(cur_key_),
		  atom_cur_key(cur_key),
		  atom_end_key(cur_key),
		  current_key(cur_key_) { }

	bool check(unsigned long long key, unsigned long long final_key) const {
		if (current_key && key >= current_key) {
			return false;
		}

		if (final_key && key >= final_key) {
			return false;
		}

		if (key > atom_end_key.load()) {
			return false;
		}

		return true;
	}

	const char* _op() const noexcept {
		switch (op) {
			case Operation::walk:
				return "walk";
			case Operation::peep:
				return "peep";
			case Operation::clean:
				return "clean";
			default:
				return "Operation::UNKNOWN";
		}
	}

	const char* _col() const noexcept {
		switch (op) {
			case Operation::walk:
				return NO_COL;
			case Operation::peep:
				return DARK_GREY;
			case Operation::clean:
				return MAGENTA;
			default:
				return NO_COL;
		}
	}
};


template <typename _Tp, size_t _Size>
class Stash {
protected:
	class Data {
		using Chunks = std::array<std::atomic<_Tp*>, _Size>;
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
			auto next = atom_next.exchange(nullptr);
			while (next) {
				auto next_next = next->atom_next.exchange(nullptr);
				delete next;
				next = next_next;
			}

			auto chunk = atom_chunk.exchange(nullptr);
			if (chunk) {
				for (auto& atom_ptr : *chunk) {
					auto ptr = atom_ptr.exchange(nullptr);
					if (ptr) {
						delete ptr;
					}
				}
				delete chunk;
			}
		}

		StashState get(std::atomic<_Tp*>** pptr_atom_ptr, size_t slot, bool spawn) {
			if (!spawn && !atom_next && !atom_chunk) {
				return StashState::StashEmpty;
			}

			auto data = this;
			size_t chunk_num = 0;
			if (slot >= _Size) {
				chunk_num = slot / _Size;
				slot = slot % _Size;

				for (size_t c = 0; c < chunk_num; ++c) {
					auto next = data->atom_next.load();
					if (!next) {
						if (!spawn) {
							return StashState::StashShort;
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
			}

			auto chunk = data->atom_chunk.load();
			if (!chunk) {
				if (!spawn) {
					return StashState::ChunkEmpty;
				}
				auto tmp = new Chunks{{ }};
				if (data->atom_chunk.compare_exchange_strong(chunk, tmp)) {
					chunk = tmp;
				} else {
					delete tmp;
				}
			}

			auto& atom_ptr = (*chunk)[slot];

			*pptr_atom_ptr = &atom_ptr;
			return StashState::Ok;
		}
	};

	Data data;

public:
	Stash(Stash&& o) noexcept
		: data(std::move(o.data)) { }

	Stash() = default;

	StashState get(std::atomic<_Tp*>** pptr_atom_ptr, size_t slot, bool spawn) {
		/* If spawn is false, get() could fail with:
		 *   StashState::StashEmpty
		 *   StashState::ChunkEmpty
		 *   StashState::StashShort
		 */
		return data.get(pptr_atom_ptr, slot, spawn);
	}
};


template <typename _Tp, size_t _Size, unsigned long long(*_CurrentKey)(), unsigned long long _Div, unsigned long long _Mod, bool _Ring>
class StashSlots : public Stash<_Tp, _Size> {
	using Stash_T = Stash<_Tp, _Size>;

	unsigned long long get_base_key(unsigned long long key) const {
		return (key / _Div) * _Div;
	}

	unsigned long long get_inc_base_key(unsigned long long key) const {
		return get_base_key(key) + _Div;
	}

	unsigned long long get_final_key(unsigned long long key) const {
		return get_base_key(key) + (_Div * _Mod);
	}

	size_t get_slot(unsigned long long key) const {
		return (key / _Div) % _Mod;
	}

public:
	StashSlots(StashSlots&& o) noexcept
		: Stash_T::Stash(std::move(o)) { }

	StashSlots() = default;

	template <typename T>
	bool next(StashContext& ctx, T* value_ptr, unsigned long long final_key) {
		auto loop = ctx.check(ctx.cur_key, final_key);

		while (loop) {
			auto new_cur_key = get_inc_base_key(ctx.cur_key);
			auto cur = get_slot(ctx.cur_key);

			L_INFO_HOOK_LOG("StashSlots::LOOP", this, "StashSlots::" CYAN "LOOP" NO_COL " - %s_Mod:%llu, current_key:%llu, cur_key:%llu, cur:%llu, final_key:%llu, atom_cur_key:%llu, atom_end_key:%llu, op:%s", ctx._col(), _Mod, ctx.current_key, ctx.cur_key, cur, final_key, ctx.atom_cur_key.load(), ctx.atom_end_key.load(), ctx._op());

			std::atomic<_Tp*>* ptr_atom_ptr = nullptr;
			switch (Stash_T::get(&ptr_atom_ptr, cur, false)) {
				case StashState::Ok:
					break;
				case StashState::ChunkEmpty:
					L_STASH(this, "StashSlots::" YELLOW "EMPTY" NO_COL " - %s_Mod:%llu, current_key:%llu, cur_key:%llu, cur:%llu, final_key:%llu, atom_cur_key:%llu, atom_end_key:%llu, op:%s", ctx._col(), _Mod, ctx.current_key, ctx.cur_key, cur, final_key, ctx.atom_cur_key.load(), ctx.atom_end_key.load(), ctx._op());
					break;
				case StashState::StashShort:
				case StashState::StashEmpty:
					L_STASH(this, "StashSlots::" YELLOW "BREAK" NO_COL " - %s_Mod:%llu, current_key:%llu, cur_key:%llu, cur:%llu, final_key:%llu, atom_cur_key:%llu, atom_end_key:%llu, op:%s", ctx._col(), _Mod, ctx.current_key, ctx.cur_key, cur, final_key, ctx.atom_cur_key.load(), ctx.atom_end_key.load(), ctx._op());
					return false;
			}

			if (ptr_atom_ptr) {
				auto& atom_ptr = *ptr_atom_ptr;
				auto ptr = atom_ptr.load();
				if (ptr) {
					auto status = ptr->next(ctx, value_ptr, new_cur_key);
					if (status) {
						if (ctx.op == StashContext::Operation::clean) {
							auto ptr = atom_ptr.exchange(nullptr);
							if (ptr) {
								L_STASH(this, "StashSlots::" LIGHT_RED "CLEAR" NO_COL " - %s_Mod:%llu, current_key:%llu, cur_key:%llu, cur:%llu, final_key:%llu, atom_cur_key:%llu, atom_end_key:%llu, op:%s", ctx._col(), _Mod, ctx.current_key, ctx.cur_key, cur, final_key, ctx.atom_cur_key.load(), ctx.atom_end_key.load(), ctx._op());
								delete ptr;
							}
						} else {
							L_STASH(this, "StashSlots::" GREEN "FOUND" NO_COL " - %s_Mod:%llu, current_key:%llu, cur_key:%llu, cur:%llu, final_key:%llu, atom_cur_key:%llu, atom_end_key:%llu, op:%s", ctx._col(), _Mod, ctx.current_key, ctx.cur_key, cur, final_key, ctx.atom_cur_key.load(), ctx.atom_end_key.load(), ctx._op());
							return true;
						}
					}
				}
			}

			loop = ctx.check(new_cur_key, final_key);

			if (loop) {
				if (ctx.op == StashContext::Operation::peep || ctx.atom_cur_key.compare_exchange_strong(ctx.cur_key, new_cur_key)) {
					ctx.cur_key = new_cur_key;
				}
			}
		}

		L_STASH(this, "StashSlots::" YELLOW "MISSING" NO_COL " - %s_Mod:%llu, current_key:%llu, cur_key:%llu, cur:%llu, final_key:%llu, atom_cur_key:%llu, atom_end_key:%llu, op:%s", ctx._col(), _Mod, ctx.current_key, ctx.cur_key, get_slot(ctx.cur_key), final_key, ctx.atom_cur_key.load(), ctx.atom_end_key.load(), ctx._op());
		return false;
	}

	template <typename T>
	bool next(StashContext& ctx, T* value_ptr) {
		return next(ctx, value_ptr, 0);
	}

	template<typename... Args>
	void put(StashContext& ctx, unsigned long long key, Args&&... args) {
		auto slot = get_slot(key);
		L_STASH(this, "StashSlots::" MAGENTA "PUT" NO_COL " - %s_Mod:%llu, current_key:%llu, key:%llu, slot:%llu, cur_key:%llu, cur:%llu, atom_cur_key:%llu, atom_end_key:%llu, op:%s", ctx._col(), _Mod, ctx.current_key, key, slot, ctx.cur_key, get_slot(ctx.cur_key), ctx.atom_cur_key.load(), ctx.atom_end_key.load(), ctx._op());

		std::atomic<_Tp*>* ptr_atom_ptr;
		Stash_T::get(&ptr_atom_ptr, slot, true);

		auto& atom_ptr = *ptr_atom_ptr;
		auto ptr = atom_ptr.load();
		if (!ptr) {
			auto tmp = new _Tp();
			if (atom_ptr.compare_exchange_strong(ptr, tmp)) {
				ptr = tmp;
			} else {
				delete tmp;
			}
		}

		ptr->put(ctx, key, std::forward<Args>(args)...);
	}

	template<typename... Args>
	void add(StashContext& ctx, unsigned long long key, Args&&... args) {
		if (key >= get_final_key(ctx.atom_cur_key)) {
			throw std::out_of_range("stash overlow");
		}

		put(ctx, key, std::forward<Args>(args)...);

		auto cur_key = ctx.atom_cur_key.load();
		auto end_key = ctx.atom_end_key.load();
		L_STASH(this, "StashSlots::" LIGHT_MAGENTA "ADD" NO_COL " - _Mod:%llu, key:%llu, atom_cur_key:%llu, atom_end_key:%llu", _Mod, key, cur_key, end_key);
		while (key < cur_key && !ctx.atom_cur_key.compare_exchange_weak(cur_key, key));
		while (key > end_key && !ctx.atom_end_key.compare_exchange_weak(end_key, key));
	}
};


template <typename _Tp, size_t _Size, unsigned long long(*_CurrentKey)()>
class StashValues : public Stash<_Tp, _Size> {
	using Stash_T = Stash<_Tp, _Size>;

	size_t walk_cur;
	size_t clean_cur;
	std::atomic_size_t atom_end;

public:
	StashValues(StashValues&& o) noexcept
		: Stash_T::Stash(std::move(o)),
		  walk_cur(std::move(o.walk_cur)),
		  clean_cur(std::move(o.clean_cur)),
		  atom_end(o.atom_end.load()) { }

	StashValues()
		: walk_cur(0),
		  clean_cur(0),
		  atom_end(0) { }

	template <typename T>
	bool next(StashContext& ctx, T* value_ptr, unsigned long long) {
		auto cur = (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur;

		auto loop = cur < ((ctx.op == StashContext::Operation::clean) ? walk_cur : atom_end.load());

		while (loop) {
			auto new_cur = cur + 1;

			L_INFO_HOOK_LOG("StashValues::LOOP", this, "StashValues::" LIGHT_CYAN "LOOP" NO_COL " - %scur:%llu, cur:%llu, atom_end:%llu, op:%s", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());

			std::atomic<_Tp*>* ptr_atom_ptr = nullptr;
			switch (Stash_T::get(&ptr_atom_ptr, cur, false)) {
				case StashState::Ok:
					break;
				case StashState::ChunkEmpty:
					L_STASH(this, "StashValues::" YELLOW "EMPTY" NO_COL " - %scur:%llu, cur:%llu, atom_end:%llu, op:%s", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());
					break;
				case StashState::StashShort:
				case StashState::StashEmpty:
					L_STASH(this, "StashValues::" YELLOW "BREAK" NO_COL " - %scur:%llu, cur:%llu, atom_end:%llu, op:%s", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());
					return false;
			}

			loop = new_cur < ((ctx.op == StashContext::Operation::clean) ? walk_cur : atom_end.load());

			if (loop) {
				switch (ctx.op) {
					case StashContext::Operation::peep:
						cur = new_cur;
						break;
					case StashContext::Operation::walk:
						cur = walk_cur = new_cur;
						break;
					case StashContext::Operation::clean:
						cur = clean_cur = new_cur;
						break;
				}
			}

			if (ptr_atom_ptr) {
				auto& atom_ptr = *ptr_atom_ptr;
				auto ptr = atom_ptr.load();
				if (ptr) {
					bool returning = false;
					if (ctx.op != StashContext::Operation::clean) {
						if (*ptr && **ptr) {
							L_STASH(this, "StashValues::" LIGHT_GREEN "FOUND" NO_COL " - %scur:%llu, cur:%llu, atom_end:%llu, op:%s", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());
							if (value_ptr) {
								*value_ptr = *ptr;
							}
							returning = true;
						}
					}
					if (ctx.op != StashContext::Operation::peep) {
						auto ptr = atom_ptr.exchange(nullptr);
						if (ptr) {
							L_STASH(this, "StashValues::" LIGHT_RED "CLEAR" NO_COL " - %scur:%llu, cur:%llu, atom_end:%llu, op:%s", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());
							delete ptr;
						}
					}
					if (returning) {
						return true;
					}
				}
			}
		}

		return false;
	}

	template<typename... Args>
	void put(StashContext& ctx, unsigned long long, Args&&... args) {
		auto slot = atom_end++;
		L_STASH(this, "StashValues::" LIGHT_MAGENTA "PUT" NO_COL " - %sslot:%llu, atom_end:%llu, op:%s", ctx._col(), slot, atom_end.load(), ctx._op()); (void)ctx;

		std::atomic<_Tp*>* ptr_atom_ptr;
		Stash_T::get(&ptr_atom_ptr, slot, true);

		auto& atom_ptr = *ptr_atom_ptr;
		auto ptr = atom_ptr.load();
		if (!ptr) {
			auto tmp = new _Tp(std::forward<Args>(args)...);
			if (atom_ptr.compare_exchange_strong(ptr, tmp)) {
				ptr = tmp;
			} else {
				delete tmp;
			}
		}
	}
};


#ifdef L_STASH_DEFINED
#undef L_STASH_DEFINED
#undef L_STASH
#endif
