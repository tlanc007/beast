//
// Copyright (c) 2018 Vinnie Falco (vinnie dot falco at gmail dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//
// Official repository: https://github.com/vinniefalco/CppCon2018
//

#ifndef CPPCON2018_WEBSOCKET_SESSION_HPP
#define CPPCON2018_WEBSOCKET_SESSION_HPP

#include "net.hpp"
#include "beast.hpp"
#include "shared_state.hpp"

#include <cstdlib>
#include <memory>
#include <string>
#include <vector>

// Forward declaration
class shared_state;

/** Represents an active WebSocket connection to the server
*/
class websocket_session : public std::enable_shared_from_this<websocket_session>
{
    beast::flat_buffer buffer_;
    websocket::stream<beast::ssl_stream <
        beast::tcp_stream <net::io_context::strand> > > _wss;
    std::shared_ptr<shared_state> state_;
    std::vector<std::shared_ptr<std::string const>> queue_;

    void fail(beast::error_code ec, char const* what);
    void on_accept(beast::error_code ec);
    void on_read(beast::error_code ec, std::size_t bytes_transferred);
    void on_write(beast::error_code ec, std::size_t bytes_transferred);

public:
    explicit websocket_session(
        beast::ssl_stream<beast::tcp_stream<net::io_context::strand> >&& stream,
        std::shared_ptr<shared_state> const& state);

    ~websocket_session();

    template<class Body, class Allocator>
    void
    run(http::request<Body, http::basic_fields<Allocator>> req);

    // Send a message
    void
    send(std::shared_ptr<std::string const> const& ss);
};

template<class Body, class Allocator>
void
websocket_session::
run(http::request<Body, http::basic_fields<Allocator>> req)
{
    // Accept the websocket handshake
    _wss.async_accept(
        req,
        std::bind(
            &websocket_session::on_accept,
            shared_from_this(),
            std::placeholders::_1));
}

#endif
