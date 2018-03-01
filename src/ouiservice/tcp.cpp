#include "tcp.h"
#include "../or_throw.h"

namespace ouinet {
namespace ouiservice {

TcpOuiServiceServer::TcpOuiServiceServer(boost::asio::io_service& ios, boost::asio::ip::tcp::endpoint endpoint):
    _ios(ios),
    _acceptor(ios),
    _endpoint(endpoint),
    _in_accept(false)
{}

void TcpOuiServiceServer::start_listen(asio::yield_context yield)
{
    sys::error_code ec;

    _acceptor.open(_endpoint.protocol(), ec);
    if (ec) {
        return or_throw(yield, ec);
    }

    _acceptor.set_option(boost::asio::socket_base::reuse_address(true));

    _acceptor.bind(_endpoint, ec);
    if (ec) {
        _acceptor.close();
        return or_throw(yield, ec);
    }

    _acceptor.listen(boost::asio::socket_base::max_connections, ec);
    if (ec) {
        _acceptor.close();
        return or_throw(yield, ec);
    }
}

void TcpOuiServiceServer::stop_listen(asio::yield_context yield)
{
    if (_acceptor.is_open()) {
        _acceptor.cancel();
        _acceptor.close();
    }
}

GenericConnection TcpOuiServiceServer::accept(asio::yield_context yield)
{
    sys::error_code ec;

    boost::asio::ip::tcp::socket socket(_ios);
    _in_accept = true;
    _acceptor.async_accept(socket, yield[ec]);
    _in_accept = false;

    if (ec) {
        return or_throw<GenericConnection>(yield, ec, GenericConnection());
    }

    return GenericConnection(std::move(socket));
}

void TcpOuiServiceServer::cancel_accept()
{
    if (_acceptor.is_open()) {
        _acceptor.cancel();
    }
}

bool TcpOuiServiceServer::is_accepting()
{
    return _in_accept;
}

} // ouiservice namespace
} // ouinet namespace
