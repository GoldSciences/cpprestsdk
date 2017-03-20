/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <system_error>
#include "cpprest/asyncrt_utils.h"

namespace web { namespace http {

/// Binds an individual reference to a string value.
/// <typeparam name="key_type">The type of string value.</typeparam>
/// <typeparam name="_t">The type of the value to bind to.</typeparam>
/// <param name="text">The string value.
/// <param name="ref">The value to bind to.
/// Returns true if the binding succeeds, false otherwise.
template<typename key_type, typename _t>
CASABLANCA_DEPRECATED("This API is deprecated and will be removed in a future release, std::istringstream instead.")
bool bind(const key_type &text, _t &ref) // const
{
    utility::istringstream_t iss(text);
    iss >> ref;
    if (iss.fail() || !iss.eof())
    {
        return false;
    }

    return true;
}

/// Binds an individual reference to a string value.
/// This specialization is need because <c>istringstream::&gt;&gt;</c> delimits on whitespace.
/// <typeparam name="key_type">The type of the string value.</typeparam>
/// <param name="text">The string value.
/// <param name="ref">The value to bind to.
/// Returns true if the binding succeeds, false otherwise.
template <typename key_type>
CASABLANCA_DEPRECATED("This API is deprecated and will be removed in a future release.")
bool bind(const key_type &text, utility::string_t &ref) //const
{
    ref = text;
    return true;
}

/// Represents HTTP headers, acts like a map.
class http_headers
{
public:
    /// Function object to perform case insensitive comparison of wstrings.
    struct _case_insensitive_cmp
    {
        bool operator()(const utility::string_t &str1, const utility::string_t &str2) const
        {
#ifdef _WIN32
            return _wcsicmp(str1.c_str(), str2.c_str()) < 0;
#else
            return utility::cmp::icmp(str1, str2) < 0;
#endif
        }
    };

    /// STL-style typedefs
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::key_type key_type;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::key_compare key_compare;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::allocator_type allocator_type;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::size_type size_type;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::difference_type difference_type;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::pointer pointer;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::const_pointer const_pointer;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::reference reference;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::const_reference const_reference;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::iterator iterator;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::const_iterator const_iterator;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::reverse_iterator reverse_iterator;
    typedef std::map<utility::string_t, utility::string_t, _case_insensitive_cmp>::const_reverse_iterator const_reverse_iterator;

    /// Constructs an empty set of HTTP headers.
    http_headers() {}

    /// Copy constructor.
    /// <param name="other">An <c>http_headers</c> object to copy from.
    http_headers(const http_headers &other) : m_headers(other.m_headers) {}

    /// Assignment operator.
    /// <param name="other">An <c>http_headers</c> object to copy from.
    http_headers &operator=(const http_headers &other)
    {
        if(this != &other)
        {
            m_headers = other.m_headers;
        }
        return *this;
    }

    /// Move constructor.
    /// <param name="other">An <c>http_headers</c> object to move.
    http_headers(http_headers &&other) : m_headers(std::move(other.m_headers)) {}

    /// Move assignment operator.
    /// <param name="other">An <c>http_headers</c> object to move.
    http_headers &operator=(http_headers &&other)
    {
        if(this != &other)
        {
            m_headers = std::move(other.m_headers);
        }
        return *this;
    }

    /// Adds a header field using the '&lt;&lt;' operator.
    /// <param name="name">The name of the header field.
    /// <param name="value">The value of the header field.
    /// If the header field exists, the value will be combined as comma separated string.
    template<typename _t1>
    void add(const key_type& name, const _t1& value)
    {
        if (has(name))
        {
            m_headers[name].append(_XPLATSTR(", ")).append(utility::conversions::details::print_string(value));
        }
        else
        {
            m_headers[name] = utility::conversions::details::print_string(value);
        }
    }

    /// Removes a header field.
    /// <param name="name">The name of the header field.
    void remove(const key_type& name)
    {
        m_headers.erase(name);
    }

    /// Removes all elements from the headers.
    void clear() { m_headers.clear(); }

    /// Checks if there is a header with the given key.
    /// <param name="name">The name of the header field.
    /// Returns true if there is a header with the given name, false otherwise.
    bool has(const key_type& name) const { return m_headers.find(name) != m_headers.end(); }

    /// Returns the number of header fields.
    /// <returns>Number of header fields.
    size_type size() const { return m_headers.size(); }

    /// Tests to see if there are any header fields.
    /// Returns true if there are no headers, false otherwise.
    bool empty() const { return m_headers.empty(); }

    /// Returns a reference to header field with given name, if there is no header field one is inserted.
    utility::string_t & operator[](const key_type &name) { return m_headers[name]; }

    /// Checks if a header field exists with given name and returns an iterator if found. Otherwise
    /// and iterator to end is returned.
    /// <param name="name">The name of the header field.
    /// Returns an iterator to where the HTTP header is found.
    iterator find(const key_type &name) { return m_headers.find(name); }
    const_iterator find(const key_type &name) const { return m_headers.find(name); }

    /// Attempts to match a header field with the given name using the '>>' operator.
    /// <param name="name">The name of the header field.
    /// <param name="value">The value of the header field.
    /// Returns true if header field was found and successfully stored in value parameter.
    template<typename _t1>
    bool match(const key_type &name, _t1 &value) const
    {
        auto iter = m_headers.find(name);
        if (iter != m_headers.end())
        {
            // Check to see if doesn't have a value.
            if(iter->second.empty())
            {
                bind_impl(iter->second, value);
                return true;
            }
            return bind_impl(iter->second, value);
        }
        else
        {
            return false;
        }
    }

    /// Returns an iterator referring to the first header field.
    /// Returns an iterator to the beginning of the HTTP headers
    iterator begin() { return m_headers.begin(); }
    const_iterator begin() const { return m_headers.begin(); }

    /// Returns an iterator referring to the past-the-end header field.
    /// Returns an iterator to the element past the end of the HTTP headers.
    iterator end() { return m_headers.end(); }
    const_iterator end() const { return m_headers.end(); }

    /// Gets the content length of the message.
    /// Returns the length of the content.
    _ASYNCRTIMP utility::size64_t content_length() const;

    /// Sets the content length of the message.
    /// <param name="length">The length of the content.
    _ASYNCRTIMP void set_content_length(utility::size64_t length);

    /// Gets the content type of the message.
    /// Returns the content type of the body.
    _ASYNCRTIMP utility::string_t content_type() const;

    /// Sets the content type of the message.
    /// <param name="type">The content type of the body.
    _ASYNCRTIMP void set_content_type(utility::string_t type);

    /// Gets the cache control header of the message.
    /// Returns the cache control header value.
    _ASYNCRTIMP utility::string_t cache_control() const;

    /// Sets the cache control header of the message.
    /// <param name="control">The cache control header value.
    _ASYNCRTIMP void set_cache_control(utility::string_t control);

    /// Gets the date header of the message.
    /// Returns the date header value.
    _ASYNCRTIMP utility::string_t date() const;

    /// Sets the date header of the message.
    /// <param name="date">The date header value.
    _ASYNCRTIMP void set_date(const utility::datetime& date);

private:

    template<typename _t>
    bool bind_impl(const key_type &text, _t &ref) const
    {
        utility::istringstream_t iss(text);
        iss.imbue(std::locale::classic());
        iss >> ref;
        if (iss.fail() || !iss.eof())
        {
            return false;
        }

        return true;
    }

    bool bind_impl(const key_type &text, ::utility::string_t &ref) const
    {
        ref = text;
        return true;
    }

    // Headers are stored in a map with case insensitive key.
    std::map<utility::string_t, utility::string_t, _case_insensitive_cmp> m_headers;
};

}}