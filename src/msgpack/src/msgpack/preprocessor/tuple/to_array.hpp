# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Edward Diener 2011.                                    *
#  *     (C) Copyright Paul Mensonides 2011.                                  *
#  *     Distributed under the Boost Software License, Version 1.0. (See      *
#  *     accompanying file LICENSE_1_0.txt or copy at                         *
#  *     http://www.boost.org/LICENSE_1_0.txt)                                *
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef MSGPACK_PREPROCESSOR_TUPLE_TO_ARRAY_HPP
# define MSGPACK_PREPROCESSOR_TUPLE_TO_ARRAY_HPP
#
# include "../cat.hpp"
# include "../config/config.hpp"
# include "../facilities/overload.hpp"
# include "size.hpp"
# include "../variadic/size.hpp"
#
# /* MSGPACK_PP_TUPLE_TO_ARRAY */
#
# if MSGPACK_PP_VARIADICS
#    if MSGPACK_PP_VARIADICS_MSVC
#        define MSGPACK_PP_TUPLE_TO_ARRAY(...) MSGPACK_PP_TUPLE_TO_ARRAY_I(MSGPACK_PP_OVERLOAD(MSGPACK_PP_TUPLE_TO_ARRAY_, __VA_ARGS__), (__VA_ARGS__))
#        define MSGPACK_PP_TUPLE_TO_ARRAY_I(m, args) MSGPACK_PP_TUPLE_TO_ARRAY_II(m, args)
#        define MSGPACK_PP_TUPLE_TO_ARRAY_II(m, args) MSGPACK_PP_CAT(m ## args,)
#        define MSGPACK_PP_TUPLE_TO_ARRAY_1(tuple) (MSGPACK_PP_TUPLE_SIZE(tuple), tuple)
#    else
#        define MSGPACK_PP_TUPLE_TO_ARRAY(...) MSGPACK_PP_OVERLOAD(MSGPACK_PP_TUPLE_TO_ARRAY_, __VA_ARGS__)(__VA_ARGS__)
#        define MSGPACK_PP_TUPLE_TO_ARRAY_1(tuple) (MSGPACK_PP_VARIADIC_SIZE tuple, tuple)
#    endif
#    define MSGPACK_PP_TUPLE_TO_ARRAY_2(size, tuple) (size, tuple)
# else
#    define MSGPACK_PP_TUPLE_TO_ARRAY(size, tuple) (size, tuple)
# endif
#
# endif
