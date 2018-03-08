#pragma once

#include "generic_connection.h"
#include "util/wait_condition.h"

namespace ouinet {

inline
void full_duplex( GenericConnection& c1
                , GenericConnection& c2
                , asio::yield_context& yield)
{
    static const auto half_duplex
        = []( GenericConnection& in
            , GenericConnection& out
            , asio::yield_context& yield)
    {
        sys::error_code ec;
        std::array<uint8_t, 2048> data;
    
        for (;;) {
            size_t length = in.async_read_some(asio::buffer(data), yield[ec]);
            if (ec) break;
    
            asio::async_write(out, asio::buffer(data, length), yield[ec]);
            if (ec) break;
        }
    };

    assert(&c1.get_io_service() == &c2.get_io_service());

    WaitCondition wait_condition(c1.get_io_service());

    asio::spawn
        ( yield
        , [&, b = wait_condition.lock()](asio::yield_context yield) {
              half_duplex(c1, c2, yield);
          });

    asio::spawn
        ( yield
        , [&, b = wait_condition.lock()](asio::yield_context yield) {
              half_duplex(c2, c1, yield);
          });

    wait_condition.wait(yield);
}

} // ouinet namespace
