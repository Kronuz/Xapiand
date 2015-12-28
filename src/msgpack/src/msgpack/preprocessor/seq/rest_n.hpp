# /* **************************************************************************
#  *                                                                          *
#  *     (C) Copyright Paul Mensonides 2002.
#  *     Distributed under the Boost Software License, Version 1.0. (See
#  *     accompanying file LICENSE_1_0.txt or copy at
#  *     http://www.boost.org/LICENSE_1_0.txt)
#  *                                                                          *
#  ************************************************************************** */
#
# /* See http://www.boost.org for most recent version. */
#
# ifndef MSGPACK_PREPROCESSOR_SEQ_REST_N_HPP
# define MSGPACK_PREPROCESSOR_SEQ_REST_N_HPP
#
# include "../arithmetic/inc.hpp"
# include "../config/config.hpp"
# include "../facilities/identity.hpp"
# include "detail/split.hpp"
# include "../tuple/elem.hpp"
#
# /* MSGPACK_PP_SEQ_REST_N */
#
# if ~MSGPACK_PP_CONFIG_FLAGS() & MSGPACK_PP_CONFIG_EDG()
#    define MSGPACK_PP_SEQ_REST_N(n, seq) MSGPACK_PP_TUPLE_ELEM(2, 1, MSGPACK_PP_SEQ_SPLIT(MSGPACK_PP_INC(n), MSGPACK_PP_IDENTITY( (nil) seq )))()
# else
#    define MSGPACK_PP_SEQ_REST_N(n, seq) MSGPACK_PP_SEQ_REST_N_I(n, seq)
#    define MSGPACK_PP_SEQ_REST_N_I(n, seq) MSGPACK_PP_TUPLE_ELEM(2, 1, MSGPACK_PP_SEQ_SPLIT(MSGPACK_PP_INC(n), MSGPACK_PP_IDENTITY( (nil) seq )))()
# endif
#
# endif
