#pragma once

namespace ouinet {

class Yield {
  public:
    Yield(asio::yield_context asio_yield)
        : _asio_yield(asio_yield)
    {}

  private:
    template<class... As> friend class CoroHandler;
    asio::yield_context _asio_yield;
};

template<class... Args>
class CoroHandler
{
  private:
    template<class... As> friend class ::boost::asio::async_result;

    using AsioHandler = typename asio::handler_type<asio::yield_context,
          void(sys::error_code, Args&&...) >::type;

  public:
    CoroHandler(Yield ctx)
        : _asio_handler(ctx._asio_yield)
    { }
  
    void operator()(sys::error_code ec, Args&&... args)
    {
        _asio_handler(ec, args...);
    }

  private:
    AsioHandler _asio_handler;
};

} // ouinet namespace


namespace boost { namespace asio {

template< typename ReturnType
        , class... Args
        >
struct handler_type< ouinet::Yield
                   , ReturnType(boost::system::error_code, Args&&...)
                   >
{
    typedef ouinet::CoroHandler<Args...> type;
};

template<typename... Args>
class async_result<ouinet::CoroHandler<Args...>>
{
  private:
    using AsioResult = async_result<typename ouinet::CoroHandler<Args...>::AsioHandler>;

  public:
    using type = typename AsioResult::type;
  
    explicit async_result(ouinet::CoroHandler<Args...>& h)
      : _asio_result(h._asio_handler)
    {
    }
  
    type get()
    {
      return _asio_result.get();
    }
  
  private:
    AsioResult _asio_result;
};

}} // boost::asio namespace
