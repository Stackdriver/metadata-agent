#ifndef BOOST_NETWORK_PROTOCOL_HTTP_IMPL_LOCAL_ASYNC_CONNECTION_BASE_IPP_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_IMPL_LOCAL_ASYNC_CONNECTION_BASE_IPP_20170307

// Note: This file is mostly a copy of
// boost/network/protocol/http/client/connection/async_base.hpp,
// except for the specialized bits.

// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copryight 2013-2017 Google, Inc.
// Copyright 2010 Dean Michael Berris <dberris@google.com>
// Copyright 2010 (C) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/traits/delegate_factory.hpp>
#include "local_async_normal.hpp"
#include "local_async_connection_base.hpp"

namespace boost {
namespace network {
namespace http {
namespace impl {

#define Tag tags::http_async_8bit_local_resolve
#define version_major 1
#define version_minor 1

using acb = async_connection_base<Tag, version_major, version_minor>;

acb::connection_ptr acb::new_connection(
    acb::resolve_function resolve, acb::resolver_type &resolver,
    bool follow_redirect, bool always_verify_peer, bool https, int timeout,
    bool remove_chunk_markers,
    optional<acb::string_type> certificate_filename,
    optional<acb::string_type> const &verify_path,
    optional<acb::string_type> certificate_file,
    optional<acb::string_type> private_key_file,
    optional<acb::string_type> ciphers,
    optional<string_type> sni_hostname,
    long ssl_options) {
  typedef http_async_connection<Tag, version_major, version_minor>
      async_connection;
  typedef typename delegate_factory<Tag>::type delegate_factory_type;
  auto delegate = delegate_factory_type::new_connection_delegate(
      resolver.get_io_service(), https, always_verify_peer,
      certificate_filename, verify_path, certificate_file, private_key_file,
      ciphers, sni_hostname, ssl_options);
  auto temp = std::make_shared<async_connection>(
      resolver, resolve, follow_redirect, timeout, remove_chunk_markers,
      std::move(delegate));
  BOOST_ASSERT(temp != nullptr);
  return temp;
}

#undef version_minor
#undef version_major
#undef Tag

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_IMPL_LOCAL_ASYNC_CONNECTION_BASE_IPP_20170307
