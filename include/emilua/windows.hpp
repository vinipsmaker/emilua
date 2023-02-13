/* Copyright (c) 2022 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

#include <boost/predef/os/windows.h>

#if BOOST_OS_WINDOWS
#include <boost/nowide/convert.hpp>
#endif

namespace emilua {

#if BOOST_OS_WINDOWS
template<class... Args>
auto widen_on_windows(Args&&... args)
    -> decltype(nowide::widen(std::forward<Args>(args)...))
{
    return nowide::widen(std::forward<Args>(args)...);
}

template<class... Args>
auto narrow_on_windows(Args&&... args)
    -> decltype(nowide::narrow(std::forward<Args>(args)...))
{
    return nowide::narrow(std::forward<Args>(args)...);
}
#else
template<class T>
auto widen_on_windows(T&& t) -> decltype(std::forward<T>(t))
{
    return std::forward<T>(t);
}

template<class... Args>
std::string narrow_on_windows(Args&&... args)
{
    return std::string(std::forward<Args>(args)...);
}
#endif

} // namespace emilua
