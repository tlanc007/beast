#include "beast.hpp"
#include "httpSession.hpp"

class DetectSession : public std::enable_shared_from_this<DetectSession>
{
public:
    DetectSession (tcp::socket socket_,
                   const std::shared_ptr <const std::string>& docRoot_)
    : _stream {std::move (socket_) }
    , _docRoot {docRoot_}
    { }
    
    // Launch detector
    void run ()
    {
        _stream.expires_after (std::chrono::seconds (5) );
        
        async_detect_ssl(_stream, _buffer,
                         beast::bind_front_handler(&DetectSession::onDetect,
                                                   this->shared_from_this () ) );
    }
    
    void onDetect (beast::error_code ec_, boost::tribool result_)
    {
        if (ec_) {
            return fail (ec_, "detect");
        }
        
        if (result_) {
            assert(0 ==1 && "Need to add ssl support");
        }
        
        // plain session
        std::make_shared <HttpSession> (std::move(_stream),
                                         std::move(_buffer),
                                         _docRoot)->run();
        
    }
    
private:
    beast::tcp_stream<net::io_context::strand> _stream;
    std::shared_ptr <const std::string> _docRoot;
    beast::flat_buffer _buffer;
};


