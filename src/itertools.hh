/*
* Copyright (c) 2019 Dubalu LLC
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

#include <iterator>

namespace itertools {

//  _____                     __
// |_   _| __ __ _ _ __  ___ / _| ___  _ __ _ __ ___
//   | || '__/ _` | '_ \/ __| |_ / _ \| '__| '_ ` _ \
//   | || | | (_| | | | \__ \  _| (_) | |  | | | | | |
//   |_||_|  \__,_|_| |_|___/_|  \___/|_|  |_| |_| |_|
//
template <typename F, typename I>
class Transform {
	F _fn;
	I _begin;
	I _end;

public:
	class iterator : public std::iterator<std::forward_iterator_tag, typename I::value_type, typename I::difference_type, typename I::pointer, typename I::reference> {
		F& _fn;
		I _it;

	public:
		iterator(F& fn, I it) : _fn{fn}, _it{it} { }
		bool operator!=(const iterator& other) const {
			return _it != other._it;
		}
		iterator& operator++() {
			++_it;
			return *this;
		}
		auto operator*() const {
			return _fn(*_it);
		}
	};

	Transform(F fn, I begin, I end) :
		_fn{fn}, _begin{begin}, _end{end} { }

	iterator begin() {
		return {_fn, _begin};
	}

	iterator end() {
		return {_fn, _end};
	}
};

template <typename F, typename I>
auto
transform(F fn, I begin, I end) {
	return Transform<F, I>{fn, begin, end};
}


//   ____ _           _
//  / ___| |__   __ _(_)_ __
// | |   | '_ \ / _` | | '_ \
// | |___| | | | (_| | | | | |
//  \____|_| |_|\__,_|_|_| |_|
//
template <typename I1, typename I2>
class Chain {
	I1 _begin1;
	I1 _end1;
	I2 _begin2;
	I2 _end2;

public:
	class iterator : public std::iterator<std::forward_iterator_tag, typename I2::value_type, typename I2::difference_type, typename I2::pointer, typename I2::reference> {
		I1 _it1;
		I1 _end1;
		I2 _it2;

	public:
		iterator(I1 it1, I1 end1, I2 it2) :
			_it1{it1}, _end1{end1}, _it2{it2} { }

		bool operator!=(const iterator& other) const {
			return _it1 != other._it1 || _it2 != other._it2;
		}

		iterator& operator++() {
			if (_it1 != _end1) {
				++_it1;
			} else {
				++_it2;
			}
			return *this;
		}

		auto operator*() const {
			if (_it1 != _end1) {
				return *_it1;
			} else {
				return *_it2;
			}
		}
	};

	Chain(I1 begin1, I1 end1, I2 begin2, I2 end2) :
		_begin1{begin1}, _end1{end1}, _begin2{begin2}, _end2{end2} { }

	iterator begin() {
		return {_begin1, _end1, _begin2};
	}

	iterator end() {
		return {_end1, _end1, _end2};
	}
};

template <typename I1, typename I2>
auto
chain(I1 begin1, I1 end1, I2 begin2, I2 end2) {
	return Chain<I1, I2>{begin1, end1, begin2, end2};
}

}
