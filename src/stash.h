/*
 * Copyright (c) 2015-2019 Dubalu LLC
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

#include <array>                 // for std::array
#include <atomic>                // for std::atomic

#include "cassert.h"             // for ASSERT
#include "log.h"                 // for L_NOTHING


// #define L_STASH L_COLLECT
#ifndef L_STASH
#define L_STASH_DEFINED
#define L_STASH L_NOTHING
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

	unsigned long long begin_key;
	unsigned long long end_key;

	std::atomic_ullong atom_first_valid_key;
	std::atomic_ullong atom_last_valid_key;

	StashContext(StashContext&& o) noexcept
		: op(std::move(o.op)),
		  begin_key(std::move(o.begin_key)),
		  end_key(std::move(o.end_key)),
		  atom_first_valid_key(o.atom_first_valid_key.load()),
		  atom_last_valid_key(o.atom_last_valid_key.load()) { }

	explicit StashContext(unsigned long long begin_key)
		: op(Operation::walk),
		  begin_key(begin_key),
		  end_key(begin_key),
		  atom_first_valid_key(begin_key),
		  atom_last_valid_key(begin_key) { }

	bool check(unsigned long long key, unsigned long long limit_key) const {
		if (end_key && key >= end_key) {
			return false;
		}

		if (limit_key && key >= limit_key) {
			return false;
		}

		if (key > atom_last_valid_key.load()) {
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
				return "unknown";
		}
	}

	const char* _col() const noexcept {
		switch (op) {
			case Operation::walk: {
				static constexpr auto clear_color = CLEAR_COLOR;
				return clear_color.c_str();
			}
			case Operation::peep: {
				static constexpr auto dim_grey = DIM_GREY;
				return dim_grey.c_str();
			}
			case Operation::clean: {
				static constexpr auto purple = PURPLE;
				return purple.c_str();
			}
			default: {
				static constexpr auto clear_color = CLEAR_COLOR;
				return clear_color.c_str();
			}
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

		~Data() noexcept {
			try {
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
			} catch (...) {
				L_EXC("Unhandled exception in destructor");
			}
		}

		StashState get(std::atomic<_Tp*>** pptr_atom_ptr, size_t slot, bool spawn) {
			if (!spawn && !atom_next && !atom_chunk) {
				return StashState::StashEmpty;
			}

			auto _data = this;
			if (slot >= _Size) {
				size_t chunk_num = slot / _Size;
				slot = slot % _Size;

				for (size_t c = 0; c < chunk_num; ++c) {
					auto next = _data->atom_next.load();
					if (!next) {
						if (!spawn) {
							return StashState::StashShort;
						}
						auto tmp = new Data;
						if (_data->atom_next.compare_exchange_strong(next, tmp)) {
							next = tmp;
						} else {
							delete tmp;
						}
					}
					_data = next;
				}
			}

			auto chunk = _data->atom_chunk.load();
			if (!chunk) {
				if (!spawn) {
					return StashState::ChunkEmpty;
				}
				auto tmp = new Chunks{{ }};
				if (_data->atom_chunk.compare_exchange_strong(chunk, tmp)) {
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

	unsigned long long get_dec_base_key(unsigned long long key) const {
		return get_base_key(key) - _Div;
	}

	unsigned long long get_end_base_key(unsigned long long key) const {
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
	bool next(StashContext& ctx, T* value_ptr, unsigned long long limit_key) {
		bool found = false;

		auto loop = ctx.check(ctx.begin_key, limit_key);

		while (loop) {
			auto new_first_valid_key = get_inc_base_key(ctx.begin_key);
			auto cur = get_slot(ctx.begin_key);

			L_DEBUG_HOOK("StashSlots::LOOP", "StashSlots::" + CYAN + "LOOP" + CLEAR_COLOR + " - {}_Mod:{}, begin_key:{}, end_key:{}, cur:{}, limit_key:{}, atom_first_valid_key:{}, atom_last_valid_key:{}, op:{}", ctx._col(), _Mod, ctx.begin_key, ctx.end_key, cur, limit_key, ctx.atom_first_valid_key.load(), ctx.atom_last_valid_key.load(), ctx._op());

			std::atomic<_Tp*>* ptr_atom_ptr = nullptr;
			switch (Stash_T::get(&ptr_atom_ptr, cur, false)) {
				case StashState::Ok:
					break;
				case StashState::ChunkEmpty:
					L_STASH("StashSlots::" + SADDLE_BROWN + "EMPTY" + CLEAR_COLOR + " - {}_Mod:{}, begin_key:{}, end_key:{}, cur:{}, limit_key:{}, atom_first_valid_key:{}, atom_last_valid_key:{}, op:{}", ctx._col(), _Mod, ctx.begin_key, ctx.end_key, cur, limit_key, ctx.atom_first_valid_key.load(), ctx.atom_last_valid_key.load(), ctx._op());
					break;
				case StashState::StashShort:
				case StashState::StashEmpty:
					L_STASH("StashSlots::" + SADDLE_BROWN + "BREAK" + CLEAR_COLOR + " - {}_Mod:{}, begin_key:{}, end_key:{}, cur:{}, limit_key:{}, atom_first_valid_key:{}, atom_last_valid_key:{}, op:{}", ctx._col(), _Mod, ctx.begin_key, ctx.end_key, cur, limit_key, ctx.atom_first_valid_key.load(), ctx.atom_last_valid_key.load(), ctx._op());
					goto ret_next;
			}

			if (ptr_atom_ptr) {
				auto& atom_ptr = *ptr_atom_ptr;
				auto ptr = atom_ptr.load();
				if (ptr) {
					auto status = ptr->next(ctx, value_ptr, new_first_valid_key);
					if (status) {
						if (ctx.op == StashContext::Operation::clean) {
							ptr = atom_ptr.exchange(nullptr);
							if (ptr) {
								L_STASH("StashSlots::" + LIGHT_RED + "CLEAR" + CLEAR_COLOR + " - {}_Mod:{}, begin_key:{}, end_key:{}, cur:{}, limit_key:{}, atom_first_valid_key:{}, atom_last_valid_key:{}, op:{}", ctx._col(), _Mod, ctx.begin_key, ctx.end_key, cur, limit_key, ctx.atom_first_valid_key.load(), ctx.atom_last_valid_key.load(), ctx._op());
								delete ptr;
							}
						} else {
							L_STASH("StashSlots::" + FOREST_GREEN + "FOUND" + CLEAR_COLOR + " - {}_Mod:{}, begin_key:{}, end_key:{}, cur:{}, limit_key:{}, atom_first_valid_key:{}, atom_last_valid_key:{}, op:{}", ctx._col(), _Mod, ctx.begin_key, ctx.end_key, cur, limit_key, ctx.atom_first_valid_key.load(), ctx.atom_last_valid_key.load(), ctx._op());
							found = true;
							goto ret_next;
						}
					}
				}
			}

			loop = ctx.check(new_first_valid_key, limit_key);

			if (loop) {
				ctx.begin_key = new_first_valid_key;
			}
		}

		L_STASH("StashSlots::" + SADDLE_BROWN + "MISSING" + CLEAR_COLOR + " - {}_Mod:{}, begin_key:{}, end_key:{}, cur:{}, limit_key:{}, atom_first_valid_key:{}, atom_last_valid_key:{}, op:{}", ctx._col(), _Mod, ctx.begin_key, ctx.end_key, get_slot(ctx.begin_key), limit_key, ctx.atom_first_valid_key.load(), ctx.atom_last_valid_key.load(), ctx._op());

	ret_next:
		if (ctx.op != StashContext::Operation::peep) {
			if (!found) {
				unsigned long long new_cur_key;
				if (!limit_key || (ctx.end_key && ctx.end_key < limit_key)) {
					ASSERT(ctx.end_key);
					new_cur_key = get_base_key(ctx.end_key);
				} else {
					new_cur_key = get_base_key(limit_key);
				}
				if (new_cur_key > ctx.begin_key) {
					ctx.begin_key = new_cur_key;
				}
			}
			auto new_first_valid_key = get_dec_base_key(ctx.begin_key);
			auto first_valid_key = ctx.atom_first_valid_key.load();
			while (new_first_valid_key > first_valid_key && !ctx.atom_first_valid_key.compare_exchange_weak(first_valid_key, new_first_valid_key));
		}

		return found;
	}

	template <typename T>
	bool next(StashContext& ctx, T* value_ptr) {
		return next(ctx, value_ptr, 0);
	}

	template<typename... Args>
	void put(StashContext& ctx, unsigned long long key, Args&&... args) {
		auto slot = get_slot(key);
		L_STASH("StashSlots::" + PURPLE + "PUT" + CLEAR_COLOR + " - {}_Mod:{}, end_key:{}, key:{}, slot:{}, begin_key:{}, cur:{}, atom_first_valid_key:{}, atom_last_valid_key:{}, op:{}", ctx._col(), _Mod, ctx.end_key, key, slot, ctx.begin_key, get_slot(ctx.begin_key), ctx.atom_first_valid_key.load(), ctx.atom_last_valid_key.load(), ctx._op());

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
		if (key >= get_end_base_key(ctx.atom_first_valid_key.load())) {
			throw std::out_of_range("stash overlow");
		}

		put(ctx, key, std::forward<Args>(args)...);

		auto first_valid_key = ctx.atom_first_valid_key.load();
		auto last_valid_key = ctx.atom_last_valid_key.load();
		L_STASH("StashSlots::" + LIGHT_PURPLE + "ADD" + CLEAR_COLOR + " - _Mod:{}, key:{}, atom_first_valid_key:{}, atom_last_valid_key:{}", _Mod, key, first_valid_key, last_valid_key);
		while (key < first_valid_key && !ctx.atom_first_valid_key.compare_exchange_weak(first_valid_key, key));
		while (key > last_valid_key && !ctx.atom_last_valid_key.compare_exchange_weak(last_valid_key, key));
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

			L_DEBUG_HOOK("StashValues::LOOP", "StashValues::" + LIGHT_SKY_BLUE + "LOOP" + CLEAR_COLOR + " - {}cur:{}, cur:{}, atom_end:{}, op:{}", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());

			std::atomic<_Tp*>* ptr_atom_ptr = nullptr;
			switch (Stash_T::get(&ptr_atom_ptr, cur, false)) {
				case StashState::Ok:
					break;
				case StashState::ChunkEmpty:
					L_STASH("StashValues::" + SADDLE_BROWN + "EMPTY" + CLEAR_COLOR + " - {}cur:{}, cur:{}, atom_end:{}, op:{}", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());
					break;
				case StashState::StashShort:
				case StashState::StashEmpty:
					L_STASH("StashValues::" + SADDLE_BROWN + "BREAK" + CLEAR_COLOR + " - {}cur:{}, cur:{}, atom_end:{}, op:{}", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());
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
							L_STASH("StashValues::" + LIGHT_GREEN + "FOUND" + CLEAR_COLOR + " - {}cur:{}, cur:{}, atom_end:{}, op:{}", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());
							if (value_ptr) {
								*value_ptr = *ptr;
							}
							returning = true;
						}
					}
					if (ctx.op != StashContext::Operation::peep) {
						ptr = atom_ptr.exchange(nullptr);
						if (ptr) {
							L_STASH("StashValues::" + LIGHT_RED + "CLEAR" + CLEAR_COLOR + " - {}cur:{}, cur:{}, atom_end:{}, op:{}", ctx._col(), cur, (ctx.op == StashContext::Operation::clean) ? clean_cur : walk_cur, atom_end.load(), ctx._op());
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
	void put([[maybe_unused]] StashContext& ctx, unsigned long long, Args&&... args) {
		auto slot = atom_end++;
		L_STASH("StashValues::" + LIGHT_PURPLE + "PUT" + CLEAR_COLOR + " - {}slot:{}, atom_end:{}, op:{}", ctx._col(), slot, atom_end.load(), ctx._op());

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
