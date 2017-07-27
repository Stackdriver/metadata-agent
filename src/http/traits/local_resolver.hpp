#ifndef BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_LOCAL_RESOLVER_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_LOCAL_RESOLVER_20170307

// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2017 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "../local_tags.hpp"

#include <boost/network/protocol/http/traits/resolver.hpp>
#include <boost/asio/ip/basic_resolver.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/network/protocol/http/tags.hpp>

namespace boost {
namespace network {
namespace http {

template<> struct resolver<tags::http_async_8bit_local_resolve> {
  typedef boost::asio::ip::basic_resolver<boost::asio::local::stream_protocol> type;
};

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_TRAITS_LOCAL_RESOLVER_20170307
