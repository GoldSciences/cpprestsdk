// HTTP Library: Request and reply message definitions. For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>
#include <system_error>

#include "pplx/pplxtasks.h"
#include "cpprest/json.h"
#include "cpprest/uri.h"
#include "cpprest/http_headers.h"
#include "cpprest/details/cpprest_compat.h"
#include "cpprest/asyncrt_utils.h"
#include "cpprest/streams.h"
#include "cpprest/containerstream.h"

namespace web
{
namespace http
{

// URI class has been moved from web::http namespace to web namespace.
// The below using declarations ensure we don't break existing code.
// Please use the web::uri class going forward.
using web::uri;
using web::uri_builder;

namespace client {
    class http_client;
}

// Predefined method strings for the standard HTTP methods mentioned in the HTTP 1.1 specification.
typedef utility::string_t method;

// Common HTTP methods.
class methods
{
public:
#define _METHODS
#define DAT(a,b)	_ASYNCRTIMP	const static	method				a;
#include "cpprest/details/http_constants.dat"
#undef _METHODS
#undef DAT
};

typedef	unsigned short															status_code;

// Predefined values for all of the standard HTTP 1.1 response status codes.
class status_codes {
public:
#define _PHRASES
#define DAT(a,b,c)				const static	status_code			a = b;
#include "cpprest/details/http_constants.dat"
#undef _PHRASES
#undef DAT
};

namespace details {

// Constants for MIME types.
class mime_types {
public:
#define _MIME_TYPES
#define DAT(a,b)	_ASYNCRTIMP	const static	utility::string_t	a;
#include "cpprest/details/http_constants.dat"
#undef _MIME_TYPES
#undef DAT
};

// Constants for charset types.
class charset_types {
public:
#define _CHARSET_TYPES
#define DAT(a,b)	_ASYNCRTIMP const static	utility::string_t	a;
#include "cpprest/details/http_constants.dat"
#undef _CHARSET_TYPES
#undef DAT
};

}

// Message direction
namespace message_direction
{
    // Enumeration used to denote the direction of a message: a request with a body is an upload, a response with a body is a download.
    enum direction {
        upload,
        download
    };
}

typedef	utility::string_t reason_phrase;
typedef	std::function<void(message_direction::direction, utility::size64_t)>	progress_handler;

struct http_status_to_phrase {
    unsigned short																id;
    reason_phrase																phrase;
};

// Constants for the HTTP headers mentioned in RFC 2616.
class header_names
{
public:
#define _HEADER_NAMES
#define DAT(a,b) _ASYNCRTIMP const static utility::string_t a;
#include "cpprest/details/http_constants.dat"
#undef _HEADER_NAMES
#undef DAT
};

// Represents an HTTP error. This class holds an error message and an optional error code.
class http_exception : public std::exception
{
    std::error_code																m_errorCode;
    std::string																	m_msg;
public:

    // Creates an <c>http_exception</c> with just a string message and no error code.
    http_exception(const utility::string_t &whatArg)
        : m_msg(utility::conversions::to_utf8string(whatArg)) {}

#ifdef _WIN32
    // Creates an <c>http_exception</c> with just a string message and no error code.
    http_exception(std::string whatArg) : m_msg(std::move(whatArg)) {}
#endif

    // Creates an <c>http_exception</c> with from a error code using the current platform error category.
    // The message of the error code will be used as the what() string message.
    http_exception(int errorCode) : m_errorCode(utility::details::create_error_code(errorCode)) { m_msg = m_errorCode.message(); }

    // Creates an http_exception with from a error code using the current platform error category.
    http_exception(int errorCode, const utility::string_t &whatArg)
        : m_errorCode(utility::details::create_error_code(errorCode)),
          m_msg(utility::conversions::to_utf8string(whatArg))
    {}

#ifdef _WIN32
    // Creates an http_exception with from a error code using the current platform error category.
    http_exception(int errorCode, std::string whatArg) :
        m_errorCode(utility::details::create_error_code(errorCode)),
        m_msg(std::move(whatArg))
    {}
#endif

    // Creates an http_exception with from a error code and category. The message of the error code will be used as the what string message.
										http_exception(int errorCode, const std::error_category &cat) : m_errorCode(std::error_code(errorCode, cat)) { m_msg = m_errorCode.message(); }
    /// Gets a string identifying the cause of the exception. Returns a null terminated character string.
    const char*							what() const CPPREST_NOEXCEPT { return m_msg.c_str(); }

    // Retrieves the underlying error code causing this exception.
    const std::error_code &				error_code() const { return m_errorCode; }
};

namespace details
{
// Base class for HTTP messages.
// This class is to store common functionality so it isn't duplicated on both the request and response side.
class http_msg_base {
protected:
    // Stream to read the message body.
    // By default this is an invalid stream. The user could set the instream on
    // a request by calling set_request_stream(...). This would also be set when
    // set_body() is called - a stream from the body is constructed and set.
    // Even in the presense of msg body this stream could be invalid. An example
    // would be when the user sets an ostream for the response. With that API the
    // user does not provide the ability to read the msg body.
    // Thus m_instream is valid when there is a msg body and it can actually be read
    concurrency::streams::istream							m_inStream;

    // stream to write the msg body
    // By default this is an invalid stream. The user could set this on the response
    // (for http_client). In all the other cases we would construct one to transfer
    // the data from the network into the message body.
    concurrency::streams::ostream							m_outStream;

    http_headers											m_headers;
    bool													m_default_outstream;
    pplx::task_completion_event<utility::size64_t>			m_data_available;	// The TCE is used to signal the availability of the message body.
public:

    friend class http::client::http_client;

    virtual													~http_msg_base		() {}
    _ASYNCRTIMP												http_msg_base		();


    http_headers &											headers				() { return m_headers; }

    _ASYNCRTIMP void										set_body			(const concurrency::streams::istream &instream, const utf8string &contentType);
    _ASYNCRTIMP void										set_body			(const concurrency::streams::istream &instream, const utf16string &contentType);
    _ASYNCRTIMP void										set_body			(const concurrency::streams::istream &instream, utility::size64_t contentLength, const utf8string &contentType);
    _ASYNCRTIMP void										set_body			(const concurrency::streams::istream &instream, utility::size64_t contentLength, const utf16string &contentType);

    // Helper function for extract functions. Parses the Content-Type header and check to make sure it matches, throws an exception if not.
    // ignore_content_type	: If true ignores the Content-Type header value.
    // check_content_type	: Function to verify additional information on Content-Type.
    // Returns a string containing the charset, an empty string if no Content-Type header is empty.
    utility::string_t										parse_and_check_content_type(bool ignore_content_type, const std::function<bool(const utility::string_t&)> &check_content_type);

    _ASYNCRTIMP utf8string									extract_utf8string(bool ignore_content_type = false);
    _ASYNCRTIMP utf16string									extract_utf16string(bool ignore_content_type = false);
    _ASYNCRTIMP utility::string_t							extract_string(bool ignore_content_type = false);

    _ASYNCRTIMP json::value									_extract_json(bool ignore_content_type = false);
    _ASYNCRTIMP std::vector<unsigned char>					_extract_vector();

    virtual _ASYNCRTIMP utility::string_t					to_string() const;

    // Completes this message
    virtual _ASYNCRTIMP void								_complete(utility::size64_t bodySize, const std::exception_ptr &exceptionPtr = std::exception_ptr());

    // Set the stream through which the message body could be read
    void													set_instream(const concurrency::streams::istream &instream)  { m_inStream = instream; }

    // Get the stream through which the message body could be read
    const concurrency::streams::istream &					instream() const { return m_inStream; }

    // Set the stream through which the message body could be written
    void													set_outstream(const concurrency::streams::ostream &outstream, bool is_default)  { m_outStream = outstream; m_default_outstream = is_default; }

    // Get the stream through which the message body could be written
    const concurrency::streams::ostream &					outstream() const { return m_outStream; }

    const pplx::task_completion_event<utility::size64_t> &	_get_data_available() const { return m_data_available; }

    // Prepare the message with an output stream to receive network data
    _ASYNCRTIMP void										_prepare_to_receive_data();

    // Determine the content length.
    // Returns:
    // size_t::max if there is content with unknown length (transfer_encoding:chunked)
    // 0           if there is no content
    // length      if there is content with known length
    // 
    // This routine should only be called after a msg (request/response) has been completely constructed.
    _ASYNCRTIMP size_t										_get_content_length();
};

// Base structure for associating internal server information with an HTTP request/response.
class _http_server_context
{
public:
    _http_server_context() {}
    virtual ~_http_server_context() {}
private:
};

/// Internal representation of an HTTP response.
class _http_response final : public http::details::http_msg_base
{
    std::unique_ptr<_http_server_context>					m_server_context;
    http::status_code										m_status_code;
    http::reason_phrase										m_reason_phrase;
public:
															_http_response		()																		: m_status_code((std::numeric_limits<uint16_t>::max)()) {}
															_http_response		(http::status_code code)												: m_status_code(code)									{}

	http::status_code										status_code			()																const	{ return m_status_code;							}
    void													set_status_code		(http::status_code code)												{ m_status_code = code;							}
    const http::reason_phrase &								reason_phrase		()																const	{ return m_reason_phrase;						}
    void													set_reason_phrase	(const http::reason_phrase &reason)										{ m_reason_phrase = reason;						}
    _ASYNCRTIMP utility::string_t							to_string			()																const;

	_http_server_context *									_get_server_context	()																const	{ return m_server_context.get();				}
    void													_set_server_context	(std::unique_ptr<details::_http_server_context> server_context)			{ m_server_context = std::move(server_context);	}
};

} // namespace details

// Represents an HTTP response.
class http_response
{
    std::shared_ptr<http::details::_http_response>			_m_impl;
public:
    // Constructs a response with an empty status code, no headers, and no body.
															http_response		()																		: _m_impl(std::make_shared<details::_http_response>()) { }
															http_response		(http::status_code code)												: _m_impl(std::make_shared<details::_http_response>(code)) { }
    // Gets the status code of the response message.
    http::status_code										status_code			()																const	{ return _m_impl->status_code(); }
    // Sets the status code of the response message. This will overwrite any previously set status code.
    void													set_status_code		(http::status_code code)										const	{ _m_impl->set_status_code(code); }

	// Gets the reason phrase of the response message.
    // If no reason phrase is set it will default to the standard one corresponding to the status code.
    const http::reason_phrase &								reason_phrase		()																const	{ return _m_impl->reason_phrase(); }

    // Sets the reason phrase of the response message.
    // If no reason phrase is set it will default to the standard one corresponding to the status code.
    void													set_reason_phrase	(const http::reason_phrase &reason)								const	{ _m_impl->set_reason_phrase(reason); }

    // Gets the headers of the response message. Use the "add" method to fill in desired headers.
    http_headers &											headers				()																		{ return _m_impl->headers(); }
    const http_headers &									headers				()																const	{ return _m_impl->headers(); }
    // Generates a string representation of the message, including the body when possible.
    // Mainly this should be used for debugging purposes as it has to copy the message body and doesn't have excellent performance.
    // Returns a string representation of this HTTP request.
    // Note this function is synchronous and doesn't wait for the entire message body to arrive. 
	// If the message body has arrived by the time this function is called and it is has a textual Content-Type it will be included. 
	// Otherwise just the headers will be present.
    utility::string_t										to_string			()																const	{ return _m_impl->to_string(); }

    // Extracts the body of the response message as a string value, checking that the content type is a MIME text type.
    // A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    // Returns string containing body of the message.
    pplx::task<utility::string_t>							extract_string		(bool ignore_content_type_header = false)						const	{
        auto														impl				= _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type_header](utility::size64_t) { return impl->extract_string(ignore_content_type_header); });
    }

    // Extracts the body of the response message as a UTF-8 string value, checking that the content type is a MIME text type.
    // A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    // Returns String containing body of the message.
    pplx::task<utf8string>									extract_utf8string	(bool ignore_content_type_header = false)								const	{
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type_header](utility::size64_t) { return impl->extract_utf8string(ignore_content_type_header); });
    }

    // Extracts the body of the response message as a UTF-16 string value, checking that the content type is a MIME text type.
    // A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    // Returns string containing body of the message.
    pplx::task<utf16string> extract_utf16string(bool ignore_content_type_header = false) const {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type_header](utility::size64_t) { return impl->extract_utf16string(ignore_content_type_header); });
    }

    // Extracts the body of the response message into a json value, checking that the content type is application/json.
    // A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    // Returns JSON value from the body of this message.
    pplx::task<json::value> extract_json(bool ignore_content_type_header = false) const {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type_header](utility::size64_t) { return impl->_extract_json(ignore_content_type_header); });
    }

    /// Extracts the body of the response message into a vector of bytes.
    /// Returns the body of the message as a vector of bytes.
    pplx::task<std::vector<unsigned char>> extract_vector() const {
        auto impl = _m_impl;
        return pplx::create_task(_m_impl->_get_data_available()).then([impl](utility::size64_t) { return impl->_extract_vector(); });
    }

    // Sets the body of the message to a textual string and set the "Content-Type" header. Assumes
    // the character encoding of the string is UTF-8.
	//
	// <param name="body_text">String containing body text.
    // <param name="content_type">MIME type to set the "Content-Type" header to. Default to "text/plain; charset=utf-8".
    // This will overwrite any previously set body data and "Content-Type" header.
    void set_body(utf8string &&body_text, const utf8string &content_type = utf8string("text/plain; charset=utf-8")) {
        const auto length = body_text.size();
        _m_impl->set_body(concurrency::streams::bytestream::open_istream<std::string>(std::move(body_text)), length, content_type);
    }

    // Sets the body of the message to a textual string and set the "Content-Type" header. Assumes
    // the character encoding of the string is UTF-8.
    // <param name="body_text">String containing body text.
    // <param name="content_type">MIME type to set the "Content-Type" header to. Default to "text/plain; charset=utf-8".
    // This will overwrite any previously set body data and "Content-Type" header.
    void set_body(const utf8string &body_text, const utf8string &content_type = utf8string("text/plain; charset=utf-8"))
    {
        _m_impl->set_body(concurrency::streams::bytestream::open_istream<std::string>(body_text), body_text.size(), content_type);
    }

    // Sets the body of the message to a textual string and set the "Content-Type" header. Assumes
    // the character encoding of the string is UTF-16 will perform conversion to UTF-8.
    // <param name="body_text">String containing body text.
    // <param name="content_type">MIME type to set the "Content-Type" header to. Default to "text/plain".
    // This will overwrite any previously set body data and "Content-Type" header.
    void set_body(const utf16string &body_text, utf16string content_type = ::utility::conversions::to_utf16string("text/plain")) {
        if (content_type.find(::utility::conversions::to_utf16string("charset=")) != content_type.npos) {
            throw std::invalid_argument("content_type can't contain a 'charset'.");
        }

        auto utf8body = utility::conversions::utf16_to_utf8(body_text);
        auto length = utf8body.size();
        _m_impl->set_body(concurrency::streams::bytestream::open_istream<std::string>(
        		std::move(utf8body)),
        		length,
        		std::move(content_type.append(::utility::conversions::to_utf16string("; charset=utf-8"))));
    }

    // Sets the body of the message to contain json value. If the 'Content-Type' header hasn't already been set it will be set to 'application/json'.
    // This will overwrite any previously set body data.
    void set_body(const json::value &body_data) {
        auto body_text = utility::conversions::to_utf8string(body_data.serialize());
        auto length = body_text.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_text)), length, _XPLATSTR("application/json"));
    }

    // Sets the body of the message to the contents of a byte vector. If the 'Content-Type' header hasn't already been set it will be set to 'application/octet-stream'.
    // This will overwrite any previously set body data.
    void set_body(std::vector<unsigned char> &&body_data) {
        auto length = body_data.size();
        set_body(concurrency::streams::bytestream::open_istream(std::move(body_data)), length);
    }

    // Sets the body of the message to the contents of a byte vector. If the 'Content-Type' header hasn't already been set it will be set to 'application/octet-stream'.
    // This will overwrite any previously set body data.
    void set_body(const std::vector<unsigned char> &body_data) { set_body(concurrency::streams::bytestream::open_istream(body_data), body_data.size()); }

    // Defines a stream that will be relied on to provide the body of the HTTP message when it is sent.
    // <param name="stream">A readable, open asynchronous stream.
    // <param name="content_type">A string holding the MIME type of the message body.
    // This cannot be used in conjunction with any other means of setting the body of the request.
    // The stream will not be read until the message is sent.
    void set_body(const concurrency::streams::istream &stream, const utility::string_t &content_type = _XPLATSTR("application/octet-stream")) { _m_impl->set_body(stream, content_type); }

    // Defines a stream that will be relied on to provide the body of the HTTP message when it is sent.
    // <param name="stream">A readable, open asynchronous stream.
    // <param name="content_length">The size of the data to be sent in the body.
    // <param name="content_type">A string holding the MIME type of the message body.
    // This cannot be used in conjunction with any other means of setting the body of the request.
    // The stream will not be read until the message is sent.
    void set_body(const concurrency::streams::istream &stream, utility::size64_t content_length, const utility::string_t &content_type = _XPLATSTR("application/octet-stream")) {
        _m_impl->set_body(stream, content_length, content_type);
    }

    // Produces a stream which the caller may use to retrieve data from an incoming request.
    // Returns a readable, open asynchronous stream.
	//
	// This cannot be used in conjunction with any other means of getting the body of the request.
    // It is not necessary to wait until the message has been sent before starting to write to the stream, but it is advisable to do so, 
	// since it will allow the network I/O to start earlier and the work of sending data can be overlapped with the production of more data.
    concurrency::streams::istream body() const { return _m_impl->instream(); }

    // Signals the user (client) when all the data for this response message has been received.
    // Returns a task which is completed when all of the response body has been received.
    pplx::task<http::http_response> content_ready() const {
        http_response resp = *this;
        return pplx::create_task(_m_impl->_get_data_available()).then([resp](utility::size64_t) mutable { return resp; });
    }

    std::shared_ptr<http::details::_http_response> _get_impl() const { return _m_impl; }

    http::details::_http_server_context * _get_server_context() const { return _m_impl->_get_server_context(); }
    void _set_server_context(std::unique_ptr<http::details::_http_server_context> server_context) { _m_impl->_set_server_context(std::move(server_context)); }

};

namespace details {
// Internal representation of an HTTP request message.
class _http_request final : public http::details::http_msg_base, public std::enable_shared_from_this<_http_request>
{
	pplx::task<void>										_reply_impl	(http_response response);	// Actual initiates sending the response, without checking if a response has already been sent.
	http::method											m_method;
	pplx::details::atomic_long								m_initiated_response;	// Tracks whether or not a response has already been started for this message.
	std::unique_ptr<http::details::_http_server_context>	m_server_context;
	pplx::cancellation_token								m_cancellationToken;
	http::uri												m_base_uri;
	http::uri												m_uri;
	utility::string_t										m_listener_path;
	concurrency::streams::ostream							m_response_stream;
	std::shared_ptr<progress_handler>						m_progress_handler;
	pplx::task_completion_event<http_response>				m_response;
public:
	virtual													~_http_request			() {}
	_ASYNCRTIMP												_http_request			(http::method mtd);
	_ASYNCRTIMP												_http_request			(std::unique_ptr<http::details::_http_server_context> server_context);
	
	http::method &											method					()																				{ return m_method; }
	uri &													request_uri				()																				{ return m_uri; }
	_ASYNCRTIMP uri											absolute_uri			()																		const;
	_ASYNCRTIMP uri											relative_uri			()																		const;
	_ASYNCRTIMP void										set_request_uri			(const uri&);
	const pplx::cancellation_token &						cancellation_token		()																		const	{ return m_cancellationToken; }
	void													set_cancellation_token	(const pplx::cancellation_token &token)											{ m_cancellationToken = token; }
	_ASYNCRTIMP utility::string_t							to_string				()																		const;
	_ASYNCRTIMP pplx::task<void>							reply					(const http_response &response);
	pplx::task<http_response>								get_response			()																				{ return pplx::task<http_response>(m_response); }
	_ASYNCRTIMP pplx::task<void>							_reply_if_not_already	(http::status_code status);
	void													set_response_stream		(const concurrency::streams::ostream &stream)									{ m_response_stream = stream; }
	void													set_progress_handler	(const progress_handler &handler)												{ m_progress_handler = std::make_shared<progress_handler>(handler); }
	const concurrency::streams::ostream &					_response_stream		()																		const	{ return m_response_stream; }
	const std::shared_ptr<progress_handler> &				_progress_handler		()																		const	{ return m_progress_handler; }
	http::details::_http_server_context *					_get_server_context		()																		const	{ return m_server_context.get(); }
	void													_set_server_context		(std::unique_ptr<http::details::_http_server_context> server_context)			{ m_server_context = std::move(server_context); }
	void													_set_listener_path		(const utility::string_t &path)													{ m_listener_path = path; }
	void													_set_base_uri			(const http::uri &base_uri)														{ m_base_uri = base_uri; }
};
}  // namespace details

// Represents an HTTP request.
class http_request {
	friend class http::details::_http_request;
	friend class http::client::http_client;
	
	http_request(std::unique_ptr<http::details::_http_server_context> server_context) : _m_impl(std::make_shared<details::_http_request>(std::move(server_context))) {}
	
	std::shared_ptr<http::details::_http_request>	_m_impl;

public:
													~http_request	()										{}
													http_request	()										: _m_impl(std::make_shared<http::details::_http_request>(methods::GET)) {}	// Constructs a new HTTP request with the 'GET' method.
													http_request	(http::method rquestMethod)				: _m_impl(std::make_shared<http::details::_http_request>(std::move(rquestMethod))) {}	// Constructs a new HTTP request with the given request method.
	
	// Get the method (GET/PUT/POST/DELETE) of the request message.
	const http::method &							method			()								const	{ return _m_impl->method();		}
	void											set_method		(const http::method &method)	const	{ _m_impl->method() = method;	}
	
	// Get the underling URI of the request message. Returns the uri of this message.
	const uri &										request_uri		()								const	{ return _m_impl->request_uri(); }
	
	// Set the underling URI of the request message.
	void											set_request_uri	(const uri& uri)						{ return _m_impl->set_request_uri(uri); }
	
	// Gets a reference the URI path, query, and fragment part of this request message.
	// This will be appended to the base URI specified at construction of the http_client.
	// Returns a string.
	// When the request is the one passed to a listener's handler, the relative URI is the request URI less the listener's path. 
	// In all other circumstances, request_uri() and relative_uri() will return the same value.
	uri												relative_uri	()								const	{ return _m_impl->relative_uri(); }
	
	// Get an absolute URI with scheme, host, port, path, query, and fragment part of the request message.
	// Absolute URI is only valid after this http_request object has been passed to http_client::request().
	uri												absolute_uri	()								const	{ return _m_impl->absolute_uri(); }
	
	// Gets a reference to the headers of the response message. Use the http_headers::add to fill in desired headers.
	http_headers &									headers			()										{ return _m_impl->headers(); }
	const http_headers &							headers			()								const	{ return _m_impl->headers(); }
	
	// Extract the body of the request message as a string value, checking that the content type is a MIME text type.
	// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
	// ignore_content_type_header: If true, ignores the Content-Type header and assumes UTF-8.
	// Returns string containing body of the message.
	pplx::task<utility::string_t>					extract_string	(bool ignore_content_type_header = false)	{
	    auto impl = _m_impl;
	    return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type_header](utility::size64_t) { return impl->extract_string(ignore_content_type_header); });
	}
	pplx::task<utf8string> extract_utf8string(bool ignore_content_type_header = false) {
	    auto impl = _m_impl;
	    return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type_header](utility::size64_t) { return impl->extract_utf8string(ignore_content_type_header); });
	}
	
	// Extract the body of the request message as a UTF-16 string value, checking that the content type is a MIME text type.
	// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
	// <param name="ignore_content_type">If true, ignores the Content-Type header and assumes UTF-16.
	// Returns string containing body of the message.
	pplx::task<utf16string> extract_utf16string(bool ignore_content_type_header = false) {
	    auto impl = _m_impl;
	    return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type_header](utility::size64_t) { return impl->extract_utf16string(ignore_content_type_header); });
	}
	
	// Extracts the body of the request message into a json value, checking that the content type is application/json.
	// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
	// <param name="ignore_content_type">If true, ignores the Content-Type header and assumes UTF-8.
	// <returns>JSON value from the body of this message.
	pplx::task<json::value> extract_json(bool ignore_content_type_header = false) const {
	    auto impl = _m_impl;
	    return pplx::create_task(_m_impl->_get_data_available()).then([impl, ignore_content_type_header](utility::size64_t) { return impl->_extract_json(ignore_content_type_header); });
	}
	
	// Extract the body of the response message into a vector of bytes. Extracting a vector can be done on
	// Returns the body of the message as a vector of bytes.
	pplx::task<std::vector<unsigned char>> extract_vector() const     {
	    auto impl = _m_impl;
	    return pplx::create_task(_m_impl->_get_data_available()).then([impl](utility::size64_t) { return impl->_extract_vector(); });
	}
	
	// Sets the body of the message to a textual string and set the "Content-Type" header. Assumes
	// the character encoding of the string is UTF-8.
	// <param name="body_text">String containing body text.
	// <param name="content_type">MIME type to set the "Content-Type" header to. Default to "text/plain; charset=utf-8".
	// This will overwrite any previously set body data and "Content-Type" header.
	void set_body(utf8string &&body_text, const utf8string &content_type = utf8string("text/plain; charset=utf-8")) {
	    const auto length = body_text.size();
	    _m_impl->set_body(concurrency::streams::bytestream::open_istream<std::string>(std::move(body_text)), length, content_type);
	}
	
	// Sets the body of the message to a textual string and set the "Content-Type" header. Assumes
	// the character encoding of the string is UTF-8.
	// <param name="body_text">String containing body text.
	// <param name="content_type">MIME type to set the "Content-Type" header to. Default to "text/plain; charset=utf-8".
	// This will overwrite any previously set body data and "Content-Type" header.
	void set_body(const utf8string &body_text, const utf8string &content_type = utf8string("text/plain; charset=utf-8")) {
	    _m_impl->set_body(concurrency::streams::bytestream::open_istream<std::string>(body_text), body_text.size(), content_type);
	}
	
	// Sets the body of the message to a textual string and set the "Content-Type" header. Assumes
	// the character encoding of the string is UTF-16 will perform conversion to UTF-8.
	// <param name="body_text">String containing body text.
	// <param name="content_type">MIME type to set the "Content-Type" header to. Default to "text/plain".
	// This will overwrite any previously set body data and "Content-Type" header.
	void set_body(const utf16string &body_text, utf16string content_type = ::utility::conversions::to_utf16string("text/plain"))
	{
	    if(content_type.find(::utility::conversions::to_utf16string("charset=")) != content_type.npos) {
	        throw std::invalid_argument("content_type can't contain a 'charset'.");
	    }
	
	    auto utf8body = utility::conversions::utf16_to_utf8(body_text);
	    auto length = utf8body.size();
	    _m_impl->set_body(concurrency::streams::bytestream::open_istream(
	    		std::move(utf8body)),
	    		length,
	    		std::move(content_type.append(::utility::conversions::to_utf16string("; charset=utf-8"))));
	}
	
	// Sets the body of the message to contain json value. If the 'Content-Type'
	// header hasn't already been set it will be set to 'application/json'.
	// <param name="body_data">json value.
	// This will overwrite any previously set body data.
	void set_body(const json::value &body_data) {
	    auto body_text = utility::conversions::to_utf8string(body_data.serialize());
	    auto length = body_text.size();
	    _m_impl->set_body(concurrency::streams::bytestream::open_istream(std::move(body_text)), length, _XPLATSTR("application/json"));
	}
	
	// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
	// header hasn't already been set it will be set to 'application/octet-stream'.
	// <param name="body_data">Vector containing body data.
	// This will overwrite any previously set body data.
	void set_body(std::vector<unsigned char> &&body_data) {
	    auto length = body_data.size();
	    _m_impl->set_body(concurrency::streams::bytestream::open_istream(std::move(body_data)), length, _XPLATSTR("application/octet-stream"));
	}
	
	// Sets the body of the message to the contents of a byte vector. If the 'Content-Type'
	// header hasn't already been set it will be set to 'application/octet-stream'.
	// <param name="body_data">Vector containing body data.
	// This will overwrite any previously set body data.
	void set_body(const std::vector<unsigned char> &body_data) {
	    set_body(concurrency::streams::bytestream::open_istream(body_data), body_data.size());
	}
	
	// Defines a stream that will be relied on to provide the body of the HTTP message when it is sent.
	// <param name="stream">A readable, open asynchronous stream.
	// <param name="content_type">A string holding the MIME type of the message body.
	// This cannot be used in conjunction with any other means of setting the body of the request.
	// The stream will not be read until the message is sent.
	void set_body(const concurrency::streams::istream &stream, const utility::string_t &content_type = _XPLATSTR("application/octet-stream")) {
	    _m_impl->set_body(stream, content_type);
	}
	
	// Defines a stream that will be relied on to provide the body of the HTTP message when it is sent.
	// <param name="stream">A readable, open asynchronous stream.
	// <param name="content_length">The size of the data to be sent in the body.
	// <param name="content_type">A string holding the MIME type of the message body.
	// This cannot be used in conjunction with any other means of setting the body of the request.
	// The stream will not be read until the message is sent.
	void set_body(const concurrency::streams::istream &stream, utility::size64_t content_length, const utility::string_t &content_type = _XPLATSTR("application/octet-stream")) {
	    _m_impl->set_body(stream, content_length, content_type);
	}
	
	// Produces a stream which the caller may use to retrieve data from an incoming request.
	// Returns a readable, open asynchronous stream.
	// This cannot be used in conjunction with any other means of getting the body of the request.
	// It is not necessary to wait until the message has been sent before starting to write to the
	// stream, but it is advisable to do so, since it will allow the network I/O to start earlier
	// and the work of sending data can be overlapped with the production of more data.
	concurrency::streams::istream body() const { return _m_impl->instream(); }
	
	// Defines a stream that will be relied on to hold the body of the HTTP response message that
	// results from the request.
	// <param name="stream">A writable, open asynchronous stream.
	   /// If this function is called, the body of the response should not be accessed in any other
	// way.
	void															set_response_stream			(const concurrency::streams::ostream &stream)						{ return _m_impl->set_response_stream(stream); }
	
	// Defines a callback function that will be invoked for every chunk of data uploaded or downloaded
	// as part of the request.
	// <param name="handler">A function representing the progress handler. It's parameters are:
	//    up:       a <c>message_direction::direction</c> value  indicating the direction of the message
	//              that is being reported.
	//    progress: the number of bytes that have been processed so far.
	// 
	//   This function will be called at least once for upload and at least once for
	//   the download body, unless there is some exception generated. An HTTP message with an error
	//   code is not an exception. This means, that even if there is no body, the progress handler will be called.
	//
	//   Setting the chunk size on the http_client does not guarantee that the client will be using
	//   exactly that increment for uploading and downloading data.
	//
	//   The handler will be called only once for each combination of argument values, in order. Depending
	//   on how a service responds, some download values may come before all upload values have been reported.
	//
	//   The progress handler will be called on the thread processing the request. This means that
	//   the implementation of the handler must take care not to block the thread or do anything
	//   that takes significant amounts of time. In particular, do not do any kind of I/O from within
	//   the handler, do not update user interfaces, and to not acquire any locks. If such activities
	//   are necessary, it is the handler's responsibility to execute that work on a separate thread.
	void															set_progress_handler		(const progress_handler &handler)									{ return _m_impl->set_progress_handler(handler); }
	
	// Asynchronously responses to this HTTP request. Returns an asynchronous operation that is completed once response is sent.
	// <param name="response">Response to send.
	pplx::task<void>												reply						(const http_response &response)								const	{ return _m_impl->reply(response); }
	
	// Asynchronously responses to this HTTP request. Returns an asynchronous operation that is completed once response is sent.
	// <param name="status">Response status code.
	pplx::task<void>												reply						(http::status_code status)									const	{ return reply(http_response(status)); }
	
	// Responds to this HTTP request. Returns an asynchronous operation that is completed once response is sent.
	// <param name="status">Response status code.
	// <param name="body_data">Json value to use in the response body.
	pplx::task<void>												reply						(http::status_code status, const json::value &body_data)	const	{
	    http_response response(status);
	    response.set_body(body_data);
	    return reply(response);
	}
	
	// Responds to this HTTP request with a string.
	// Assumes the character encoding of the string is UTF-8.
	// <param name="status">Response status code.
	// <param name="body_data">UTF-8 string containing the text to use in the response body.
	// <param name="content_type">Content type of the body.
	// Returns an asynchronous operation that is completed once response is sent.
	//  Callers of this function do NOT need to block waiting for the response to be
	// sent to before the body data is destroyed or goes out of scope.
	    pplx::task<void>											reply						(http::status_code status, utf8string &&body_data, const utf8string &content_type = "text/plain; charset=utf-8") const
	{
	    http_response response(status);
	    response.set_body(std::move(body_data), content_type);
	    return reply(response);
	}
	
	// Responds to this HTTP request with a string.
	// Assumes the character encoding of the string is UTF-8.
	// <param name="status">Response status code.
	// <param name="body_data">UTF-8 string containing the text to use in the response body.
	// <param name="content_type">Content type of the body.
	// Returns an asynchronous operation that is completed once response is sent.
	//  Callers of this function do NOT need to block waiting for the response to be
	// sent to before the body data is destroyed or goes out of scope.
	    pplx::task<void>											reply						(http::status_code status, const utf8string &body_data, const utf8string &content_type = "text/plain; charset=utf-8") const	{
	    http_response response(status);
	    response.set_body(body_data, content_type);
	    return reply(response);
	}
	
	// Responds to this HTTP request with a string. Assumes the character encoding
	// of the string is UTF-16 will perform conversion to UTF-8.
	// <param name="status">Response status code.
	// <param name="body_data">UTF-16 string containing the text to use in the response body.
	// <param name="content_type">Content type of the body.
	// Returns an asynchronous operation that is completed once response is sent.
	//  Callers of this function do NOT need to block waiting for the response to be
	// sent to before the body data is destroyed or goes out of scope.
	    pplx::task<void>											reply						(http::status_code status, const utf16string &body_data, const utf16string &content_type = ::utility::conversions::to_utf16string("text/plain")) const	{
	    http_response response(status);
	    response.set_body(body_data, content_type);
	    return reply(response);
	}
	
	// Responds to this HTTP request.
	// <param name="status">Response status code.
	// <param name="content_type">A string holding the MIME type of the message body.
	// <param name="body">An asynchronous stream representing the body data.
	// Returns a task that is completed once a response from the request is received.
	pplx::task<void>												reply						(status_code status, const concurrency::streams::istream &body, const utility::string_t &content_type = _XPLATSTR("application/octet-stream")) const	{
	    http_response response(status);
	    response.set_body(body, content_type);
	    return reply(response);
	}
	
	// Responds to this HTTP request.
	// <param name="status">Response status code.
	// <param name="content_length">The size of the data to be sent in the body..
	// <param name="content_type">A string holding the MIME type of the message body.
	// <param name="body">An asynchronous stream representing the body data.
	// Returns a task that is completed once a response from the request is received.
			pplx::task<void>										reply						(status_code status, const concurrency::streams::istream &body, utility::size64_t content_length, const utility::string_t &content_type = _XPLATSTR("application/octet-stream"))	const	{
	    http_response response(status);
	    response.set_body(body, content_length, content_type);
	    return reply(response);
	}
	
	// Signals the user (listener) when all the data for this request message has been received.
	// Returns a task which is completed when all of the response body has been received
			pplx::task<http_request>								content_ready				()																		const	{
	    http_request req = *this;
	    return pplx::create_task(_m_impl->_get_data_available()).then([req](utility::size64_t) mutable { return req; });
	}
	
	// Gets a task representing the response that will eventually be sent.
	// Returns a task that is completed once response is sent.
			pplx::task<http_response>								get_response				()																		const	{ return _m_impl->get_response(); }
	
	// Generates a string representation of the message, including the body when possible.
	// Mainly this should be used for debugging purposes as it has to copy the
	// message body and doesn't have excellent performance.
	// Returns a string representation of this HTTP request.
	// Note this function is synchronous and doesn't wait for the
	// entire message body to arrive. If the message body has arrived by the time this
	// function is called and it is has a textual Content-Type it will be included.
	// Otherwise just the headers will be present.
			utility::string_t										to_string					()																		const	{ return _m_impl->to_string(); }
	// Sends a response if one has not already been sent.
			pplx::task<void>										_reply_if_not_already		(status_code status) { return _m_impl->_reply_if_not_already(status); }
	// Gets the server context associated with this HTTP message.
			http::details::_http_server_context *					_get_server_context			()																		const	{ return _m_impl->_get_server_context(); }
	// These are used for the initial creation of the HTTP request.
	static	http_request											_create_request				(std::unique_ptr<http::details::_http_server_context> server_context)			{ return http_request(std::move(server_context));			}
			void													_set_server_context			(std::unique_ptr<http::details::_http_server_context> server_context)			{ _m_impl->_set_server_context(std::move(server_context));	}
			void													_set_listener_path			(const utility::string_t &path)													{ _m_impl->_set_listener_path(path);						}
			const std::shared_ptr<http::details::_http_request> &	_get_impl					()																		const	{ return _m_impl;											}
			void													_set_cancellation_token		(const pplx::cancellation_token &token)											{ _m_impl->set_cancellation_token(token);					}
			const pplx::cancellation_token &						_cancellation_token			()																		const	{ return _m_impl->cancellation_token();						}
			void													_set_base_uri				(const http::uri &base_uri)														{ _m_impl->_set_base_uri(base_uri);							}
};

namespace client {
class http_pipeline;
}

// HTTP client handler class, used to represent an HTTP pipeline stage.
// When a request goes out, it passes through a series of stages, customizable by the application and/or libraries.
// The default stage will interact with lower-level communication layers to actually send the message on the network. 
// When creating a client instance, an application may add pipeline stages in front of the already existing stages. 
// Each stage has a reference to the next stage available in the http_pipeline_stage::next_stage value.
class http_pipeline_stage : public std::enable_shared_from_this<http_pipeline_stage>
{
    std::shared_ptr<http_pipeline_stage>							m_next_stage;
    friend class ::web::http::client::http_pipeline;

    void															set_next_stage				(const std::shared_ptr<http_pipeline_stage> &next)			{ m_next_stage = next; }
public:
    virtual															~http_pipeline_stage		()															= default;
																	http_pipeline_stage			()															= default;
																	http_pipeline_stage			(const http_pipeline_stage &)								= delete;

	http_pipeline_stage &											operator=					(const http_pipeline_stage &)								= delete;
    // Runs this stage against the given request and passes onto the next stage. Returns a task of the HTTP response.
    virtual pplx::task<http_response>								propagate					(http_request request)										= 0;

protected:
    // Gets the next stage in the pipeline. Returns a shared pointer to a pipeline stage.
    const std::shared_ptr<http_pipeline_stage> &					next_stage					()													const	{ return m_next_stage; }

    // Gets a shared pointer to this pipeline stage. Returns a shared pointer to a pipeline stage.
    CASABLANCA_DEPRECATED("This api is redundant. Use 'shared_from_this()' directly instead.")
    std::shared_ptr<http_pipeline_stage>							current_stage				()															{ return this->shared_from_this(); }
};

}}
