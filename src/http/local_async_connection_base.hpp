#ifndef BOOST_NETWORK_PROTOCOL_HTTP_IMPL_LOCAL_ASYNC_CONNECTION_BASE_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_IMPL_LOCAL_ASYNC_CONNECTION_BASE_20170307

// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2013-2017 Google, Inc.
// Copyright 2010 Dean Michael Berris <dberris@google.com>
// Copyright 2010 (C) Sinefunc, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/network/traits/string.hpp>
#include <boost/network/protocol/http/response.hpp>
#include <boost/network/protocol/http/traits/delegate_factory.hpp>
#include "traits/local_resolver.hpp"
#include <boost/network/protocol/http/traits/resolver_policy.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

template<class Tag, unsigned int version_major, unsigned int version_minor>
struct async_connection_base;

template<class Tag, unsigned int version_major, unsigned int version_minor>
struct http_async_connection;

#define Tag tags::http_async_8bit_local_resolve
#define version_major 1
#define version_minor 1

template<> struct async_connection_base<Tag, version_major, version_minor> {
  typedef async_connection_base<Tag, version_major, version_minor> this_type;
  typedef resolver_policy<Tag>::type resolver_base;
  typedef resolver_base::resolver_type resolver_type;
  typedef resolver_base::resolve_function resolve_function;
  typedef string<Tag>::type string_type;
  typedef basic_request<Tag> request;
  typedef basic_response<Tag> response;
  typedef iterator_range<char const *> char_const_range;
  typedef function<void(char_const_range const &, system::error_code const &)>
      body_callback_function_type;
  typedef function<bool(string_type &)> body_generator_function_type;
  typedef shared_ptr<this_type> connection_ptr;

  // This is the factory function which constructs the appropriate async
  // connection implementation with the correct delegate chosen based on
  // the
  // tag.
  static connection_ptr new_connection(
      resolve_function resolve, resolver_type &resolver, bool follow_redirect,
      bool always_verify_peer, bool https, int timeout,
      optional<string_type> certificate_filename = optional<string_type>(),
      optional<string_type> const &verify_path = optional<string_type>(),
      optional<string_type> certificate_file = optional<string_type>(),
      optional<string_type> private_key_file = optional<string_type>(),
      optional<string_type> ciphers = optional<string_type>(),
      long ssl_options = 0);

  // This is the pure virtual entry-point for all asynchronous
  // connections.
  virtual response start(request const &request, string_type const &method,
                         bool get_body, body_callback_function_type callback,
                         body_generator_function_type generator) = 0;

  virtual ~async_connection_base() {}
};

#undef version_minor
#undef version_major
#undef Tag

}  // namespace impl

}  // namespace http

}  // namespace network

}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_IMPL_LOCAL_ASYNC_CONNECTION_BASE_20170307
