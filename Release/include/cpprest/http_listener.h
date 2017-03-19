/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* HTTP Library: HTTP listener (server-side) APIs
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_HTTP_LISTENER_H
#define _CASA_HTTP_LISTENER_H

#include <limits>
#include <functional>

#include "cpprest/http_msg.h"
#if !defined(_WIN32) && !defined(__cplusplus_winrt)
#include <boost/asio/ssl.hpp>
#endif

#if !defined(_WIN32) || (_WIN32_WINNT >= _WIN32_WINNT_VISTA && !defined(__cplusplus_winrt))

namespace web
{
namespace http
{
/// HTTP listener is currently in beta.
namespace experimental
{
/// HTTP server side library.
namespace listener
{

/// Configuration class used to set various options when constructing and http_listener instance.
class http_listener_config
{
public:

    /// Create an http_listener configuration with default options.
    http_listener_config()
        : m_timeout(utility::seconds(120))
    {}

    /// Copy constructor.
    /// <param name="other">http_listener_config to copy.
    http_listener_config(const http_listener_config &other)
        : m_timeout(other.m_timeout)
#ifndef _WIN32
        , m_ssl_context_callback(other.m_ssl_context_callback)
#endif
    {}

    /// Move constructor.
    /// <param name="other">http_listener_config to move from.
    http_listener_config(http_listener_config &&other)
        : m_timeout(std::move(other.m_timeout))
#ifndef _WIN32
        , m_ssl_context_callback(std::move(other.m_ssl_context_callback))
#endif
    {}

    /// Assignment operator.
    /// <returns>http_listener_config instance.
    http_listener_config & operator=(const http_listener_config &rhs)
    {
        if(this != &rhs)
        {
            m_timeout = rhs.m_timeout;
#ifndef _WIN32
            m_ssl_context_callback = rhs.m_ssl_context_callback;
#endif
        }
        return *this;
    }

    /// Assignment operator.
    /// <returns>http_listener_config instance.
    http_listener_config & operator=(http_listener_config &&rhs)
    {
        if(this != &rhs)
        {
            m_timeout = std::move(rhs.m_timeout);
#ifndef _WIN32
            m_ssl_context_callback = std::move(rhs.m_ssl_context_callback);
#endif
        }
        return *this;
    }

    /// Get the timeout
    /// Returns the timeout (in seconds).
    utility::seconds timeout() const
    {
        return m_timeout;
    }

    /// Set the timeout
    /// <param name="timeout">The timeout (in seconds) used for each send and receive operation on the client.
    void set_timeout(utility::seconds timeout)
    {
        m_timeout = std::move(timeout);
    }

#ifndef _WIN32
    /// Get the callback of ssl context
    /// Returns the function defined by the user of http_listener_config to configure a ssl context.
    const std::function<void(boost::asio::ssl::context&)>& get_ssl_context_callback() const
    {
        return m_ssl_context_callback;
    }

    /// Set the callback of ssl context
    /// <param name="ssl_context_callback">The function to configure a ssl context which will setup https connections.
    void set_ssl_context_callback(const std::function<void(boost::asio::ssl::context&)> &ssl_context_callback)
    {
        m_ssl_context_callback = ssl_context_callback;
    }
#endif

private:

    utility::seconds m_timeout;
#ifndef _WIN32
    std::function<void(boost::asio::ssl::context&)> m_ssl_context_callback;
#endif
};

namespace details
{

/// Internal class for pointer to implementation design pattern.
class http_listener_impl
{
public:

    http_listener_impl()
        : m_closed(true)
        , m_close_task(pplx::task_from_result())
    {
    }

    _ASYNCRTIMP http_listener_impl(http::uri address);
    _ASYNCRTIMP http_listener_impl(http::uri address, http_listener_config config);

    _ASYNCRTIMP pplx::task<void> open();
    _ASYNCRTIMP pplx::task<void> close();

    /// Handler for all requests. The HTTP host uses this to dispatch a message to the pipeline.
    /// Only HTTP server implementations should call this API.
    _ASYNCRTIMP void handle_request(http::http_request msg);

    const http::uri & uri() const { return m_uri; }

    const http_listener_config & configuration() const { return m_config; }

    // Handlers
    std::function<void(http::http_request)> m_all_requests;
    std::map<http::method, std::function<void(http::http_request)>> m_supported_methods;

private:

    // Default implementation for TRACE and OPTIONS.
    void handle_trace(http::http_request message);
    void handle_options(http::http_request message);

    // Gets a comma separated string containing the methods supported by this listener.
    utility::string_t get_supported_methods() const;

    http::uri m_uri;
    http_listener_config m_config;

    // Used to record that the listener is closed.
    bool m_closed;
    pplx::task<void> m_close_task;
};

} // namespace details

/// A class for listening and processing HTTP requests at a specific URI.
class http_listener
{
public:

    /// Create a listener from a URI.
    /// The listener will not have been opened when returned.
    /// <param name="address">URI at which the listener should accept requests.
    http_listener(http::uri address)
        : m_impl(utility::details::make_unique<details::http_listener_impl>(std::move(address)))
    {
    }

    /// Create a listener with specified URI and configuration.
    /// <param name="address">URI at which the listener should accept requests.
    /// <param name="config">Configuration to create listener with.
    http_listener(http::uri address, http_listener_config config)
        : m_impl(utility::details::make_unique<details::http_listener_impl>(std::move(address), std::move(config)))
    {
    }

    /// Default constructor.
    /// The resulting listener cannot be used for anything, but is useful to initialize a variable
    /// that will later be overwritten with a real listener instance.
    http_listener()
        : m_impl(utility::details::make_unique<details::http_listener_impl>())
    {
    }

    /// Destructor frees any held resources.
    /// Call close() before allowing a listener to be destroyed.
    _ASYNCRTIMP ~http_listener();

    /// Asynchronously open the listener, i.e. start accepting requests.
    /// Returns a task that will be completed once this listener is actually opened, accepting requests.
    pplx::task<void> open()
    {
        return m_impl->open();
    }

    /// Asynchronously stop accepting requests and close all connections.
    /// Returns a task that will be completed once this listener is actually closed, no longer accepting requests.
        /// This function will stop accepting requests and wait for all outstanding handler calls
    /// to finish before completing the task. Waiting on the task returned from close() within
    /// a handler and blocking waiting for its result will result in a deadlock.
    ///
    /// Call close() before allowing a listener to be destroyed.
        pplx::task<void> close()
    {
        return m_impl->close();
    }

    /// Add a general handler to support all requests.
    /// <param name="handler">Function object to be called for all requests.
    void support(const std::function<void(http_request)> &handler)
    {
        m_impl->m_all_requests = handler;
    }

    /// Add support for a specific HTTP method.
    /// <param name="method">An HTTP method.
    /// <param name="handler">Function object to be called for all requests for the given HTTP method.
    void support(const http::method &method, const std::function<void(http_request)> &handler)
    {
        m_impl->m_supported_methods[method] = handler;
    }

    /// Get the URI of the listener.
    /// Returns the URI this listener is for.
    const http::uri & uri() const { return m_impl->uri(); }

    /// Get the configuration of this listener.
    /// <returns>Configuration this listener was constructed with.
    const http_listener_config & configuration() const { return m_impl->configuration(); }

    /// Move constructor.
    /// <param name="other">http_listener instance to construct this one from.
    http_listener(http_listener &&other)
        : m_impl(std::move(other.m_impl))
    {
    }

    /// Move assignment operator.
    /// <param name="other">http_listener to replace this one with.
    http_listener &operator=(http_listener &&other)
    {
        if(this != &other)
        {
            m_impl = std::move(other.m_impl);
        }
        return *this;
    }

private:

    // No copying of listeners.
    http_listener(const http_listener &other);
    http_listener &operator=(const http_listener &other);

    std::unique_ptr<details::http_listener_impl> m_impl;
};

}}}}

#endif
#endif
