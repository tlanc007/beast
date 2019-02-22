
// simple version for now
#include "beast.hpp"
#include "handleRequest.hpp"
#include "websocketSession.hpp"


class HttpSession : public std::enable_shared_from_this <HttpSession>
{
public:
    HttpSession (beast::tcp_stream<net::io_context::strand>&& stream_,
                 beast::flat_buffer&& buffer_,
                 const std::shared_ptr <const std::string>& docRoot_)
    : _stream {std::move (stream_) }
    , _buffer {std::move (buffer_) }
    , _docRoot {docRoot_}
    { }
    
    beast::tcp_stream<net::io_context::strand> releaseStream()
    {
        return std::move (_stream);
    }
    
    void run()
    {
        doRead();
    }
    
    void doRead()
    {
        _req = {};
        _stream.expires_after(std::chrono::seconds(5) );
        
        http::async_read(_stream, _buffer, _req,
                         beast::bind_front_handler(&HttpSession::onRead,
                                                   shared_from_this () ) );
    }
    
    void onRead (beast::error_code ec_,
                 std::size_t bytesTransferred_)
    {
        boost::ignore_unused (bytesTransferred_);
        
        // connection got closed
        if (ec_ == http::error::end_of_stream) {
            return doClose();
        }
        
        if (ec_) {
            return fail (ec_, "HttpSession read");
        }
        
        // see if WebSocket upgrade
        if (websocket::is_upgrade (_req) ) {
            // Transfer stream to new WebSocket session
            return makeWebSocketSession (releaseStream(),
                                         std::move (_req) );
        }
        
        // Send the response
        handle_request (*_docRoot, std::move (_req),
                        [this] (auto&& response)
                        {
                            // extent lifetime of the message; shared_ptr
                            using responseType = typename std::decay<decltype(response)>::type;
                            auto sp {std::make_shared<responseType> (std::forward<decltype(response)>(response) ) };
                            auto self {shared_from_this() };
                            
                            // Store a type-erased version of the shared
                            // pointer in the class to keep it alive.
                            self->_res = sp;
                            
                            http::async_write (this->_stream, *sp,
                                               [self, sp] (beast::error_code ec_,
                                                           std::size_t bytes_)
                                               {
                                                   self->onWrite (ec_, bytes_,
                                                                   sp->need_eof() );
                                               } );
                        } );
    }
    
    void onWrite (beast::error_code ec_,
                  std::size_t bytesTransferred_,
                  bool close)
    {
        boost::ignore_unused(bytesTransferred_);
        if (ec_) {
            return fail (ec_, "write");
        }
        
        // done with response to delete it
        _res = nullptr;
        
        // another request
        doRead();
    }
    
    void doClose ()
    {
        beast::error_code ec;
        //_stream.shutdown(tcp::socket::shutdown_send, ec);
        //_stream.async_shutdown(beast::bind_front_handler(&HttpSession::onShutdown,
        beast::get_lowest_layer(_stream).socket().shutdown(tcp::socket::shutdown_both, ec);
        beast::get_lowest_layer(_stream).socket().close(ec);
    }

private:
    beast::tcp_stream<net::io_context::strand> _stream;
    beast::flat_buffer _buffer;
    std::shared_ptr<const std::string> _docRoot;
    http::request <http::string_body> _req;
    std::shared_ptr <void> _res;
};
