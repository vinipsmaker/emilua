/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

template<class T>
struct Socket
{
    template<class... Args>
    Socket(Args&&... args) : socket{std::forward<Args>(args)...} {}

    T socket;
    std::size_t nbusy = 0; //< TODO: use to errcheck transfers between actors
};

} // namespace emilua
