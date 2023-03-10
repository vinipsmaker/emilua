/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

#include <boost/asio/io_context.hpp>

namespace emilua {

enum ContextType: lua_Integer
{
    regular_context,
    main,
    test,
    worker,
    error_category,
};

std::shared_ptr<vm_context> make_vm(boost::asio::io_context& ioctx,
                                    emilua::app_context& appctx,
                                    std::filesystem::path entry_point,
                                    ContextType lua_context);

} // namespace emilua
