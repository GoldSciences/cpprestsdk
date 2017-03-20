// URI parsing implementation. For the latest on this and related APIs, please see: https://github.com/Microsoft/cpprestsdk
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#pragma once

#include <string>

namespace web { namespace details
{
    namespace uri_parser
    {
		// Parses the uri, attempting to determine its validity. This function accepts both uris ('http://msn.com') and uri relative-references ('path1/path2?query')
		bool validate(const utility::string_t &encoded_string);

		// Parses the uri, setting each provided string to the value of that component. Components that are not part of the provided text are set to the empty string. 
		// Component strings DO NOT contain their beginning or ending delimiters.
		// This function accepts both uris ('http://msn.com') and uri relative-references ('path1/path2?query')
		bool parse(const utility::string_t &encoded_string, uri_components &components);

		// Unreserved characters are those that are allowed in a URI but do not have a reserved purpose. They include:
		// - A-Z
		// - a-z
		// - 0-9
		// - '-' (hyphen)
		// - '.' (period)
		// - '_' (underscore)
		// - '~' (tilde)
		inline bool is_unreserved(int c) { return ::utility::details::is_alnum((char)c) || c == '-' || c == '.' || c == '_' || c == '~'; }

		// General delimiters serve as the delimiters between different uri components. General delimiters include /?#[]@:
		inline bool is_gen_delim(int c) { return c == ':' || c == '/' || c == '?' || c == '#' || c == '[' || c == ']' || c == '@'; }

		// Subdelimiters are those characters that may have a defined meaning within component of a uri for a particular scheme. 
		// They do not serve as delimiters in any case between uri segments. sub_delimiters include !$&'()*+,;=
		inline bool is_sub_delim(int c) {
			switch (c) {
			case '!' :
			case '$' :
			case '&' :
			case '\'':
			case '(' :
			case ')' :
			case '*' :
			case '+' :
			case ',' :
			case ';' :
			case '=' :
				return true;
			default:
				return false;
			}
		}

		// Reserved characters includes the general delimiters and sub delimiters. Some characters are neither reserved nor unreserved, and must be percent-encoded.
		inline bool is_reserved(int c) { return is_gen_delim(c) || is_sub_delim(c); }

		// Legal characters in the scheme portion include:
		// - Any alphanumeric character
		// - '+' (plus)
		// - '-' (hyphen)
		// - '.' (period)
		//
		// Note that the scheme must BEGIN with an alpha character.
		inline bool is_scheme_character(int c) { return ::utility::details::is_alnum((char)c) || c == '+' || c == '-' || c == '.'; }

		// Legal characters in the user information portion include:
		// - Any unreserved character
		// - The percent character ('%'), and thus any percent-endcoded octet
		// - The sub-delimiters
		// - ':' (colon)
		inline bool is_user_info_character(int c) { return is_unreserved(c) || is_sub_delim(c) || c == '%' || c == ':'; }

		// Legal characters in the host portion include:
		// - Any unreserved character
		// - The percent character ('%'), and thus any percent-endcoded octet
		// - The sub-delimiters
		// - ':' (colon)
		// - '[' (open bracket)
		// - ']' (close bracket)
		inline bool is_host_character(int c) { return is_unreserved(c) || is_sub_delim(c) || c == '%' || c == ':' || c == '[' || c == ']'; }

		// Legal characters in the authority portion include:
		// - Any unreserved character
		// - The percent character ('%'), and thus any percent-endcoded octet
		// - The sub-delimiters
		// - ':' (colon)
		// - IPv6 requires '[]' allowed for it to be valid URI and passed to underlying platform for IPv6 support
		inline bool is_authority_character(int c) { return is_unreserved(c) || is_sub_delim(c) || c == '%' || c == '@' || c == ':' || c == '[' || c == ']'; }

		// Legal characters in the path portion include:
		// - Any unreserved character
		// - The percent character ('%'), and thus any percent-endcoded octet
		// - The sub-delimiters
		// - ':' (colon)
		// - '@' (ampersand)
		inline bool is_path_character(int c) { return is_unreserved(c) || is_sub_delim(c) || c == '%' || c == '/' || c == ':' || c == '@'; }

		// Legal characters in the query portion include:
		// - Any path character
		// - '?' (question mark)
		inline bool is_query_character(int c) { return is_path_character(c) || c == '?'; }

		// Legal characters in the fragment portion include:
		// - Any path character
		// - '?' (question mark)
		inline bool is_fragment_character(int c) { return is_query_character(c); }	// this is intentional, they have the same set of legal characters
		// Parses the uri, setting the given pointers to locations inside the given buffer.
		// 'encoded' is expected to point to an encoded zero-terminated string containing a uri
		bool inner_parse
			(	const utility::char_t *encoded
			,	const utility::char_t **begin_scheme	, const utility::char_t **scheme_end
			,	const utility::char_t **begin_uinfo		, const utility::char_t **uinfo_end
			,	const utility::char_t **begin_host		, const utility::char_t **host_end
			,	_Out_ int * port
			,	const utility::char_t **begin_path		, const utility::char_t **path_end
			,	const utility::char_t **begin_query		, const utility::char_t **query_end
			,	const utility::char_t **begin_fragment	, const utility::char_t **fragment_end
			);
		}
	}
}
