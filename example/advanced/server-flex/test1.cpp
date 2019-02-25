
#include <algorithm>
#include <cstdlib>
#include <functional>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include "beast.hpp"
#include "listener.hpp"

#include <fmt/printf.h>

#include <catch2/catch.hpp>

// setup namespaces
namespace beast = boost::beast;                 // from <boost/beast.hpp>
namespace http = beast::http;                   // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;         // from <boost/beast/websocket.hpp>
namespace net = boost::asio;                    // from <boost/asio.hpp>
namespace ssl = net::ssl;               // from <boost/asio/ssl.hpp>
using tcp = net::ip::tcp;               // from <boost/asio/ip/tcp.hpp>

constexpr auto portStr {"8080"};
constexpr auto port {static_cast<unsigned int> (8080) };
constexpr auto docRootSrc {"/Users/tal/Desktop/other/rmr/public_html"};
constexpr auto target {"/simple.html"};
const std::string simpleHtmlBody {R"""(<html>
<body>B</body>
</html>
)"""};
constexpr auto localhost {"127.0.0.1"};
constexpr auto threads {1};
const auto address {net::ip::make_address(localhost) };
const auto docRoot {std::make_shared <std::string> (docRootSrc) };

// Report a failure
void fail(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

// may need to be changed
using streamType = beast::tcp_stream<net::io_context::executor_type>;

http::response<http::string_body> gres;
    
class WSClient : public std::enable_shared_from_this<WSClient>
{
public:
    explicit WSClient (net::io_context& ioc_)
    : _resolver (ioc_)
    , _ws (ioc_)
    { }
        
    void run (const char* host_,
              const char* port_,
              const char* text_)
    {
        _host = host_;
        _text = text_;
        
        // look up domain
        _resolver.async_resolve(host_, port_,
                                beast::bind_front_handler(&WSClient::onResolve,
                                                          shared_from_this() ) );
    }
    
    void onResolve (beast::error_code ec_,
                    tcp::resolver::results_type results_)
    {
        if (ec_) {
            return fail (ec_, "WSClient resolve");
        }
        
        // set timeout for the operation
        beast::get_lowest_layer(_ws).expires_after (std::chrono::seconds (5) );
        
        // Make connection on the IP from lookup
        beast::async_connect(beast::get_lowest_layer(_ws),
                             results_,
                             beast::bind_front_handler(&WSClient::onConnect,
                                                       shared_from_this() ) );
    }
    
    void onConnect (beast::error_code ec_, tcp::resolver::results_type::endpoint_type)
    {
        if (ec_) {
            return fail (ec_, "WSClient connect");
        }
        
        // perform ws handshake
        _ws.async_handshake (_host, "/",
                             beast::bind_front_handler(&WSClient::onHandshake,
                                                       shared_from_this () ) );
    }
    
    void onHandshake(beast::error_code ec_)
    {
        if(ec_) {
            return fail(ec_, "WSClient handshake");
        }
        
        // Send the message
        _ws.async_write(
                        net::buffer(_text),
                        beast::bind_front_handler(
                                                  &WSClient::onWrite,
                                                  shared_from_this() ) );
    }
    
    void onWrite(
             beast::error_code ec_,
             std::size_t bytesTransferred_)
    {
        boost::ignore_unused(bytesTransferred_);
        
        if(ec_) {
            return fail(ec_, "WSCleint write");
        }
        
        // Read a message into our buffer
        _ws.async_read(
                       _buffer,
                       beast::bind_front_handler(
                                                 &WSClient::onRead,
                                                 shared_from_this() ) );
    }

    void onRead(
            beast::error_code ec_,
            std::size_t bytesTransferred_)
    {
        boost::ignore_unused(bytesTransferred_);
        
        if(ec_) {
            return fail(ec_, "read");
        }
        
        // Close the WebSocket connection
        _ws.async_close(websocket::close_code::normal,
                        beast::bind_front_handler(
                                                  &WSClient::onClose,
                                                  shared_from_this() ) );
    }
    
    void onClose(beast::error_code ec_)
    {
        if(ec_) {
            return fail(ec_, "WSClient close");
        }
        
        // If we get here then the connection is closed gracefully
        
        // The make_printable() function helps print a ConstBufferSequence
        fmt::print("buffer data: {}\n", beast::make_printable(_buffer.data() ) );
        auto result {fmt::format("{}", beast::make_printable(_buffer.data() ) ) };
        REQUIRE (_text == result );
    }

private:
    tcp::resolver _resolver;
    websocket::stream<beast::tcp_stream<net::io_context::executor_type> > _ws;
    beast::multi_buffer _buffer;
    std::string _host;
    std::string _text;
};
    
class Client : public std::enable_shared_from_this<Client>
{
    
public:
    Client (net::io_context& ioc_)
    : _resolver {ioc_}
    , _stream {ioc_}
    { }
    
    void run (const char* host_,
              const char* port_,
              const char* target_,
              int version_)
    {
        _req.version (version_);
        _req.method(http::verb::get);
        _req.target(target_);
        _req.set(http::field::host, host_);
        _req.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        
        _resolver.async_resolve(host_, port_, beast::bind_front_handler(&Client::onResolve, shared_from_this() ) );
    }
    
    void onResolve (beast::error_code ec_,
                    tcp::resolver::results_type results_)
    {
        REQUIRE(!ec_);
        if (ec_) {
            return fail (ec_, "resolve");
        }
        
        // set timeout
        _stream.expires_after(std::chrono::seconds (5) );
        
        // Make connection on the IP address
        beast::async_connect(_stream, results_,
                             beast::bind_front_handler(&Client::onConnect,
                                                       shared_from_this()) );
    }
    
    void onConnect (beast::error_code ec_, tcp::resolver::results_type::endpoint_type)
    {
        REQUIRE(!ec_);
        if (ec_) {
            return fail (ec_, "connect");
        }
        
        // Set timeout on operation
        _stream.expires_after(std::chrono::seconds (5) );
        
        // Send HTTP request to host
        http::async_write(_stream, _req,
                          beast::bind_front_handler(&Client::onWrite,
                                                    shared_from_this()  ) );
    }
    
    void onWrite(beast::error_code ec_,
                 std::size_t bytesTransferred_)
    {
        boost::ignore_unused(bytesTransferred_);
        REQUIRE(!ec_);
        if (ec_) {
            return fail (ec_, "write");
        }
        
        // Receive HTTP response
        http::async_read(_stream, _buffer, gres,
                         beast::bind_front_handler(&Client::onRead,
                                                   shared_from_this() ) );
    }
    
    void onRead (beast::error_code ec_,
                 std::size_t bytesTransferred_)
    {
        boost::ignore_unused(bytesTransferred_);
        
        if (ec_) {
            return fail (ec_, "Client read");
        }

        // gracefully close the socket
        _stream.socket().shutdown(tcp::socket::shutdown_both, ec_);
        
        // not connected
        if (ec_ && ec_ != beast::errc::not_connected) {
            return fail (ec_, "shutdown");
        }
        
        // connection closed gracefully
    }

private:
    tcp::resolver _resolver;
    streamType _stream;
    beast::flat_buffer _buffer;
    http::request<http::empty_body> _req;
};

TEST_CASE("Simple server")
{
    SECTION("setup")
    {
        net::io_context ioc {threads};
        std::make_shared <listener> (ioc,
                                     tcp::endpoint {address, port},
                                     docRoot)->run();
        
        // Capture SIGINT and SIGTERM
        
        // Run the I/O service on the requested number of threads
        // forcing to 1 thread (at lest for now ...
        SECTION("valid target: /simple.html")
        {
            std::make_shared<Client>(ioc)->run (localhost, portStr, target, 11/* version*/);
            ioc.run();
            fmt::print("{}\n", gres);
            REQUIRE(gres.version() == 11);
            REQUIRE(gres.result() == http::status::ok);
            REQUIRE(gres.body() == simpleHtmlBody);
            //auto header {gres.base() };
            auto hdr {gres.base()};
            for (const auto& e: gres) {
                fmt::print("cbegin: name- {} val- {}\n", e.name_string(), e.value() );
            }
            fmt::print("\n");
        }
        
        SECTION("invalid target: no root simple.html")
        {
            gres = {};
            std::make_shared<Client>(ioc)->run (localhost, portStr, "simple.html", 11/* version*/);
            ioc.run();
            fmt::print("{}\n", gres);
            REQUIRE(gres.result() == http::status::bad_request);
            REQUIRE(gres.body() == "Illegal request-target");
        }
        
        SECTION("websocket valid target: /")
        {
            std::make_shared<WSClient>(ioc)->run (localhost,
                                                  portStr,
                                                  "Howdy.");
            ioc.run();
        }
    }
}
    

