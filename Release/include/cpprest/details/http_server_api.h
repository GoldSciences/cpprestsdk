// HTTP Library: exposes the entry points to the http server transport apis.
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#if _WIN32_WINNT < _WIN32_WINNT_VISTA
#error "Error: http server APIs are not supported in XP"
#endif //_WIN32_WINNT < _WIN32_WINNT_VISTA

#include <memory>

#include "cpprest/http_listener.h"

namespace web { namespace http
{
namespace experimental {
namespace details
{
class http_server;
// Singleton class used to register for http requests and send responses.
//
// The lifetime is tied to http listener registration. When the first listener registers an instance is created
// and when the last one unregisters the receiver stops and is destroyed. It can be started back up again if listeners are again registered.
class http_server_api {
    static	pplx::extensibility::critical_section_t			s_lock;																											// Used to lock access to the server api registration
    static	void											unsafe_register_server_api	(std::unique_ptr<http_server> server_api);											// Registers a server API set -- this assumes the lock has already been taken
    static	std::unique_ptr<http_server>					s_server_api;																									// Static instance of the HTTP server API.
    static	pplx::details::atomic_long						s_registrations;																								// Number of registered listeners;

															http_server_api();	// Static only class. No creation.
public:
    static	bool									__cdecl	has_listener				();																					// Returns whether or not any listeners are registered.
    static	void									__cdecl	register_server_api			(std::unique_ptr<http_server> server_api);											// Registers a HTTP server API.
    static	void									__cdecl	unregister_server_api		();																					// Clears the http server API.
    static	pplx::task<void>						__cdecl	register_listener			(_In_ web::http::experimental::listener::details::http_listener_impl *pListener);	// Registers a listener for HTTP requests and starts receiving.
    static	pplx::task<void>						__cdecl	unregister_listener			(_In_ web::http::experimental::listener::details::http_listener_impl *pListener);	// Unregisters the given listener and stops listening for HTTP requests.
    static	http_server *							__cdecl	server_api					();																					// Gets static HTTP server API. Could be null if no registered listeners.
};

}}}} // namespaces
