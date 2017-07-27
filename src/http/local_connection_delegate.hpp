#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_CONNECTION_DELEGATE_HPP_
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_CONNECTION_DELEGATE_HPP_

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2011-2017 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <functional>

#include <boost/asio/local/stream_protocol.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

struct local_connection_delegate {
  // TODO: this is similar enough to connection_delegate that it may be possible to refactor.
  virtual void connect(boost::asio::local::stream_protocol::endpoint &endpoint,
                       std::function<void(system::error_code const &)> handler) = 0;
  virtual void write(
      asio::streambuf &command_streambuf,
      std::function<void(system::error_code const &, size_t)> handler) = 0;
  virtual void read_some(
      asio::mutable_buffers_1 const &read_buffer,
      std::function<void(system::error_code const &, size_t)> handler) = 0;
  virtual void disconnect() = 0;
  virtual ~local_connection_delegate() {}
};

} /* impl */

} /* http */

} /* network */

} /* boost */

#endif /* BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_CONNECTION_DELEGATE_HPP_ \
          */
