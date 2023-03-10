/* Copyright (c) 2021 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

namespace emilua {

extern char byte_span_key;
extern char byte_span_mt_key;

struct byte_span_handle
{
    byte_span_handle()
        : size(0)
        , capacity(0)
    {}

    byte_span_handle(const byte_span_handle& o)
        : data(o.data)
        , size(o.size)
        , capacity(o.capacity)
    {}

    byte_span_handle(lua_Integer size, lua_Integer capacity)
        : data{std::make_shared_for_overwrite<unsigned char[]>(capacity)}
        , size(size)
        , capacity(capacity)
    {}

    byte_span_handle(std::shared_ptr<unsigned char[]> data, lua_Integer size,
                     lua_Integer capacity)
        : data(std::move(data))
        , size(size)
        , capacity(capacity)
    {}

    explicit operator std::string_view() const
    {
        return {
            reinterpret_cast<char*>(data.get()),
            static_cast<std::string_view::size_type>(size)};
    }

    const std::shared_ptr<unsigned char[]> data;
    const lua_Integer size;
    const lua_Integer capacity;
};

void init_byte_span(lua_State* L);

} // namespace emilua
