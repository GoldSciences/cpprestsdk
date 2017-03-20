// Listener code: Given a location/postal code, the listener queries different services for weather, things to do: events, movie and pictures and returns it to the client.
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#include "casalens.h"

#include "cpprest/filestream.h"

const utility::string_t		casalens_creds::	url_events				= U("http://api.eventful.com/json/events/search?...&location="			);
const utility::string_t		casalens_creds::	url_movies				= U("http://data.tmsapi.com/v1/movies/showings?"						);
const utility::string_t		casalens_creds::	url_images				= U("https://api.datamarket.azure.com/Bing/Search/Image?$format=json"	);
const utility::string_t		casalens_creds::	url_bmaps				= U("http://dev.virtualearth.net/REST/v1/Locations"						);
const utility::string_t		casalens_creds::	url_gmaps				= U("http://maps.googleapis.com/maps/api/geocode/json"					);
const utility::string_t		casalens_creds::	url_weather				= U("http://api.openweathermap.org/data/2.1/find/name?q="				);

// --- Fill in the api keys for the different services here. Refer Readme.txt for details on how to obtain the key for the services.
const utility::string_t		casalens_creds::	keyname_events			= U("app_key"	);
const utility::string_t		casalens_creds::	keyname_movies			= U("api_key"	);
const utility::string_t		casalens_creds::	keyname_images			= U("username"	);
const utility::string_t		casalens_creds::	keyname_bmaps			= U("key"		);
const utility::string_t		casalens_creds::	key_events				= U(""			);
const utility::string_t		casalens_creds::	key_movies				= U(""			);
const utility::string_t		casalens_creds::	key_images				= U(""			);
const utility::string_t		casalens_creds::	key_bmaps				= U(""			);

const utility::string_t		CasaLens::			json_key_events			= U("events"	);
const utility::string_t		CasaLens::			json_key_movies			= U("movies"	);
const utility::string_t		CasaLens::			json_key_weather		= U("weather"	);
const utility::string_t		CasaLens::			json_key_images			= U("images"	);
const utility::string_t		CasaLens::			json_key_error			= U("error"		);

							CasaLens::			CasaLens				(utility::string_t url)	: m_listener(url) {
    m_listener.support(web::http::methods::GET , std::bind(&CasaLens::handle_get , this, std::placeholders::_1));
    m_listener.support(web::http::methods::POST, std::bind(&CasaLens::handle_post, this, std::placeholders::_1));

    m_htmlcontentmap[U("/"						)]	= std::make_tuple(U("AppCode.html"			), U("text/html"				));
    m_htmlcontentmap[U("/js/default.js"			)]	= std::make_tuple(U("js/default.js"			), U("application/javascript"	));
    m_htmlcontentmap[U("/css/default.css"		)]	= std::make_tuple(U("css/default.css"		), U("text/css"					));
    m_htmlcontentmap[U("/image/logo.png"		)]	= std::make_tuple(U("image/logo.png"		), U("application/octet-stream"	));
    m_htmlcontentmap[U("/image/bing-logo.jpg"	)]	= std::make_tuple(U("image/bing-logo.jpg"	), U("application/octet-stream"	));
    m_htmlcontentmap[U("/image/wall.jpg"		)]	= std::make_tuple(U("image/wall.jpg"		), U("application/octet-stream"	));
}


// Handler to process HTTP::GET requests.
// Replies to the request with data.
void						CasaLens::			handle_get				(web::http::http_request message) {    
    auto												path					= message.relative_uri().path();
    auto												content_data			= m_htmlcontentmap.find(path);
    if (content_data == m_htmlcontentmap.end()) {
        message.reply(web::http::status_codes::NotFound, U("Path not found")).then([](pplx::task<void> t) { handle_error(t); });
        return;
    }

    auto												file_name				= std::get<0>(content_data->second);
    auto												content_type			= std::get<1>(content_data->second);
    concurrency::streams::fstream::open_istream(file_name, std::ios::in).then([=](concurrency::streams::istream is) {
        message.reply(web::http::status_codes::OK, is, content_type).then([](pplx::task<void> t) { handle_error(t); });
    }).then([=](pplx::task<void>& t) {
        try {
            t.get();
        }
        catch(...) {																						// opening the file (open_istream) failed.
            message.reply(web::http::status_codes::InternalError).then([](pplx::task<void> t) { handle_error(t); });	// Reply with an error.
        }																									
    });
}

// Respond to HTTP::POST messages
// Post data will contain the postal code or location string.
// Aggregate location data from different services and reply to the POST request.
void						CasaLens::			handle_post				(web::http::http_request message) { 
    auto												path					= message.relative_uri().path();
    if (0 == path.compare(U("/"))) {
        message.extract_string().then([=](const utility::string_t& location) {
            get_data(message, location); 
        }).then([](pplx::task<void> t) { handle_error(t); });
    }
    else
        message.reply(web::http::status_codes::NotFound, U("Path not found")).then([](pplx::task<void> t) { handle_error(t); });
}

#ifdef _WIN32
int												wmain					(int argc, wchar_t *args[])	{
#else
int												main					(int argc, char *args[])	{
#endif
    if(argc != 2) {
        wprintf(U("Usage: casalens.exe port\n"));
        return -1;
    }
    std::wstring										address					= U("http://localhost:");
    address.append(args[1]);

	CasaLens											listener				= {address};
    listener.open().wait();

    std::wcout << utility::string_t(U("Listening for requests at: ")) << address << std::endl;

    std::string											line;
    std::wcout << U("Hit Enter to close the listener.");
    std::getline(std::cin, line);

    listener.close().wait();
    return 0;
}