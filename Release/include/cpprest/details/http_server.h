// HTTP Library: interface to implement HTTP server to service http_listeners.
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#if _WIN32_WINNT < _WIN32_WINNT_VISTA
#error "Error: http server APIs are not supported in XP"
#endif //_WIN32_WINNT < _WIN32_WINNT_VISTA

#include "cpprest/http_listener.h"

namespace web { namespace http
{
namespace experimental {
namespace details
{

/// Interface http listeners interact with for receiving and responding to http requests.
class http_server {
public:
    virtual						~http_server		()																					{}		// Release any held resources.

	virtual pplx::task<void>	start				()																					= 0;	// Start listening for incoming requests.
    virtual pplx::task<void>	register_listener	(_In_ web::http::experimental::listener::details::http_listener_impl *pListener)	= 0;	// Registers an http listener.
    virtual pplx::task<void>	unregister_listener	(_In_ web::http::experimental::listener::details::http_listener_impl *pListener)	= 0;	// Unregisters an http listener.
    virtual pplx::task<void>	stop				()																					= 0;	// Stop processing and listening for incoming requests.
    virtual pplx::task<void>	respond				(http::http_response response)														= 0;	// Asynchronously sends the specified http response. Returns a operation which is completed once the response has been sent.
};

} // namespace details
} // namespace experimental
}} // namespace web::http
