#ifndef BOOST_NETWORK_PROTOCOL_HTTP_LOCAL_TAGS_HPP_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_LOCAL_TAGS_HPP_20170307

// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2017 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/tags.hpp>
#include <boost/network/protocol/http/tags.hpp>

namespace boost {
namespace network {
namespace http {
namespace tags {

struct local {
  typedef mpl::true_::type is_local;
};

using namespace boost::network::tags;

template <class Tag>
struct components;

typedef mpl::vector<http, client, simple, async, local, default_string>
    http_async_8bit_local_resolve_tags;
BOOST_NETWORK_DEFINE_TAG(http_async_8bit_local_resolve);

} /* tags */

} /* http */

} /* network */

} /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_LOCAL_TAGS_HPP_20170307 */
