// utility classes used by the different web:: clients
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#include "cpprest/asyncrt_utils.h"

namespace web
{

namespace http { namespace client { namespace details {
class winhttp_client;
class winrt_client;
class asio_context;
}}}
namespace websockets { namespace client { namespace details {
class winrt_callback_client;
class wspp_callback_client;
}}}

namespace details
{

class zero_memory_deleter
{
public:
    _ASYNCRTIMP void operator()(::utility::string_t *data) const;
};
typedef std::unique_ptr<::utility::string_t, zero_memory_deleter> plaintext_string;

#if defined(_WIN32) && !defined(CPPREST_TARGET_XP)
#if defined(__cplusplus_winrt)
class winrt_encryption
{
public:
    winrt_encryption() {}
    _ASYNCRTIMP winrt_encryption(const std::wstring &data);
    _ASYNCRTIMP plaintext_string decrypt() const;
private:
    ::pplx::task<Windows::Storage::Streams::IBuffer ^> m_buffer;
};
#else
class win32_encryption
{
public:
    win32_encryption() {}
    _ASYNCRTIMP win32_encryption(const std::wstring &data);
    _ASYNCRTIMP ~win32_encryption();
    _ASYNCRTIMP plaintext_string decrypt() const;
private:
    std::vector<char> m_buffer;
    size_t m_numCharacters;
};
#endif
#endif
}

/// Represents a set of user credentials (user name and password) to be used
/// for authentication.
class credentials
{
    ::utility::string_t			m_username;

#if defined(_WIN32) && !defined(CPPREST_TARGET_XP)
#if defined(__cplusplus_winrt)
    details::winrt_encryption	m_password;
#else
    details::win32_encryption	m_password;
#endif
#else
    ::utility::string_t			m_password;
#endif
public:
    
							credentials() {}	// Constructs an empty set of credentials without a user name or password.
							
							credentials(utility::string_t username, const utility::string_t & password) :	// Constructs credentials from given user name and password.
        m_username(std::move(username)),
        m_password(password)
    {}

    /// The user name associated with the credentials.
    /// Returns a string containing the user name.
    const utility::string_t &username() const { return m_username; }

    /// The password for the user name associated with the credentials.
    /// Returns a string containing the password.
    CASABLANCA_DEPRECATED("This API is deprecated for security reasons to avoid unnecessary password copies stored in plaintext.")
        utility::string_t password() const
    {
#if defined(_WIN32) && !defined(CPPREST_TARGET_XP)
        return utility::string_t(*m_password.decrypt());
#else
        return m_password;
#endif
    }

    /// Checks if credentials have been set
    /// Returns true if user name and password is set, false otherwise.
    bool is_set() const { return !m_username.empty(); }

    details::plaintext_string _internal_decrypt() const {
        // Encryption APIs not supported on XP
#if defined(_WIN32) && !defined(CPPREST_TARGET_XP)
        return m_password.decrypt();
#else
        return details::plaintext_string(new ::utility::string_t(m_password));
#endif
    }
};

// web_proxy represents the concept of the web proxy, which can be auto-discovered, disabled, or specified explicitly by the user.
class web_proxy
{
    enum web_proxy_mode_internal{ use_default_, use_auto_discovery_, disabled_, user_provided_ };
    web::uri					m_address;
    web_proxy_mode_internal		m_mode;
    web::credentials			m_credentials;
public:
    enum	web_proxy_mode { use_default = use_default_, use_auto_discovery = use_auto_discovery_, disabled  = disabled_};

								web_proxy			()									: m_address(_XPLATSTR(""))	, m_mode(use_default_)									{}
								web_proxy			( web_proxy_mode mode )				: m_address(_XPLATSTR(""))	, m_mode(static_cast<web_proxy_mode_internal>(mode))	{}
								web_proxy			( uri address )						: m_address(address)		, m_mode(user_provided_)								{}
    const uri&					address				()							const	{ return m_address		; }		// Gets this proxy's URI address. Returns an empty URI if not explicitly set by user. Returns a reference to this proxy's URI.
    const web::credentials&		credentials			()							const	{ return m_credentials	; }		// Gets the credentials used for authentication with this proxy. Returns Credentials to for this proxy.

    // Sets the credentials to use for authentication with this proxy.
    void						set_credentials		(web::credentials cred)				{
        if( m_mode == disabled_ ) {
            throw std::invalid_argument("Cannot attach credentials to a disabled proxy");
        }
        m_credentials = std::move(cred);
    }

    bool						is_default			()							const	{ return m_mode == use_default_			; }	// Checks if this proxy was constructed with default settings. Returns true if default, false otherwise.
    bool						is_disabled			()							const	{ return m_mode == disabled_			; }	// Checks if using a proxy is disabled. Returns true if disabled, false otherwise.
    bool						is_auto_discovery	()							const	{ return m_mode == use_auto_discovery_	; }	// Checks if the auto discovery protocol, WPAD, is to be used. Returns true if auto discovery enabled, false otherwise.
    bool						is_specified		()							const	{ return m_mode == user_provided_		; }	// Checks if a proxy address is explicitly specified by the user. Returns true if a proxy address was explicitly specified, false otherwise.
};

}
