//
// detail/local_resolver_service.hpp
// ~~~~~~~~~~~~~~~~~~~~~~~~~~~
//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2017 Google, Inc.
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef BOOST_ASIO_DETAIL_LOCAL_RESOLVER_SERVICE_HPP
#define BOOST_ASIO_DETAIL_LOCAL_RESOLVER_SERVICE_HPP

#if defined(_MSC_VER) && (_MSC_VER >= 1200)
# pragma once
#endif // defined(_MSC_VER) && (_MSC_VER >= 1200)

#include <boost/asio/detail/config.hpp>

#if !defined(BOOST_ASIO_WINDOWS_RUNTIME)

#include <boost/asio/ip/basic_resolver_iterator.hpp>
#include <boost/asio/ip/basic_resolver_query.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/detail/addressof.hpp>
#include <boost/asio/detail/resolve_endpoint_op.hpp>
#include <boost/asio/detail/resolve_op.hpp>
#include <boost/asio/detail/resolver_service_base.hpp>
#include <boost/asio/detail/resolver_service.hpp>
#include <boost/asio/detail/socket_ops.hpp>
#include <network/uri/detail/decode.hpp>
#include <sys/stat.h>

#include "../logging.h"

#include <boost/asio/detail/push_options.hpp>

namespace boost {
namespace asio {
namespace detail {

#define Protocol boost::asio::local::stream_protocol

template<>
class resolver_service<Protocol> : public resolver_service_base
{
public:
  // The implementation type of the resolver. A cancellation token is used to
  // indicate to the background thread that the operation has been cancelled.
  typedef socket_ops::shared_cancel_token_type implementation_type;

  // The endpoint type.
  typedef typename Protocol::endpoint endpoint_type;

  // The query type.
  typedef boost::asio::ip::basic_resolver_query<Protocol> query_type;

  // The iterator type.
  typedef boost::asio::ip::basic_resolver_iterator<Protocol> iterator_type;

  // Constructor.
  resolver_service(boost::asio::io_service& io_service)
    : resolver_service_base(io_service)
  {
    //LOG(ERROR) << "Constructing resolver_service()";
  }

  // Resolve a query to a list of entries.
  iterator_type resolve(implementation_type&, const query_type& query,
      boost::system::error_code& ec)
  {
    //LOG(ERROR) << "resolve() " << query.host_name();
    std::string path = ::network::detail::decode(query.host_name());
    //LOG(ERROR) << "decoded " << path;
    struct stat buffer;
    int result = stat(path.c_str(), &buffer);
    if (result || !S_ISSOCK(buffer.st_mode)) {
      LOG(ERROR) << "resolve: unable to stat or not a socket " << path;
      ec = boost::asio::error::host_not_found;
    } else {
      //LOG(ERROR) << "resolve: everything ok with " << path;
      ec = boost::system::error_code();
    }
    return ec ? iterator_type() : iterator_type::create(
        endpoint_type(path), path, query.service_name());
  }

  // Asynchronously resolve a query to a list of entries.
  template <typename Handler>
  void async_resolve(implementation_type& impl,
      const query_type& query, Handler& handler)
  {
    // Allocate and construct an operation to wrap the handler.
    typedef resolve_op<Protocol, Handler> op;
    typename op::ptr p = { boost::asio::detail::addressof(handler),
      boost_asio_handler_alloc_helpers::allocate(
        sizeof(op), handler), 0 };
    p.p = new (p.v) op(impl, query, io_service_impl_, handler);

    BOOST_ASIO_HANDLER_CREATION((p.p, "resolver", &impl, "async_resolve"));

    start_resolve_op(p.p);
    p.v = p.p = 0;
  }

  // Resolve an endpoint to a list of entries.
  iterator_type resolve(implementation_type&,
      const endpoint_type& endpoint, boost::system::error_code& ec)
  {
    // An existing endpoint always resolves.
    return iterator_type::create(endpoint, "", "");
  }

  // Asynchronously resolve an endpoint to a list of entries.
  template <typename Handler>
  void async_resolve(implementation_type& impl,
      const endpoint_type& endpoint, Handler& handler)
  {
    // Allocate and construct an operation to wrap the handler.
    typedef resolve_endpoint_op<Protocol, Handler> op;
    typename op::ptr p = { boost::asio::detail::addressof(handler),
      boost_asio_handler_alloc_helpers::allocate(
        sizeof(op), handler), 0 };
    p.p = new (p.v) op(impl, endpoint, io_service_impl_, handler);

    BOOST_ASIO_HANDLER_CREATION((p.p, "resolver", &impl, "async_resolve"));

    start_resolve_op(p.p);
    p.v = p.p = 0;
  }
};

#undef Protocol

} // namespace detail
} // namespace asio
} // namespace boost

#include <boost/asio/detail/pop_options.hpp>

#endif // !defined(BOOST_ASIO_WINDOWS_RUNTIME)

#endif // BOOST_ASIO_DETAIL_LOCAL_RESOLVER_SERVICE_HPP
