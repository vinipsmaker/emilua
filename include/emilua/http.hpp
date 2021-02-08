/* Copyright (c) 2020 Vinícius dos Santos Oliveira

   Distributed under the Boost Software License, Version 1.0. (See accompanying
   file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt) */

#pragma once

#include <boost/http/socket.hpp>
#include <boost/http/response.hpp>
#include <boost/http/request.hpp>

#include <emilua/core.hpp>

#ifndef EMILUA_HTTP_SOCKET_BUFFER_SIZE
#define EMILUA_HTTP_SOCKET_BUFFER_SIZE 8192
#endif // EMILUA_HTTP_SOCKET_BUFFER_SIZE

namespace emilua {

extern char http_key;
extern char http_request_mt_key;
extern char http_response_mt_key;
extern char http_socket_mt_key;
extern char https_socket_mt_key;

using Headers = boost::container::flat_multimap<
    std::string, std::string, TransparentStringComp>;

struct Request
    : public boost::http::basic_request<
        std::string, Headers, std::vector<std::uint8_t>
    >
{
    bool busy = false;
};

struct Response
    : public boost::http::basic_response<
        std::string, Headers, std::vector<std::uint8_t>
    >
{
    bool busy = false;
};

template<class T>
struct Socket
{
    struct Buffer
    {
        char data[EMILUA_HTTP_SOCKET_BUFFER_SIZE];
    };

    Socket(std::add_rvalue_reference_t<T> s)
        : buffer{std::make_shared<Buffer>()}
        , socket{asio::buffer(buffer->data), std::move(s)}
    {}

    std::shared_ptr<Buffer> buffer;
    http::basic_socket<T, http::default_socket_settings> socket;
    std::size_t nbusy = 0; //< TODO: use to errcheck transfers between actors
};

void init_http(lua_State* L);

} // namespace emilua

namespace boost::http {

template<>
struct is_request_message<emilua::Request>: public std::true_type
{};

template<>
struct is_response_message<emilua::Response>: public std::true_type
{};

} // namespace boost::http
