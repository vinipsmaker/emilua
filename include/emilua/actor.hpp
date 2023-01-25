/* Copyright (c) 2020, 2023 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <emilua/core.hpp>

#if EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES
#include <sys/syscall.h>
#include <boost/asio/local/datagram_protocol.hpp>
#endif // EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES

namespace emilua {

extern char inbox_key;

void init_actor_module(lua_State* L);

#if EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES
extern char linux_container_chan_mt_key;

struct linux_container_reaper : public pending_operation
{
    linux_container_reaper(int childpidfd, pid_t childpid)
        : pending_operation{/*shared_ownership=*/false}
        , childpidfd{childpidfd}
        , childpid{childpid}
    {}

    ~linux_container_reaper()
    {
        close(childpidfd);
    }

    void cancel() noexcept override
    {
        syscall(SYS_pidfd_send_signal, childpidfd, SIGKILL, /*info=*/NULL,
                /*flags=*/0);
    }

    int childpidfd;
    pid_t childpid;
};

struct linux_container_address
{
    linux_container_address(asio::io_context& ioctx)
        : dest{ioctx}
    {}

    asio::local::datagram_protocol::socket dest;
    linux_container_reaper* reaper = nullptr;
};
#endif // EMILUA_CONFIG_ENABLE_LINUX_NAMESPACES

} // namespace emilua
