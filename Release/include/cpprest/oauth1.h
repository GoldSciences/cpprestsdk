/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* HTTP Library: Oauth 1.0
*
* For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#ifndef _CASA_OAUTH1_H
#define _CASA_OAUTH1_H

#include "cpprest/http_msg.h"
#include "cpprest/details/web_utilities.h"

namespace web
{
namespace http
{
namespace client
{
    // Forward declaration to avoid circular include dependency.
    class http_client_config;
}

/// oAuth 1.0 library.
namespace oauth1
{
namespace details
{

class oauth1_handler;

// State currently used by oauth1_config to authenticate request.
// The state varies for every request (due to timestamp and nonce).
// The state also contains extra transmitted protocol parameters during
// authorization flow (i.e. 'oauth_callback' or 'oauth_verifier').
class oauth1_state
{
public:
    oauth1_state(utility::string_t timestamp, utility::string_t nonce,
            utility::string_t extra_key=utility::string_t(),
            utility::string_t extra_value=utility::string_t()) :
        m_timestamp(std::move(timestamp)),
        m_nonce(std::move(nonce)),
        m_extra_key(std::move(extra_key)),
        m_extra_value(std::move(extra_value))
    {}

    const utility::string_t& timestamp() const { return m_timestamp; }
    void set_timestamp(utility::string_t timestamp) { m_timestamp = std::move(timestamp); }

    const utility::string_t& nonce() const { return m_nonce; }
    void set_nonce(utility::string_t nonce) { m_nonce = std::move(nonce); }

    const utility::string_t& extra_key() const { return m_extra_key; }
    void set_extra_key(utility::string_t key) { m_extra_key = std::move(key); }

    const utility::string_t& extra_value() const { return m_extra_value; }
    void set_extra_value(utility::string_t value) { m_extra_value = std::move(value); }

private:
    utility::string_t m_timestamp;
    utility::string_t m_nonce;
    utility::string_t m_extra_key;
    utility::string_t m_extra_value;
};

// Constant strings for OAuth 1.0.
typedef utility::string_t oauth1_string;
class oauth1_strings
{
public:
#define _OAUTH1_STRINGS
#define DAT(a_, b_) _ASYNCRTIMP static const oauth1_string a_;
#include "cpprest/details/http_constants.dat"
#undef _OAUTH1_STRINGS
#undef DAT
};

} // namespace web::http::oauth1::details

/// oAuth functionality is currently in beta.
namespace experimental
{

/// Constant strings for OAuth 1.0 signature methods.
typedef utility::string_t oauth1_method;
class oauth1_methods
{
public:
#define _OAUTH1_METHODS
#define DAT(a,b) _ASYNCRTIMP static const oauth1_method a;
#include "cpprest/details/http_constants.dat"
#undef _OAUTH1_METHODS
#undef DAT
};

/// Exception type for OAuth 1.0 errors.
class oauth1_exception : public std::exception
{
public:
    oauth1_exception(utility::string_t msg) : m_msg(utility::conversions::to_utf8string(std::move(msg))) {}
    ~oauth1_exception() CPPREST_NOEXCEPT {}
    const char* what() const CPPREST_NOEXCEPT { return m_msg.c_str(); }

private:
    std::string m_msg;
};

/// OAuth 1.0 token and associated information.
class oauth1_token
{
public:

    /// Constructs an initially empty invalid access token.
    oauth1_token() {}

    /// Constructs a OAuth1 token from a given access token and secret.
    /// <param name="access_token">Access token string.
    /// <param name="secret">Token secret string.
    oauth1_token(utility::string_t access_token, utility::string_t secret) :
        m_token(std::move(access_token)),
        m_secret(std::move(secret))
    {}

    /// Get access token validity state.
    /// If true, token is a valid access token.
    /// Returns access token validity state of the token.
    bool is_valid_access_token() const { return !(access_token().empty() || secret().empty()); }

    /// Get access token.
    /// Returns the access token string.
    const utility::string_t& access_token() const { return m_token; }

    /// Set access token.
    /// <param name="access_token">Access token string to set.
    void set_access_token(utility::string_t &&access_token) { m_token = std::move(access_token); }

    /// Set access token.
    /// <param name="access_token">Access token string to set.
    void set_access_token(const utility::string_t &access_token) { m_token = access_token; }

    /// Get token secret.
    /// Returns token secret string.
    const utility::string_t& secret() const { return m_secret; }

    /// Set token secret.
    /// <param name="secret">Token secret string to set.
    void set_secret(utility::string_t &&secret) { m_secret = std::move(secret); }

    /// Set token secret.
    /// <param name="secret">Token secret string to set.
    void set_secret(const utility::string_t &secret) { m_secret = secret; }

    /// Retrieves any additional parameters.
    /// Returns a map containing the additional parameters.
    const std::map<utility::string_t, utility::string_t> &additional_parameters() const { return m_additional_parameters; }

    /// Sets a specific parameter additional parameter.
    /// <param name="paramName">Parameter name.
    /// <param name="paramValue">Parameter value.
    void set_additional_parameter(utility::string_t &&paramName, utility::string_t &&paramValue)
    {
        m_additional_parameters[std::move(paramName)] = std::move(paramValue);
    }

    /// Sets a specific parameter additional parameter.
    /// <param name="paramName">Parameter name.
    /// <param name="paramValue">Parameter value.
    void set_additional_parameter(const utility::string_t &paramName, const utility::string_t &paramValue)
    {
        m_additional_parameters[paramName] = paramValue;
    }

    /// Clears all additional parameters.
    void clear_additional_parameters() { m_additional_parameters.clear(); }

private:
    friend class oauth1_config;

    utility::string_t m_token;
    utility::string_t m_secret;
    std::map<utility::string_t, utility::string_t> m_additional_parameters;
};

/// OAuth 1.0 configuration class.
class oauth1_config
{
public:
    oauth1_config(utility::string_t consumer_key, utility::string_t consumer_secret,
            utility::string_t temp_endpoint, utility::string_t auth_endpoint,
            utility::string_t token_endpoint, utility::string_t callback_uri,
            oauth1_method method, utility::string_t realm=utility::string_t()) :
        m_consumer_key(std::move(consumer_key)),
        m_consumer_secret(std::move(consumer_secret)),
        m_temp_endpoint(std::move(temp_endpoint)),
        m_auth_endpoint(std::move(auth_endpoint)),
        m_token_endpoint(std::move(token_endpoint)),
        m_callback_uri(std::move(callback_uri)),
        m_realm(std::move(realm)),
        m_method(std::move(method)),
        m_is_authorization_completed(false)
    {}

    /// Builds an authorization URI to be loaded in a web browser/view.
    /// The URI is built with auth_endpoint() as basis.
    /// The method creates a task for HTTP request to first obtain a
    /// temporary token. The authorization URI build based on this token.
    /// Returns authorization URI to be loaded in a web browser/view.
    _ASYNCRTIMP pplx::task<utility::string_t> build_authorization_uri();

    /// Fetch an access token based on redirected URI.
    /// The URI is expected to contain 'oauth_verifier'
    /// parameter, which is then used to fetch an access token using the
    /// token_from_verifier() method.
    /// See: http://tools.ietf.org/html/rfc5849#section-2.2
    /// The received 'oauth_token' is parsed and verified to match the current token().
    /// When access token is successfully obtained, set_token() is called, and config is
    /// ready for use by oauth1_handler.
    /// <param name="redirected_uri">The URI where web browser/view was redirected after resource owner's authorization.
    /// Returns task that fetches the access token based on redirected URI.
    _ASYNCRTIMP pplx::task<void> token_from_redirected_uri(const web::http::uri& redirected_uri);

    /// Creates a task with HTTP request to fetch an access token from the token endpoint.
    /// The request exchanges a verifier code to an access token.
    /// If successful, the resulting token is set as active via set_token().
    /// See: http://tools.ietf.org/html/rfc5849#section-2.3
    /// <param name="verifier">Verifier received via redirect upon successful authorization.
    /// Returns task that fetches the access token based on the verifier.
    pplx::task<void> token_from_verifier(utility::string_t verifier)
    {
        return _request_token(_generate_auth_state(details::oauth1_strings::verifier, std::move(verifier)), false);
    }

    /// Creates a task with HTTP request to fetch an access token from the token endpoint.
    /// If successful, the resulting token is set as active via set_token().
    /// Returns task that fetches the access token based on the verifier.
    pplx::task<void> refresh_token(const utility::string_t &key)
    {
        return _request_token(_generate_auth_state(key, m_token.additional_parameters().at(key)), false);
    }

    /// Get consumer key used in authorization and authentication.
    /// <returns>Consumer key string.
    const utility::string_t& consumer_key() const { return m_consumer_key; }
    /// Set consumer key used in authorization and authentication.
    /// <param name="key">Consumer key string to set.
    void set_consumer_key(utility::string_t key) { m_consumer_key = std::move(key); }

    /// Get consumer secret used in authorization and authentication.
    /// <returns>Consumer secret string.
    const utility::string_t& consumer_secret() const { return m_consumer_secret; }
    /// Set consumer secret used in authorization and authentication.
    /// <param name="secret">Consumer secret string to set.
    void set_consumer_secret(utility::string_t secret) { m_consumer_secret = std::move(secret); }

    /// Get temporary token endpoint URI string.
    /// Returns temporary token endpoint URI string.
    const utility::string_t& temp_endpoint() const { return m_temp_endpoint; }
    /// Set temporary token endpoint URI string.
    /// <param name="temp_endpoint">Temporary token endpoint URI string to set.
    void set_temp_endpoint(utility::string_t temp_endpoint) { m_temp_endpoint = std::move(temp_endpoint); }

    /// Get authorization endpoint URI string.
    /// Returns authorization endpoint URI string.
    const utility::string_t& auth_endpoint() const { return m_auth_endpoint; }
    /// Set authorization endpoint URI string.
    /// <param name="auth_endpoint">Authorization endpoint URI string to set.
    void set_auth_endpoint(utility::string_t auth_endpoint) { m_auth_endpoint = std::move(auth_endpoint); }

    /// Get token endpoint URI string.
    /// Returns token endpoint URI string.
    const utility::string_t& token_endpoint() const { return m_token_endpoint; }
    /// Set token endpoint URI string.
    /// <param name="token_endpoint">Token endpoint URI string to set.
    void set_token_endpoint(utility::string_t token_endpoint) { m_token_endpoint = std::move(token_endpoint); }

    /// Get callback URI string.
    /// <returns>Callback URI string.
    const utility::string_t& callback_uri() const { return m_callback_uri; }
    /// Set callback URI string.
    /// <param name="callback_uri">Callback URI string to set.
    void set_callback_uri(utility::string_t callback_uri) { m_callback_uri = std::move(callback_uri); }

    /// Get token.
    /// Returns token.
    _ASYNCRTIMP const oauth1_token& token() const;

    /// Set token.
    /// <param name="token">Token to set.
    void set_token(oauth1_token token)
    {
        m_token = std::move(token);
        m_is_authorization_completed = true;
    }

    /// Get signature method.
    /// Returns signature method.
    const oauth1_method& method() const { return m_method; }
    /// Set signature method.
    /// <param name="method">Signature method.
    void set_method(oauth1_method method) { m_method = std::move(method); }

    /// Get authentication realm.
    /// Returns authentication realm string.
    const utility::string_t& realm() const { return m_realm; }
    /// Set authentication realm.
    /// <param name="realm">Authentication realm string to set.
    void set_realm(utility::string_t realm) { m_realm = std::move(realm); }

    /// Returns enabled state of the configuration.
    /// The oauth1_handler will perform OAuth 1.0 authentication only if
    /// this method returns true.
    /// Return value is true if access token is valid (=fetched or manually set)
    /// and both consumer_key() and consumer_secret() are set (=non-empty).
    /// Returns the configuration enabled state.
    bool is_enabled() const { return token().is_valid_access_token() && !(consumer_key().empty() || consumer_secret().empty()); }

    // Builds signature base string according to:
    // http://tools.ietf.org/html/rfc5849#section-3.4.1.1
    _ASYNCRTIMP utility::string_t _build_signature_base_string(http_request request, details::oauth1_state state) const;

    // Builds HMAC-SHA1 signature according to:
    // http://tools.ietf.org/html/rfc5849#section-3.4.2
    utility::string_t _build_hmac_sha1_signature(http_request request, details::oauth1_state state) const
    {
        auto text(_build_signature_base_string(std::move(request), std::move(state)));
        auto digest(_hmac_sha1(_build_key(), std::move(text)));
        auto signature(utility::conversions::to_base64(std::move(digest)));
        return signature;
    }

    // Builds PLAINTEXT signature according to:
    // http://tools.ietf.org/html/rfc5849#section-3.4.4
    utility::string_t _build_plaintext_signature() const
    {
        return _build_key();
    }

    details::oauth1_state _generate_auth_state(utility::string_t extra_key, utility::string_t extra_value)
    {
        return details::oauth1_state(_generate_timestamp(), _generate_nonce(), std::move(extra_key), std::move(extra_value));
    }

    details::oauth1_state _generate_auth_state()
    {
        return details::oauth1_state(_generate_timestamp(), _generate_nonce());
    }

    /// Gets map of parameters to sign.
    /// <returns>Map of parameters.
    const std::map<utility::string_t, utility::string_t>& parameters() const { return m_parameters_to_sign; }

    /// Adds a key value parameter.
    /// <param name="key">Key as a string value.
    /// <param name="value">Value as a string value.
    void add_parameter(const utility::string_t &key, const utility::string_t &value) { m_parameters_to_sign[key] = value; }

    /// Adds a key value parameter.
    /// <param name="key">Key as a string value.
    /// <param name="value">Value as a string value.
    void add_parameter(utility::string_t &&key, utility::string_t &&value) { m_parameters_to_sign[std::move(key)] = std::move(value); }

    /// Sets entire map or parameters replacing all previously values.
    /// <param name="parameters">Map of values.
    void set_parameters(const std::map<utility::string_t, utility::string_t> &parameters)
    {
        m_parameters_to_sign.clear();
        m_parameters_to_sign = parameters;
    }

    /// Clears all parameters.
    void clear_parameters() { m_parameters_to_sign.clear(); }

    /// Get the web proxy object
    /// Returns a reference to the web proxy object.
    const web_proxy& proxy() const
    {
        return m_proxy;
    }

    /// Set the web proxy object that will be used by token_from_code and token_from_refresh
    /// <param name="proxy">A reference to the web proxy object.
    void set_proxy(const web_proxy& proxy)
    {
        m_proxy = proxy;
    }

private:
    friend class web::http::client::http_client_config;
    friend class web::http::oauth1::details::oauth1_handler;

    oauth1_config() :
        m_is_authorization_completed(false)
    {}

    utility::string_t _generate_nonce()
    {
        return m_nonce_generator.generate();
    }

    static utility::string_t _generate_timestamp()
    {
        return utility::conversions::details::to_string_t(utility::datetime::utc_timestamp());
    }

    _ASYNCRTIMP static std::vector<unsigned char> __cdecl _hmac_sha1(const utility::string_t& key, const utility::string_t& data);

    static utility::string_t _build_base_string_uri(const uri& u);

    utility::string_t _build_normalized_parameters(web::http::uri u, const details::oauth1_state& state) const;

    utility::string_t _build_signature(http_request request, details::oauth1_state state) const;

    utility::string_t _build_key() const
    {
        return uri::encode_data_string(consumer_secret()) + _XPLATSTR("&") + uri::encode_data_string(m_token.secret());
    }

    void _authenticate_request(http_request &req)
    {
        _authenticate_request(req, _generate_auth_state());
    }

    _ASYNCRTIMP void _authenticate_request(http_request &req, details::oauth1_state state);

    _ASYNCRTIMP pplx::task<void> _request_token(details::oauth1_state state, bool is_temp_token_request);

    utility::string_t m_consumer_key;
    utility::string_t m_consumer_secret;
    oauth1_token m_token;

    utility::string_t m_temp_endpoint;
    utility::string_t m_auth_endpoint;
    utility::string_t m_token_endpoint;
    utility::string_t m_callback_uri;
    utility::string_t m_realm;
    oauth1_method m_method;

	std::map<utility::string_t, utility::string_t> m_parameters_to_sign;

    web::web_proxy m_proxy;

    utility::nonce_generator m_nonce_generator;
    bool m_is_authorization_completed;
};

} // namespace web::http::oauth1::experimental

namespace details
{

class oauth1_handler : public http_pipeline_stage
{
public:
    oauth1_handler(std::shared_ptr<experimental::oauth1_config> cfg) :
        m_config(std::move(cfg))
    {}

    virtual pplx::task<http_response> propagate(http_request request) override
    {
        if (m_config)
        {
            m_config->_authenticate_request(request);
        }
        return next_stage()->propagate(request);
    }

private:
    std::shared_ptr<experimental::oauth1_config> m_config;
};

}}}}

#endif
