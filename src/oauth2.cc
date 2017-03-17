#include "oauth2.h"

#define BOOST_NETWORK_ENABLE_HTTPS
#include <boost/network/protocol/http/client.hpp>
#include <boost/network/uri.hpp>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/pkcs12.h>

#include "json.h"
#include "logging.h"

namespace http = boost::network::http;

namespace google {

namespace {

#if 0
std::string GetString(const json::Object* obj, const std::string& field) {
  return obj->Get<json::String>(field);
#if 0
  auto value_it = obj->find(field);
  if (value_it == obj->end()) {
    LOG(ERROR) << "There is no " << field << " in " << *obj;
    return "";
  }
  if (value_it->second->type() != json::StringType) {
    LOG(ERROR) << field << " " << *value_it->second << " is not a string";
    return "";
  }
  return value_it->second->As<json::String>()->value();
#endif
}
#endif

std::unique_ptr<json::Value> GetMetadataToken() {
  http::client client;
  http::client::request request("http://metadata.google.internal/computeMetadata/v1/instance/service-accounts/default/token");
  request << boost::network::header("Metadata-Flavor", "Google");
  http::client::response response = client.get(request);
  LOG(ERROR) << "Token response: " << body(response);
  std::unique_ptr<json::Value> parsed_token = json::JSONParser::FromString(body(response));
  LOG(ERROR) << "Parsed token: " << *parsed_token;
  return parsed_token;
}

namespace base64 {

namespace {
constexpr unsigned char base64url_chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
}

// See http://stackoverflow.com/questions/180947.
std::string encode(const std::string &source) {
  std::string result;
#if 0
  const std::size_t remainder = source.size() % 3;
  for (std::size_t i = 0; i < source.size(); i += 3) {
    const unsigned char c0 = source[i];
    const unsigned char c1 = remainder > 0 ? source[i + 1] : 0u;
    const unsigned char c2 = remainder > 1 ? source[i + 2] : 0u;
    result.push_back(base64url_chars[0x3f & c0 >> 2]);
    result.push_back(base64url_chars[0x3f & c0 << 4 | c1 >> 4]);
    if (remainder > 0) {
      result.push_back(base64url_chars[0x3f & c1 << 2 | c2 >> 6]);
    }
    if (remainder > 1) {
      result.push_back(base64url_chars[0x3f & c2]);
    }
  }
#endif
  unsigned int code_buffer = 0;
  int code_buffer_size = -6;
  for (unsigned char c : source) {
    code_buffer = (code_buffer << 8) | c;
    code_buffer_size += 8;
    while (code_buffer_size >= 0) {
      result.push_back(base64url_chars[(code_buffer >> code_buffer_size) & 0x3f]);
      code_buffer_size -= 6;
    }
  }
  if (code_buffer_size > -6) {
    code_buffer = (code_buffer << 8);
    code_buffer_size += 8;
    result.push_back(base64url_chars[(code_buffer >> code_buffer_size) & 0x3f]);
  }
  // No padding needed.
  return result;
}

#if 0
std::string decode(const std::string &source) {
  std::string result;

  std::vector<int> T(256,-1);
  for (int i=0; i<64; i++) T[base64url_chars[i]] = i;

  int val=0, valb=-8;
  for (uchar c : source) {
    if (T[c] == -1) break;
    val = (val<<6) + T[c];
    valb += 6;
    if (valb>=0) {
      result.push_back(char((val>>valb)&0xFF));
      valb-=8;
    }
  }
  return result;
}
#endif

}  // base64

template<class T, class R, R(*D)(T*)>
struct Deleter {
  void operator()(T* p) { if (p) D(p); }
};

class PKey {
  using PKCS8_Deleter = Deleter<PKCS8_PRIV_KEY_INFO, void, PKCS8_PRIV_KEY_INFO_free>;
  using BIO_Deleter = Deleter<BIO, int, BIO_free>;
  using EVP_PKEY_Deleter = Deleter<EVP_PKEY, void, EVP_PKEY_free>;
 public:
  PKey() {}
  PKey(const std::string& private_key_pem) {
    std::unique_ptr<BIO, BIO_Deleter> buf(
        BIO_new_mem_buf((void*) private_key_pem.data(), -1));
    if (buf == nullptr) {
      LOG(ERROR) << "BIO_new_mem_buf failed";
      return;
    }
    std::unique_ptr<PKCS8_PRIV_KEY_INFO, PKCS8_Deleter> p8inf(
        PEM_read_bio_PKCS8_PRIV_KEY_INFO(buf.get(), NULL, NULL, NULL));
    if (p8inf == nullptr) {
      LOG(ERROR) << "PEM_read_bio_PKCS8_PRIV_KEY_INFO failed";
      return;
    }
    private_key_.reset(EVP_PKCS82PKEY(p8inf.get()));
    if (private_key_ == nullptr) {
      LOG(ERROR) << "EVP_PKCS82PKEY failed";
    }
  }

  std::string ToString() const {
    std::unique_ptr<BIO, BIO_Deleter> mem(BIO_new(BIO_s_mem()));
    EVP_PKEY_print_private(mem.get(), private_key_.get(), 0, NULL);
    char* pp;
    long len = BIO_get_mem_data(mem.get(), &pp);
    return std::string(pp, len);
  }

  EVP_PKEY* private_key() const {
    // A const_cast is fine because this is passed into functions that don't
    // modify pkey, but have silly signatures.
    return const_cast<EVP_PKEY*>(private_key_.get());
  }

 private:
  std::unique_ptr<EVP_PKEY, EVP_PKEY_Deleter> private_key_;
};

class Finally {
 public:
  Finally(std::function<void()> cleanup) : cleanup_(cleanup) {}
  ~Finally() { cleanup_(); }
 private:
  std::function<void()> cleanup_;
};

std::string Sign(const std::string& data, const PKey& pkey) {
#if 0
  LOG(ERROR) << "Signing '" << data << "' with '" << pkey.ToString() << "'";
#endif
  unsigned int capacity = EVP_PKEY_size(pkey.private_key());
  std::unique_ptr<unsigned char> result(new unsigned char[capacity]);

  char error[1024];

  EVP_MD_CTX ctx;
  EVP_SignInit(&ctx, EVP_sha256());

  Finally cleanup([&ctx, &error]() {
    if (EVP_MD_CTX_cleanup(&ctx) == 0) {
      ERR_error_string_n(ERR_get_error(), error, sizeof(error));
      LOG(ERROR) << "EVP_MD_CTX_cleanup failed: " << error;
    }
  });

  if (EVP_SignUpdate(&ctx, data.data(), data.size()) == 0) {
    ERR_error_string_n(ERR_get_error(), error, sizeof(error));
    LOG(ERROR) << "EVP_SignUpdate failed: " << error;
    return "";
  }

  unsigned int actual_result_size = 0;
  if (EVP_SignFinal(&ctx, result.get(), &actual_result_size, pkey.private_key()) == 0) {
    ERR_error_string_n(ERR_get_error(), error, sizeof(error));
    LOG(ERROR) << "EVP_SignFinal failed: " << error;
    return "";
  }
#if 0
  for (int i = 0; i < actual_result_size; ++i) {
    LOG(ERROR) << "Signature char '" << static_cast<int>(result.get()[i]) << "'";
  }
#endif
  return std::string(reinterpret_cast<char*>(result.get()), actual_result_size);
}

double SecondsSinceEpoch(
    const std::chrono::time_point<std::chrono::system_clock>& t) {
  return std::chrono::duration_cast<std::chrono::seconds>(
      t.time_since_epoch()).count();
}


// To allow logging headers. TODO: move to a common location.
std::ostream& operator<<(
    std::ostream& o,
    const http::client::request::headers_container_type& hv) {
  o << "[";
  for (const auto& h : hv) {
    o << " " << h.first << ": " << h.second;
  }
  o << " ]";
}


std::unique_ptr<json::Value> ComputeToken(const std::string& credentials_file) {
  std::string filename = credentials_file;
  if (filename.empty()) {
    const char* creds_env_var = std::getenv("GOOGLE_APPLICATION_CREDENTIALS");
    if (creds_env_var) {
      filename = creds_env_var;
    } else {
      // TODO: On Windows, "C:/ProgramData/Google/Auth/application_default_credentials.json"
      filename = "/etc/google/auth/application_default_credentials.json";
    }
  }
  std::ifstream input(filename);
  if (!input.good()) {
    return nullptr;
  }
  LOG(INFO) << "Reading credentials from " << filename;
  std::unique_ptr<json::Value> creds_json = json::JSONParser::FromStream(input);
  LOG(INFO) << "Retrieved credentials from " << filename << ": " << *creds_json;
  if (creds_json->type() != json::ObjectType) {
    LOG(ERROR) << "Credentials " << *creds_json << " is not an object!";
    return nullptr;
  }
  const json::Object* creds = creds_json->As<json::Object>();

  auto email_it = creds->find("client_email");
  if (email_it == creds->end()) {
    LOG(ERROR) << "There is no client_email in " << *creds;
    return nullptr;
  }
  if (email_it->second->type() != json::StringType) {
    LOG(ERROR) << "client_email " << *email_it->second << " is not a string";
    return nullptr;
  }
  const std::string service_account_email =
      email_it->second->As<json::String>()->value();

  auto key_it = creds->find("private_key");
  if (key_it == creds->end()) {
    LOG(ERROR) << "There is no private_key in " << *creds;
    return nullptr;
  }
  if (key_it->second->type() != json::StringType) {
    LOG(ERROR) << "private_key " << *key_it->second << " is not a string";
    return nullptr;
  }
  const std::string private_key_pem =
      key_it->second->As<json::String>()->value();

  LOG(INFO) << "Retrieved private key from " << filename;

  PKey private_key(private_key_pem);

  // Make a POST request to https://www.googleapis.com/oauth2/v3/token
  // with the body
  // grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=$JWT_HEADER.$CLAIM_SET.$SIGNATURE
  //
  // The trailing part of that body has three variables that need to be expanded.
  // Namely, $JWT_HEADER, $CLAIM_SET, and $SIGNATURE, separated by periods.
  //
  // $CLAIM_SET is a base64url encoding of a JSON object with five fields:
  // iss, scope, aud, exp, and iat.
  // iss: Service account email. We get this from user in the config file.
  // scope: Basically the requested scope (e.g. "permissions") for the token. For
  //   our purposes, this is the constant string
  //   "https://www.googleapis.com/auth/monitoring".
  // aud: Assertion target. Since we are asking for an access token, this is the
  //   constant string "https://www.googleapis.com/oauth2/v3/token". This is the
  //   same as the URL we are posting to.
  // iat: Time of the assertion (i.e. now) in units of "seconds from Unix epoch".
  // exp: Expiration of assertion. For us this is 'iat' + 3600 seconds.
  //
  // $SIGNATURE is the base64url encoding of the signature of the string
  // $JWT_HEADER.$CLAIM_SET
  // where $JWT_HEADER and $CLAIM_SET are defined as above. Note that they are
  // separated by the period character. The signature algorithm used should be
  // SHA-256. The private key used to sign the data comes from the user. The
  // private key to use is the one associated with the service account email
  // address (i.e. the email address specified in the 'iss' field above).

  LOG(INFO) << "Getting an OAuth2 token";
  http::client client;
  http::client::request request("https://www.googleapis.com/oauth2/v3/token");
  std::string grant_type = boost::network::uri::encoded(
      "urn:ietf:params:oauth:grant-type:jwt-bearer");
  //std::string jwt_header = "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9";
  //std::string jwt_header = base64::encode("{\"alg\":\"RS256\",\"typ\":\"JWT\"}");
  std::unique_ptr<json::Value> jwt_object = json::object({
    {"alg", json::string("RS256")},
    {"typ", json::string("JWT")},
  });
  std::string jwt_header = base64::encode(jwt_object->ToString());
  auto now = std::chrono::system_clock::now();
  auto exp = now + std::chrono::hours(1);
  std::unique_ptr<json::Value> claim_set_object = json::object({
    {"iss", json::string(service_account_email)},
    {"scope", json::string("https://www.googleapis.com/auth/monitoring")},
    {"aud", json::string("https://www.googleapis.com/oauth2/v3/token")},
    {"iat", json::number(SecondsSinceEpoch(now))},
    {"exp", json::number(SecondsSinceEpoch(exp))},
  });
  LOG(INFO) << "claim_set = " << claim_set_object->ToString();
  std::string claim_set = base64::encode(claim_set_object->ToString());
  LOG(INFO) << "encoded claim_set = " << claim_set;
  std::string signature = base64::encode(
      Sign(jwt_header + "." + claim_set, private_key));
  std::string request_body = "grant_type=" + grant_type + "&assertion=" + jwt_header + "." + claim_set + "." + signature;
  //request << boost::network::header("Connection", "close");
  request << boost::network::header("Content-Length", std::to_string(request_body.size()));
  request << boost::network::header("Content-Type", "application/x-www-form-urlencoded");
  request << boost::network::body(request_body);
  LOG(INFO) << "About to send request: " << request.uri().string()
            << " headers: " << request.headers()
            << " body: " << request.body();
  http::client::response response = client.post(request);
  LOG(ERROR) << "Token response: " << body(response);
  std::unique_ptr<json::Value> parsed_token = json::JSONParser::FromString(body(response));
  LOG(ERROR) << "Parsed token: " << *parsed_token;

  return parsed_token;
}

}

std::string OAuth2::GetAuthHeaderValue() {
  // Build in a 60 second slack.
  if (auth_header_value_.empty() ||
      token_expiration_ <
          std::chrono::system_clock::now() + std::chrono::seconds(60)) {
    // Token expired; retrieve new value.
    std::unique_ptr<json::Value> token_json = ComputeToken(credentials_file_);
    if (token_json == nullptr) {
      LOG(INFO) << "Getting auth token from metadata server";
      token_json = std::move(GetMetadataToken());
    }
    if (token_json == nullptr) {
      LOG(ERROR) << "Unable to get auth token";
      return "";
    }
    if (token_json->type() != json::ObjectType) {
      LOG(ERROR) << "Token " << *token_json << " is not an object!";
      return "";
    }
    // This object should be of the form:
    // {
    //  "access_token" : $THE_ACCESS_TOKEN,
    //  "token_type" : "Bearer",
    //  "expires_in" : 3600
    // }
    const json::Object* token = token_json->As<json::Object>();

    auto access_token_it = token->find("access_token");
    if (access_token_it == token->end()) {
      LOG(ERROR) << "There is no access_token in " << *token;
      return "";
    }
    if (access_token_it->second->type() != json::StringType) {
      LOG(ERROR) << "access_token " << *access_token_it->second << " is not a string";
      return "";
    }
    const std::string access_token =
        access_token_it->second->As<json::String>()->value();

    auto token_type_it = token->find("token_type");
    if (token_type_it == token->end()) {
      LOG(ERROR) << "There is no token_type in " << *token;
      return "";
    }
    if (token_type_it->second->type() != json::StringType) {
      LOG(ERROR) << "token_type " << *token_type_it->second << " is not a string";
      return "";
    }
    const std::string token_type =
        token_type_it->second->As<json::String>()->value();

    auto expires_in_it = token->find("expires_in");
    if (expires_in_it == token->end()) {
      LOG(ERROR) << "There is no expires_in in " << *token;
      return "";
    }
    if (expires_in_it->second->type() != json::NumberType) {
      LOG(ERROR) << "expires_in " << *expires_in_it->second << " is not a number";
      return "";
    }
    const double expires_in =
        expires_in_it->second->As<json::Number>()->value();

    if (token_type != "Bearer") {
      LOG(ERROR) << "Token type is not 'Bearer', but '" << token_type << "'";
    }

    auth_header_value_ = token_type + " " + access_token;
    token_expiration_ =
        std::chrono::system_clock::now() +
        std::chrono::seconds(static_cast<long>(expires_in));
  }
  return auth_header_value_;
}

}  // google

#if 0
//==============================================================================
//==============================================================================
//==============================================================================
// OAuth2 submodule.
//
// The main method in this module is wg_oauth2_get_auth_header(). The job of
// this method is to provide an authorization token for use in API calls.
// The value returned is preformatted for the caller's as an HTTP header in the
// following form:
// Authorization: Bearer ${access_token}
//
// There are two approaches the code takes in order to get ${access_token}.
// The easy route is to just ask the metadata server for a token.
// The harder route is to format and sign a request to the OAuth2 server and get
// a token that way.
// Which approach we take depends on the value of 'cred_ctx'. If it is NULL
// (i.e. if there are no user-supplied credentials), then we try the easy route.
// Otherwise we do the harder route.
//
// The reason we don't always do the easy case unconditionally is that the
// metadata server may not always be able to provide an auth token. Since you
// cannot add scopes to an existing VM, some people may want to go the harder
// route instead.
//
// Following is a detailed explanation of the easy route and the harder route.
//
//
// THE EASY ROUTE
//
// Make a GET request to the metadata server at the following URL:
// http://169.254.169.254/computeMetadata/v1beta1/instance/service-accounts/default/token
//
// If our call is successful, the server will respond with a json object looking
// like this:
// {
//  "access_token" : $THE_ACCESS_TOKEN
//  "token_type" : "Bearer",
//  "expires_in" : 3600
// }
//
// We extract $THE_ACCESS_TOKEN from the JSON response then insert it into an
// HTTP header string for the caller's convenience. That header string looks
// like this:
// Authorization: Bearer $THE_ACCESS_TOKEN
//
// We return this string (owned by caller) on success. Upon failure, we return
// NULL.
//
//
// THE HARDER ROUTE
//
// The algorithm used here is described in
// https://developers.google.com/identity/protocols/OAuth2ServiceAccount
// in the section "Preparing to make an authorized API call", under the tab
// "HTTP/Rest".
//
// There is more detail in the documentation, but what it boils down to is this:
//
// Make a POST request to https://www.googleapis.com/oauth2/v3/token
// with the body
// grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer&assertion=$JWT_HEADER.$CLAIM_SET.$SIGNATURE
//
// The trailing part of that body has three variables that need to be expanded.
// Namely, $JWT_HEADER, $CLAIM_SET, and $SIGNATURE, separated by periods.
//
// $JWT_HEADER is the base64url encoding of this constant JSON record:
// {"alg":"RS256","typ":"JWT"}
// Because this header is constant, its base64url encoding is also constant,
// and can be hardcoded as:
// eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9
//
// $CLAIM_SET is a base64url encoding of a JSON object with five fields:
// iss, scope, aud, exp, and iat.
// iss: Service account email. We get this from user in the config file.
// scope: Basically the requested scope (e.g. "permissions") for the token. For
//   our purposes, this is the constant string
//   "https://www.googleapis.com/auth/monitoring".
// aud: Assertion target. Since we are asking for an access token, this is the
//   constant string "https://www.googleapis.com/oauth2/v3/token". This is the
//   same as the URL we are posting to.
// iat: Time of the assertion (i.e. now) in units of "seconds from Unix epoch".
// exp: Expiration of assertion. For us this is 'iat' + 3600 seconds.
//
// $SIGNATURE is the base64url encoding of the signature of the string
// $JWT_HEADER.$CLAIM_SET
// where $JWT_HEADER and $CLAIM_SET are defined as above. Note that they are
// separated by the period character. The signature algorithm used should be
// SHA-256. The private key used to sign the data comes from the user. The
// private key to use is the one associated with the service account email
// address (i.e. the email address specified in the 'iss' field above).
//
// If our call is successful, the result will be the same as indicated above
// in the section entitled "THE EASY ROUTE".
//
//
// EXAMPLE USAGE
//
// char auth_header[256];
// if (wg_oauth2_get_auth_header(auth_header, sizeof(auth_header),
//                               oauth2_ctx, credential_ctx) != 0) {
//   return -1; // error
// }
// do_a_http_post_with(auth_header);
//
//==============================================================================
//==============================================================================
//==============================================================================

// Opaque to callers.
typedef struct oauth2_ctx_s oauth2_ctx_t;

// Either creates a new "Authorization: Bearer XXX" header or returns a cached
// one. Caller owns the returned string. Returns NULL if there is an error.
static int wg_oauth2_get_auth_header(char *result, size_t result_size,
                                     oauth2_ctx_t *ctx,
                                     const credential_ctx_t *cred_ctx);

// Allocate and construct an oauth2_ctx_t.
static oauth2_ctx_t *wg_oauth2_cxt_create();
// Deallocate and destroy an oauth2_ctx_t.
static void wg_oauth2_ctx_destroy(oauth2_ctx_t *);

//------------------------------------------------------------------------------
// Private implementation starts here.
//------------------------------------------------------------------------------
struct oauth2_ctx_s {
  pthread_mutex_t mutex;
  cdtime_t token_expire_time;
  char auth_header[256];
};

static int wg_oauth2_get_auth_header_nolock(oauth2_ctx_t *ctx,
    const credential_ctx_t *cred_ctx);

static int wg_oauth2_sign(unsigned char *signature, size_t sig_capacity,
                          unsigned int *actual_sig_size,
                          const char *buffer, size_t size, EVP_PKEY *pkey);

static void wg_oauth2_base64url_encode(char **buffer, size_t *buffer_size,
                                       const unsigned char *source,
                                       size_t source_size);

static int wg_oauth2_parse_result(char **result_buffer, size_t *result_size,
                                  time_t *expires_in, const char *json);

static int wg_oauth2_talk_to_server_and_store_result(oauth2_ctx_t *ctx,
    const char *url, const char *body, const char **headers, int num_headers,
    cdtime_t now);

static int wg_oauth2_get_auth_header(char *result, size_t result_size,
    oauth2_ctx_t *ctx, const credential_ctx_t *cred_ctx) {
  // Do the whole operation under lock so that there are no races with regard
  // to the token, we don't spam the server, etc.
  pthread_mutex_lock(&ctx->mutex);
  int error = wg_oauth2_get_auth_header_nolock(ctx, cred_ctx);
  if (error == 0) {
    sstrncpy(result, ctx->auth_header, result_size);
  }
  pthread_mutex_unlock(&ctx->mutex);
  return error;
}

static int wg_oauth2_get_auth_header_nolock(oauth2_ctx_t *ctx,
    const credential_ctx_t *cred_ctx) {
  // The URL to get the auth token from the metadata server.
  static const char gcp_metadata_fetch_auth_token[] =
    "http://169.254.169.254/computeMetadata/v1beta1/instance/service-accounts/default/token";

  cdtime_t now = cdtime();
  // Try to reuse an existing token. We build in a minute of slack in order to
  // avoid timing problems (clock skew, races, etc).
  if (ctx->token_expire_time > now + TIME_T_TO_CDTIME_T(60)) {
    // Token still valid!
    return 0;
  }
  // Retire the old token.
  ctx->token_expire_time = 0;
  ctx->auth_header[0] = 0;

  // If there are no user-supplied credentials, try to get the token from the
  // metadata server. This is THE EASY ROUTE as described in the documentation
  // for this method.
  const char *headers[] = { gcp_metadata_header };
  if (cred_ctx == NULL) {
    INFO("write_gcm: Asking metadata server for auth token");
    return wg_oauth2_talk_to_server_and_store_result(ctx,
        gcp_metadata_fetch_auth_token, NULL,
        headers, STATIC_ARRAY_SIZE(headers), now);
  }

  // If there are user-supplied credentials, format and sign a request to the
  // OAuth2 server. This is THE HARDER ROUTE as described in the documentation
  // for this submodule. This involves posting a body to a URL. The URL is
  // constant. The body needs to be constructed as described
  // in the comments for this submodule.
  const char *url = "https://www.googleapis.com/oauth2/v3/token";

  char body[2048];  // Should be big enough.
  char *bptr = body;
  size_t bsize = sizeof(body);

  bufprintf(&bptr, &bsize, "%s",
            "grant_type=urn%3Aietf%3Aparams%3Aoauth%3Agrant-type%3Ajwt-bearer"
            "&assertion=");

  // Save a pointer to the start of the jwt_header because we will need to
  // sign $JWT_HEADER.$CLAIM_SET shortly.
  const char *jwt_header_begin = bptr;

  // The body has three variables that need to be filled in: jwt_header,
  // claim_set, and signature.

  // 'jwt_header' is easy. It is the base64url encoding of
  // {"alg":"RS256","typ":"JWT"}
  // which is eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9
  // In addition, we're going to need a . separator shortly, so we add it now.
  bufprintf(&bptr, &bsize, "eyJhbGciOiJSUzI1NiIsInR5cCI6IkpXVCJ9.");

  // Build 'claim_set' and append its base64url encoding.
  {
    char claim_set[1024];
    unsigned long long iat = CDTIME_T_TO_TIME_T(now);
    unsigned long long exp = iat + 3600;  // + 1 hour.

    int result = snprintf(
        claim_set, sizeof(claim_set),
        "{"
        "\"iss\": \"%s\","
        "\"scope\": \"https://www.googleapis.com/auth/monitoring\","
        "\"aud\": \"%s\","
        "\"iat\": %llu,"
        "\"exp\": %llu"
        "}",
        cred_ctx->email,
        url,
        iat,
        exp);
    if (result < 0 || result >= sizeof(claim_set)) {
      ERROR("write_gcm: Error building claim_set.");
      return -1;
    }
    wg_oauth2_base64url_encode(&bptr, &bsize,
                               (unsigned char*)claim_set, result);
  }

  // Sign the bytes in the buffer that are in the range [jtw_header_start, bptr)
  // Referring to the above documentation, this refers to the part of the body
  // consisting of $JWT_HEADER.$CLAIM_SET
  {
    unsigned char signature[1024];
    unsigned int actual_sig_size;
    if (wg_oauth2_sign(signature, sizeof(signature), &actual_sig_size,
                       jwt_header_begin, bptr - jwt_header_begin,
                       cred_ctx->private_key) != 0) {
      ERROR("write_gcm: Can't sign.");
      return -1;
    }

    // Now that we have the signature, append a '.' and the base64url encoding
    // of 'signature' to the buffer.
    bufprintf(&bptr, &bsize, ".");
    wg_oauth2_base64url_encode(&bptr, &bsize, signature, actual_sig_size);
  }

  // Before using the buffer, check for overflow or error.
  if (bsize < 2) {
    ERROR("write_gcm: Buffer overflow or error while building oauth2 body");
    return -1;
  }
  return wg_oauth2_talk_to_server_and_store_result(ctx, url, body, NULL, 0,
      now);
}

static int wg_oauth2_talk_to_server_and_store_result(oauth2_ctx_t *ctx,
    const char *url, const char *body, const char **headers, int num_headers,
    cdtime_t now) {
  char response[2048];
  if (wg_curl_get_or_post(response, sizeof(response), url, body,
      headers, num_headers) != 0) {
    return -1;
  }

  // Fill ctx->auth_header with the string "Authorization: Bearer $TOKEN"
  char *resultp = ctx->auth_header;
  size_t result_size = sizeof(ctx->auth_header);
  bufprintf(&resultp, &result_size, "Authorization: Bearer ");
  time_t expires_in;
  if (wg_oauth2_parse_result(&resultp, &result_size, &expires_in,
                             response) != 0) {
    ERROR("write_gcm: wg_oauth2_parse_result failed");
    return -1;
  }

  if (result_size < 2) {
    ERROR("write_gcm: Error or buffer overflow when building auth_header");
    return -1;
  }
  ctx->token_expire_time = now + TIME_T_TO_CDTIME_T(expires_in);
  return 0;
}

static int wg_oauth2_sign(unsigned char *signature, size_t sig_capacity,
                          unsigned int *actual_sig_size,
                          const char *buffer, size_t size, EVP_PKEY *pkey) {
  if (sig_capacity < EVP_PKEY_size(pkey)) {
    ERROR("write_gcm: signature buffer not big enough.");
    return -1;
  }
  EVP_MD_CTX ctx;
  EVP_SignInit(&ctx, EVP_sha256());

  char err_buf[1024];
  if (EVP_SignUpdate(&ctx, buffer, size) == 0) {
    ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
    ERROR("write_gcm: EVP_SignUpdate failed: %s", err_buf);
    EVP_MD_CTX_cleanup(&ctx);
    return -1;
  }

  if (EVP_SignFinal(&ctx, signature, actual_sig_size, pkey) == 0) {
    ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
    ERROR ("write_gcm: EVP_SignFinal failed: %s", err_buf);
    EVP_MD_CTX_cleanup(&ctx);
    return -1;
  }
  if (EVP_MD_CTX_cleanup(&ctx) == 0) {
    ERR_error_string_n(ERR_get_error(), err_buf, sizeof(err_buf));
    ERROR ("write_gcm: EVP_MD_CTX_cleanup failed: %s", err_buf);
    return -1;
  }
  return 0;
}

static void wg_oauth2_base64url_encode(char **buffer, size_t *buffer_size,
                                       const unsigned char *source,
                                       size_t source_size) {
  const char *codes =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

  size_t i;
  unsigned int code_buffer = 0;
  int code_buffer_size = 0;
  for (i = 0; i < source_size; ++i) {
    code_buffer = (code_buffer << 8) | source[i];  // Add 8 bits to the right.
    code_buffer_size += 8;
    do {
      // Remove six bits from the left (there will never be more than 12).
      unsigned int next_code = (code_buffer >> (code_buffer_size - 6)) & 0x3f;
      code_buffer_size -= 6;
      // This is not fast, but we don't care much about performance here.
      bufprintf(buffer, buffer_size, "%c", codes[next_code]);
    } while (code_buffer_size >= 6);
  }
  // Flush code buffer. Our server does not want the trailing = or == characters
  // normally present in base64 encoding.
  if (code_buffer_size != 0) {
    code_buffer = (code_buffer << 8);
    code_buffer_size += 8;
    unsigned int next_code = (code_buffer >> (code_buffer_size - 6)) & 0x3f;
    bufprintf(buffer, buffer_size, "%c", codes[next_code]);
  }
}

static int wg_oauth2_parse_result(char **result_buffer, size_t *result_size,
                                  time_t *expires_in, const char *json) {
  long long temp;
  if (wg_extract_toplevel_json_long_long(json, "expires_in", &temp) != 0) {
    ERROR("write_gcm: Can't find expires_in in result.");
    return -1;
  }

  char *access_token;
  if (wg_extract_toplevel_json_string(json, "access_token", &access_token)
      != 0) {
    ERROR("write_gcm: Can't find access_token in result.");
    return -1;
  }

  *expires_in = (time_t)temp;
  bufprintf(result_buffer, result_size, "%s", access_token);
  sfree(access_token);
  return 0;
}

static oauth2_ctx_t *wg_oauth2_cxt_create() {
  oauth2_ctx_t *ctx = calloc(1, sizeof(*ctx));
  if (ctx == NULL) {
    ERROR("write_gcm: wg_oauth2_cxt_create: calloc failed.");
    return NULL;
  }
  pthread_mutex_init(&ctx->mutex, NULL);
  return ctx;
}

static void wg_oauth2_ctx_destroy(oauth2_ctx_t *ctx) {
  if (ctx == NULL) {
    return;
  }
  pthread_mutex_destroy(&ctx->mutex);
  sfree(ctx);
}
#endif
