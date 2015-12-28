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
# ifndef MSGPACK_PREPROCESSOR_SEQ_CAT_HPP
# define MSGPACK_PREPROCESSOR_SEQ_CAT_HPP
#
# include "../arithmetic/dec.hpp"
# include "../config/config.hpp"
# include "../control/if.hpp"
# include "fold_left.hpp"
# include "seq.hpp"
# include "size.hpp"
# include "../tuple/eat.hpp"
#
# /* MSGPACK_PP_SEQ_CAT */
#
# define MSGPACK_PP_SEQ_CAT(seq) \
    MSGPACK_PP_IF( \
        MSGPACK_PP_DEC(MSGPACK_PP_SEQ_SIZE(seq)), \
        MSGPACK_PP_SEQ_CAT_I, \
        MSGPACK_PP_SEQ_HEAD \
    )(seq) \
    /**/
# define MSGPACK_PP_SEQ_CAT_I(seq) MSGPACK_PP_SEQ_FOLD_LEFT(MSGPACK_PP_SEQ_CAT_O, MSGPACK_PP_SEQ_HEAD(seq), MSGPACK_PP_SEQ_TAIL(seq))
#
# define MSGPACK_PP_SEQ_CAT_O(s, st, elem) MSGPACK_PP_SEQ_CAT_O_I(st, elem)
# define MSGPACK_PP_SEQ_CAT_O_I(a, b) a ## b
#
# /* MSGPACK_PP_SEQ_CAT_S */
#
# define MSGPACK_PP_SEQ_CAT_S(s, seq) \
    MSGPACK_PP_IF( \
        MSGPACK_PP_DEC(MSGPACK_PP_SEQ_SIZE(seq)), \
        MSGPACK_PP_SEQ_CAT_S_I_A, \
        MSGPACK_PP_SEQ_CAT_S_I_B \
    )(s, seq) \
    /**/
# define MSGPACK_PP_SEQ_CAT_S_I_A(s, seq) MSGPACK_PP_SEQ_FOLD_LEFT_ ## s(MSGPACK_PP_SEQ_CAT_O, MSGPACK_PP_SEQ_HEAD(seq), MSGPACK_PP_SEQ_TAIL(seq))
# define MSGPACK_PP_SEQ_CAT_S_I_B(s, seq) MSGPACK_PP_SEQ_HEAD(seq)
#
# endif
