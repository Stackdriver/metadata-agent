#ifndef LOCAL_STREAM_HTTP
#define LOCAL_STREAM_HTTP

// Copyright Dean Michael Berris 2010.
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include <boost/asio/detail/operation.hpp>
#include <boost/asio/detail/socket_ops.hpp>
#include <boost/asio/ip/basic_resolver_iterator.hpp>
#include <boost/asio/ip/basic_resolver_query.hpp>
#include <boost/asio/ip/resolver_service.hpp>
#include <boost/asio/local/stream_protocol.hpp>
#include <boost/network/protocol/http/client.hpp>
#include <boost/network/protocol/http/tags.hpp>
#include <boost/network/tags.hpp>
#include <boost/network/uri.hpp>
#include <sys/stat.h>

#include "local_stream_delegate.hpp"
#include "logging.h"

namespace boost {

namespace asio {

namespace detail {

template<typename Handler>
class resolve_op<boost::asio::local::stream_protocol, Handler> : public operation
{
public:
  BOOST_ASIO_DEFINE_HANDLER_PTR(resolve_op);

  typedef boost::asio::ip::basic_resolver_query<boost::asio::local::stream_protocol> query_type;
  typedef boost::asio::ip::basic_resolver_iterator<boost::asio::local::stream_protocol> iterator_type;

  resolve_op(socket_ops::weak_cancel_token_type cancel_token,
      const query_type& query, io_service_impl& ios, Handler& handler)
    : operation(&resolve_op::do_complete),
      cancel_token_(cancel_token),
      query_(query),
      io_service_impl_(ios),
      handler_(BOOST_ASIO_MOVE_CAST(Handler)(handler))
  {
  }

  ~resolve_op()
  {
  }

  static void do_complete(io_service_impl* owner, operation* base,
      const boost::system::error_code& /*ec*/,
      std::size_t /*bytes_transferred*/)
  {
    // Take ownership of the operation object.
    resolve_op* o(static_cast<resolve_op*>(base));
    ptr p = { boost::asio::detail::addressof(o->handler_), o, o };

    if (owner && owner != &o->io_service_impl_)
    {
      // The operation is being run on the worker io_service. Time to perform
      // the resolver operation.
    
      // Perform the blocking host resolution operation.
      if (o->cancel_token_.expired())
        o->ec_ = boost::asio::error::operation_aborted;
      else {
        LOG(ERROR) << "async_resolve() " << o->query_.host_name();
        struct stat buffer;
        int result = stat(o->query_.host_name().c_str(), &buffer);
        if (result || !S_ISSOCK(buffer.st_mode)) {
          LOG(ERROR) << "resolve: unable to stat or not a socket " << o->query_.host_name();
          o->ec_ = boost::asio::error::host_not_found;
        } else {
          LOG(ERROR) << "resolve: everything ok with " << o->query_.host_name();
          o->ec_ = boost::system::error_code();
        }
      }

      // Pass operation back to main io_service for completion.
      o->io_service_impl_.post_deferred_completion(o);
      p.v = p.p = 0;
    }
    else
    {
      // The operation has been returned to the main io_service. The completion
      // handler is ready to be delivered.

      BOOST_ASIO_HANDLER_COMPLETION((o));

      // Make a copy of the handler so that the memory can be deallocated
      // before the upcall is made. Even if we're not about to make an upcall,
      // a sub-object of the handler may be the true owner of the memory
      // associated with the handler. Consequently, a local copy of the handler
      // is required to ensure that any owning sub-object remains valid until
      // after we have deallocated the memory here.
      detail::binder2<Handler, boost::system::error_code, iterator_type>
        handler(o->handler_, o->ec_, iterator_type());
      p.h = boost::asio::detail::addressof(handler.handler_);
      if (!o->ec_)
      {
        handler.arg2_ = iterator_type::create(
            boost::asio::local::stream_protocol::endpoint(o->query_.host_name()),
            o->query_.host_name(), o->query_.service_name());
      }
      p.reset();

      if (owner)
      {
        fenced_block b(fenced_block::half);
        BOOST_ASIO_HANDLER_INVOCATION_BEGIN((handler.arg1_, "..."));
        boost_asio_handler_invoke_helpers::invoke(handler, handler.handler_);
        BOOST_ASIO_HANDLER_INVOCATION_END;
      }
    }
  }

private:
  socket_ops::weak_cancel_token_type cancel_token_;
  query_type query_;
  io_service_impl& io_service_impl_;
  Handler handler_;
  boost::system::error_code ec_;
};

template<>
class resolver_service<boost::asio::local::stream_protocol> : public resolver_service_base
{
public:
  // The implementation type of the resolver. A cancellation token is used to
  // indicate to the background thread that the operation has been cancelled.
  typedef socket_ops::shared_cancel_token_type implementation_type;

  // The endpoint type.
  typedef typename boost::asio::local::stream_protocol::endpoint endpoint_type;

  // The query type.
  typedef boost::asio::ip::basic_resolver_query<boost::asio::local::stream_protocol> query_type;

  // The iterator type.
  typedef boost::asio::ip::basic_resolver_iterator<boost::asio::local::stream_protocol> iterator_type;

  // Constructor.
  resolver_service(boost::asio::io_service& io_service)
    : resolver_service_base(io_service)
  {
    LOG(ERROR) << "Constructing resolver_service()";
  }

  // Resolve a query to a list of entries.
  iterator_type resolve(implementation_type&, const query_type& query,
      boost::system::error_code& ec)
  {
    LOG(ERROR) << "resolve() " << query.host_name();
    struct stat buffer;
    int result = stat(query.host_name().c_str(), &buffer);
    if (result || !S_ISSOCK(buffer.st_mode)) {
      LOG(ERROR) << "resolve: unable to stat or not a socket " << query.host_name();
      ec = boost::asio::error::host_not_found;
    } else {
      LOG(ERROR) << "resolve: everything ok with " << query.host_name();
      ec = boost::system::error_code();
    }
    return ec ? iterator_type() : iterator_type::create(
        endpoint_type(query.host_name()), query.host_name(), query.service_name());
  }

  // Asynchronously resolve a query to a list of entries.
  template <typename Handler>
  void async_resolve(implementation_type& impl,
      const query_type& query, Handler& handler)
  {
    // Allocate and construct an operation to wrap the handler.
    typedef resolve_op<boost::asio::local::stream_protocol, Handler> op;
    typename op::ptr p = { boost::asio::detail::addressof(handler),
      boost_asio_handler_alloc_helpers::allocate(
        sizeof(op), handler), 0 };
    p.p = new (p.v) op(impl, query, io_service_impl_, handler);

    BOOST_ASIO_HANDLER_CREATION((p.p, "resolver", &impl, "async_resolve"));

    start_resolve_op(p.p);
    p.v = p.p = 0;
  }

  // Resolve an endpoint to a list of entries.
  iterator_type resolve(implementation_type&,
      const endpoint_type& endpoint, boost::system::error_code& ec)
  {
    // An existing endpoint always resolves.
    return iterator_type::create(endpoint, "", "");
  }

  // Asynchronously resolve an endpoint to a list of entries.
  template <typename Handler>
  void async_resolve(implementation_type& impl,
      const endpoint_type& endpoint, Handler& handler)
  {
    // Allocate and construct an operation to wrap the handler.
    typedef resolve_endpoint_op<boost::asio::local::stream_protocol, Handler> op;
    typename op::ptr p = { boost::asio::detail::addressof(handler),
      boost_asio_handler_alloc_helpers::allocate(
        sizeof(op), handler), 0 };
    p.p = new (p.v) op(impl, endpoint, io_service_impl_, handler);

    BOOST_ASIO_HANDLER_CREATION((p.p, "resolver", &impl, "async_resolve"));

    start_resolve_op(p.p);
    p.v = p.p = 0;
  }
};

} /* detail */

#if 0
namespace local {

template <typename InternetProtocol,
    typename ResolverService = boost::asio::ip::resolver_service<InternetProtocol> >
class basic_resolver
  : public basic_io_object<ResolverService>
{
public:
  /// The protocol type.
  typedef boost::asio::local::stream_protocol protocol_type;

  /// The endpoint type.
  typedef boost::asio::local::stream_protocol::endpoint endpoint_type;

  /// The query type.
  typedef boost::asio::ip::basic_resolver_query<InternetProtocol> query;

  /// The iterator type.
  typedef boost::asio::ip::basic_resolver_iterator<InternetProtocol> iterator;

  /// Constructor.
  /**
   * This constructor creates a basic_resolver.
   *
   * @param io_service The io_service object that the resolver will use to
   * dispatch handlers for any asynchronous operations performed on the timer.
   */
  explicit basic_resolver(boost::asio::io_service& io_service)
    : basic_io_object<ResolverService>(io_service)
  {
  }

  /// Cancel any asynchronous operations that are waiting on the resolver.
  /**
   * This function forces the completion of any pending asynchronous
   * operations on the host resolver. The handler for each cancelled operation
   * will be invoked with the boost::asio::error::operation_aborted error code.
   */
  void cancel()
  {
    return this->service.cancel(this->implementation);
  }

  /// Perform forward resolution of a query to a list of entries.
  /**
   * This function is used to resolve a query into a list of endpoint entries.
   *
   * @param q A query object that determines what endpoints will be returned.
   *
   * @returns A forward-only iterator that can be used to traverse the list
   * of endpoint entries.
   *
   * @throws boost::system::system_error Thrown on failure.
   *
   * @note A default constructed iterator represents the end of the list.
   *
   * A successful call to this function is guaranteed to return at least one
   * entry.
   */
  iterator resolve(const query& q)
  {
    boost::system::error_code ec;
    iterator i = this->resolve(q, ec);
    boost::asio::detail::throw_error(ec, "resolve");
    return i;
  }

  /// Perform forward resolution of a query to a list of entries.
  /**
   * This function is used to resolve a query into a list of endpoint entries.
   *
   * @param q A query object that determines what endpoints will be returned.
   *
   * @param ec Set to indicate what error occurred, if any.
   *
   * @returns A forward-only iterator that can be used to traverse the list
   * of endpoint entries. Returns a default constructed iterator if an error
   * occurs.
   *
   * @note A default constructed iterator represents the end of the list.
   *
   * A successful call to this function is guaranteed to return at least one
   * entry.
   */
  iterator resolve(const query& q, boost::system::error_code& ec)
  {
    const std::string& path = q.host_name();
    ifstream f(path.c_str());
    if (f.good()) {
      return iterator::create(endpoint_type(path), path, q.service());
    }
    // Piggy-back on a system failure
    return this->service.resolve(this->implementation, q, ec);
  }

  /// Asynchronously perform forward resolution of a query to a list of entries.
  /**
   * This function is used to asynchronously resolve a query into a list of
   * endpoint entries.
   *
   * @param q A query object that determines what endpoints will be returned.
   *
   * @param handler The handler to be called when the resolve operation
   * completes. Copies will be made of the handler as required. The function
   * signature of the handler must be:
   * @code void handler(
   *   const boost::system::error_code& error, // Result of operation.
   *   resolver::iterator iterator             // Forward-only iterator that can
   *                                           // be used to traverse the list
   *                                           // of endpoint entries.
   * ); @endcode
   * Regardless of whether the asynchronous operation completes immediately or
   * not, the handler will not be invoked from within this function. Invocation
   * of the handler will be performed in a manner equivalent to using
   * boost::asio::io_service::post().
   *
   * @note A default constructed iterator represents the end of the list.
   *
   * A successful resolve operation is guaranteed to pass at least one entry to
   * the handler.
   */
  template <typename ResolveHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(ResolveHandler,
      void (boost::system::error_code, iterator))
  async_resolve(const query& q,
      BOOST_ASIO_MOVE_ARG(ResolveHandler) handler)
  {
    // If you get an error on the following line it means that your handler does
    // not meet the documented type requirements for a ResolveHandler.
    BOOST_ASIO_RESOLVE_HANDLER_CHECK(
        ResolveHandler, handler, iterator) type_check;

    return this->service.async_resolve(this->implementation, q,
        BOOST_ASIO_MOVE_CAST(ResolveHandler)(handler));
  }

  /// Perform reverse resolution of an endpoint to a list of entries.
  /**
   * This function is used to resolve an endpoint into a list of endpoint
   * entries.
   *
   * @param e An endpoint object that determines what endpoints will be
   * returned.
   *
   * @returns A forward-only iterator that can be used to traverse the list
   * of endpoint entries.
   *
   * @throws boost::system::system_error Thrown on failure.
   *
   * @note A default constructed iterator represents the end of the list.
   *
   * A successful call to this function is guaranteed to return at least one
   * entry.
   */
  iterator resolve(const endpoint_type& e)
  {
    boost::system::error_code ec;
    iterator i = this->service.resolve(this->implementation, e, ec);
    boost::asio::detail::throw_error(ec, "resolve");
    return i;
  }

  /// Perform reverse resolution of an endpoint to a list of entries.
  /**
   * This function is used to resolve an endpoint into a list of endpoint
   * entries.
   *
   * @param e An endpoint object that determines what endpoints will be
   * returned.
   *
   * @param ec Set to indicate what error occurred, if any.
   *
   * @returns A forward-only iterator that can be used to traverse the list
   * of endpoint entries. Returns a default constructed iterator if an error
   * occurs.
   *
   * @note A default constructed iterator represents the end of the list.
   *
   * A successful call to this function is guaranteed to return at least one
   * entry.
   */
  iterator resolve(const endpoint_type& e, boost::system::error_code& ec)
  {
    return this->service.resolve(this->implementation, e, ec);
  }

  /// Asynchronously perform reverse resolution of an endpoint to a list of
  /// entries.
  /**
   * This function is used to asynchronously resolve an endpoint into a list of
   * endpoint entries.
   *
   * @param e An endpoint object that determines what endpoints will be
   * returned.
   *
   * @param handler The handler to be called when the resolve operation
   * completes. Copies will be made of the handler as required. The function
   * signature of the handler must be:
   * @code void handler(
   *   const boost::system::error_code& error, // Result of operation.
   *   resolver::iterator iterator             // Forward-only iterator that can
   *                                           // be used to traverse the list
   *                                           // of endpoint entries.
   * ); @endcode
   * Regardless of whether the asynchronous operation completes immediately or
   * not, the handler will not be invoked from within this function. Invocation
   * of the handler will be performed in a manner equivalent to using
   * boost::asio::io_service::post().
   *
   * @note A default constructed iterator represents the end of the list.
   *
   * A successful resolve operation is guaranteed to pass at least one entry to
   * the handler.
   */
  template <typename ResolveHandler>
  BOOST_ASIO_INITFN_RESULT_TYPE(ResolveHandler,
      void (boost::system::error_code, iterator))
  async_resolve(const endpoint_type& e,
      BOOST_ASIO_MOVE_ARG(ResolveHandler) handler)
  {
    // If you get an error on the following line it means that your handler does
    // not meet the documented type requirements for a ResolveHandler.
    BOOST_ASIO_RESOLVE_HANDLER_CHECK(
        ResolveHandler, handler, iterator) type_check;

    return this->service.async_resolve(this->implementation, e,
        BOOST_ASIO_MOVE_CAST(ResolveHandler)(handler));
  }
};

} /* local */
#endif

} /* asio */

namespace network { namespace http {

namespace tags {
  struct local { typedef mpl::true_::type is_local; };
  using namespace boost::network::tags;
  typedef mpl::vector<http, client, simple, async, local, default_string> http_async_8bit_local_resolve_tags;
  BOOST_NETWORK_DEFINE_TAG(http_async_8bit_local_resolve);
} /* tags */

typedef basic_client<tags::http_async_8bit_local_resolve, 1, 1> local_client;

template<> struct resolver<tags::http_async_8bit_local_resolve> {
  typedef boost::asio::ip::basic_resolver<boost::asio::local::stream_protocol> type;
};

#define Tag tags::http_async_8bit_local_resolve
#define version_major 1
#define version_minor 1

namespace policies {

template<>
struct async_resolver<tags::http_async_8bit_local_resolve>
    : boost::enable_shared_from_this<async_resolver<Tag> >
{
    typedef typename resolver<Tag>::type resolver_type;
    typedef typename resolver_type::iterator resolver_iterator;
    typedef typename resolver_type::query resolver_query;
    typedef std::pair<resolver_iterator, resolver_iterator> resolver_iterator_pair;
    typedef typename string<Tag>::type string_type;
    typedef boost::unordered_map<string_type, resolver_iterator_pair> endpoint_cache;
    typedef boost::function<void(boost::system::error_code const &,resolver_iterator_pair)> resolve_completion_function;
    typedef boost::function<void(resolver_type&,string_type,boost::uint16_t,resolve_completion_function)> resolve_function;
protected:
    bool cache_resolved_;
    endpoint_cache endpoint_cache_;
    boost::shared_ptr<boost::asio::io_service> service_;
    boost::shared_ptr<boost::asio::io_service::strand> resolver_strand_;

    explicit async_resolver(bool cache_resolved)
        : cache_resolved_(cache_resolved), endpoint_cache_()
    {
        
    }

    void resolve(
        resolver_type & resolver_, 
        string_type const & host, 
        boost::uint16_t port,
        resolve_completion_function once_resolved
        ) 
    {
        std::string path = boost::network::uri::decoded(host);
        if (cache_resolved_) {
            typename endpoint_cache::iterator iter = 
                endpoint_cache_.find(path);
            if (iter != endpoint_cache_.end()) {
                boost::system::error_code ignored;
                once_resolved(ignored, iter->second);
                return;
            }
        }
        
        typename resolver_type::query q(
            resolver_type::protocol_type()
            , path
            , lexical_cast<string_type>(port));
        LOG(ERROR) << "Going to resolve path " << path;
        resolver_.async_resolve(
            q,
            resolver_strand_->wrap(
                boost::bind(
                    &async_resolver<Tag>::handle_resolve,
                    async_resolver<Tag>::shared_from_this(),
                    path,
                    once_resolved,
                    boost::asio::placeholders::error,
                    boost::asio::placeholders::iterator
                    )
                )
            );
    }

    void handle_resolve(
        string_type const & host,
        resolve_completion_function once_resolved, 
        boost::system::error_code const & ec,
        resolver_iterator endpoint_iterator
        )
    {
        typename endpoint_cache::iterator iter;
        bool inserted = false;
        LOG(ERROR) << "Done resolving " << host << " error " << ec;
        if (!ec && cache_resolved_) {
            boost::fusion::tie(iter, inserted) =
                endpoint_cache_.insert(
                    std::make_pair(
                        host,
                        std::make_pair(
                            endpoint_iterator,
                            resolver_iterator()
                            )
                            )
                            );
            once_resolved(ec, iter->second);
        } else {
            once_resolved(ec, std::make_pair(endpoint_iterator,resolver_iterator()));
        }
    }

};

}

namespace impl {

template<> struct async_connection_base<tags::http_async_8bit_local_resolve, 1, 1>;

template<> struct connection_delegate_factory<tags::http_async_8bit_local_resolve> {
  typedef std::shared_ptr<local_connection_delegate> connection_delegate_ptr;
  typedef typename string<Tag>::type string_type;

  // This is the factory method that actually returns the delegate instance.
  // TODO Support passing in proxy settings when crafting connections.
  static connection_delegate_ptr new_connection_delegate(
      boost::asio::io_service & service,
      bool https,
      bool always_verify_peer,
      optional<string_type> certificate_filename,
      optional<string_type> verify_path,
      optional<string_type> certificate_file,
      optional<string_type> private_key_file) {
    return std::make_shared<local_stream_delegate>(service);
  }
};

template<> struct async_connection_base<tags::http_async_8bit_local_resolve, 1, 1> {
  typedef async_connection_base<Tag,version_major,version_minor> this_type;
  typedef typename resolver_policy<Tag>::type resolver_base;
  typedef typename resolver_base::resolver_type resolver_type;
  typedef typename resolver_base::resolve_function resolve_function;
  typedef typename string<Tag>::type string_type;
  typedef basic_request<Tag> request;
  typedef basic_response<Tag> response;
  typedef iterator_range<char const *> char_const_range;
  typedef function<void(char_const_range const &, system::error_code const &)>
      body_callback_function_type;
  typedef function<bool(string_type&)> body_generator_function_type;
  typedef shared_ptr<this_type> connection_ptr;

  // This is the factory function which constructs the appropriate async
  // connection implementation with the correct delegate chosen based on the
  // tag.
  static connection_ptr new_connection(
      resolve_function resolve,
      resolver_type & resolver,
      bool follow_redirect,
      bool always_verify_peer,
      bool https,
      optional<string_type> certificate_filename=optional<string_type>(),
      optional<string_type> const & verify_path=optional<string_type>(),
      optional<string_type> certificate_file=optional<string_type>(),
      optional<string_type> private_key_file=optional<string_type>());

  // This is the pure virtual entry-point for all asynchronous connections.
  virtual response start(
      request const & request,
      string_type const & method,
      bool get_body,
      body_callback_function_type callback,
      body_generator_function_type generator) = 0;

  virtual ~async_connection_base() {}

};

template<>
struct http_async_connection<tags::http_async_8bit_local_resolve, 1, 1>
    : async_connection_base<Tag, version_major, version_minor>,
      protected http_async_protocol_handler<Tag, version_major, version_minor>,
      boost::enable_shared_from_this<
          http_async_connection<Tag, version_major, version_minor> > {
  typedef async_connection_base<Tag, version_major, version_minor> base;
  typedef http_async_protocol_handler<Tag, version_major, version_minor>
      protocol_base;
  typedef typename base::resolver_type resolver_type;
  typedef typename base::resolver_base::resolver_iterator resolver_iterator;
  typedef typename base::resolver_base::resolver_iterator_pair
      resolver_iterator_pair;
  typedef typename base::response response;
  typedef typename base::string_type string_type;
  typedef typename base::request request;
  typedef typename base::resolver_base::resolve_function resolve_function;
  typedef typename base::body_callback_function_type
      body_callback_function_type;
  typedef typename base::body_generator_function_type
      body_generator_function_type;
  typedef http_async_connection<Tag, version_major, version_minor> this_type;
  typedef typename delegate_factory<Tag>::type delegate_factory_type;
  typedef typename delegate_factory_type::connection_delegate_ptr
      connection_delegate_ptr;

  http_async_connection(resolver_type& resolver, resolve_function resolve,
                        bool follow_redirect, connection_delegate_ptr delegate)
      : follow_redirect_(follow_redirect),
        resolver_(resolver),
        resolve_(resolve),
        request_strand_(resolver.get_io_service()),
        delegate_(delegate) {}

  // This is the main entry point for the connection/request pipeline. We're
  // overriding async_connection_base<...>::start(...) here which is called
  // by the client.
  virtual response start(request const& request, string_type const& method,
                         bool get_body, body_callback_function_type callback,
                         body_generator_function_type generator) {
    response response_;
    this->init_response(response_, get_body);
    linearize(request, method, version_major, version_minor,
              std::ostreambuf_iterator<typename char_<Tag>::type>(
                  &command_streambuf));
    const auto data = command_streambuf.prepare(1024);
    std::size_t s1 = boost::asio::buffer_size(data);
    const char* p1 = boost::asio::buffer_cast<const char*>(data);
    LOG(ERROR) << "Full request: " << std::string(p1, s1);
    this->method = method;
    boost::uint16_t port_ = port(request);
    resolve_(resolver_, host(request), port_,
             request_strand_.wrap(boost::bind(
                 &this_type::handle_resolved, this_type::shared_from_this(),
                 port_, get_body, callback, generator, _1, _2)));
    return response_;
  }

 private:
  http_async_connection(http_async_connection const&);  // = delete

  void set_errors(boost::system::error_code const& ec) {
    boost::system::system_error error(ec);
    this->version_promise.set_exception(boost::copy_exception(error));
    this->status_promise.set_exception(boost::copy_exception(error));
    this->status_message_promise.set_exception(boost::copy_exception(error));
    this->headers_promise.set_exception(boost::copy_exception(error));
    this->source_promise.set_exception(boost::copy_exception(error));
    this->destination_promise.set_exception(boost::copy_exception(error));
    this->body_promise.set_exception(boost::copy_exception(error));
  }

  void handle_resolved(boost::uint16_t port, bool get_body,
                       body_callback_function_type callback,
                       body_generator_function_type generator,
                       boost::system::error_code const& ec,
                       resolver_iterator_pair endpoint_range) {
    if (!ec && !boost::empty(endpoint_range)) {
      // Here we deal with the case that there was an error encountered and
      // that there's still more endpoints to try connecting to.
      resolver_iterator iter = boost::begin(endpoint_range);
      resolver_type::endpoint_type endpoint(iter->endpoint().path());
      delegate_->connect(
          endpoint, request_strand_.wrap(boost::bind(
                        &this_type::handle_connected,
                        this_type::shared_from_this(), port, get_body, callback,
                        generator, std::make_pair(++iter, resolver_iterator()),
                        placeholders::error)));
    } else {
      set_errors(ec ? ec : boost::asio::error::host_not_found);
      boost::iterator_range<const char*> range;
      if (callback) callback(range, ec);
    }
  }

  void handle_connected(boost::uint16_t port, bool get_body,
                        body_callback_function_type callback,
                        body_generator_function_type generator,
                        resolver_iterator_pair endpoint_range,
                        boost::system::error_code const& ec) {
    if (!ec) {
      BOOST_ASSERT(delegate_.get() != 0);
      delegate_->write(
          command_streambuf,
          request_strand_.wrap(boost::bind(
              &this_type::handle_sent_request, this_type::shared_from_this(),
              get_body, callback, generator, placeholders::error,
              placeholders::bytes_transferred)));
    } else {
      if (!boost::empty(endpoint_range)) {
        resolver_iterator iter = boost::begin(endpoint_range);
        resolver_type::endpoint_type endpoint(iter->endpoint().path());
        delegate_->connect(
            endpoint,
            request_strand_.wrap(boost::bind(
                &this_type::handle_connected, this_type::shared_from_this(),
                port, get_body, callback, generator,
                std::make_pair(++iter, resolver_iterator()),
                placeholders::error)));
      } else {
        set_errors(ec ? ec : boost::asio::error::host_not_found);
        boost::iterator_range<const char*> range;
        if (callback) callback(range, ec);
      }
    }
  }

  enum state_t {
    version,
    status,
    status_message,
    headers,
    body
  };

  void handle_sent_request(bool get_body, body_callback_function_type callback,
                           body_generator_function_type generator,
                           boost::system::error_code const& ec,
                           std::size_t bytes_transferred) {
    if (!ec) {
      if (generator) {
        // Here we write some more data that the generator provides, before
        // we wait for data from the server.
        string_type chunk;
        if (generator(chunk)) {
          // At this point this means we have more data to write, so we write
          // it out.
          std::copy(chunk.begin(), chunk.end(),
                    std::ostreambuf_iterator<typename char_<Tag>::type>(
                        &command_streambuf));
          delegate_->write(
              command_streambuf,
              request_strand_.wrap(boost::bind(
                  &this_type::handle_sent_request,
                  this_type::shared_from_this(), get_body, callback, generator,
                  placeholders::error, placeholders::bytes_transferred)));
          return;
        }
      }
      delegate_->read_some(
          boost::asio::mutable_buffers_1(this->part.c_array(),
                                         this->part.size()),
          request_strand_.wrap(boost::bind(
              &this_type::handle_received_data, this_type::shared_from_this(),
              version, get_body, callback, placeholders::error,
              placeholders::bytes_transferred)));
    } else {
      set_errors(ec);
    }
  }

  void handle_received_data(state_t state, bool get_body,
                            body_callback_function_type callback,
                            boost::system::error_code const& ec,
                            std::size_t bytes_transferred) {
    static const long short_read_error = 335544539;
    bool is_ssl_short_read_error =
#ifdef BOOST_NETWORK_ENABLE_HTTPS
        ec.category() == asio::error::ssl_category &&
        ec.value() == short_read_error;
#else
        false && short_read_error;
#endif
    if (!ec || ec == boost::asio::error::eof || is_ssl_short_read_error) {
      logic::tribool parsed_ok;
      size_t remainder;
      switch (state) {
        case version:
          parsed_ok = this->parse_version(
              delegate_,
              request_strand_.wrap(boost::bind(
                  &this_type::handle_received_data,
                  this_type::shared_from_this(), version, get_body, callback,
                  placeholders::error, placeholders::bytes_transferred)),
              bytes_transferred);
          if (!parsed_ok || indeterminate(parsed_ok)) return;
        case status:
          parsed_ok = this->parse_status(
              delegate_,
              request_strand_.wrap(boost::bind(
                  &this_type::handle_received_data,
                  this_type::shared_from_this(), status, get_body, callback,
                  placeholders::error, placeholders::bytes_transferred)),
              bytes_transferred);
          if (!parsed_ok || indeterminate(parsed_ok)) return;
        case status_message:
          parsed_ok = this->parse_status_message(
              delegate_, request_strand_.wrap(boost::bind(
                             &this_type::handle_received_data,
                             this_type::shared_from_this(), status_message,
                             get_body, callback, placeholders::error,
                             placeholders::bytes_transferred)),
              bytes_transferred);
          if (!parsed_ok || indeterminate(parsed_ok)) return;
        case headers:
          // In the following, remainder is the number of bytes that remain
          // in the buffer. We need this in the body processing to make sure
          // that the data remaining in the buffer is dealt with before
          // another call to get more data for the body is scheduled.
          fusion::tie(parsed_ok, remainder) = this->parse_headers(
              delegate_,
              request_strand_.wrap(boost::bind(
                  &this_type::handle_received_data,
                  this_type::shared_from_this(), headers, get_body, callback,
                  placeholders::error, placeholders::bytes_transferred)),
              bytes_transferred);

          if (!parsed_ok || indeterminate(parsed_ok)) return;

          if (!get_body) {
            // We short-circuit here because the user does not
            // want to get the body (in the case of a HEAD
            // request).
            this->body_promise.set_value("");
            this->destination_promise.set_value("");
            this->source_promise.set_value("");
            this->part.assign('\0');
            this->response_parser_.reset();
            return;
          }

          if (callback) {
            // Here we deal with the spill-over data from the
            // headers processing. This means the headers data
            // has already been parsed appropriately and we're
            // looking to treat everything that remains in the
            // buffer.
            typename protocol_base::buffer_type::const_iterator begin =
                this->part_begin;
            typename protocol_base::buffer_type::const_iterator end = begin;
            std::advance(end, remainder);

            // We're setting the body promise here to an empty string because
            // this can be used as a signaling mechanism for the user to
            // determine that the body is now ready for processing, even
            // though the callback is already provided.
            this->body_promise.set_value("");

            // The invocation of the callback is synchronous to allow us to
            // wait before scheduling another read.
            callback(make_iterator_range(begin, end), ec);

            delegate_->read_some(
                boost::asio::mutable_buffers_1(this->part.c_array(),
                                               this->part.size()),
                request_strand_.wrap(boost::bind(
                    &this_type::handle_received_data,
                    this_type::shared_from_this(), body, get_body, callback,
                    placeholders::error, placeholders::bytes_transferred)));
          } else {
            // Here we handle the body data ourself and append to an
            // ever-growing string buffer.
            this->parse_body(
                delegate_,
                request_strand_.wrap(boost::bind(
                    &this_type::handle_received_data,
                    this_type::shared_from_this(), body, get_body, callback,
                    placeholders::error, placeholders::bytes_transferred)),
                remainder);
          }
          return;
        case body:
          if (ec == boost::asio::error::eof || is_ssl_short_read_error) {
            // Here we're handling the case when the connection has been
            // closed from the server side, or at least that the end of file
            // has been reached while reading the socket. This signals the end
            // of the body processing chain.
            if (callback) {
              typename protocol_base::buffer_type::const_iterator
                  begin = this->part.begin(),
                  end = begin;
              std::advance(end, bytes_transferred);

              // We call the callback function synchronously passing the error
              // condition (in this case, end of file) so that it can handle
              // it appropriately.
              callback(make_iterator_range(begin, end), ec);
            } else {
              string_type body_string;
              std::swap(body_string, this->partial_parsed);
              body_string.append(this->part.begin(), bytes_transferred);
              if (this->is_chunk_encoding)
                this->body_promise.set_value(parse_chunk_encoding(body_string));
              else
                this->body_promise.set_value(body_string);
            }
            // TODO set the destination value somewhere!
            this->destination_promise.set_value("");
            this->source_promise.set_value("");
            this->part.assign('\0');
            this->response_parser_.reset();
          } else {
            // This means the connection has not been closed yet and we want
            // to get more
            // data.
            if (callback) {
              // Here we have a body_handler callback. Let's invoke the
              // callback from here and make sure we're getting more data
              // right after.
              typename protocol_base::buffer_type::const_iterator begin =
                  this->part.begin();
              typename protocol_base::buffer_type::const_iterator end = begin;
              std::advance(end, bytes_transferred);
              callback(make_iterator_range(begin, end), ec);
              delegate_->read_some(
                  boost::asio::mutable_buffers_1(this->part.c_array(),
                                                 this->part.size()),
                  request_strand_.wrap(boost::bind(
                      &this_type::handle_received_data,
                      this_type::shared_from_this(), body, get_body, callback,
                      placeholders::error, placeholders::bytes_transferred)));
            } else {
              // Here we don't have a body callback. Let's
              // make sure that we deal with the remainder
              // from the headers part in case we do have data
              // that's still in the buffer.
              this->parse_body(
                  delegate_,
                  request_strand_.wrap(boost::bind(
                      &this_type::handle_received_data,
                      this_type::shared_from_this(), body, get_body, callback,
                      placeholders::error, placeholders::bytes_transferred)),
                  bytes_transferred);
            }
          }
          return;
        default:
          BOOST_ASSERT(false && "Bug, report this to the developers!");
      }
    } else {
      boost::system::system_error error(ec);
      this->source_promise.set_exception(boost::copy_exception(error));
      this->destination_promise.set_exception(boost::copy_exception(error));
      switch (state) {
        case version:
          this->version_promise.set_exception(boost::copy_exception(error));
        case status:
          this->status_promise.set_exception(boost::copy_exception(error));
        case status_message:
          this->status_message_promise.set_exception(
              boost::copy_exception(error));
        case headers:
          this->headers_promise.set_exception(boost::copy_exception(error));
        case body:
          this->body_promise.set_exception(boost::copy_exception(error));
          break;
        default:
          BOOST_ASSERT(false && "Bug, report this to the developers!");
      }
    }
  }

  string_type parse_chunk_encoding(string_type& body_string) {
    string_type body;
    string_type crlf = "\r\n";

    typename string_type::iterator begin = body_string.begin();
    for (typename string_type::iterator iter =
             std::search(begin, body_string.end(), crlf.begin(), crlf.end());
         iter != body_string.end();
         iter =
             std::search(begin, body_string.end(), crlf.begin(), crlf.end())) {
      string_type line(begin, iter);
      if (line.empty()) break;
      std::stringstream stream(line);
      int len;
      stream >> std::hex >> len;
      std::advance(iter, 2);
      if (!len) break;
      if (len <= body_string.end() - iter) {
        body.insert(body.end(), iter, iter + len);
        std::advance(iter, len + 2);
      }
      begin = iter;
    }

    return body;
  }

  bool follow_redirect_;
  resolver_type& resolver_;
  resolve_function resolve_;
  boost::asio::io_service::strand request_strand_;
  connection_delegate_ptr delegate_;
  boost::asio::streambuf command_streambuf;
  string_type method;
};

} /* impl */

#undef version_minor
#undef version_major
#undef Tag

} /* http */
} /* network */

} /* boost */

#endif  // LOCAL_STREAM_HTTP
