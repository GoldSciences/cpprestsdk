// Protocol independent support for URIs.
// For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <functional>

#include "cpprest/asyncrt_utils.h"
#include "cpprest/details/basic_types.h"

namespace web {
    namespace details
    {
        struct uri_components
        {
            uri_components() : m_path(_XPLATSTR("/")), m_port(-1)
            {}

            uri_components(const uri_components &other) :
                m_scheme(other.m_scheme),
                m_host(other.m_host),
                m_user_info(other.m_user_info),
                m_path(other.m_path),
                m_query(other.m_query),
                m_fragment(other.m_fragment),
                m_port(other.m_port)
            {}

            uri_components & operator=(const uri_components &other)
            {
                if (this != &other)
                {
                    m_scheme = other.m_scheme;
                    m_host = other.m_host;
                    m_user_info = other.m_user_info;
                    m_path = other.m_path;
                    m_query = other.m_query;
                    m_fragment = other.m_fragment;
                    m_port = other.m_port;
                }
                return *this;
            }

            uri_components(uri_components &&other) CPPREST_NOEXCEPT :
                m_scheme(std::move(other.m_scheme)),
                m_host(std::move(other.m_host)),
                m_user_info(std::move(other.m_user_info)),
                m_path(std::move(other.m_path)),
                m_query(std::move(other.m_query)),
                m_fragment(std::move(other.m_fragment)),
                m_port(other.m_port)
            {}

            uri_components & operator=(uri_components &&other) CPPREST_NOEXCEPT
            {
                if (this != &other)
                {
                    m_scheme = std::move(other.m_scheme);
                    m_host = std::move(other.m_host);
                    m_user_info = std::move(other.m_user_info);
                    m_path = std::move(other.m_path);
                    m_query = std::move(other.m_query);
                    m_fragment = std::move(other.m_fragment);
                    m_port = other.m_port;
                }
                return *this;
            }

            _ASYNCRTIMP utility::string_t join();

            utility::string_t m_scheme;
            utility::string_t m_host;
            utility::string_t m_user_info;
            utility::string_t m_path;
            utility::string_t m_query;
            utility::string_t m_fragment;
            int m_port;
        };
    }

    /// A single exception type to represent errors in parsing, encoding, and decoding URIs.
    class uri_exception : public std::exception
    {
    public:

        uri_exception(std::string msg) : m_msg(std::move(msg)) {}

        ~uri_exception() CPPREST_NOEXCEPT {}

        const char* what() const CPPREST_NOEXCEPT
        {
            return m_msg.c_str();
        }

    private:
        std::string m_msg;
    };

    /// A flexible, protocol independent URI implementation.
    ///
    /// URI instances are immutable. Querying the various fields on an emtpy URI will return empty strings. Querying
    /// various diagnostic members on an empty URI will return false.
        /// This implementation accepts both URIs ('http://msn.com/path') and URI relative-references
    /// ('/path?query#frag').
    ///
    /// This implementation does not provide any scheme-specific handling -- an example of this
    /// would be the following: 'http://path1/path'. This is a valid URI, but it's not a valid
    /// http-uri -- that is, it's syntactically correct but does not conform to the requirements
    /// of the http scheme (http requires a host).
    /// We could provide this by allowing a pluggable 'scheme' policy-class, which would provide
    /// extra capability for validating and canonicalizing a URI according to scheme, and would
    /// introduce a layer of type-safety for URIs of differing schemes, and thus differing semantics.
    ///
    /// One issue with implementing a scheme-independent URI facility is that of comparing for equality.
    /// For instance, these URIs are considered equal 'http://msn.com', 'http://msn.com:80'. That is --
    /// the 'default' port can be either omitted or explicit. Since we don't have a way to map a scheme
    /// to it's default port, we don't have a way to know these are equal. This is just one of a class of
    /// issues with regard to scheme-specific behavior.
        class uri
    {
    public:

            /// The various components of a URI. This enum is used to indicate which
        /// URI component is being encoded to the encode_uri_component. This allows
        /// specific encoding to be performed.
        ///
        /// Scheme and port don't allow '%' so they don't need to be encoded.
            class components
        {
        public:
            enum component
            {
                user_info,
                host,
                path,
                query,
                fragment,
                full_uri
            };
        };

            /// Encodes a URI component according to RFC 3986.
        /// Note if a full URI is specified instead of an individual URI component all
        /// characters not in the unreserved set are escaped.
            /// <param name="raw">The URI as a string.
        /// Returns the encoded string.
        _ASYNCRTIMP static utility::string_t __cdecl encode_uri(const utility::string_t &raw, uri::components::component = components::full_uri);

            /// Encodes a string by converting all characters except for RFC 3986 unreserved characters to their
        /// hexadecimal representation.
            /// <param name="utf8data">The UTF-8 string data.
        /// Returns the encoded string.
        _ASYNCRTIMP static utility::string_t __cdecl encode_data_string(const utility::string_t &utf8data);

            /// Decodes an encoded string.
            /// <param name="encoded">The URI as a string.
        /// Returns the decoded string.
        _ASYNCRTIMP static utility::string_t __cdecl decode(const utility::string_t &encoded);

            /// Splits a path into its hierarchical components.
            /// <param name="path">The path as a string
        /// Returns a <c>std::vector&lt;utility::string_t&gt;</c> containing the segments in the path.
        _ASYNCRTIMP static std::vector<utility::string_t> __cdecl split_path(const utility::string_t &path);

            /// Splits a query into its key-value components.
            /// <param name="query">The query string
        /// Returns a <c>std::map&lt;utility::string_t, utility::string_t&gt;</c> containing the key-value components of the query.
        _ASYNCRTIMP static std::map<utility::string_t, utility::string_t> __cdecl split_query(const utility::string_t &query);

            /// Validates a string as a URI.
            /// <param name="uri_string">The URI string to be validated.
        /// Returns true if the given string represents a valid URI, false otherwise.
        _ASYNCRTIMP static bool __cdecl validate(const utility::string_t &uri_string);

            /// Creates an empty uri
            uri() { m_uri = _XPLATSTR("/");};

            /// Creates a URI from the given URI components.
            /// <param name="components">A URI components object to create the URI instance.
        _ASYNCRTIMP uri(const details::uri_components &components);

            /// Creates a URI from the given encoded string. This will throw an exception if the string
        /// does not contain a valid URI. Use uri::validate if processing user-input.
            /// <param name="uri_string">A pointer to an encoded string to create the URI instance.
        _ASYNCRTIMP uri(const utility::char_t *uri_string);

            /// Creates a URI from the given encoded string. This will throw an exception if the string
        /// does not contain a valid URI. Use uri::validate if processing user-input.
            /// <param name="uri_string">An encoded URI string to create the URI instance.
        _ASYNCRTIMP uri(const utility::string_t &uri_string);

            /// Copy constructor.
            uri(const uri &other) :
            m_uri(other.m_uri),
            m_components(other.m_components)
        {}

            /// Copy assignment operator.
            uri & operator=(const uri &other)
        {
            if (this != &other)
            {
                m_uri = other.m_uri;
                m_components = other.m_components;
            }
            return *this;
        }

            /// Move constructor.
            uri(uri &&other) CPPREST_NOEXCEPT :
            m_uri(std::move(other.m_uri)),
            m_components(std::move(other.m_components))
        {}

            /// Move assignment operator
            uri & operator=(uri &&other) CPPREST_NOEXCEPT
        {
            if (this != &other)
            {
                m_uri = std::move(other.m_uri);
                m_components = std::move(other.m_components);
            }
            return *this;
        }

            /// Get the scheme component of the URI as an encoded string.
            /// Returns the URI scheme as a string.
        const utility::string_t &scheme() const { return m_components.m_scheme; }

            /// Get the user information component of the URI as an encoded string.
            /// Returns the URI user information as a string.
        const utility::string_t &user_info() const { return m_components.m_user_info; }

            /// Get the host component of the URI as an encoded string.
            /// Returns the URI host as a string.
        const utility::string_t &host() const { return m_components.m_host; }

            /// Get the port component of the URI. Returns -1 if no port is specified.
            /// Returns the URI port as an integer.
        int port() const { return m_components.m_port; }

            /// Get the path component of the URI as an encoded string.
            /// Returns the URI path as a string.
        const utility::string_t &path() const { return m_components.m_path; }

            /// Get the query component of the URI as an encoded string.
            /// Returns the URI query as a string.
        const utility::string_t &query() const { return m_components.m_query; }

            /// Get the fragment component of the URI as an encoded string.
            /// Returns the URI fragment as a string.
        const utility::string_t &fragment() const { return m_components.m_fragment; }

            /// Creates a new uri object with the same authority portion as this one, omitting the resource and query portions.
            /// Returns the new uri object with the same authority.
        _ASYNCRTIMP uri authority() const;

            /// Gets the path, query, and fragment portion of this uri, which may be empty.
            /// Returns the new URI object with the path, query and fragment portion of this URI.
        _ASYNCRTIMP uri resource() const;

            /// An empty URI specifies no components, and serves as a default value
            bool is_empty() const
        {
            return this->m_uri.empty() || this->m_uri == _XPLATSTR("/");
        }

            /// A loopback URI is one which refers to a hostname or ip address with meaning only on the local machine.
                    /// Examples include "locahost", or ip addresses in the loopback range (127.0.0.0/24).
                /// Returns true if this URI references the local host, false otherwise.
        bool is_host_loopback() const
        {
            return !is_empty() && ((host() == _XPLATSTR("localhost")) || (host().size() > 4 && host().substr(0,4) == _XPLATSTR("127.")));
        }

            /// A wildcard URI is one which refers to all hostnames that resolve to the local machine (using the * or +)
            /// <example>
        /// http://*:80
        /// </example>
        bool is_host_wildcard() const
        {
            return !is_empty() && (this->host() == _XPLATSTR("*") || this->host() == _XPLATSTR("+"));
        }

            /// A portable URI is one with a hostname that can be resolved globally (used from another machine).
            /// Returns true if this URI can be resolved globally (used from another machine), false otherwise.
                /// The hostname "localhost" is a reserved name that is guaranteed to resolve to the local machine,
        /// and cannot be used for inter-machine communication. Likewise the hostnames "*" and "+" on Windows
        /// represent wildcards, and do not map to a resolvable address.
                bool is_host_portable() const
        {
            return !(is_empty() || is_host_loopback() || is_host_wildcard());
        }

        // <summary>
        /// A default port is one where the port is unspecified, and will be determined by the operating system.
        /// The choice of default port may be dictated by the scheme (http -> 80) or not.
            /// Returns true if this URI instance has a default port, false otherwise.
        bool is_port_default() const
        {
            return !is_empty() && this->port() == 0;
        }

            /// An "authority" URI is one with only a scheme, optional userinfo, hostname, and (optional) port.
            /// Returns true if this is an "authority" URI, false otherwise.
        bool is_authority() const
        {
            return !is_empty() && is_path_empty() && query().empty() && fragment().empty();
        }

            /// Returns whether the other URI has the same authority as this one
            /// <param name="other">The URI to compare the authority with.
        /// Returns true if both the URI's have the same authority, false otherwise.
        bool has_same_authority(const uri &other) const
        {
            return !is_empty() && this->authority() == other.authority();
        }

            /// Returns whether the path portion of this URI is empty
            /// Returns true if the path portion of this URI is empty, false otherwise.
        bool is_path_empty() const
        {
            return path().empty() || path() == _XPLATSTR("/");
        }

            /// Returns the full (encoded) URI as a string.
             /// Returns the full encoded URI string.
        utility::string_t to_string() const
        {
            return m_uri;
        }

        _ASYNCRTIMP bool operator == (const uri &other) const;

        bool operator < (const uri &other) const
        {
            return m_uri < other.m_uri;
        }

        bool operator != (const uri &other) const
        {
            return !(this->operator == (other));
        }

    private:
        friend class uri_builder;

        // Encodes all characters not in given set determined by given function.
        _ASYNCRTIMP static utility::string_t __cdecl encode_impl(const utility::string_t &raw, const std::function<bool __cdecl(int)>& should_encode);

        utility::string_t m_uri;
        details::uri_components m_components;
    };

} // namespace web
