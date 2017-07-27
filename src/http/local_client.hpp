#ifndef BOOST_NETWORK_PROTOCOL_HTTP_LOCAL_CLIENT_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_LOCAL_CLIENT_20170307

// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2017 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/asio/local/stream_protocol.hpp>
#include <boost/network/protocol/http/client.hpp>

#include "traits/local_resolver.hpp"
#include "local_tags.hpp"

namespace boost {
namespace network {
namespace http {

typedef basic_client<tags::http_async_8bit_local_resolve, 1, 1> local_client;

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_LOCAL_CLIENT_20170307
