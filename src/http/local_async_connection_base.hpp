#ifndef BOOST_NETWORK_PROTOCOL_HTTP_IMPL_LOCAL_ASYNC_CONNECTION_BASE_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_IMPL_LOCAL_ASYNC_CONNECTION_BASE_20170307

// Note: This file is mostly a copy of
// boost/network/protocol/http/client/connection/async_base.hpp,
// except for the specialized bits.

// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2013-2017 Google, Inc.
// Copyright 2010 Dean Michael Berris <dberris@google.com>
// Copyright 2010 (C) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/protocol/http/response.hpp>
#include <boost/network/protocol/http/traits/delegate_factory.hpp>
#include "traits/local_resolver.hpp"
#include <boost/network/protocol/http/traits/resolver_policy.hpp>
#include <boost/network/traits/string.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

template <class Tag, unsigned version_major, unsigned version_minor>
struct async_connection_base;

template<class Tag, unsigned int version_major, unsigned int version_minor>
struct http_async_connection;

#define Tag tags::http_async_8bit_local_resolve
#define version_major 1
#define version_minor 1

template<> struct async_connection_base<Tag, version_major, version_minor> {
  typedef async_connection_base<Tag, version_major, version_minor> this_type;
  typedef typename resolver_policy<Tag>::type resolver_base;
  typedef typename resolver_base::resolver_type resolver_type;
  typedef typename resolver_base::resolve_function resolve_function;
  typedef typename string<Tag>::type string_type;
  typedef basic_request<Tag> request;
  typedef basic_response<Tag> response;
  typedef typename std::array<typename char_<Tag>::type, 1024>::const_iterator const_iterator;
  typedef iterator_range<const_iterator> char_const_range;
  typedef std::function<void(char_const_range const &, boost::system::error_code const &)>
      body_callback_function_type;
  typedef std::function<bool(string_type &)> body_generator_function_type;
  typedef std::shared_ptr<this_type> connection_ptr;

  // This is the factory function which constructs the appropriate async
  // connection implementation with the correct delegate chosen based on the
  // tag.
  static connection_ptr new_connection(
      resolve_function resolve, resolver_type &resolver, bool follow_redirect,
      bool always_verify_peer, bool https, int timeout,
      bool remove_chunk_markers,
      optional<string_type> certificate_filename = optional<string_type>(),
      optional<string_type> const &verify_path = optional<string_type>(),
      optional<string_type> certificate_file = optional<string_type>(),
      optional<string_type> private_key_file = optional<string_type>(),
      optional<string_type> ciphers = optional<string_type>(),
      optional<string_type> sni_hostname = optional<string_type>(),
      long ssl_options = 0);

  // This is the pure virtual entry-point for all asynchronous
  // connections.
  virtual response start(request const &request, string_type const &method,
                         bool get_body, body_callback_function_type callback,
                         body_generator_function_type generator) = 0;

  virtual ~async_connection_base() = default;
};

#undef version_minor
#undef version_major
#undef Tag

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_IMPL_LOCAL_ASYNC_CONNECTION_BASE_20170307
