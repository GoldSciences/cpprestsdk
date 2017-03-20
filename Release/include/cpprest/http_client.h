// HTTP Library: Client-side APIs. For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#ifndef _CASA_HTTP_CLIENT_H
#define _CASA_HTTP_CLIENT_H

#if defined (__cplusplus_winrt)
#if !defined(__WRL_NO_DEFAULT_LIB__)
#define __WRL_NO_DEFAULT_LIB__
#endif
#include <wrl.h>
#include <msxml6.h>
namespace web { namespace http{namespace client{
typedef IXMLHTTPRequest2* native_handle;}}}
#else
namespace web { namespace http{namespace client{
typedef void* native_handle;}}}
#endif // __cplusplus_winrt

#include <memory>
#include <limits>

#include "pplx/pplxtasks.h"
#include "cpprest/http_msg.h"
#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "cpprest/details/web_utilities.h"
#include "cpprest/details/basic_types.h"
#include "cpprest/asyncrt_utils.h"

#if !defined(CPPREST_TARGET_XP)
#include "cpprest/oauth1.h"
#endif

#include "cpprest/oauth2.h"

#if !defined(_WIN32) && !defined(__cplusplus_winrt)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wconversion"
#endif
#include "boost/asio/ssl.hpp"
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
#endif

/// The web namespace contains functionality common to multiple protocols like HTTP and WebSockets.
namespace web
{
/// Declarations and functionality for the HTTP protocol.
namespace http
{
/// HTTP client side library.
namespace client
{

// credentials and web_proxy class has been moved from web::http::client namespace to web namespace.
// The below using declarations ensure we don't break existing code.
// Please use the web::credentials and web::web_proxy class going forward.
using web::credentials;
using web::web_proxy;

/// HTTP client configuration class, used to set the possible configuration options
/// used to create an http_client instance.
class http_client_config
{
public:
    http_client_config() :
        m_guarantee_order(false),
        m_timeout(std::chrono::seconds(30)),
        m_chunksize(0),
        m_request_compressed(false)
#if !defined(__cplusplus_winrt)
        , m_validate_certificates(true)
#endif
        , m_set_user_nativehandle_options([](native_handle)->void{})
#if !defined(_WIN32) && !defined(__cplusplus_winrt)
        , m_tlsext_sni_enabled(true)
#endif
#if defined(_WIN32) && !defined(__cplusplus_winrt)
        , m_buffer_request(false)
#endif
    {
    }

#if !defined(CPPREST_TARGET_XP)
    /// Get OAuth 1.0 configuration.
    /// Returns shared pointer to OAuth 1.0 configuration.
    const std::shared_ptr<oauth1::experimental::oauth1_config> oauth1() const { return m_oauth1; }

    /// Set OAuth 1.0 configuration.
    /// <param name="config">OAuth 1.0 configuration to set.
    void set_oauth1(oauth1::experimental::oauth1_config config) { m_oauth1 = std::make_shared<oauth1::experimental::oauth1_config>(std::move(config)); }
#endif

    /// Get OAuth 2.0 configuration.
    /// Returns shared pointer to OAuth 2.0 configuration.
    const std::shared_ptr<oauth2::experimental::oauth2_config> oauth2() const { return m_oauth2; }

    /// Set OAuth 2.0 configuration.
    /// <param name="config">OAuth 2.0 configuration to set.
    void set_oauth2(oauth2::experimental::oauth2_config config) { m_oauth2 = std::make_shared<oauth2::experimental::oauth2_config>(std::move(config)); }

    /// Get the web proxy object
    /// Returns a reference to the web proxy object.
    const web_proxy& proxy() const { return m_proxy; }

    /// Set the web proxy object
    /// <param name="proxy">A reference to the web proxy object.
    void set_proxy(web_proxy proxy) { m_proxy = std::move(proxy); }

    /// Get the client credentials
    /// Returns a reference to the client credentials.
    const http::client::credentials& credentials() const { return m_credentials; }

    /// Set the client credentials
    /// <param name="cred">A reference to the client credentials.
    void set_credentials(const http::client::credentials& cred) { m_credentials = cred; }

    /// Get the 'guarantee order' property
    /// Returns the value of the property.
    bool guarantee_order() const { return m_guarantee_order; }

    /// Set the 'guarantee order' property
    /// <param name="guarantee_order">The value of the property.
    CASABLANCA_DEPRECATED("Confusing API will be removed in future releases. If you need to order HTTP requests use task continuations.")
    void set_guarantee_order(bool guarantee_order) { m_guarantee_order = guarantee_order; }

    /// Get the timeout
    /// Returns the timeout (in seconds) used for each send and receive operation on the client.
    utility::seconds timeout() const { return std::chrono::duration_cast<utility::seconds>(m_timeout); }

    /// Get the timeout
    /// Returns the timeout (in whatever duration) used for each send and receive operation on the client.
    template <class T>
    T timeout() const { return std::chrono::duration_cast<T>(m_timeout); }
    /// Set the timeout
    /// <param name="timeout">The timeout (duration from microseconds range and up) used for each send and receive operation on the client.
    template <class T> void set_timeout(const T &timeout) { m_timeout = std::chrono::duration_cast<std::chrono::microseconds>(timeout); }

    /// Get the client chunk size. Returns the internal buffer size used by the http client when sending and receiving data from the network.
    size_t chunksize() const { return m_chunksize == 0 ? 64 * 1024 : m_chunksize; }

    /// Sets the client chunk size.
    /// <param name="size">The internal buffer size used by the http client when sending and receiving data from the network.
    /// This is a hint -- an implementation may disregard the setting and use some other chunk size.
    void set_desired_chunk_size(size_t size) { m_chunksize = size; }

    /// Returns true if the default chunk size is in use. If true, implementations are allowed to choose whatever size is best.
    /// Returns true if default, false if set by user.
    bool is_default_chunksize() const { return m_chunksize == 0; }

    /// Checks if requesting a compressed response is turned on, the default is off. Returns true if compressed response is enabled, false otherwise
    bool request_compressed_response() const { return m_request_compressed; }

    /// Request that the server responds with a compressed body.
    /// If true, in cases where the server does not support compression, this will have no effect.
    /// The response body is internally decompressed before the consumer receives the data.
    /// <param name="request_compressed">True to turn on response body compression, false otherwise.
    /// Please note there is a performance cost due to copying the request data. Currently only supported on Windows and OSX.
    void set_request_compressed_response(bool request_compressed) { m_request_compressed = request_compressed; }

#if !defined(__cplusplus_winrt)
    /// Gets the server certificate validation property. Returns true if certificates are to be verified, false otherwise.
    bool validate_certificates() const { return m_validate_certificates; }

    /// Sets the server certificate validation property.
    /// <param name="validate_certs">False to turn ignore all server certificate validation errors, true otherwise.
    /// Note ignoring certificate errors can be dangerous and should be done with caution.
    void set_validate_certificates(bool validate_certs) { m_validate_certificates = validate_certs; }
#endif

#if defined(_WIN32) && !defined(__cplusplus_winrt)
	/// Checks if request data buffering is turned on, the default is off.
	/// Returns true if buffering is enabled, false otherwise
	bool buffer_request() const { return m_buffer_request; }
	
	/// Sets the request buffering property. If true, in cases where the request body/stream doesn't support seeking the request data will be buffered.
	/// This can help in situations where an authentication challenge might be expected. Please note there is a performance cost due to copying the request data.
	void set_buffer_request(bool buffer_request) { m_buffer_request = buffer_request;
	}
#endif
	
	/// Sets a callback to enable custom setting of platform specific options.
	/// The native_handle is the following type depending on the underlying platform:
	///     Windows Desktop, WinHTTP - HINTERNET
	///     Windows Runtime, WinRT - IXMLHTTPRequest2 *
	///     All other platforms, Boost.Asio:
	///         https - boost::asio::ssl::stream<boost::asio::ip::tcp::socket &> *
	///         http - boost::asio::ip::tcp::socket *
	    /// <param name="callback">A user callback allowing for customization of the request
	void set_nativehandle_options(const std::function<void(native_handle)> &callback) { m_set_user_nativehandle_options = callback; }
	
	/// Invokes a user's callback to allow for customization of the request.
	/// <param name="handle">A internal implementation handle.
	void invoke_nativehandle_options(native_handle handle) const { m_set_user_nativehandle_options(handle); }
	
#if !defined(_WIN32) && !defined(__cplusplus_winrt)
    /// Sets a callback to enable custom setting of the ssl context, at construction time.
    /// <param name="callback">A user callback allowing for customization of the ssl context at construction time.
    void set_ssl_context_callback(const std::function<void(boost::asio::ssl::context&)>& callback) { m_ssl_context_callback = callback; }

    /// Gets the user's callback to allow for customization of the ssl context.
    const std::function<void(boost::asio::ssl::context&)>& get_ssl_context_callback() const { return m_ssl_context_callback; }

    /// Gets the TLS extension server name indication (SNI) status.
    /// Returns true if TLS server name indication is enabled, false otherwise.
    bool is_tlsext_sni_enabled() const { return m_tlsext_sni_enabled; }

    /// Sets the TLS extension server name indication (SNI) status.
    /// <param name="tlsext_sni_enabled">False to disable the TLS (ClientHello) extension for server name indication, true otherwise.
    /// Note: This setting is enabled by default as it is required in most virtual hosting scenarios.
    void set_tlsext_sni_enabled(bool tlsext_sni_enabled) { m_tlsext_sni_enabled = tlsext_sni_enabled; }
#endif

private:
#if !defined(CPPREST_TARGET_XP)
    std::shared_ptr<oauth1::experimental::oauth1_config> m_oauth1;
#endif

    std::shared_ptr<oauth2::experimental::oauth2_config> m_oauth2;
    web_proxy m_proxy;
    http::client::credentials m_credentials;
    // Whether or not to guarantee ordering, i.e. only using one underlying TCP connection.
    bool m_guarantee_order;

    std::chrono::microseconds m_timeout;
    size_t m_chunksize;
    bool m_request_compressed;

#if !defined(__cplusplus_winrt)
    // IXmlHttpRequest2 doesn't allow configuration of certificate verification.
    bool m_validate_certificates;
#endif

    std::function<void(native_handle)> m_set_user_nativehandle_options;

#if !defined(_WIN32) && !defined(__cplusplus_winrt)
    std::function<void(boost::asio::ssl::context&)> m_ssl_context_callback;
    bool m_tlsext_sni_enabled;
#endif
#if defined(_WIN32) && !defined(__cplusplus_winrt)
    bool m_buffer_request;
#endif
};

class http_pipeline;

/// HTTP client class, used to maintain a connection to an HTTP service for an extended session.
class http_client
{
public:
    /// Creates a new http_client connected to specified uri.
    /// <param name="base_uri">A string representation of the base uri to be used for all requests. Must start with either "http://" or "https://"
    _ASYNCRTIMP http_client(const uri &base_uri);

    /// Creates a new http_client connected to specified uri.
    /// <param name="base_uri">A string representation of the base uri to be used for all requests. Must start with either "http://" or "https://"
    /// <param name="client_config">The http client configuration object containing the possible configuration options to initialize the <c>http_client</c>. 
    _ASYNCRTIMP http_client(const uri &base_uri, const http_client_config &client_config);

    /// Note the destructor doesn't necessarily close the connection and release resources.
    /// The connection is reference counted with the http_responses.
    _ASYNCRTIMP ~http_client() CPPREST_NOEXCEPT;

    /// Gets the base URI.
    /// <returns>
    /// A base URI initialized in constructor
    /// 
    _ASYNCRTIMP const uri& base_uri() const;

    /// Get client configuration object
    /// Returns a reference to the client configuration object.
    _ASYNCRTIMP const http_client_config& client_config() const;

    /// Adds an HTTP pipeline stage to the client.
    /// <param name="handler">A function object representing the pipeline stage.
    _ASYNCRTIMP void add_handler(const std::function<pplx::task<http_response> __cdecl(http_request, std::shared_ptr<http::http_pipeline_stage>)> &handler);


    /// Adds an HTTP pipeline stage to the client.
    /// <param name="stage">A shared pointer to a pipeline stage.
    _ASYNCRTIMP void add_handler(const std::shared_ptr<http::http_pipeline_stage> &stage);

    /// Asynchronously sends an HTTP request.
    /// <param name="request">Request to send.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    _ASYNCRTIMP pplx::task<http_response> request(http_request request, const pplx::cancellation_token &token = pplx::cancellation_token::none());

    /// Asynchronously sends an HTTP request.
    /// <param name="mtd">HTTP request method.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    pplx::task<http_response> request(const method &mtd, const pplx::cancellation_token &token = pplx::cancellation_token::none())
    {
        http_request msg(mtd);
        return request(msg, token);
    }

    /// Asynchronously sends an HTTP request.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utility::string_t &path_query_fragment,
        const pplx::cancellation_token &token = pplx::cancellation_token::none())
    {
        http_request msg(mtd);
        msg.set_request_uri(path_query_fragment);
        return request(msg, token);
    }

    /// Asynchronously sends an HTTP request.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="body_data">The data to be used as the message body, represented using the json object library.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utility::string_t &path_query_fragment,
        const json::value &body_data,
        const pplx::cancellation_token &token = pplx::cancellation_token::none())
    {
        http_request msg(mtd);
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body_data);
        return request(msg, token);
    }

    /// Asynchronously sends an HTTP request with a string body. Assumes the
    /// character encoding of the string is UTF-8.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="content_type">A string holding the MIME type of the message body.
    /// <param name="body_data">String containing the text to use in the message body.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utf8string &path_query_fragment,
        const utf8string &body_data,
        const utf8string &content_type = "text/plain; charset=utf-8",
        const pplx::cancellation_token &token = pplx::cancellation_token::none())
    {
        http_request msg(mtd);
        msg.set_request_uri(::utility::conversions::to_string_t(path_query_fragment));
        msg.set_body(body_data, content_type);
        return request(msg, token);
    }

    /// Asynchronously sends an HTTP request with a string body. Assumes the
    /// character encoding of the string is UTF-8.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="content_type">A string holding the MIME type of the message body.
    /// <param name="body_data">String containing the text to use in the message body.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utf8string &path_query_fragment,
        utf8string &&body_data,
        const utf8string &content_type = "text/plain; charset=utf-8",
        const pplx::cancellation_token &token = pplx::cancellation_token::none())
    {
        http_request msg(mtd);
        msg.set_request_uri(::utility::conversions::to_string_t(path_query_fragment));
        msg.set_body(std::move(body_data), content_type);
        return request(msg, token);
    }

    /// Asynchronously sends an HTTP request with a string body. Assumes the
    /// character encoding of the string is UTF-16 will perform conversion to UTF-8.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="content_type">A string holding the MIME type of the message body.
    /// <param name="body_data">String containing the text to use in the message body.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utf16string &path_query_fragment,
        const utf16string &body_data,
        const utf16string &content_type = ::utility::conversions::to_utf16string("text/plain"),
        const pplx::cancellation_token &token = pplx::cancellation_token::none())
    {
        http_request msg(mtd);
        msg.set_request_uri(::utility::conversions::to_string_t(path_query_fragment));
        msg.set_body(body_data, content_type);
        return request(msg, token);
    }

    /// Asynchronously sends an HTTP request with a string body. Assumes the
    /// character encoding of the string is UTF-8.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="body_data">String containing the text to use in the message body.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utf8string &path_query_fragment,
        const utf8string &body_data,
        const pplx::cancellation_token &token)
    {
        return request(mtd, path_query_fragment, body_data, "text/plain; charset=utf-8", token);
    }

    /// Asynchronously sends an HTTP request with a string body. Assumes the
    /// character encoding of the string is UTF-8.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="body_data">String containing the text to use in the message body.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utf8string &path_query_fragment,
        utf8string &&body_data,
        const pplx::cancellation_token &token)
    {
        http_request msg(mtd);
        msg.set_request_uri(::utility::conversions::to_string_t(path_query_fragment));
        msg.set_body(std::move(body_data), "text/plain; charset=utf-8");
        return request(msg, token);
    }

    /// Asynchronously sends an HTTP request with a string body. Assumes
    /// the character encoding of the string is UTF-16 will perform conversion to UTF-8.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="body_data">String containing the text to use in the message body.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns an asynchronous operation that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utf16string &path_query_fragment,
        const utf16string &body_data,
        const pplx::cancellation_token &token)
    {
        return request(mtd, path_query_fragment, body_data, ::utility::conversions::to_utf16string("text/plain"), token);
    }

#if !defined (__cplusplus_winrt)
    /// Asynchronously sends an HTTP request.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="body">An asynchronous stream representing the body data.
    /// <param name="content_type">A string holding the MIME type of the message body.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns a task that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utility::string_t &path_query_fragment,
        const concurrency::streams::istream &body,
        const utility::string_t &content_type = _XPLATSTR("application/octet-stream"),
        const pplx::cancellation_token &token = pplx::cancellation_token::none())
    {
        http_request msg(mtd);
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body, content_type);
        return request(msg, token);
    }

    /// Asynchronously sends an HTTP request.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="body">An asynchronous stream representing the body data.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns a task that is completed once a response from the request is received.
    pplx::task<http_response> request(
        const method &mtd,
        const utility::string_t &path_query_fragment,
        const concurrency::streams::istream &body,
        const pplx::cancellation_token &token)
    {
        return request(mtd, path_query_fragment, body, _XPLATSTR("application/octet-stream"), token);
    }
#endif // __cplusplus_winrt

    /// Asynchronously sends an HTTP request.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="body">An asynchronous stream representing the body data.
    /// <param name="content_length">Size of the message body.
    /// <param name="content_type">A string holding the MIME type of the message body.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns a task that is completed once a response from the request is received.
    /// Winrt requires to provide content_length.
    pplx::task<http_response> request(
        const method &mtd,
        const utility::string_t &path_query_fragment,
        const concurrency::streams::istream &body,
        size_t content_length,
        const utility::string_t &content_type = _XPLATSTR("application/octet-stream"),
        const pplx::cancellation_token &token = pplx::cancellation_token::none())
    {
        http_request msg(mtd);
        msg.set_request_uri(path_query_fragment);
        msg.set_body(body, content_length, content_type);
        return request(msg, token);
    }

    /// Asynchronously sends an HTTP request.
    /// <param name="mtd">HTTP request method.
    /// <param name="path_query_fragment">String containing the path, query, and fragment, relative to the http_client's base URI.
    /// <param name="body">An asynchronous stream representing the body data.
    /// <param name="content_length">Size of the message body.
    /// <param name="token">Cancellation token for cancellation of this request operation.
    /// Returns a task that is completed once a response from the request is received.
    /// Winrt requires to provide content_length.
    pplx::task<http_response> request(
        const method &mtd,
        const utility::string_t &path_query_fragment,
        const concurrency::streams::istream &body,
        size_t content_length,
        const pplx::cancellation_token &token)
    {
        return request(mtd, path_query_fragment, body, content_length, _XPLATSTR("application/octet-stream"), token);
    }

private:

    std::shared_ptr<::web::http::client::http_pipeline> m_pipeline;
};

namespace details {
#if defined(_WIN32)
extern const utility::char_t * get_with_body_err_msg;
#endif

}

}}}

#endif
