#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_NORMAL_DELEGATE_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_NORMAL_DELEGATE_20170307

// Copyright 2011 Dean Michael Berris (dberris@google.com).
// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2011-2017 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <functional>
#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/streambuf.hpp>

#include "local_connection_delegate.hpp"

namespace boost {
namespace network {
namespace http {
namespace impl {

struct local_normal_delegate : local_connection_delegate {
  explicit local_normal_delegate(boost::asio::io_service &service);

  void connect(boost::asio::local::stream_protocol::endpoint &endpoint,
               std::function<void(boost::system::error_code const &)> handler) override;
  void write(boost::asio::streambuf &command_streambuf,
             std::function<void(boost::system::error_code const &, size_t)> handler)
      override;
  void read_some(boost::asio::mutable_buffers_1 const &read_buffer,
                 std::function<void(boost::system::error_code const &, size_t)> handler)
      override;
  void disconnect() override;
  ~local_normal_delegate() override = default;

  local_normal_delegate(local_normal_delegate const &) = delete;
  local_normal_delegate &operator=(local_normal_delegate) = delete;

 private:
  boost::asio::io_service &service_;
  std::unique_ptr<boost::asio::local::stream_protocol::socket> socket_;
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#ifdef BOOST_NETWORK_NO_LIB
#include "local_normal_delegate.ipp"
#endif /* BOOST_NETWORK_NO_LIB */

#endif // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_NORMAL_DELEGATE_20110819
