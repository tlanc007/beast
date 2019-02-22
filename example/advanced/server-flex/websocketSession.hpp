#include "beast.hpp"

// plain websocket
class WebSocketSession: public std::enable_shared_from_this <WebSocketSession>
{
public:
    WebSocketSession(beast::tcp_stream<net::io_context::strand>&& strand_)
    : _ws {std::move (strand_) }
    { }
    
    // Start asynchronous operation
    template <class Body, class Allocator>
    void run (http::request <Body, http::basic_fields <Allocator> > req_)
    {
        // need to add timer
        
        // accept WebSocket upgrade
        doAccept (std::move (req_) );
    }
    
    void onAccept (beast::error_code ec_)
    {
        // timer closes the socket
        if (ec_ == net::error::operation_aborted) {
            return;
        }
        
        if (ec_) {
            return fail (ec, "accept");
        }
        
        // read message
        doRead();
    }
    
    void doRead ()
    {
        // read message into buffer
        _ws.async_read(_buffer,
                       beast::bind_front_handler (&WebSocketSession::onRead,
                                                  this->shared_from_this() ) );
    }
    
    void onRead (beast::error_code ec_,
                 std::size_t bytesTransferred)
    {
        boost::ignore_unused (bytesTransferred);
        
        // Happens when timer closes the socket
        if (ec_ == net::error::operation_aborted) {
            return;
        }
        
        // This indicates that the websocket was closed
        if (ec_ == websocket::error::closed) {
            return;
        }
        
        if (ec_) {
            fail (ec_, "read");
        }
        
        activity();
        
        _ws.text(_ws.got_text() );
        _ws.async_write (_buffer.data(),
                         beast::bind_front_handler (&WebSocketSession::onWrite,
                                                    this->shared_from_this () ) );
    }
    
    void onWrite (beast::error_code ec_,
                  std::size_t bytesTransferred)
    {
        boost::ignore_unused(bytes_transferred);
        
        // Happens when timer closes the socket
        if (ec_ == net::error::operation_aborted) {
            return;
        }
        
        // This indicates that the websocket was closed
        if (ec_) {
            return fail (ec, "write");
        }
        
        // clear buffer
        _buffer.consume (_buffer.size() );
        
        // another read
        doRead();
    }

private:
    explicit websocket::stream<beast::tcp_stream<net::io_context::strand> > _ws;
    bool _close {false};
}

