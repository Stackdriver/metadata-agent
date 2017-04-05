#ifndef BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_STREAM_DELEGATE_20170307
#define BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_STREAM_DELEGATE_20170307

// Copyright 2017 Igor Peshansky (igorp@google.com).
// Copyright 2017 Google, Inc.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <memory>
#include <cstdint>
#include <functional>
#include <boost/asio/io_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/asio/placeholders.hpp>
#include <boost/asio/streambuf.hpp>

namespace boost {
namespace network {
namespace http {
namespace impl {

struct local_connection_delegate {
  // TODO: this is similar enough to connection_delegate that it may be possible to refactor.
  virtual void connect(boost::asio::local::stream_protocol::endpoint &endpoint,
                       std::function<void(boost::system::error_code const &)> handler) = 0;
  virtual void write(boost::asio::streambuf &command_streambuf,
                     std::function<void(boost::system::error_code const &, size_t)> handler) = 0;
  virtual void read_some(boost::asio::mutable_buffers_1 const &read_buffer,
                         std::function<void(boost::system::error_code const &, size_t)> handler) = 0;
  virtual ~local_connection_delegate() {}
};

struct local_stream_delegate : local_connection_delegate {
  explicit local_stream_delegate(boost::asio::io_service &service);

  void connect(boost::asio::local::stream_protocol::endpoint &endpoint,
               std::function<void(boost::system::error_code const &)> handler) override;
  void write(boost::asio::streambuf &command_streambuf,
             std::function<void(boost::system::error_code const &, size_t)> handler)
      override;
  void read_some(boost::asio::mutable_buffers_1 const &read_buffer,
                 std::function<void(boost::system::error_code const &, size_t)> handler)
      override;
  ~local_stream_delegate() override = default;

  local_stream_delegate(local_stream_delegate const &) = delete;
  local_stream_delegate &operator=(local_stream_delegate) = delete;

 private:
  boost::asio::io_service &service_;
  std::unique_ptr<boost::asio::local::stream_protocol::socket> socket_;
};

}  // namespace impl
}  // namespace http
}  // namespace network
}  // namespace boost

#endif  // BOOST_NETWORK_PROTOCOL_HTTP_CLIENT_CONNECTION_LOCAL_STREAM_DELEGATE_20170307
