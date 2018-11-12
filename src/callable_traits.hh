/*
 * Copyright (C) 2013, 2016 Stephan Hohe
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

#include <cstddef>
#include <functional>
#include <stddef.h>
#include <tuple>

/**
 * Traits
 *
 * callable_traits deduces the following properties from the type T of a callable:
 *
 * The return type
 *   typename callable_traits<T>::return_type
 *
 * The number of arguments
 *   callable_traits<T>::arguments
 *
 * The arguments type (as tuple)
 *   typename callable_traits<T>::argument_types
 *
 * The individual argument types
 *   typename callable_traits<T>::argument_type<N>
 *
 * A function type representing the call:
 *   typename callable_traits<T>::function_type
*/

// There are three basic kinds of callable types:

// function types
struct function_tag {};
// function pointer types
struct function_ptr_tag {};
// classes with operator()
struct functor_tag {};


namespace detail {

//  _   _      _
// | | | | ___| |_ __   ___ _ __ ___
// | |_| |/ _ \ | '_ \ / _ \ '__/ __|
// |  _  |  __/ | |_) |  __/ |  \__ \
// |_| |_|\___|_| .__/ \___|_|  |___/
//              |_|

/** Remove reference and cv qualification */
template <typename T>
using remove_cvref_t = typename std::remove_cv< typename std::remove_reference<T>::type >::type;


/** Count the number of types given to the template */
template <typename... Types>
struct types_count;

template <>
struct types_count<> {
	static constexpr std::size_t value = 0;
};


template <typename Type, typename... Types>
struct types_count<Type, Types...> {
	static constexpr std::size_t value = types_count<Types...>::value + 1;
};


/** Get the nth type given to the template */
template <std::size_t n, typename... Types>
struct types_n;

template <std::size_t N, typename Type, typename... Types>
struct types_n<N, Type, Types...> : types_n<N-1, Types...> {
};

template <typename Type, typename... Types>
struct types_n<0, Type, Types...> {
	using type = Type;
};


/** Test if a type is in a list given types */
template <typename Q, typename... Ts>
struct types_has;

template <typename Q>
struct types_has<Q> {
	static constexpr bool value = false;
};

template <typename Q, typename... Ts>
struct types_has<Q, Q, Ts...> {
	static constexpr bool value = true;
};

template <typename Q, typename T, typename... Ts>
struct types_has<Q, T, Ts...> : types_has<Q, Ts...> {
};


//  _____                 _   _
// |  ___|   _ _ __   ___| |_(_) ___  _ __
// | |_ | | | | '_ \ / __| __| |/ _ \| '_ \
// |  _|| |_| | | | | (__| |_| | (_) | | | |
// |_|   \__,_|_| |_|\___|\__|_|\___/|_| |_|
//

namespace {

/** Define traits for a function type */
template <typename Fun>
struct function_traits;

template <typename Ret, typename... Args>
struct function_traits<Ret(Args...)> {
	using function_type = Ret(Args...);

	static constexpr std::size_t arguments = types_count<Args...>::value;

	using arguments_type = std::tuple<Args...>;

	template <size_t I>
	struct argument_type {
		using type = typename std::tuple_element<I, arguments_type>::type;
	};

	using return_type = Ret;
};

template <typename Ret, typename... Args>
const std::size_t function_traits<Ret(Args...)>::arguments;


//  __  __                _                 _____                 _   _
// |  \/  | ___ _ __ ___ | |__   ___ _ __  |  ___|   _ _ __   ___| |_(_) ___  _ __
// | |\/| |/ _ \ '_ ` _ \| '_ \ / _ \ '__| | |_ | | | | '_ \ / __| __| |/ _ \| '_ \
// | |  | |  __/ | | | | | |_) |  __/ |    |  _|| |_| | | | | (__| |_| | (_) | | | |
// |_|  |_|\___|_| |_| |_|_.__/ \___|_|    |_|   \__,_|_| |_|\___|\__|_|\___/|_| |_|
//

// Tags for member function qualifiers
struct const_tag {};
struct volatile_tag {};
struct lref_tag {};
struct rref_tag {};
struct noexcept_tag {};

namespace {

template <typename Class, typename Func, typename... Qual>
struct member_function_traits_q : function_traits<Func> {
	using class_type = Class;
	static constexpr bool is_const = types_has<const_tag, Qual...>::value;
	static constexpr bool is_volatile = types_has<volatile_tag, Qual...>::value;
	static constexpr bool is_lref = types_has<lref_tag, Qual...>::value;
	static constexpr bool is_rref = types_has<rref_tag, Qual...>::value;
#if __cpp_noexcept_function_type
	static constexpr bool is_noexcept = types_has<noexcept_tag, Qual...>::value;
#endif
};

// We need these until C++17 in case someone takes the address of one
// of those static variables or passses it by reference to a function
template <typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_const;
template <typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_volatile;
template <typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_lref;
template <typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_rref;
#if __cpp_noexcept_function_type
template <typename Class, typename Func, typename... Qual>
const bool member_function_traits_q<Class, Func, Qual...>::is_noexcept;
#endif

} // namespace


template <typename MemFun>
struct member_function_traits;

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...)>
	: member_function_traits_q<Class, Ret(Args...)> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const>
	: member_function_traits_q<Class, Ret(Args...), const_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile>
	: member_function_traits_q<Class, Ret(Args...), volatile_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile>
	: member_function_traits_q<Class, Ret(Args...), const_tag, volatile_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) &>
	: member_function_traits_q<Class, Ret(Args...), lref_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const &>
	: member_function_traits_q<Class, Ret(Args...), const_tag, lref_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile &>
	: member_function_traits_q<Class, Ret(Args...), volatile_tag, lref_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile &>
	: member_function_traits_q<Class, Ret(Args...), const_tag, volatile_tag, lref_tag> {
};


template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) &&>
	: member_function_traits_q<Class, Ret(Args...), rref_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const &&>
	: member_function_traits_q<Class, Ret(Args...), const_tag, rref_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile &&>
	: member_function_traits_q<Class, Ret(Args...), volatile_tag, rref_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile &&>
	: member_function_traits_q<Class, Ret(Args...), const_tag, volatile_tag, rref_tag> {
};


#if __cpp_noexcept_function_type
template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) noexcept>
	: member_function_traits_q<Class, Ret(Args...), noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const noexcept>
	: member_function_traits_q<Class, Ret(Args...), const_tag, noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile noexcept>
	: member_function_traits_q<Class, Ret(Args...), volatile_tag, noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile noexcept>
	: member_function_traits_q<Class, Ret(Args...), const_tag, volatile_tag, noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) & noexcept>
	: member_function_traits_q<Class, Ret(Args...), lref_tag, noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const & noexcept>
	: member_function_traits_q<Class, Ret(Args...), const_tag, lref_tag, noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile & noexcept>
	: member_function_traits_q<Class, Ret(Args...), volatile_tag, lref_tag, noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile & noexcept>
	: member_function_traits_q<Class, Ret(Args...), const_tag, volatile_tag, lref_tag, noexcept_tag> {
};


template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) && noexcept>
	: member_function_traits_q<Class, Ret(Args...), rref_tag, noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const && noexcept>
	: member_function_traits_q<Class, Ret(Args...), const_tag, rref_tag, noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) volatile && noexcept>
	: member_function_traits_q<Class, Ret(Args...), volatile_tag, rref_tag, noexcept_tag> {
};

template <typename Class, typename Ret, typename... Args>
struct member_function_traits<Ret (Class::*)(Args...) const volatile && noexcept>
	: member_function_traits_q<Class, Ret(Args...), const_tag, volatile_tag, rref_tag, noexcept_tag> {
};
#endif // __cpp_noexcept_function_type

} // namespace


//  _____                 _
// |  ___|   _ _ __   ___| |_ ___  _ __
// | |_ | | | | '_ \ / __| __/ _ \| '__|
// |  _|| |_| | | | | (__| || (_) | |
// |_|   \__,_|_| |_|\___|\__\___/|_|
//

template <typename Class>
using call_operator_traits = member_function_traits<decltype(&Class::operator())>;

// classes with operator()
template <typename Class>
struct functor_traits : function_traits<typename call_operator_traits<Class>::function_type> {
	using call_operator = call_operator_traits<Class>;
};


//   ____      _ _       _     _
//  / ___|__ _| | | __ _| |__ | | ___
// | |   / _` | | |/ _` | '_ \| |/ _ \
// | |__| (_| | | | (_| | |_) | |  __/
//  \____\__,_|_|_|\__,_|_.__/|_|\___|
//

/** Define traits for a operator() member function pointer type */

// classes with operator()
template <typename Callable>
struct callable_traits : functor_traits<Callable> {
	using callable_category = functor_tag;
};

// functions
template <typename Ret, typename... Args>
struct callable_traits<Ret(Args...)> : function_traits<Ret(Args...)> {
	using callable_category = function_tag;
};

// function pointers
template <typename Ret, typename... Args>
struct callable_traits<Ret (*)(Args...)> : function_traits<Ret(Args...)> {
	using callable_category = function_ptr_tag;
};

} // namespace detail


template <typename MemFunPtr>
struct member_function_traits : detail::member_function_traits<detail::remove_cvref_t<MemFunPtr>> {
};


template <typename Func>
struct function_traits : detail::function_traits<detail::remove_cvref_t<Func>> {
};

template <typename Func>
struct function_traits<Func*> : detail::function_traits<detail::remove_cvref_t<Func>> {
};


template <typename Class>
struct functor_traits : detail::functor_traits<detail::remove_cvref_t<Class>> {
};


// Main template

/** Traits for a callable (function/functor/lambda/...) */
template <typename Callable>
struct callable_traits : detail::callable_traits<detail::remove_cvref_t<Callable>> {
};


/** Convert a callable to a std::function<> */
template <typename Callable>
std::function<typename callable_traits<Callable>::function_type> to_stdfunction(Callable fun) {
	std::function<typename callable_traits<Callable>::function_type> stdfun(std::forward<Callable>(fun));
	return stdfun;
}
