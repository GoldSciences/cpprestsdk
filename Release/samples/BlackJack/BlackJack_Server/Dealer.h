// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#include "messagetypes.h"
#include "Table.h"

#ifndef __DEALER_H__92873492834__
#define __DEALER_H__92873492834__

// Contains the main logic of the black jack dealer.
class Dealer {
	static std::map<utility::string_t, std::shared_ptr<BJTable>>	s_tables						;
	static int														nextId							;

			web::http::experimental::listener::http_listener		m_listener						;   

    void															handle_get						(web::http::http_request message);
    void															handle_put						(web::http::http_request message);
    void															handle_post						(web::http::http_request message);
    void															handle_delete					(web::http::http_request message);
public:
																	Dealer							()									{}
																	Dealer							(utility::string_t url)				: m_listener(url)				{
		m_listener.support(web::http::methods::GET	, std::bind(&Dealer::handle_get		, this, std::placeholders::_1));
		m_listener.support(web::http::methods::PUT	, std::bind(&Dealer::handle_put		, this, std::placeholders::_1));
		m_listener.support(web::http::methods::POST	, std::bind(&Dealer::handle_post	, this, std::placeholders::_1));
		m_listener.support(web::http::methods::DEL	, std::bind(&Dealer::handle_delete	, this, std::placeholders::_1));
    
		utility::ostringstream_t											nextIdString;
		nextIdString << nextId;

		std::shared_ptr<DealerTable>										tbl								= std::make_shared<DealerTable>(nextId, 8, 6);
		nextId															+= 1;
		s_tables[nextIdString.str()]									= tbl;
	}
	inline	pplx::task<void>										open							()									{ return m_listener.open	(); }
	inline	pplx::task<void>										close							()									{ return m_listener.close	(); }
};

#endif // __DEALER_H__92873492834__