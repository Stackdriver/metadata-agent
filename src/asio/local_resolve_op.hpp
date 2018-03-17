//
// detail/local_resolve_op.hpp
// ~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2017 Google, Inc.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_LOCAL_RESOLVE_OP_HPP
#define BOOST_ASIO_DETAIL_LOCAL_RESOLVE_OP_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>
#include <boost/asio/error.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/ip/basic_resolver_iterator.hpp>
#include <boost/asio/ip/basic_resolver_query.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/detail/addressof.hpp>
#include <boost/asio/detail/bind_handler.hpp>
#include <boost/asio/detail/fenced_block.hpp>
#include <boost/asio/detail/handler_alloc_helpers.hpp>
#include <boost/asio/detail/handler_invoke_helpers.hpp>
#include <boost/asio/detail/operation.hpp>
#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/detail/resolve_op.hpp>
#include <network/uri/detail/decode.hpp>
#include <sstream>
#include <sys/stat.h>

#include "../logging.h"

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

#define Protocol boost::asio::local::stream_protocol

template<typename Handler>
class resolve_op<Protocol, Handler> : public operation
{
public:
  BOOST_ASIO_DEFINE_HANDLER_PTR(resolve_op);

  typedef boost::asio::ip::basic_resolver_query<Protocol> query_type;
  typedef boost::asio::ip::basic_resolver_iterator<Protocol> iterator_type;

  resolve_op(socket_ops::weak_cancel_token_type cancel_token,
      const query_type& query, io_service_impl& ios, Handler& handler)
    : operation(&resolve_op::do_complete),
      cancel_token_(cancel_token),
      query_(query),
      io_service_impl_(ios),
      handler_(BOOST_ASIO_MOVE_CAST(Handler)(handler))
  {
  }

  ~resolve_op()
  {
  }

  static void do_complete(io_service_impl* owner, operation* base,
      const boost::system::error_code& /*ec*/,
      std::size_t /*bytes_transferred*/)
  {
    // Take ownership of the operation object.
    resolve_op* o(static_cast<resolve_op*>(base));
    ptr p = { boost::asio::detail::addressof(o->handler_), o, o };

    if (owner && owner != &o->io_service_impl_)
    {
      // The operation is being run on the worker io_service. Time to perform
      // the resolver operation.
    
      // Perform the blocking host resolution operation.
      if (o->cancel_token_.expired())
        o->ec_ = boost::asio::error::operation_aborted;
      else {
        //LOG(ERROR) << "async_resolve() " << o->query_.host_name();
        std::string path = ::network::detail::decode(o->query_.host_name());
        //LOG(ERROR) << "decoded " << path;
        struct stat buffer;
        int result = stat(path.c_str(), &buffer);
        if (result || !S_ISSOCK(buffer.st_mode)) {
          LOG(ERROR) << "resolve: unable to stat or not a socket " << path;
          o->ec_ = boost::asio::error::host_not_found;
        } else {
          //LOG(ERROR) << "resolve: everything ok with " << path;
          o->ec_ = boost::system::error_code();
        }
      }

      // Pass operation back to main io_service for completion.
      o->io_service_impl_.post_deferred_completion(o);
      p.v = p.p = 0;
    }
    else
    {
      // The operation has been returned to the main io_service. The completion
      // handler is ready to be delivered.

      BOOST_ASIO_HANDLER_COMPLETION((o));

      // Make a copy of the handler so that the memory can be deallocated
      // before the upcall is made. Even if we're not about to make an upcall,
      // a sub-object of the handler may be the true owner of the memory
      // associated with the handler. Consequently, a local copy of the handler
      // is required to ensure that any owning sub-object remains valid until
      // after we have deallocated the memory here.
      detail::binder2<Handler, boost::system::error_code, iterator_type>
        handler(o->handler_, o->ec_, iterator_type());
      p.h = boost::asio::detail::addressof(handler.handler_);
      if (!o->ec_)
      {
        std::string path = ::network::detail::decode(o->query_.host_name());
        handler.arg2_ = iterator_type::create(
            Protocol::endpoint(path),
            path, o->query_.service_name());
      }
      p.reset();

      if (owner)
      {
        fenced_block b(fenced_block::half);
        BOOST_ASIO_HANDLER_INVOCATION_BEGIN((handler.arg1_, "..."));
        boost_asio_handler_invoke_helpers::invoke(handler, handler.handler_);
        BOOST_ASIO_HANDLER_INVOCATION_END;
      }
    }
  }

private:
  socket_ops::weak_cancel_token_type cancel_token_;
  query_type query_;
  io_service_impl& io_service_impl_;
  Handler handler_;
  boost::system::error_code ec_;
};

#undef Protocol

} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // BOOST_ASIO_DETAIL_LOCAL_RESOLVE_OP_HPP
