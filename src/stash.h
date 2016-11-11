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
#include <memory>    // for shared_ptr


class StashException { };
class StashContinue : StashException { };
class StashEmpty : StashException { };
class StashEmptyChunk : StashException { };
class StashOutOfRange : StashException { };
class StashSlotsEmpty : StashException { };
class StashSlotsEmptyChunk : StashException { };
class StashSlotsOutOfRange : StashException { };
class StashValuesEmpty : StashException { };
class StashValuesEmptyChunk : StashException { };
class StashValuesOutOfRange : StashException { };


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
	private:
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
				for (auto & a : *s) {
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

		std::atomic<Bin*>& bin(size_t slot, bool spawn) {
			if (!spawn && !next && ! chunk) {
				throw StashEmpty();
			}

			size_t chunk = 0;
			if (slot >= _Size) {
				chunk = slot / _Size;
				slot = slot % _Size;
			}

			auto data = this;
			for (size_t c = 0; c < chunk; ++c) {
				auto ptr = data->next.load();
				if (!ptr) {
					if (!spawn) {
						throw StashOutOfRange();
					}
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
			if (!ptr) {
				if (!spawn) {
					throw StashEmptyChunk();
				}
				auto tmp = new Chunks{ {} };
				if (data->chunk.compare_exchange_strong(ptr, tmp)) {
					ptr = tmp;
				} else {
					delete tmp;
				}
			}

			return (*ptr)[slot];
		}
	};

	Data data;
	std::atomic_size_t pos;

public:
	Stash(Stash&& o) noexcept
		: data(std::move(o.data)),
		  pos(std::move(o.pos)) { }

	Stash(size_t pos_)
		: pos(pos_) { }

	~Stash() {
		clear();
	}

	void clear() {
		data.clear();
	}

	std::atomic<Bin*>& get_bin(size_t slot) {
		/* This could fail with:
		 *   StashEmpty
		 *   StashEmptyChunk
		 *   StashOutOfRange
		 */
		return Stash::data.bin(slot, false);
	}

	std::atomic<Bin*>& spawn_bin(size_t slot) {
		/* This shouldn't fail. */
		return Stash::data.bin(slot, true);
	}

	template <typename T>
	_Tp& _put(std::atomic<Bin*>& bin, T&& value) {
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
	using Stash = Stash<_Tp, _Size>;
	using Bin = typename Stash::Bin;
	using Nil = typename Stash::Nil;

	size_t get_slot(uint64_t key) {
		auto slot = ((key + _Sum) / _Div) % _Mod;
		// std::cout << "size:" << _Size << ", div:" << _Div << ", mod:" << _Mod << ", key:" << key << ", slot:" << slot << std::endl;
		return slot;
	}

	_Tp& put(uint64_t key) {
		auto slot = get_slot(key);

		auto& bin = Stash::spawn_bin(slot);

		if (!_Ring) {
			// This is not a ring, any new (older) item added could move the position
			auto pos = Stash::pos.load();
			while (slot < pos && !Stash::pos.compare_exchange_weak(pos, slot));
		}

		return Stash::_put(bin, Nil());
	}

	bool check_pos(size_t pos, size_t initial_pos, size_t final_pos, size_t last_pos) {
		/* There are basically two cases where pos is valid (v) based in the
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

		if (!_Ring && pos == 0) {
			return false;
		}

		if (final_pos >= initial_pos) {
			if (pos < initial_pos || pos > final_pos) {
				// We're beyond final_pos, stop
				return false;
			}
		} else {
			if (pos < initial_pos) {
				if (pos > final_pos) {
					// We're beyond final_pos, stop
					return false;
				} else if (pos == final_pos && final_pos == last_pos) {
					return false;
				}
			}
		}

		return true;
	}

	size_t increment_pos(size_t pos, bool keep_going, size_t initial_pos, size_t final_pos, size_t last_pos) {
		auto new_pos = (pos + 1) % _Mod;

		if (!check_pos(new_pos, initial_pos, final_pos, last_pos)) {
			if (keep_going) {
				final_pos = get_slot(_CurrentKey());
				if (!check_pos(new_pos, initial_pos, final_pos, last_pos)) {
					throw StashContinue();
				}
			} else {
				throw StashContinue();
			}
		}

		Stash::pos.compare_exchange_strong(pos, new_pos);

		return final_pos;
	}

	Bin* current_bin_ptr(size_t pos) {
		try {
			auto& bin = Stash::get_bin(pos);
			auto ptr = bin.load();
			if (!ptr) {
				throw StashSlotsEmptyChunk();
			}
			return ptr;
		} catch (const StashEmpty&) {
			throw StashSlotsEmpty();
		} catch (const StashEmptyChunk&) {
			throw StashSlotsEmptyChunk();
		} catch (const StashOutOfRange&) {
			throw StashSlotsOutOfRange();
		}
	}

public:
	StashSlots(StashSlots&& o) noexcept
		: Stash::Stash(std::move(o)) { }

	StashSlots()
		: Stash::Stash(0) { }

	StashSlots(uint64_t key)
		: Stash::Stash(get_slot(key)) { }

	auto next(bool final, uint64_t final_key, bool keep_going) {
		// std::cout << "\tTimeStash<_Mod=" << _Mod << ">::next(" << (final ? "true" : "false") <<", " << final_key << ", " << (keep_going ? "true" : "false") << ")" << std::endl;

		auto pos = Stash::pos.load();

		// Always start one slot earlier (just in case).
		if (_Ring) {
			while (!Stash::pos.compare_exchange_weak(pos, (pos - 1) % _Mod));
		} else {
			while (pos > 0 && !Stash::pos.compare_exchange_weak(pos, pos - 1));
		}

		keep_going &= final;
		if (keep_going) {
			final_key = _CurrentKey();
		}
		auto initial_pos = pos;
		auto last_pos = static_cast<size_t>((initial_pos + _Mod - 1) % _Mod);
		auto final_pos = final ? get_slot(final_key) : last_pos;

		do {
			Bin* ptr = nullptr;
			try {
				// std::cout << "\t\tTimeStash<_Mod=" << _Mod << "> pos:" << pos << ", initial_pos:" << initial_pos << ", final:" << (final ? "true" : "false") << ", final_pos:" << final_pos << ", last_pos:" << last_pos << std::endl;
				ptr = current_bin_ptr(pos);
				return ptr->val.next(final && pos == final_pos, final_key, keep_going);
			} catch (const StashValuesEmpty&) {
			} catch (const StashValuesEmptyChunk&) {
			} catch (const StashValuesOutOfRange&) {
			} catch (const StashSlotsEmpty&) {
				throw StashContinue();
			} catch (const StashSlotsEmptyChunk&) {
			} catch (const StashSlotsOutOfRange&) {
				throw StashContinue();
			} catch (const StashContinue&) { }

			final_pos = increment_pos(pos, keep_going, initial_pos, final_pos, last_pos);

			if (_Ring && !final && ptr) {
				// Dispose if it's not in the final slice
				ptr->val.clear();
			}

			pos = Stash::pos.load();
		} while (true);
	}

	template <typename T>
	auto add(T&& value, uint64_t key) {
		if (!key) key = _CurrentKey();
		return put(key).add(std::forward<T>(value), key);
	}
};


template <typename _Tp, size_t _Size>
class StashValues : public Stash<std::shared_ptr<_Tp>, _Size> {
	using Stash = Stash<std::shared_ptr<_Tp>, _Size>;
	using Bin = typename Stash::Bin;
	using Nil = typename Stash::Nil;

	std::atomic_size_t idx;

public:
	StashValues(StashValues&& o) noexcept
		: Stash::Stash(std::move(o)),
		  idx(o.idx.load()) { }

	StashValues()
		: Stash::Stash(0),
		  idx(0) { }

	StashValues(uint64_t)
		: Stash::Stash(0),
		  idx(0) { }

	void increment_pos(size_t pos) {
		auto new_pos = pos + 1;
		Stash::pos.compare_exchange_strong(pos, new_pos);
	}

	Bin* current_bin_ptr(size_t pos) {
		try {
			auto& bin = Stash::get_bin(pos);
			auto ptr = bin.load();
			if (!ptr) {
				throw StashValuesEmptyChunk();
			}
			return ptr;
		} catch (const StashEmpty&) {
			throw StashValuesEmpty();
		} catch (const StashEmptyChunk&) {
			throw StashValuesEmptyChunk();
		} catch (const StashOutOfRange&) {
			throw StashValuesOutOfRange();
		}
	}

	auto next(bool, uint64_t, bool) {
		// std::cout << "\t\tLogStash::next()" << std::endl;
		auto pos = Stash::pos.load();
		// std::cout << "\t\t\tLogStash pos:" << pos << std::endl;
		Bin* ptr = current_bin_ptr(pos);
		increment_pos(pos);
		std::shared_ptr<_Tp> val(ptr->val);
		if (!val) {
			throw StashValuesEmptyChunk();
		}
		ptr->val.reset();
		return val;
	}

	template <typename T>
	auto add(T&& value, uint64_t) {
		auto& bin = Stash::spawn_bin(idx++);
		return Stash::_put(bin, std::forward<T>(value));
	}
};
