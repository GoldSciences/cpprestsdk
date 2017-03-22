// Websocket client side implementation
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#ifndef _CASA_WS_CLIENT_H
#define _CASA_WS_CLIENT_H

#include "cpprest/details/basic_types.h"

#if !defined(CPPREST_EXCLUDE_WEBSOCKETS)

#include <memory>
#include <limits>
#include <condition_variable>
#include <mutex>

#include "pplx/pplxtasks.h"
#include "cpprest/uri.h"
#include "cpprest/details/web_utilities.h"
#include "cpprest/http_headers.h"
#include "cpprest/asyncrt_utils.h"
#include "cpprest/ws_msg.h"

namespace web
{

// For backwards compatibility for when in the experimental namespace.
// At next major release this should be deleted.
namespace experimental = web;

// In the past namespace was accidentally called 'web_sockets'. To avoid breaking code
// alias it. At our next major release this should be deleted.
namespace web_sockets = websockets;

namespace websockets
{
/// WebSocket client side library.
namespace client
{

/// Websocket close status values.
enum class websocket_close_status
{
    normal = 1000,
    going_away = 1001,
    protocol_error = 1002,
    unsupported = 1003, //or data_mismatch
    abnormal_close = 1006,
    inconsistent_datatype = 1007,
    policy_violation = 1008,
    too_large = 1009,
    negotiate_error = 1010,
    server_terminate = 1011,
};

/// Websocket client configuration class, used to set the possible configuration options
/// used to create an websocket_client instance.
class websocket_client_config
{
    web::http::http_headers							m_headers;
public:
    utf8string										m_sni_hostname				;
    web::web_proxy									m_proxy						;
    web::credentials								m_credentials				;
    bool											m_sni_enabled				= true;	// Determines if Server Name Indication (SNI) is enabled. 
    bool											m_validate_certificates		= true;

													websocket_client_config		()														= default;

    //const web_proxy&								proxy						()												const	{ return m_proxy;							}	// Get the web proxy object Returns a reference to the web proxy object.
    //const web::credentials&							credentials					()												const	{ return m_credentials;						}	// Get the client credentials. Returns a reference to the client credentials.
    //bool											is_sni_enabled				()												const	{ return m_sni_enabled;						}	// Returns true if enabled, false otherwise.
    //bool											validate_certificates		()												const	{ return m_validate_certificates;			}	// Gets the server certificate validation property. Returns true if certificates are to be verified, false otherwise.
    void											set_proxy					(const web_proxy &proxy)								{ m_proxy = proxy;							}	// Set the web proxy object <param name="proxy">The web proxy object.
    void											set_credentials				(const web::credentials &cred)							{ m_credentials = cred;						}	// Set the client credentials
    void											disable_sni					()														{ m_sni_enabled = false;					}	// Disables Server Name Indication (SNI). Default is on.
    void											set_server_name				(const utf8string &name)								{ m_sni_hostname = name;					}	// Sets the server host name to use for TLS Server Name Indication (SNI). By default the host name is set to the websocket URI host.
    const utf8string &								server_name					()												const	{ return m_sni_hostname;					}	// Gets the server host name to usefor TLS Server Name Indication (SNI). Rreturns host name as a string.
	web::http::http_headers &						headers						()														{ return m_headers;							}	// Gets the headers of the HTTP request message used in the WebSocket protocol handshake. Returns HTTP headers for the WebSocket protocol handshake. Use http_headers::add() to fill in desired headers.
    const web::http::http_headers &					headers						()												const	{ return m_headers;							}	// Gets a const reference to the headers of the WebSocket protocol handshake HTTP message.
    _ASYNCRTIMP void								add_subprotocol				(const ::utility::string_t &subprotocolName);			// Adds a subprotocol to the request headers. If additional subprotocols have already been specified, the new one will just be added.
    _ASYNCRTIMP std::vector<::utility::string_t>	subprotocols				()												const;	// Gets list of the specified subprotocols. Returns Vector of all the subprotocols. If you want all the subprotocols in a comma separated string they can be directly looked up in the headers using 'Sec-WebSocket-Protocol'.
    void											set_validate_certificates	(bool validate_certs)									{ m_validate_certificates = validate_certs;	}	// Sets the server certificate validation property. Note ignoring certificate errors can be dangerous and should be done with caution.
};

// Represents a websocket error. This class holds an error message and an optional error code.
class websocket_exception : public std::exception {
    std::error_code m_errorCode;
    std::string m_msg;
public:
																		websocket_exception						(const utility::string_t &whatArg)							: m_msg(utility::conversions::to_utf8string(whatArg))																{}
																		websocket_exception						(int errorCode)												: m_errorCode(utility::details::create_error_code(errorCode))														{ m_msg = m_errorCode.message(); }
																		websocket_exception						(int errorCode, const utility::string_t &whatArg)			: m_errorCode(utility::details::create_error_code(errorCode)), m_msg(utility::conversions::to_utf8string(whatArg))	{}	
																		websocket_exception						(int errorCode, std::string whatArg)						: m_errorCode(utility::details::create_error_code(errorCode)), m_msg(std::move(whatArg))							{}
																		websocket_exception						(int errorCode, const std::error_category &cat)				: m_errorCode(std::error_code(errorCode, cat))																		{ m_msg = m_errorCode.message(); }
																		websocket_exception						(std::error_code code, const utility::string_t &whatArg)	: m_errorCode(std::move(code)), m_msg(utility::conversions::to_utf8string(whatArg))									{}
#ifdef _WIN32
																		websocket_exception						(std::string whatArg)										: m_msg(std::move(whatArg))																							{}	
																		websocket_exception						(std::error_code code, std::string whatArg)					: m_errorCode(std::move(code)), m_msg(std::move(whatArg))															{}
#endif
    const char*															what									()																												const	noexcept	{ return m_msg.c_str(); }	// Gets a string identifying the cause of the exception. Returns a null terminated character string.
    const std::error_code &												error_code								()																												const	noexcept	{ return m_errorCode; }	// Gets the underlying error code for the cause of the exception. Returns the <c>error_code</c> object associated with the exception.
};

namespace details
{

// Interface to be implemented by the websocket client callback implementations.
class websocket_client_callback_impl
{
public:
    virtual																~websocket_client_callback_impl			()																														noexcept	{}
																		websocket_client_callback_impl			(websocket_client_config config)																									: m_config(std::move(config))			{}
    virtual pplx::task<void>											connect									()																																	= 0;
    virtual pplx::task<void>											send									(websocket_outgoing_message &msg)																									= 0;
    virtual void														set_message_handler						(const std::function<void(const websocket_incoming_message&)>& handler)																= 0;
    virtual pplx::task<void>											close									()																																	= 0;
    virtual pplx::task<void>											close									(websocket_close_status close_status, const utility::string_t &close_reason = _XPLATSTR(""))										= 0;
    virtual void														set_close_handler						(const std::function<void(websocket_close_status, const utility::string_t&, const std::error_code&)>& handler)						= 0;

	const web::uri&														uri										()																															const	{ return m_uri; }
    void																set_uri									(const web::uri &uri)																												{ m_uri = uri; }
    const websocket_client_config&										config									()																															const	{ return m_config; }
    static void															verify_uri								(const web::uri& uri)																												{
        if (uri.scheme() != _XPLATSTR("ws") && uri.scheme() != _XPLATSTR("wss")) {	// Most of the URI schema validation is taken care by URI class. We only need to check certain things specific to websockets.
            throw std::invalid_argument("URI scheme must be 'ws' or 'wss'");
        }
        if (uri.host().empty()) {
            throw std::invalid_argument("URI must contain a hostname.");
        }
        if (!uri.fragment().empty()) {	// Fragment identifiers are meaningless in the context of WebSocket URIs and MUST NOT be used on these URIs.
            throw std::invalid_argument("WebSocket URI must not contain fragment identifiers");
        }
    }

protected:
    web::uri m_uri;
    websocket_client_config m_config;
};

// Interface to be implemented by the websocket client task implementations.
class websocket_client_task_impl {
public:
    _ASYNCRTIMP virtual													~websocket_client_task_impl				() CPPREST_NOEXCEPT;
    _ASYNCRTIMP															websocket_client_task_impl				(websocket_client_config config);
    _ASYNCRTIMP pplx::task<websocket_incoming_message>					receive									();
    _ASYNCRTIMP void													close_pending_tasks_with_error			(const websocket_exception &exc);

    const std::shared_ptr<websocket_client_callback_impl> &				callback_client							()																															const	{ return m_callback_client; };

private:
    void																set_handler								();
    // When a message arrives, if there are tasks waiting for a message, signal the topmost one. Else enqueue the message in a queue.
    std::mutex															m_receive_queue_lock					;	// lock to guard access to the queue & m_client_closed
    std::queue<websocket_incoming_message>								m_receive_msg_queue						;	// Queue to store incoming messages when there are no tasks waiting for a message
    std::queue<pplx::task_completion_event<websocket_incoming_message>>	m_receive_task_queue					;	// Queue to maintain the receive tasks when there are no messages(yet).
    bool																m_client_closed							;	// Initially set to false, becomes true if a close frame is received from the server or if the underlying connection is aborted or terminated.
    std::shared_ptr<websocket_client_callback_impl>						m_callback_client;
};
}

/// Websocket client class, used to maintain a connection to a remote host for an extended session.
class websocket_client {
    std::shared_ptr<details::websocket_client_task_impl>				m_client;
public:
																		websocket_client						()																																	: m_client(std::make_shared<details::websocket_client_task_impl>(websocket_client_config())) {}
																		websocket_client						(websocket_client_config config)																									: m_client(std::make_shared<details::websocket_client_task_impl>(std::move(config))) {}
	// Connects to the remote network destination. The connect method initiates the websocket handshake with the remote network destination, takes care of the protocol upgrade request.
	// Returns an asynchronous operation that is completed once the client has successfully connected to the websocket server.
    pplx::task<void>													connect									(const web::uri &uri)																												{
        m_client->callback_client()->verify_uri(uri);
        m_client->callback_client()->set_uri(uri);
        auto client = m_client;
        return m_client->callback_client()->connect().then([client](pplx::task<void> result)
        {
            try {
                result.get();
            }
            catch (const websocket_exception& ex) {
                client->close_pending_tasks_with_error(ex);
                throw;
            }
        });
    }
	pplx::task<void>													send									(websocket_outgoing_message msg)																									{ return m_client->callback_client()->send(msg);	}	// Sends a websocket message to the server. Returns an asynchronous operation that is completed once the message is sent.
	pplx::task<websocket_incoming_message>								receive									()																																	{ return m_client->receive();						}	// Receive a websocket message. Returns an asynchronous operation that is completed when a message has been received by the client endpoint.
	pplx::task<void>													close									()																																	{ return m_client->callback_client()->close();		}	// Closes a websocket client connection, sends a close frame to the server and waits for a close message from the server. Returns an asynchronous operation that is completed the connection has been successfully closed.
	// Closes a websocket client connection, sends a close frame to the server and waits for a close message from the server. Returns an asynchronous operation that is completed the connection has been successfully closed.
	// close_status: Endpoint MAY use the following pre-defined status codes when sending a Close frame.
	// close_reason: While closing an established connection, an endpoint may indicate the reason for closure.
	pplx::task<void>													close									(websocket_close_status close_status, const utility::string_t& close_reason=_XPLATSTR(""))											{ return m_client->callback_client()->close(close_status, close_reason); }
	const web::uri&														uri										()																															const	{ return m_client->callback_client()->uri	();	}	
	const websocket_client_config&										config									()																															const	{ return m_client->callback_client()->config();	}
private:
};

/// Websocket client class, used to maintain a connection to a remote host for an extended session, uses callback APIs for handling receive and close event instead of async task.
/// For some scenarios would be a alternative for the websocket_client like if you want to special handling on close event.
class websocket_callback_client {
    std::shared_ptr<details::websocket_client_callback_impl>			m_client;

public:
    _ASYNCRTIMP															websocket_callback_client				();
    _ASYNCRTIMP															websocket_callback_client				(websocket_client_config client_config);

    // Connects to the remote network destination. The connect method initiates the websocket handshake with the remote network destination, takes care of the protocol upgrade request.
    // Returns an asynchronous operation that is completed once the client has successfully connected to the websocket server.
    pplx::task<void>													connect									(const web::uri &uri)																												{
        m_client->verify_uri(uri);
        m_client->set_uri(uri);
        return m_client->connect();
    }
    // Sends a websocket message to the server. Returns an asynchronous operation that is completed once the message is sent.
    pplx::task<void>													send									(websocket_outgoing_message msg)																									{ return m_client->send(msg);	}
    // Set the received handler for notification of client websocket messages.
    // <param name="handler">A function representing the incoming websocket messages handler. It's parameters are:
    //    msg:  a <c>websocket_incoming_message</c> value indicating the message received
    // 
    // If this handler is not set before connecting incoming messages will be missed.
    void																set_message_handler						(const std::function<void(const websocket_incoming_message& msg)>& handler)															{ m_client->set_message_handler(handler);	}

	// Closes a websocket client connection, sends a close frame to the server and waits for a close message from the server.
	// Returns an asynchronous operation that is completed the connection has been successfully closed.
    pplx::task<void> close() { return m_client->close(); }

    // Closes a websocket client connection, sends a close frame to the server and waits for a close message from the server.
    // <param name="close_status">Endpoint MAY use the following pre-defined status codes when sending a Close frame.
    // <param name="close_reason">While closing an established connection, an endpoint may indicate the reason for closure.
    // Returns an asynchronous operation that is completed the connection has been successfully closed.
    pplx::task<void>													close									(websocket_close_status close_status, const utility::string_t& close_reason = _XPLATSTR(""))										{ return m_client->close(close_status, close_reason);	}

    // Set the closed handler for notification of client websocket closing event.
    // <param name="handler">The handler for websocket closing event, It's parameters are:
    //   close_status: The pre-defined status codes used by the endpoint when sending a Close frame.
    //   reason: The reason string used by the endpoint when sending a Close frame.
    //   error: The error code if the websocket is closed with abnormal error.
    void																set_close_handler						(const std::function<
		void(websocket_close_status close_status, const utility::string_t& reason, const std::error_code& error)
	>& handler)																																																										{ m_client->set_close_handler(handler);		}

    
    const web::uri&														uri										()																															const	{ return m_client->uri		();				}	// Gets the websocket client URI. Returns URI connected to.
    const websocket_client_config&										config									()																															const	{ return m_client->config	();				}	// Gets the websocket client config object. Returns a reference to the client configuration object.
};

}}}

#endif

#endif