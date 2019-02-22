
#include "beast.hpp"
//#include "httpSession.hpp"
#include "detectSession.hpp"

// Accepts incoming connections and launches the sessions
class listener : public std::enable_shared_from_this<listener>
{
public:
    listener (net::io_context& ioc_,
              tcp::endpoint endpoint_,
              const std::shared_ptr <const std::string>& docRoot_)
    : _acceptor {ioc_}
    , _docRoot {docRoot_}
    {
        beast::error_code ec;
        
        // Open acceptor
        _acceptor.open (endpoint_.protocol(), ec);
        if (ec) {
            fail (ec, "open");
            
            return;
        }
        
        // Allow address reuse
        _acceptor.set_option(net::socket_base::reuse_address(true), ec);
        if(ec)
        {
            fail(ec, "set_option");
            return;
        }
        
        // Bind to the server address
        _acceptor.bind(endpoint_, ec);
        if(ec)
        {
            fail(ec, "listener bind");
            return;
        }
        
        // Start listening for connections
        _acceptor.listen(
                         net::socket_base::max_listen_connections, ec);
        if(ec)
        {
            fail(ec, "listen");
            return;
        }
    }
    
    // Start accepting incoming connections
    void run ()
    {
        assert (_acceptor.is_open() && "listener::run acceptor not open");
        if (!_acceptor.is_open () ) {
            return;
        }
        doAccept ();
    }
    
    void doAccept ()
    {
        _acceptor.async_accept (
            beast::bind_front_handler (
                &listener::onAccept,
                shared_from_this()  ) );
    }
    
    void onAccept (beast::error_code ec_, tcp::socket socket_)
    {
        if (ec_) {
            fail (ec_, "listener accept");
        }
        
        else {
            // Create the detector http_session and run it
            std::make_shared<DetectSession>(std::move(socket_),
                                          _docRoot)->run();
        }
        
        // Accept another connection
        //doAccept();
    }
    
private:
    tcp::acceptor _acceptor;
    std::shared_ptr<const std::string> _docRoot;
};

