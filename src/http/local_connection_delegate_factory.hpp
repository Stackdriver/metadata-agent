#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_LOCAL_CONNECTION_DELEGATE_FACTORY_HPP_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_LOCAL_CONNECTION_DELEGATE_FACTORY_HPP_20170307

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2011-2017 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "local_connection_delegate.hpp"
#include "local_normal_delegate.hpp"
#include "local_tags.hpp"

namespace boost {
namespace network {
namespace http {
namespace impl {

struct local_normal_delegate;

#define Tag tags::http_async_8bit_local_resolve

template<> struct connection_delegate_factory<Tag> {
  typedef std::shared_ptr<local_connection_delegate> connection_delegate_ptr;
  typedef typename string<Tag>::type string_type;

  // This is the factory method that actually returns the delegate
  // instance.
  // TODO Support passing in proxy settings when crafting connections.
  static connection_delegate_ptr new_connection_delegate(
      boost::asio::io_service& service, bool https, bool always_verify_peer,
      optional<string_type> certificate_filename,
      optional<string_type> verify_path, optional<string_type> certificate_file,
      optional<string_type> private_key_file, optional<string_type> ciphers,
      long ssl_options) {
    connection_delegate_ptr delegate;
    delegate.reset(new local_normal_delegate(service));
    return delegate;
  }
};

#undef Tag

} /* impl */
} /* http */
} /* network */
} /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_LOCAL_CONNECTION_DELEGATE_FACTORY_HPP_20170307 \
          */
