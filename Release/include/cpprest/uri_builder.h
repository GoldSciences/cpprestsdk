// Builder style class for creating URIs. For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#include <sstream>
#include <string>
#include <vector>

#include "cpprest/base_uri.h"
#include "cpprest/details/uri_parser.h"

namespace web
{
    /// Builder for constructing URIs incrementally.
    class uri_builder
    {
    public:

            /// Creates a builder with an initially empty URI.
            uri_builder() {}

            /// Creates a builder with a existing URI object.
            /// <param name="uri_str">Encoded string containing the URI.
        uri_builder(const uri &uri_str): m_uri(uri_str.m_components) {}

            /// Get the scheme component of the URI as an encoded string.
            /// Returns the URI scheme as a string.
        const utility::string_t &scheme() const { return m_uri.m_scheme; }

            /// Get the user information component of the URI as an encoded string.
            /// Returns the URI user information as a string.
        const utility::string_t &user_info() const { return m_uri.m_user_info; }

            /// Get the host component of the URI as an encoded string.
            /// Returns the URI host as a string.
        const utility::string_t &host() const { return m_uri.m_host; }

            /// Get the port component of the URI. Returns -1 if no port is specified.
            /// Returns the URI port as an integer.
        int port() const { return m_uri.m_port; }

            /// Get the path component of the URI as an encoded string.
            /// Returns the URI path as a string.
        const utility::string_t &path() const { return m_uri.m_path; }

            /// Get the query component of the URI as an encoded string.
            /// Returns the URI query as a string.
        const utility::string_t &query() const { return m_uri.m_query; }

            /// Get the fragment component of the URI as an encoded string.
            /// Returns the URI fragment as a string.
        const utility::string_t &fragment() const { return m_uri.m_fragment; }

            /// Set the scheme of the URI.
            /// <param name="scheme">Uri scheme.
        /// Returns a reference to this <c>uri_builder</c> to support chaining.
        uri_builder & set_scheme(const utility::string_t &scheme)
        {
            m_uri.m_scheme = scheme;
            return *this;
        }

            /// Set the user info component of the URI.
            /// <param name="user_info">User info as a decoded string.
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.
        /// Returns a reference to this <c>uri_builder</c> to support chaining.
        uri_builder & set_user_info(const utility::string_t &user_info, bool do_encoding = false)
        {
            m_uri.m_user_info = do_encoding ? uri::encode_uri(user_info, uri::components::user_info) : user_info;
            return *this;
        }

            /// Set the host component of the URI.
            /// <param name="host">Host as a decoded string.
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.
        /// Returns a reference to this <c>uri_builder</c> to support chaining.
        uri_builder & set_host(const utility::string_t &host, bool do_encoding = false)
        {
            m_uri.m_host = do_encoding ? uri::encode_uri(host, uri::components::host) : host;
            return *this;
        }

            /// Set the port component of the URI.
            /// <param name="port">Port as an integer.
        /// Returns a reference to this <c>uri_builder</c> to support chaining.
        uri_builder & set_port(int port)
        {
            m_uri.m_port = port;
            return *this;
        }

            /// Set the port component of the URI.
            /// <param name="port">Port as a string.
        /// Returns a reference to this <c>uri_builder</c> to support chaining.
        /// When string can't be converted to an integer the port is left unchanged.
        uri_builder & set_port(const utility::string_t &port)
        {
            utility::istringstream_t portStream(port);
            int port_tmp;
            portStream >> port_tmp;
            if(portStream.fail() || portStream.bad())
            {
                throw std::invalid_argument("invalid port argument, must be non empty string containing integer value");
            }
            m_uri.m_port = port_tmp;
            return *this;
        }

            /// Set the path component of the URI.
            /// <param name="path">Path as a decoded string.
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.
        /// Returns a reference to this <c>uri_builder</c> to support chaining.
        uri_builder & set_path(const utility::string_t &path, bool do_encoding = false)
        {
            m_uri.m_path = do_encoding ? uri::encode_uri(path, uri::components::path) : path;
            return *this;
        }


            /// Set the query component of the URI.
            /// <param name="query">Query as a decoded string.
        /// <param name="do_encoding">Specify whether apply URI encoding to the given string.
        /// Returns a reference to this <c>uri_builder</c> to support chaining.
        uri_builder & set_query(const utility::string_t &query, bool do_encoding = false)
        {
            m_uri.m_query = do_encoding ? uri::encode_uri(query, uri::components::query) : query;
            return *this;
        }

            /// Set the fragment component of the URI.
            /// <param name="fragment">Fragment as a decoded string.
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.
        /// Returns a reference to this <c>uri_builder</c> to support chaining.
        uri_builder & set_fragment(const utility::string_t &fragment, bool do_encoding = false)
        {
            m_uri.m_fragment = do_encoding ? uri::encode_uri(fragment, uri::components::fragment) : fragment;
            return *this;
        }

            /// Clears all components of the underlying URI in this uri_builder.
            void clear()
        {
            m_uri = details::uri_components();
        }

            /// Appends another path to the path of this uri_builder.
            /// <param name="path">Path to append as a already encoded string.
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.
        /// Returns a reference to this uri_builder to support chaining.
        _ASYNCRTIMP uri_builder &append_path(const utility::string_t &path, bool do_encoding = false);

            /// Appends another query to the query of this uri_builder.
            /// <param name="query">Query to append as a decoded string.
        /// <param name="do_encoding">Specify whether to apply URI encoding to the given string.
        /// Returns a reference to this uri_builder to support chaining.
        _ASYNCRTIMP uri_builder &append_query(const utility::string_t &query, bool do_encoding = false);

            /// Appends an relative uri (Path, Query and fragment) at the end of the current uri.
            /// <param name="relative_uri">The relative uri to append.
        /// Returns a reference to this uri_builder to support chaining.
        _ASYNCRTIMP uri_builder &append(const uri &relative_uri);

            /// Appends another query to the query of this uri_builder, encoding it first. This overload is useful when building a query segment of
        /// the form "element=10", where the right hand side of the query is stored as a type other than a string, for instance, an integral type.
            /// <param name="name">The name portion of the query string
        /// <param name="value">The value portion of the query string
        /// Returns a reference to this uri_builder to support chaining.
        template<typename T>
        uri_builder &append_query(const utility::string_t &name, const T &value, bool do_encoding = true)
        {
            auto encodedName = name;
            auto encodedValue = utility::conversions::details::print_string(value);

            if (do_encoding)
            {
                auto encodingCheck = [](int ch)
                {
                    switch (ch)
                    {
                        // Encode '&', ';', and '=' since they are used
                        // as delimiters in query component.
                    case '&':
                    case ';':
                    case '=':
                    case '%':
                    case '+':
                        return true;
                    default:
                        return !::web::details::uri_parser::is_query_character(ch);
                    }
                };
                encodedName = uri::encode_impl(encodedName, encodingCheck);
                encodedValue = uri::encode_impl(encodedValue, encodingCheck);
            }

            auto encodedQuery = encodedName;
            encodedQuery.append(_XPLATSTR("="));
            encodedQuery.append(encodedValue);
            // The query key value pair was already encoded by us or the user separately.
            return append_query(encodedQuery, false);
        }

            /// Combine and validate the URI components into a encoded string. An exception will be thrown if the URI is invalid.
            /// Returns the created URI as a string.
        _ASYNCRTIMP utility::string_t to_string();

            /// Combine and validate the URI components into a URI class instance. An exception will be thrown if the URI is invalid.
            /// Returns the create URI as a URI class instance.
        _ASYNCRTIMP uri to_uri();

            /// Validate the generated URI from all existing components of this uri_builder.
            /// <returns>Whether the URI is valid.
        _ASYNCRTIMP bool is_valid();

    private:
        details::uri_components m_uri;
    };
} // namespace web
