#include "local_stream_http.h"

namespace boost { namespace network { namespace http { namespace impl {

#if 0
async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::new_connection(
    boost::function<void (boost::asio::ip::basic_resolver<boost::asio::local::stream_protocol, boost::asio::ip::resolver_service<boost::asio::local::stream_protocol> >&, std::string, unsigned short, boost::function<void (boost::system::error_code const&, std::pair<boost::asio::ip::basic_resolver_iterator<boost::asio::local::stream_protocol>, boost::asio::ip::basic_resolver_iterator<boost::asio::local::stream_protocol> >)>)>,
    boost::asio::ip::basic_resolver<boost::asio::local::stream_protocol, boost::asio::ip::resolver_service<boost::asio::local::stream_protocol> >&,
    bool,
    bool,
    bool,
    boost::optional<std::string>,
    boost::optional<std::string> const&,
    boost::optional<std::string>,
    boost::optional<std::string>)
#endif

#define Tag tags::http_async_8bit_local_resolve
#define version_major 1
#define version_minor 1

async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::connection_ptr async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::new_connection(
    async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::resolve_function resolve,
    async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::resolver_type & resolver,
    bool follow_redirect,
    bool always_verify_peer,
    bool https,
    int timeout,
    optional<async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::string_type> certificate_filename,
    optional<async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::string_type> const & verify_path,
    optional<async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::string_type> certificate_file,
    optional<async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::string_type> private_key_file,
    optional<async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::string_type> ciphers,
    long ssl_options) {
  typedef http_async_connection<Tag,version_major,version_minor>
      async_connection;
  typedef typename delegate_factory<Tag>::type delegate_factory_type;
  async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>::connection_ptr temp;
  temp.reset(
      new async_connection(
          resolver,
          resolve,
          follow_redirect,
          delegate_factory_type::new_connection_delegate(
              resolver.get_io_service(),
              https,
              timeout,
              always_verify_peer,
              certificate_filename,
              verify_path,
              certificate_file,
              private_key_file,
              ciphers,
              ssl_options)));
  BOOST_ASSERT(temp.get() != 0);
  return temp;
}

#undef version_minor
#undef version_major
#undef Tag

} /* impl */
} /* http */
} /* network */
} /* boost */
