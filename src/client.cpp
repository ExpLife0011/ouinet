#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/version.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/spawn.hpp>
#include <boost/asio/connect.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <fstream>

#include <ipfs_cache/client.h>
#include <ipfs_cache/error.h>

#ifdef USE_GNUNET
#   include <gnunet_channels/channel.h>
#   include <gnunet_channels/service.h>
#endif

#include <i2poui.h>

#include "namespaces.h"
#include "fetch_http_page.h"
#include "client_front_end.h"
#include "generic_connection.h"
#include "util.h"
#include "async_sleep.h"
#include "increase_open_file_limit.h"
#include "endpoint.h"
#include "cache_control.h"
#include "or_throw.h"
#include "request_routing.h"
#include "full_duplex_forward.h"
#include "client_config.h"
#include "client.h"

#ifdef __ANDROID__
#include "redirect_to_android_log.h"
#endif // ifdef __ANDROID__

#include "util/signal.h"

using namespace std;
using namespace ouinet;

namespace posix_time = boost::posix_time;

using tcp         = asio::ip::tcp;
using string_view = beast::string_view;
using Request     = http::request<http::string_body>;
using Response    = http::response<http::dynamic_body>;

static const boost::filesystem::path OUINET_PID_FILE = "pid";

//------------------------------------------------------------------------------
#define ASYNC_DEBUG(code, ...) [&] () mutable {\
    auto task = _front_end.notify_task(util::str(__VA_ARGS__));\
    return code;\
}()

//------------------------------------------------------------------------------
class Client::State {
private:
    using Request  = http::request<http::string_body>;
    using Response = http::response<http::dynamic_body>;

    struct I2P {
        i2poui::Service service;
        i2poui::Connector connector;
    };

public:
    State(asio::io_service& ios)
        : _ios(ios)
#ifdef __ANDROID__
        , _cout_guard(std::cout)
        , _cerr_guard(std::cerr)
#endif // ifdef __ANDROID__
    {}

    bool start(int argc, char* argv[]);
    void stop() { _shutdown_signal(); }

private:
    void serve_request(GenericConnection& con, asio::yield_context yield);

    GenericConnection connect_to_injector(asio::yield_context yield);

    void handle_connect_request( GenericConnection& client_c
                               , const Request& req
                               , asio::yield_context yield);

    CacheControl::CacheEntry
    fetch_stored( const Request& request
                , request_route::Config& request_config
                , asio::yield_context yield);

    Response fetch_fresh( const Request& request
                        , request_route::Config& request_config
                        , asio::yield_context yield);

    CacheControl build_cache_control(request_route::Config& request_config);

    void do_listen(asio::yield_context yield);

private:
    asio::io_service& _ios;
    ClientConfig _config;
#ifdef USE_GNUNET
    std::unique_ptr<gnunet_channels::Service> _gnunet_service;
#endif
    std::unique_ptr<I2P> _i2p;
    std::unique_ptr<ipfs_cache::Client> _ipfs_cache;

    ClientFrontEnd _front_end;
    Signal<void()> _shutdown_signal;

#ifdef __ANDROID__
    // While these two objects are 'alive', everything that is written
    // into std::{cout,cerr} will be written into Android's log.
    RedirectToAndroidLog _cout_guard;
    RedirectToAndroidLog _cerr_guard;
#endif // ifdef __ANDROID__
};

//------------------------------------------------------------------------------
static
void handle_bad_request( GenericConnection& con
                       , const Request& req
                       , string message
                       , asio::yield_context yield)
{
    http::response<http::string_body> res{http::status::bad_request, req.version()};

    res.set(http::field::server, BOOST_BEAST_VERSION_STRING);
    res.set(http::field::content_type, "text/html");
    res.keep_alive(req.keep_alive());
    res.body() = message;
    res.prepare_payload();

    sys::error_code ec;
    http::async_write(con, res, yield[ec]);
}

//------------------------------------------------------------------------------
GenericConnection
Client::State::connect_to_injector(asio::yield_context yield)
{
    namespace error = asio::error;

    struct Visitor {
        using Ret = GenericConnection;

        sys::error_code ec;
        Client::State& client;
        asio::yield_context yield;

        Visitor(Client::State& client, asio::yield_context yield)
            : client(client), yield(yield) {}

        Ret operator()(const tcp::endpoint& ep) {
            tcp::socket socket(client._ios);
            socket.async_connect(ep, yield[ec]);
            return or_throw(yield, ec, GenericConnection(move(socket)));
        }

#ifdef USE_GNUNET
        Ret operator()(const GnunetEndpoint& ep) {
            using Channel = gnunet_channels::Channel;

            if (!client._gnunet_service) {
                return or_throw<Ret>(yield, error::no_protocol_option);
            }

            Channel ch(*client._gnunet_service);
            ch.connect(ep.host, ep.port, yield[ec]);

            return or_throw(yield, ec, GenericConnection(move(ch)));
        }
#endif

        Ret operator()(const I2PEndpoint&) {
            if (!client._i2p) {
                return or_throw<Ret>(yield, error::no_protocol_option);
            }

            i2poui::Channel ch(client._i2p->service);
            ch.connect(client._i2p->connector, yield[ec]);

            // TODO: Remove the second argument to GenericConnection once
            // i2poui Channel implements the 'close' member function.
            return or_throw(yield, ec, GenericConnection(move(ch), [](auto&){}));
        }
    };

    Visitor visitor(*this, yield);

    return boost::apply_visitor(visitor, _config.injector_endpoint());
}

//------------------------------------------------------------------------------
void Client::State::handle_connect_request( GenericConnection& client_c
                                          , const Request& req
                                          , asio::yield_context yield)
{
    // https://tools.ietf.org/html/rfc2817#section-5.2

    sys::error_code ec;

    if (!_front_end.is_injector_proxying_enabled()) {
        return ASYNC_DEBUG( handle_bad_request( client_c
                                              , req
                                              , "Forwarding disabled"
                                              , yield[ec])
                          , "Forwarding disabled");
    }

    auto injector_c = connect_to_injector(yield[ec]);

    if (ec) {
        // TODO: Does an RFC dicate a particular HTTP status code?
        return handle_bad_request(client_c, req, "Can't connect to injector", yield[ec]);
    }

    auto disconnect_injector_slot = _shutdown_signal.connect([&injector_c] {
        injector_c.close();
    });

    http::async_write(injector_c, const_cast<Request&>(req), yield[ec]);

    if (ec) {
        // TODO: Does an RFC dicate a particular HTTP status code?
        return handle_bad_request(client_c, req, "Can't contact the injector", yield[ec]);
    }

    beast::flat_buffer buffer;
    Response res;
    http::async_read(injector_c, buffer, res, yield[ec]);

    if (ec) {
        // TODO: Does an RFC dicate a particular HTTP status code?
        return handle_bad_request(client_c, req, "Can't contact the injector", yield[ec]);
    }

    http::async_write(client_c, res, yield[ec]);

    if (ec) return fail(ec, "sending connect response");

    if (!(200 <= unsigned(res.result()) && unsigned(res.result()) < 300)) {
        return;
    }

    full_duplex(client_c, injector_c, yield);
}

//------------------------------------------------------------------------------
CacheControl::CacheEntry
Client::State::fetch_stored( const Request& request
                           , request_route::Config& request_config
                           , asio::yield_context yield)
{
    using CacheEntry = CacheControl::CacheEntry;

    const bool cache_is_disabled
        = !request_config.enable_cache
       || !_ipfs_cache
       || !_front_end.is_ipfs_cache_enabled();

    if (cache_is_disabled) {
        return or_throw<CacheControl::CacheEntry>( yield ,
                asio::error::operation_not_supported);
    }

    sys::error_code ec;
    // Get the content from cache
    auto key = request.target();

    auto content = _ipfs_cache->get_content(key.to_string(), yield[ec]);

    if (ec) return or_throw<CacheEntry>(yield, ec);

    // If the content does not have a meaningful time stamp,
    // an error should have been reported.
    assert(!content.ts.is_not_a_date_time());

    http::response_parser<Response::body_type> parser;
    parser.eager(true);
    parser.put(asio::buffer(content.data), ec);

    assert(!ec && "Malformed cache entry");

    if (!parser.is_done()) {
#ifndef NDEBUG
        cerr << "------- WARNING: Unfinished message in cache --------" << endl;
        assert(parser.is_header_done() && "Malformed response head did not cause error");
        auto response = parser.get();
        cerr << request << response.base() << "<" << response.body().size() << " bytes in body>" << endl;
        cerr << "-----------------------------------------------------" << endl;
#endif
        ec = asio::error::not_found;
    }

    if (ec) return or_throw<CacheEntry>(yield, ec);

    return CacheEntry{content.ts, parser.release()};
}

//------------------------------------------------------------------------------
Response Client::State::fetch_fresh( const Request& request
                                   , request_route::Config& request_config
                                   , asio::yield_context yield)
{
    using namespace asio::error;
    using request_route::responder;

    sys::error_code last_error = operation_not_supported;

    while (!request_config.responders.empty()) {
        auto r = request_config.responders.front();
        request_config.responders.pop();

        switch (r) {
            case responder::origin: {
                assert(0 && "TODO");
                continue;
            }
            case responder::proxy: {
                assert(0 && "TODO");
                continue;
            }
            case responder::injector: {
                if (!_front_end.is_injector_proxying_enabled()) {
                    continue;
                }
                sys::error_code ec;
                auto inj_con = connect_to_injector(yield[ec]);
                if (ec) {
                    last_error = ec;
                    continue;
                }
                // Forward the request to the injector
                auto res = fetch_http_page(_ios, inj_con, request, yield[ec]);
                if (!ec) return res;
                last_error = ec;
                continue;
            }
            case responder::_front_end: {
                return _front_end.serve( _config.injector_endpoint()
                                       , request
                                       , _ipfs_cache.get());
            }
        }
    }

    return or_throw<Response>(yield, last_error);
}

//------------------------------------------------------------------------------
CacheControl
Client::State::build_cache_control(request_route::Config& request_config)
{
    CacheControl cache_control;

    cache_control.fetch_stored =
        [&] (const Request& request, asio::yield_context yield) {
            return ASYNC_DEBUG( fetch_stored(request, request_config, yield)
                              , "Fetch from cache: " , request.target());
        };

    cache_control.fetch_fresh =
        [&] (const Request& request, asio::yield_context yield) {
            return ASYNC_DEBUG( fetch_fresh(request, request_config, yield)
                              , "Fetch from origin: ", request.target());
        };

    cache_control.max_cached_age(_config.max_cached_age());

    return cache_control;
}

//------------------------------------------------------------------------------
void Client::State::serve_request( GenericConnection& con
                                 , asio::yield_context yield)
{
    namespace rr = request_route;
    using rr::responder;

    auto close_con_slot = _shutdown_signal.connect([&con] {
        con.close();
    });

    // These access mechanisms are attempted in order for requests by default.
    const rr::Config default_request_config
        { true
        , queue<responder>({responder::injector})};

    rr::Config request_config;

    CacheControl cache_control = build_cache_control(request_config);

    sys::error_code ec;
    beast::flat_buffer buffer;

    // Expressions to test the request against and mechanisms to be used.
    // TODO: Create once and reuse.
    using Match = pair<const ouinet::reqexpr::reqex, const rr::Config>;

    auto method_getter([](const Request& r) {return r.method_string();});
    auto host_getter([](const Request& r) {return r["Host"];});
    auto target_getter([](const Request& r) {return r.target();});

    const vector<Match> matches({
        // Handle requests to <http://localhost/> internally.
        Match( reqexpr::from_regex(host_getter, "localhost")
             , {false, queue<responder>({responder::_front_end})} ),

        // NOTE: The matching of HTTP methods below can be simplified,
        // leaving expanded for readability.

        // NOTE: The injector mechanism is temporarily used in some matches
        // instead of the mechanisms following it (commented out)
        // since the later are not implemented yet.

        // Send unsafe HTTP method requests to the origin server
        // (or the proxy if that does not work).
        // NOTE: The cache need not be disabled as it should know not to
        // fetch requests in these cases.
        Match( !reqexpr::from_regex(method_getter, "(GET|HEAD|OPTIONS|TRACE)")
             , {false, queue<responder>({responder::injector/*responder::origin, responder::proxy*/})} ),
        // Do not use cache for safe but uncacheable HTTP method requests.
        // NOTE: same as above.
        Match( reqexpr::from_regex(method_getter, "(OPTIONS|TRACE)")
             , {false, queue<responder>({responder::injector/*responder::origin, responder::proxy*/})} ),
        // Do not use cache for validation HEADs.
        // Caching these is not yet supported.
        Match( reqexpr::from_regex(method_getter, "HEAD")
             , {false, queue<responder>({responder::injector})} ),
        // Force cache and default mechanisms for this site.
        Match( reqexpr::from_regex(target_getter, "https?://(www\\.)?example.com/.*")
             , {true, queue<responder>()} ),
        // Force cache and particular mechanisms for this site.
        Match( reqexpr::from_regex(target_getter, "https?://(www\\.)?example.net/.*")
             , {true, queue<responder>({responder::injector})} ),
    });

    // Process the different requests that may come over the same connection.
    for (;;) {  // continue for next request; break for no more requests
        Request req;

        // Read the (clear-text) HTTP request
        ASYNC_DEBUG(http::async_read(con, buffer, req, yield[ec]), "Read request");

        if (ec == http::error::end_of_stream) break;
        if (ec) return fail(ec, "read");

        // Attempt connection to origin for CONNECT requests
        if (req.method() == http::verb::connect) {
            return ASYNC_DEBUG( handle_connect_request(con, req, yield)
                              , "Connect ", req.target());
        }

        request_config = route_choose_config(req, matches, default_request_config);

        auto res = ASYNC_DEBUG( cache_control.fetch(req, yield[ec])
                              , "Fetch "
                              , req.target());

        if (ec) {
#ifndef NDEBUG
            cerr << "----- WARNING: Error fetching --------" << endl;
            cerr << "Error Code: " << ec.message() << endl;
            cerr << req.base() << res.base() << endl;
            cerr << "--------------------------------------" << endl;
#endif

            // TODO: Better error message.
            ASYNC_DEBUG(handle_bad_request(con, req, "Not cached", yield), "Send error");
            if (req.keep_alive()) continue;
            else return;
        }

        // Forward the response back
        ASYNC_DEBUG(http::async_write(con, res, yield[ec]), "Write response ", req.target());
        if (ec == http::error::end_of_stream) break;
        if (ec) return fail(ec, "write");
    }
}

//------------------------------------------------------------------------------
void Client::State::do_listen(asio::yield_context yield)
{
    const auto local_endpoint = _config.local_endpoint();
    const auto ipns = _config.ipns();

    sys::error_code ec;

    // Open the acceptor
    tcp::acceptor acceptor(_ios);

    acceptor.open(local_endpoint.protocol(), ec);
    if (ec) return fail(ec, "open");

    acceptor.set_option(asio::socket_base::reuse_address(true));

    // Bind to the server address
    acceptor.bind(local_endpoint, ec);
    if (ec) return fail(ec, "bind");

    // Start listening for connections
    acceptor.listen(asio::socket_base::max_connections, ec);
    if (ec) return fail(ec, "listen");

    auto shutdown_acceptor_slot = _shutdown_signal.connect([&acceptor] {
        acceptor.close();
    });

    if (ipns.size()) {
        ipfs_cache::Client cache( _ios
                                , ipns
                                , (_config.repo_root()/"ipfs").native());

        _ipfs_cache = make_unique<ipfs_cache::Client>(move(cache));
    }

    auto shutdown_ipfs_slot = _shutdown_signal.connect([this] {
        _ipfs_cache = nullptr;
    });

    cout << "Client accepting on " << acceptor.local_endpoint() << endl;

    WaitCondition wait_condition(_ios); 

    for(;;)
    {
        tcp::socket socket(_ios);

        acceptor.async_accept(socket, yield[ec]);
        if(ec) {
            if (ec == asio::error::operation_aborted) break;
            fail(ec, "accept");
            if (!async_sleep(_ios, chrono::seconds(1), _shutdown_signal, yield)) {
                break;
            }
        } else {
            static const auto tcp_shutter = [](tcp::socket& s) {
                s.shutdown(tcp::socket::shutdown_both);
                s.close();
            };

            auto connection
                = make_shared<GenericConnection>(move(socket), tcp_shutter);

            asio::spawn( _ios
                       , [ this
                         , connection
                         , lock = wait_condition.lock()
                         ](asio::yield_context yield) mutable {
                             serve_request(*connection, yield);
                         });
        }
    }

    wait_condition.wait(yield);
}

//------------------------------------------------------------------------------
// NOTE: If you modify the signature of this function, please make sure you
// don't break Android build.
bool Client::State::start(int argc, char* argv[])
{
    ClientConfig config;

    try {
        config = ClientConfig(argc, argv);
    }
    catch (std::exception& e) {
        cerr << e.what() << endl;
        return false;
    }

    _config = config;

#ifndef __ANDROID__
    if (exists(_config.repo_root()/OUINET_PID_FILE)) {
        cerr << "Existing PID file " << config.repo_root()/OUINET_PID_FILE
             << "; another client process may be running"
             << ", otherwise please remove the file." << endl;
        return false;
    }
    // Acquire a PID file for the life of the process
    util::PidFile pid_file(config.repo_root()/OUINET_PID_FILE);
#endif

    asio::spawn
        ( _ios
        , [this]
          (asio::yield_context yield) {
              auto injector_ep = _config.injector_endpoint();

#ifdef USE_GNUNET
              if (is_gnunet_endpoint(injector_ep)) {
                  namespace gc = gnunet_channels;

                  string gnunet_cfg
                      = (_config.repo_root()/"gnunet"/"peer.conf").native();

                  auto service = make_unique<gc::Service>(gnunet_cfg, _ios);

                  sys::error_code ec;

                  cout << "Setting up GNUnet ..." << endl;
                  service->async_setup(yield[ec]);

                  if (ec) {
                      cerr << "Failed to setup GNUnet service: "
                           << ec.message() << endl;
                      return;
                  }

                  cout << "GNUnet ID: " << service->identity() << endl;

                  _gnunet_service = move(service);
              } else
#endif
              if (is_i2p_endpoint(injector_ep)) {
                  auto ep = boost::get<I2PEndpoint>(injector_ep).pubkey;

                  i2poui::Service service((_config.repo_root()/"i2p").native(), _ios);
                  sys::error_code ec;
                  i2poui::Connector connector = service.build_connector(ep, yield[ec]);

                  if (ec) {
                      cerr << "Failed to setup I2Poui service: "
                           << ec.message() << endl;
                      return;
                  }

                  _i2p = make_unique<I2P>(I2P{move(service), move(connector)});
              }

              do_listen(yield);
          });

    return true;
}

//------------------------------------------------------------------------------
Client::Client(asio::io_service& ios)
    : _state(make_unique<State>(ios))
{}

Client::~Client()
{
}

bool Client::start(int argc, char* argv[])
{
    return _state->start(argc, argv);
}

void Client::stop()
{
    _state->stop();
}

//------------------------------------------------------------------------------
#ifndef __ANDROID__
int main(int argc, char* argv[])
{
    asio::io_service ios;

    Signal<void()> shutdown_signal;

    asio::signal_set signals(ios, SIGINT, SIGTERM);

    Client client(ios);

    signals.async_wait([&client](const sys::error_code& ec, int signal_number) {
            client.stop();
        });

    if (!client.start(argc, argv)) {
        cerr << "Failed to start the client." << endl;
        return 1;
    }

    ios.run();

    // TODO: Remove this once work on clean exit is done.
    cerr << "Clean exit" << endl;

    return EXIT_SUCCESS;
}
#endif
