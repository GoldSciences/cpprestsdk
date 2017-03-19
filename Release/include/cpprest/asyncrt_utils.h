// Various common utilities.
// For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <system_error>
#include <random>
#include <locale.h>

#include "pplx/pplxtasks.h"
#include "cpprest/details/basic_types.h"

#if !defined(_WIN32) || (_MSC_VER >= 1700)
#include <chrono>
#endif

#ifndef _WIN32
#include <boost/algorithm/string.hpp>
#if !defined(ANDROID) && !defined(__ANDROID__) // CodePlex 269
#include <xlocale.h>
#endif
#endif

/// Various utilities for string conversions and date and time manipulation.
namespace utility
{

// Left over from VS2010 support, remains to avoid breaking.
typedef std::chrono::seconds seconds;

/// Functions for converting to/from std::chrono::seconds to xml string.
namespace timespan
{
    // http://www.w3.org/TR/xmlschema-2/#duration
    _ASYNCRTIMP utility::string_t			__cdecl	seconds_to_xml_duration		(utility::seconds numSecs);
    _ASYNCRTIMP utility::seconds			__cdecl	xml_duration_to_seconds		(const utility::string_t &timespanString);
}

/// Functions for Unicode string conversions.
namespace conversions
{
    _ASYNCRTIMP std::string					__cdecl	utf16_to_utf8				(const utf16string &w);
    _ASYNCRTIMP utf16string					__cdecl	utf8_to_utf16				(const std::string &s);
    _ASYNCRTIMP utf16string					__cdecl	usascii_to_utf16			(const std::string &s);
    _ASYNCRTIMP utf16string					__cdecl	latin1_to_utf16				(const std::string &s);
    _ASYNCRTIMP utf8string					__cdecl	latin1_to_utf8				(const std::string &s);
#ifdef _UTF16_STRINGS
    _ASYNCRTIMP utility::string_t			__cdecl	to_string_t					(std::string &&s);
#else
    inline utility::string_t&&						to_string_t					(std::string &&s)														{ return std::move(s); }
#endif
#ifdef _UTF16_STRINGS
    inline utility::string_t&&						to_string_t					(utf16string &&s)														{ return std::move(s); }
#else
    _ASYNCRTIMP utility::string_t			__cdecl	to_string_t					(utf16string &&s);
#endif
#ifdef _UTF16_STRINGS
    _ASYNCRTIMP utility::string_t			__cdecl	to_string_t					(const std::string &s);
#else
    inline const utility::string_t&					to_string_t					(const std::string &s)													{ return s; }
#endif
#ifdef _UTF16_STRINGS
    inline const utility::string_t&					to_string_t					(const utf16string &s)													{ return s; }
#else
    _ASYNCRTIMP utility::string_t			__cdecl	to_string_t					(const utf16string &s);
#endif
    _ASYNCRTIMP	utf16string					__cdecl	to_utf16string				(const std::string &value);
    inline		const utf16string&					to_utf16string				(const utf16string& value)												{ return value; }
    inline		utf16string&& to_utf16string(utf16string&& value)
    {
        return std::move(value);
    }
    inline		std::string&&						to_utf8string				(std::string&& value)													{ return std::move(value); }
    inline		const std::string&					to_utf8string				(const std::string& value)												{ return value; }
    _ASYNCRTIMP	std::string					__cdecl	to_utf8string				(const utf16string &value);
    _ASYNCRTIMP	utility::string_t			__cdecl	to_base64					(const std::vector<unsigned char>& data);
    _ASYNCRTIMP	utility::string_t			__cdecl	to_base64					(uint64_t data);
    _ASYNCRTIMP	std::vector<unsigned char>	__cdecl	from_base64					(const utility::string_t& str);

    template <typename Source>
    CASABLANCA_DEPRECATED("All locale-sensitive APIs will be removed in a future update. Use stringstreams directly if locale support is required.")
				utility::string_t					print_string				(const Source &val, const std::locale& loc = std::locale())
    {
        utility::ostringstream_t							oss;
        oss.imbue(loc);
        oss << val;
        if (oss.bad()) {
            throw std::bad_cast();
        }
        return oss.str();
    }

    CASABLANCA_DEPRECATED("All locale-sensitive APIs will be removed in a future update. Use stringstreams directly if locale support is required.")
    inline		utility::string_t					print_string				(const utility::string_t &val)												{ return val; }

    namespace details
    {
#if defined(__ANDROID__)
        template<class T>
        inline std::string								to_string					(const T& t)																{
            std::ostringstream									os;
            os.imbue(std::locale::classic());
            os << t;
            return os.str();
        }
#endif
        template<class T>
        inline utility::string_t						to_string_t					(T&& t)																		{
#ifdef _UTF16_STRINGS
            using std::to_wstring;
            return to_wstring(std::forward<T>(t));
#else
#if !defined(__ANDROID__)
            using std::to_string;
#endif
            return to_string(std::forward<T>(t));
#endif
        }

        template <typename Source>
        utility::string_t								print_string				(const Source &val)															{
            utility::ostringstream_t							oss;
            oss.imbue(std::locale::classic());
            oss << val;
            if (oss.bad()) {
                throw std::bad_cast();
            }
            return oss.str();
        }

        inline const utility::string_t&					print_string				(const utility::string_t &val)												{ return val; }
        template <typename Target>
        Target											scan_string					(const utility::string_t &str)												{
            Target t;
            utility::istringstream_t iss(str);
            iss.imbue(std::locale::classic());
            iss >> t;
            if (iss.bad()) {
                throw std::bad_cast();
            }
            return t;
        }

        inline const utility::string_t&					scan_string					(const utility::string_t &str)												{ return str; }
    }

    template <typename Target>
    CASABLANCA_DEPRECATED("All locale-sensitive APIs will be removed in a future update. Use stringstreams directly if locale support is required.")
    Target												scan_string					(const utility::string_t &str, const std::locale &loc = std::locale())	{
        Target t;
        utility::istringstream_t iss(str);
        iss.imbue(loc);
        iss >> t;
        if (iss.bad()) {
            throw std::bad_cast();
        }
        return t;
    }

    CASABLANCA_DEPRECATED("All locale-sensitive APIs will be removed in a future update. Use stringstreams directly if locale support is required.")
    inline utility::string_t							scan_string					(const utility::string_t &str)											{ return str; }
}

namespace details
{
    /// Cross platform RAII container for setting thread local locale.
    class scoped_c_thread_locale {
#ifdef _WIN32
        std::string											m_prevLocale;
        int													m_prevThreadSetting;
#elif !(defined(ANDROID) || defined(__ANDROID__))
        locale_t											m_prevLocale;
#endif
        scoped_c_thread_locale(const scoped_c_thread_locale &);
        scoped_c_thread_locale & operator=(const scoped_c_thread_locale &);
    public:
        _ASYNCRTIMP											scoped_c_thread_locale		();
        _ASYNCRTIMP											~scoped_c_thread_locale		();

#if !defined(ANDROID) && !defined(__ANDROID__) // CodePlex 269
#ifdef _WIN32
        typedef _locale_t									xplat_locale;
#else
        typedef locale_t									xplat_locale;
#endif

        static _ASYNCRTIMP xplat_locale				__cdecl	c_locale					();
#endif
    };
    // Our own implementation of alpha numeric instead of std::isalnum to avoid taking global lock for performance reasons.
    inline bool									__cdecl	is_alnum					(char ch)																	{
        return (ch >= '0' && ch <= '9')
            || (ch >= 'A' && ch <= 'Z')
            || (ch >= 'a' && ch <= 'z')
			;
    }
    template <typename _Type, typename... _Args>
    std::unique_ptr<_Type>		make_unique											(_Args&&... args)															{ return std::unique_ptr<_Type>(new _Type(args...)); }
    template <typename _Type>
    std::unique_ptr<_Type> make_unique() {
        return std::unique_ptr<_Type>(new _Type());
    }

    template <typename _Type, typename _Arg1>
    std::unique_ptr<_Type> make_unique(_Arg1&& arg1) {
        return std::unique_ptr<_Type>(new _Type(std::forward<_Arg1>(arg1)));
    }

    template <typename _Type, typename _Arg1, typename _Arg2>
    std::unique_ptr<_Type> make_unique(_Arg1&& arg1, _Arg2&& arg2) {
        return std::unique_ptr<_Type>(new _Type(std::forward<_Arg1>(arg1), std::forward<_Arg2>(arg2)));
    }

    //template <typename _Type, typename _Arg1, typename _Arg2, typename _Arg3>
    //std::unique_ptr<_Type> make_unique(_Arg1&& arg1, _Arg2&& arg2, _Arg3&& arg3) {
    //    return std::unique_ptr<_Type>(new _Type(std::forward<_Arg1>(arg1), std::forward<_Arg2>(arg2), std::forward<_Arg3>(arg3)));
    //}
	//
    //template <typename _Type, typename _Arg1, typename _Arg2, typename _Arg3, typename _Arg4>
    //std::unique_ptr<_Type> make_unique(_Arg1&& arg1, _Arg2&& arg2, _Arg3&& arg3, _Arg4&& arg4) {
    //    return std::unique_ptr<_Type>(new _Type(std::forward<_Arg1>(arg1), std::forward<_Arg2>(arg2), std::forward<_Arg3>(arg3), std::forward<_Arg4>(arg4)));
    //}

    // Cross platform utility function for performing case insensitive string comparision.
    inline bool					str_icmp											(const utility::string_t &left, const utility::string_t &right)				{
#ifdef _WIN32
        return _wcsicmp(left.c_str(), right.c_str()) == 0;
#else
        return boost::iequals(left, right);
#endif
    }

#ifdef _WIN32

	// Category error type for Windows OS errors.
	class windows_category_impl : public std::error_category {
	public:
		virtual const char *						name						()						const CPPREST_NOEXCEPT { return "windows"; }
		_ASYNCRTIMP virtual std::string				message						(int errorCode)			const CPPREST_NOEXCEPT;
		_ASYNCRTIMP virtual std::error_condition	default_error_condition		(int errorCode)			const CPPREST_NOEXCEPT;
	};
	_ASYNCRTIMP const std::error_category &	__cdecl	windows_category			();	// Gets the one global instance of the windows error category.
	#else
	_ASYNCRTIMP const std::error_category &	__cdecl	linux_category				();	// Gets the one global instance of the linux error category.
	#endif
	/// Gets the one global instance of the current platform's error category.
	_ASYNCRTIMP const std::error_category & __cdecl platform_category	();
	/// Creates an instance of std::system_error from a OS error code.
	inline std::system_error __cdecl				create_system_error	(unsigned long errorCode) {
		std::error_code code((int)errorCode, platform_category());
		return std::system_error(code, code.message());
	}

	inline std::error_code		__cdecl create_error_code		(unsigned long errorCode)	{ return std::error_code((int)errorCode, platform_category()); }						// Creates a std::error_code from a OS error code.
	inline utility::string_t	__cdecl create_error_message	(unsigned long errorCode)	{ return utility::conversions::to_string_t(create_error_code(errorCode).message()); }	// Creates the corresponding error message from a OS error code.
}

class datetime
{
public:
    typedef	uint64_t					interval_type;
    enum	date_format					{ RFC_1123, ISO_8601 };	// Defines the supported date and time string formats.

	static _ASYNCRTIMP datetime	__cdecl	utc_now				();	// Returns the current UTC time.

    // An invalid UTC timestamp value.
    enum : interval_type { utc_timestamp_invalid = static_cast<interval_type>(-1) };
    // Returns seconds since Unix/POSIX time epoch at 01-01-1970 00:00:00. If time is before epoch, utc_timestamp_invalid is returned.
    static interval_type				utc_timestamp		() {
        const auto seconds = utc_now().to_interval() / _secondTicks;
        if (seconds >= 11644473600LL)
            return seconds - 11644473600LL;
        else
            return utc_timestamp_invalid;
    }
										datetime			()											: m_interval(0)							{}
    // Creates datetime from a string representing time in UTC in RFC 1123 format. Returns a datetime of zero if not successful.
    static _ASYNCRTIMP datetime	__cdecl	from_string			(const utility::string_t& timestring, date_format format = RFC_1123);
    // Returns a string representation of the datetime.
    _ASYNCRTIMP utility::string_t		to_string			(date_format format = RFC_1123)		const;
    // Returns the integral time value.
    interval_type						to_interval			()									const	{ return m_interval;					}
    datetime							operator-			(interval_type value)				const	{ return datetime(m_interval - value);	}
    datetime							operator+			(interval_type value)				const	{ return datetime(m_interval + value);	}
    bool								operator==			(datetime dt)						const	{ return m_interval == dt.m_interval;	}
    bool								operator!=			(const datetime& dt)				const	{ return !(*this == dt);				}
    static interval_type				from_milliseconds	(unsigned int milliseconds)					{ return milliseconds*_msTicks;			}
    static interval_type				from_seconds		(unsigned int seconds)						{ return seconds*_secondTicks;			}
    static interval_type				from_minutes		(unsigned int minutes)						{ return minutes*_minuteTicks;			}
    static interval_type				from_hours			(unsigned int hours)						{ return hours*_hourTicks;				}
    static interval_type				from_days			(unsigned int days)							{ return days*_dayTicks;				}
    bool								is_initialized		()									const	{ return m_interval != 0;				}

private:

    friend int							operator-			(datetime t1, datetime t2);

    static const interval_type			_msTicks			= static_cast<interval_type>(10000);
    static const interval_type			_secondTicks		= 1000		*_msTicks;
    static const interval_type			_minuteTicks		= 60		*_secondTicks;
    static const interval_type			_hourTicks			= 60*60		*_secondTicks;
    static const interval_type			_dayTicks			= 24*60*60	*_secondTicks;

	interval_type						m_interval			;	// Storing as hundreds of nanoseconds 10e-7, i.e. 1 here equals 100ns.

    // Private constructor. Use static methods to create an instance.
										datetime			(interval_type interval)					: m_interval(interval){}
#ifdef _WIN32
    // void* to avoid pulling in windows.h
    static _ASYNCRTIMP bool		__cdecl	datetime::system_type_to_datetime(/*SYSTEMTIME*/ void* psysTime, uint64_t seconds, datetime * pdt);
#else
    static datetime						timeval_to_datetime	(const timeval &time);
#endif

};

#ifndef _WIN32

// temporary workaround for the fact that
// utf16char is not fully supported in GCC
class cmp {
    static char							tolower				(char c)									{
        if (c >= 'A' && c <= 'Z')
            return static_cast<char>(c - 'A' + 'a');
        return c;
    }
public:
    static int							icmp				(std::string left, std::string right)		{
        size_t i;
        for (i = 0; i < left.size(); ++i) {
            if (i == right.size()) 
				return 1;

            auto l = cmp::tolower(left[i]);
            auto r = cmp::tolower(right[i]);
            if (l > r) 
				return 1;
            if (l < r) 
				return -1;
        }
        if (i < right.size()) 
			return -1;
        return 0;
    }
};

#endif

inline int							operator-					(datetime t1, datetime t2)			{
    auto									diff = (t1.m_interval - t2.m_interval);
    diff								/= 10 * 1000 * 1000;		// Round it down to seconds
    return static_cast<int>(diff);
}

// Nonce string generator class.
class nonce_generator {
    static const utility::string_t	c_allowed_chars;
    std::mt19937					m_random;
    int								m_length;
public:
									nonce_generator				(int length=32) :
        m_random(static_cast<unsigned int>(utility::datetime::utc_timestamp())),
        m_length(length)
    {}
    // Generate a nonce string containing random alphanumeric characters (A-Za-z0-9). Length of the generated string is set by length().
    _ASYNCRTIMP utility::string_t	generate					();
    int								length						()							const	{ return m_length; }
    void							set_length					(int length)						{ m_length = length; }
};

} // namespace utility;
