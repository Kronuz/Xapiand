//
// MessagePack for C++ static resolution routine
//
// Copyright (C) 2015 KONDO Takatoshi
//
//    Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//    http://www.boost.org/LICENSE_1_0.txt)
//
#pragma once

#include "exception.h"

#include <typeinfo>


namespace msgpack {


struct type_error : public BaseException, public std::bad_cast {
    template<typename... Args>
    type_error(Args&&... args) : BaseException(std::forward<Args>(args)...), std::bad_cast() { }
};


struct container_size_overflow : public BaseException, public std::runtime_error {
    template<typename... Args>
    container_size_overflow(Args&&... args) : BaseException(std::forward<Args>(args)...), std::runtime_error(message) { }
};


struct unpack_error : public BaseException, public std::runtime_error {
    template<typename... Args>
    unpack_error(Args&&... args) : BaseException(std::forward<Args>(args)...), std::runtime_error(message) { }
};


struct parse_error : public unpack_error {
    using unpack_error::unpack_error;
};


struct insufficient_bytes : public unpack_error {
    using unpack_error::unpack_error;
};


struct size_overflow : public unpack_error {
    using unpack_error::unpack_error;
};


struct array_size_overflow : public size_overflow {
    using size_overflow::size_overflow;
};


struct map_size_overflow : public size_overflow {
    using size_overflow::size_overflow;
};


struct str_size_overflow : public size_overflow {
    using size_overflow::size_overflow;
};


struct bin_size_overflow : public size_overflow {
    using size_overflow::size_overflow;
};


struct ext_size_overflow : public size_overflow {
    using size_overflow::size_overflow;
};


struct depth_size_overflow : public size_overflow {
    using size_overflow::size_overflow;
};


}  // namespace msgpack
