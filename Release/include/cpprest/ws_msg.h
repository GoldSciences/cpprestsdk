/***
* Copyright (C) Microsoft. All rights reserved.
* Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
*
* =+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*
* Websocket incoming and outgoing message definitions.
*
* =-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-
****/
#pragma once

#include "cpprest/details/basic_types.h"

#if !defined(CPPREST_EXCLUDE_WEBSOCKETS)

#include <memory>
#include <limits>

#include "pplx/pplxtasks.h"
#include "cpprest/streams.h"
#include "cpprest/containerstream.h"
#include "cpprest/uri.h"
#include "cpprest/asyncrt_utils.h"

namespace web
{
namespace websockets
{
namespace client
{

namespace details
{
    class winrt_callback_client;
    class wspp_callback_client;
#if defined(__cplusplus_winrt)
    ref class ReceiveContext;
#endif
}

/// The different types of websocket message.
/// Text type contains UTF-8 encoded data.
/// Interpretation of Binary type is left to the application.
/// Note: The fragment types and control frames like close, ping, pong are not supported on WinRT.
enum class websocket_message_type
{
    text_message,
    binary_message,
    close,
    ping,
    pong
};

/// Represents an outgoing websocket message
class websocket_outgoing_message
{
public:

#if !defined(__cplusplus_winrt)
    /// Sets a the outgoing message to be an unsolicited pong message.
    /// This is useful when the client side wants to check whether the server is alive.
    void set_pong_message()
    {
        this->set_message_pong();
    }
#endif

    /// Sets a UTF-8 message as the message body.
    /// <param name="data">UTF-8 String containing body of the message.
    void set_utf8_message(std::string &&data)
    {
        this->set_message(concurrency::streams::container_buffer<std::string>(std::move(data)));
    }

    /// Sets a UTF-8 message as the message body.
    /// <param name="data">UTF-8 String containing body of the message.
    void set_utf8_message(const std::string &data)
    {
        this->set_message(concurrency::streams::container_buffer<std::string>(data));
    }

    /// Sets a UTF-8 message as the message body.
    /// <param name="istream">casablanca input stream representing the body of the message.
    /// Upon sending, the entire stream may be buffered to determine the length.
    void set_utf8_message(const concurrency::streams::istream &istream)
    {
        this->set_message(istream, SIZE_MAX, websocket_message_type::text_message);
    }

    /// Sets a UTF-8 message as the message body.
    /// <param name="istream">casablanca input stream representing the body of the message.
    /// <param name="len">number of bytes to send.
    void set_utf8_message(const concurrency::streams::istream &istream, size_t len)
    {
        this->set_message(istream, len, websocket_message_type::text_message);
    }

    /// Sets binary data as the message body.
    /// <param name="istream">casablanca input stream representing the body of the message.
    /// <param name="len">number of bytes to send.
    void set_binary_message(const concurrency::streams::istream &istream, size_t len)
    {
        this->set_message(istream, len, websocket_message_type::binary_message);
    }

    /// Sets binary data as the message body.
    /// <param name="istream">Input stream representing the body of the message.
    /// Upon sending, the entire stream may be buffered to determine the length.
    void set_binary_message(const concurrency::streams::istream &istream)
    {
        this->set_message(istream, SIZE_MAX, websocket_message_type::binary_message);
    }

private:
    friend class details::winrt_callback_client;
    friend class details::wspp_callback_client;

    pplx::task_completion_event<void> m_body_sent;
    concurrency::streams::streambuf<uint8_t> m_body;
    websocket_message_type m_msg_type;
    size_t m_length;

    void signal_body_sent() const
    {
        m_body_sent.set();
    }

    void signal_body_sent(const std::exception_ptr &e) const
    {
        m_body_sent.set_exception(e);
    }

    const pplx::task_completion_event<void> & body_sent() const { return m_body_sent; }

#if !defined(__cplusplus_winrt)
	void set_message_pong()
	{
        concurrency::streams::container_buffer<std::string> buffer("");
		m_msg_type = websocket_message_type::pong;
		m_length = static_cast<size_t>(buffer.size());
		m_body = buffer;
	}
#endif

    void set_message(const concurrency::streams::container_buffer<std::string> &buffer)
    {
        m_msg_type = websocket_message_type::text_message;
        m_length = static_cast<size_t>(buffer.size());
        m_body = buffer;
    }

    void set_message(const concurrency::streams::istream &istream, size_t len, websocket_message_type msg_type)
    {
        m_msg_type = msg_type;
        m_length = len;
        m_body = istream.streambuf();
    }
};

/// Represents an incoming websocket message
class websocket_incoming_message
{
public:

    /// Extracts the body of the incoming message as a string value, only if the message type is UTF-8.
    /// A body can only be extracted once because in some cases an optimization is made where the data is 'moved' out.
    /// Returns string containing body of the message.
    _ASYNCRTIMP pplx::task<std::string> extract_string() const;

    /// Produces a stream which the caller may use to retrieve body from an incoming message.
    /// Can be used for both UTF-8 (text) and binary message types.
    /// Returns a readable, open asynchronous stream.
        /// This cannot be used in conjunction with any other means of getting the body of the message.
        concurrency::streams::istream body() const
    {
        auto to_uint8_t_stream = [](const concurrency::streams::streambuf<uint8_t> &buf) -> concurrency::streams::istream
        {
            return buf.create_istream();
        };
        return to_uint8_t_stream(m_body);
    }

    /// Returns the length of the received message.
    size_t length() const
    {
        return static_cast<size_t>(m_body.size());
    }

    /// Returns the type of the received message.
    CASABLANCA_DEPRECATED("Incorrectly spelled API, use message_type() instead.")
    websocket_message_type messge_type() const
    {
        return m_msg_type;
    }

    /// Returns the type of the received message, either string or binary.
    /// <returns>websocket_message_type
    websocket_message_type message_type() const
    {
        return m_msg_type;
    }

private:
    friend class details::winrt_callback_client;
    friend class details::wspp_callback_client;
#if defined(__cplusplus_winrt)
    friend ref class details::ReceiveContext;
#endif

    // Store message body in a container buffer backed by a string.
    // Allows for optimization in the string message cases.
    concurrency::streams::container_buffer<std::string> m_body;
    websocket_message_type m_msg_type;
};

}}}

#endif
