#pragma once

#include <boost/asio/io_service.hpp>
#include <boost/asio/spawn.hpp>

#include "fail.h"
#include "or_throw.h"
#include "generic_connection.h"
#include "util/signal.h"
#include "util.h"
#include "connect_to_host.h"
#include "ssl/util.h"

namespace ouinet {

// Send the HTTP request `req` over the connection `con`
// (which may be already an SSL tunnel)
// and return the HTTP response.
template<class RequestType>
inline
http::response<http::dynamic_body>
fetch_http_page( asio::io_service& ios
               , GenericConnection& con
               , RequestType req
               , Signal<void()>& abort_signal
               , asio::yield_context yield)
{
    http::response<http::dynamic_body> res;

    sys::error_code ec;

    auto close_con_slot = abort_signal.connect([&con] {
        con.close();
    });

    // Send the HTTP request to the remote host
    http::async_write(con, req, yield[ec]);

    // Ignore end_of_stream error, there may still be data in the receive
    // buffer we can read.
    if (ec == http::error::end_of_stream) {
        ec = sys::error_code();
    }

    if (ec) return or_throw(yield, ec, move(res));

    beast::flat_buffer buffer;

    // Receive the HTTP response
    http::async_read(con, buffer, res, yield[ec]);

    return or_throw(yield, ec, move(res));
}

// Retrieve the HTTP/HTTPS URL in the proxy request `req`
// (i.e. with a target like ``https://x.y/z``, not just ``/z``)
// and return the HTTP response.
template<class RequestType>
http::response<http::dynamic_body>
fetch_http_page( asio::io_service& ios
               , RequestType req
               , Signal<void()>& abort_signal
               , asio::yield_context yield)
{
    using namespace std;
    using Response = http::response<http::dynamic_body>;

    auto target = req.target().to_string();
    sys::error_code ec;

    // Parse the URL to tell HTTP/HTTPS, host, port.
    util::url_match url;
    if (!util::match_http_url(target, url)) {
        ec = asio::error::operation_not_supported;  // unsupported URL
        return or_throw<Response>(yield, ec);
    }
    string url_port;
    bool ssl(false);
    if (url.port.length() > 0)
        url_port = url.port;
    else if (url.scheme == "https") {
        url_port = "443";
        ssl = true;
    } else  // url.scheme == "http"
        url_port = "80";

    auto con = connect_to_host(ios, url.host, url_port, abort_signal, yield[ec]);
    if (ec) return or_throw<Response>(yield, ec);

    if (ssl) {
        // Subsequent access to the connection will use the encrypted channel.
        con = ssl::util::client_handshake(move(con), url.host, yield[ec]);
        if (ec) {
            cerr << "SSL client handshake error: "
                 << url.host << ": " << ec.message() << endl;
            return or_throw<Response>(yield, ec);
        }
    }

    // Now that we have a connection to the origin
    // we can send a non-proxy request to it
    // (i.e. with target "/foo..." and not "http://example.com/foo...").
    // Actually some web servers do not like the full form.
    RequestType origin_req(req);
    origin_req.target(target.substr(target.find( url.path
                                               // Length of "http://" or "https://",
                                               // do not fail on "http(s)://FOO/FOO".
                                               , url.scheme.length() + 3)));
    return fetch_http_page(ios, con, origin_req, abort_signal, yield);
}

}
