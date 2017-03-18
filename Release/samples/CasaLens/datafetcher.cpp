// Listener code: Given a location/postal code, the listener queries different services for weather, things to do: events, movie and pictures and returns it to the client.
//
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
#include "casalens.h"

#include "cpprest/http_client.h"

#include <locale>

// In case of any failure to fetch data from a service, create a json object with "error" key and value containing the exception details.
pplx::task<web::json::value> CasaLens::handle_exception(pplx::task<web::json::value>& t, const utility::string_t& field_name) {
    try {
        t.get();
    }
    catch(const std::exception& ex) {
        web::json::value error_json				= web::json::value::object();
        error_json[field_name]					= web::json::value::object();
        error_json[field_name][error_json_key]	= web::json::value::string(utility::conversions::to_string_t(ex.what()));
        return pplx::task_from_result<web::json::value>(error_json);
    }

    return t;
}

// Given postal code, query eventful service and obtain num_events upcoming events at that location.
// Returns a task of json::value of event data
// JSON result Format:
// {"events":["title":"event1","url":"http://event1","starttime":"ddmmyy",..]}
pplx::task<web::json::value> CasaLens::get_events(const utility::string_t& postal_code) {
    web::http::uri_builder ub(casalens_creds::events_url + postal_code);

    ub.append_query(casalens_creds::events_keyname, casalens_creds::events_key);
    auto event_url = ub.to_string();

	web::http::client::http_client ev_client(event_url);
    return ev_client.request(web::http::methods::GET, event_url).then([](web::http::http_response resp)
    {
        return resp.extract_json();
    }).then([](web::json::value event_json)
    {
        web::json::value event_result_node = web::json::value::object();

        if (!event_json[U("events")][U("event")].is_null()) {
            event_result_node[U("events")] = web::json::value::array();

            int i = 0;
            for(auto& iter : event_json[U("events")][U("event")].as_array()) {
                const auto &event = iter;

				utility::string_t					key;
				web::json::object::const_iterator	objectIterator;
				key	= U("title"			);	objectIterator = event.as_object().find(key);	if (objectIterator == event.as_object().end()) { throw web::json::json_exception(key.c_str()); }	event_result_node[events_json_key][i][U("title"			)] = objectIterator->second;
				key	= U("url"			);	objectIterator = event.as_object().find(key);	if (objectIterator == event.as_object().end()) { throw web::json::json_exception(key.c_str()); }	event_result_node[events_json_key][i][U("url"			)] = objectIterator->second;
				key	= U("start_time"	);	objectIterator = event.as_object().find(key);	if (objectIterator == event.as_object().end()) { throw web::json::json_exception(key.c_str()); }	event_result_node[events_json_key][i][U("start_time"	)] = objectIterator->second;
				key	= U("description"	);	objectIterator = event.as_object().find(key);	if (objectIterator == event.as_object().end()) { throw web::json::json_exception(key.c_str()); }	event_result_node[events_json_key][i][U("description"	)] = objectIterator->second;
				key	= U("venue_address"	);	objectIterator = event.as_object().find(key);	if (objectIterator == event.as_object().end()) { throw web::json::json_exception(key.c_str()); }	event_result_node[events_json_key][i][U("venue_address"	)] = objectIterator->second;
				key	= U("city_name"		);	objectIterator = event.as_object().find(key);	if (objectIterator == event.as_object().end()) { throw web::json::json_exception(key.c_str()); }	event_result_node[events_json_key][i][U("city_name"		)] = objectIterator->second;
                if (i++ > num_events)
                    break;
            }
        }
        else
        { // Event data is null, we hit an error.
            event_result_node[events_json_key]					= web::json::value::object();
            event_result_node[events_json_key][error_json_key]	= event_json[U("events")][U("description")];
        }

        return event_result_node;
    }).then([=](pplx::task<web::json::value> t)
    {
        return handle_exception(t, events_json_key);
    });
}

// Query openweathermap API and obtain weather information at the given location.
// Returns a task of json::value containing the weather data.
// Format:
// {"weather":{"temperature":"49","pressure":"xxx"..}}
pplx::task<web::json::value> CasaLens::get_weather(const utility::string_t& postal_code) {
    utility::string_t weather_url(casalens_creds::weather_url);

    web::http::uri_builder ub_weather(weather_url.append(postal_code));
    ub_weather.append_query(U("units"), U("imperial"));

    web::http::client::http_client weather_client(ub_weather.to_string());
    return weather_client.request(web::http::methods::GET).then([](web::http::http_response resp)
    {
        return resp.extract_string(); 
    }).then([](utility::string_t weather_str)
    {
        web::json::value weather_json = web::json::value::parse(weather_str);
        auto& j = weather_json[U("list")][0][U("main")];

        web::json::value weather_result_node = web::json::value::object();
        weather_result_node[weather_json_key] = web::json::value::object();
        auto& w = weather_result_node[U("weather")];
        w[U("temperature")] = j[U("temp")];
        w[U("pressure")] = j[U("pressure")];
        w[U("temp_min")] = j[U("temp_min")];
        w[U("temp_max")] = j[U("temp_max")];
        w[U("image")] = web::json::value::string(U("http://openweathermap.org/img/w/") + weather_json[U("list")][0][U("weather")][0][U("icon")].as_string() + U(".png"));
        w[U("description")] = weather_json[U("list")][0][U("weather")][0][U("description")];
        return weather_result_node;
    }).then([=](pplx::task<web::json::value> t)
    {
        return handle_exception(t, weather_json_key);
    });
}

// Query bing images to fetch some image URLs of the given location
// Returns a task of json::Value
// JSON result format:
// {"images":["url1", "url2"]}
pplx::task<web::json::value> CasaLens::get_pictures(const utility::string_t& location, const utility::string_t& count) {
    web::http::client::http_client_config config;
    web::http::client::credentials cred(casalens_creds::images_keyname, casalens_creds::images_key);
    config.set_credentials(cred);
    utility::string_t bing_url(casalens_creds::images_url);
    web::http::uri_builder ub_bing(bing_url);
    ub_bing.append_query(U("Query"), U("'") + location + U("'"));
    ub_bing.append_query(U("$top"), count);
    ub_bing.append_query(U("ImageFilters"), U("'Size:Medium'"));

    web::http::client::http_client bing_client(ub_bing.to_string(), config);
    return bing_client.request(web::http::methods::GET).then([](web::http::http_response resp)
    {
        return resp.extract_json();
    }).then([](web::json::value image_json)
    {
        web::json::value image_result_node = web::json::value::object();
        image_result_node[images_json_key] = web::json::value::array();

        int i = 0;
        for(auto& val : image_json[U("d")][U("results")].as_object()) {
            const web::json::object &image = val.second.as_object();
            auto iMediaUrl = image.find(U("MediaUrl"));
            if (iMediaUrl == image.end()) {
                throw web::json::json_exception(U("MediaUrl key not found"));
            }
            image_result_node[images_json_key][i] = iMediaUrl->second;
            if (i++ > num_images)
                break;
        }
        return image_result_node;
    }).then([=](pplx::task<web::json::value> t)
    {
        return handle_exception(t, images_json_key);
    });
}

// Get the current date
std::wstring CasaLens::get_date() {
    time_t t = time(0);
    struct tm now;
    std::wostringstream date;
    if (0 == localtime_s(&now, &t)) {
        date << (now.tm_year + 1900) << '-' 
            << (now.tm_mon + 1) << '-'
            <<  now.tm_mday;
    }
    return date.str();
}

// Query tmsapi and fetch current movie showtimes at local theaters, for the given postal code
// Also quert bing images for movie posters 
// Returns a task of JSON value
// JSON result format: 
// "movies":[{"title":"abc","theatre":[{"name":"theater1","datetime":["dd-mm-yyThh:mm"]},{"name":"theater2","datetime":["ddmmyy"]}],"poster":"img-url"}}]}..
pplx::task<web::json::value> CasaLens::get_movies(const utility::string_t& postal_code) {
    web::http::uri_builder ub_movie(casalens_creds::movies_url);

    ub_movie.append_query(U("startDate"), get_date());
    ub_movie.append_query(U("zip"), postal_code);
    ub_movie.append_query(casalens_creds::movies_keyname, casalens_creds::movies_key);
    ub_movie.append_query(U("imageSize"), U("Sm"));

    web::http::client::http_client tms_client(ub_movie.to_string());
    return tms_client.request(web::http::methods::GET).then([](web::http::http_response resp)
    {
        return resp.extract_json();
    }).then([](web::json::value movie_json)
    {
        // From the data obtained from tmsapi, construct movie JSON value to be sent to the client
        // We will collect data for 5 movies, and 5 showtime info per movie.
        web::json::value movie_result_node = web::json::value::object();

        if (movie_json.size()) {
            auto temp = web::json::value::array();
            int i = 0;
            for(auto iter = movie_json.as_array().cbegin(); iter != movie_json.as_array().cend() && i < num_movies; iter++, i++) {
                web::json::object::const_iterator 
				objectIterator	= iter->as_object().find(U("showtimes"	));	if (objectIterator == iter->as_object().end()) { throw web::json::json_exception(U("showtimes key not found")); } const web::json::value & showtimes	= objectIterator->second;
                objectIterator	= iter->as_object().find(U("title"		));	if (objectIterator == iter->as_object().end()) { throw web::json::json_exception(U("title key not found"	)); } temp[i][U("title")]					= objectIterator->second;

				utility::string_t	current_theater;
				int					j					= 0;
                int					showtime_index		= 0;
                int					theater_index		= -1;
                for (auto iter2 = showtimes.as_array().cbegin(); iter2 != showtimes.as_array().cend() &&  j < num_movies; iter2++,j++) {
                    objectIterator	= iter2->as_object().find(U("theatre"	));	if (objectIterator == iter2->as_object().end()) { throw web::json::json_exception(U("theatre key not found"));	} const web::json::object & theatre		= objectIterator->second.as_object();
                    objectIterator	= theatre			.find(U("name"		));	if (objectIterator == theatre.end())			{ throw web::json::json_exception(U("name key not found"));		} const utility::string_t theater		= objectIterator->second.as_string();
                    if (0 != theater.compare(current_theater)) {	// new theater
                        temp[i][U("theatre")][++theater_index][U("name")] = web::json::value::string(theater); // Add theater entry
                        showtime_index		= 0;
                        current_theater		= theater;
                    }
                    objectIterator	= iter2->as_object().find(U("dateTime"	));	if (objectIterator == iter2->as_object().end()) { throw web::json::json_exception(U("dateTime key not found")); } temp[i][U("theatre")][theater_index][U("datetime")][showtime_index++] = objectIterator->second; // Update the showtime for same theater
                }
            }
            movie_result_node[movies_json_key] = std::move(temp);
        }
        else {
            movie_result_node[movies_json_key] = web::json::value::object();
            movie_result_node[movies_json_key][error_json_key] = web::json::value::string(U("Failed to fetch movie data"));
        }

        return pplx::task_from_result(movie_result_node);
    }).then([=](web::json::value movie_result)
    {
        try {
            // For every movie, obtain movie poster URL from bing
            std::vector<utility::string_t> movie_list;
            std::vector<pplx::task<web::json::value>> poster_tasks;
            auto date = get_date();
            std::wstring year = date.substr(0, date.find(U("-"))); 

            for(auto& iter : movie_result[movies_json_key].as_object()) {
                auto title			= iter.second[U("title")].as_string();
                auto searchStr		= title + U(" ") + year + U(" new movie poster");
                poster_tasks.push_back(get_pictures(searchStr, U("1")));
                movie_list	.push_back(title);
            }

            pplx::when_all(poster_tasks.begin(), poster_tasks.end()).wait();
            web::json::value		resp_data			= web::json::value::object();

            for(unsigned int i = 0; i < poster_tasks.size(); i++) {
                auto					jval				= poster_tasks[i].get();
                auto					poster_url			= jval.as_array().begin()[0];
                movie_result[movies_json_key][i][U("poster")] = poster_url;
            }
        }
        catch(...) {	// Ignore any failure in fetching movie posters. Just return
        }				// the movie data to the client.

        return pplx::task_from_result(movie_result);
    }).then([=](pplx::task<web::json::value> t) {
        return handle_exception(t, movies_json_key);
    });
}

bool CasaLens::is_number(const std::string& s) {
    return !s.empty() && std::find_if(s.begin(), 
        s.end(), [](char c) { return !std::isdigit(c, std::locale()); }) == s.end();
}

void CasaLens::fetch_data(web::http::http_request message, const std::wstring& postal_code, const std::wstring& location) {
    web::json::value resp_data;

    try {
        m_rwlock.lock_read();
        resp_data = m_data[postal_code];
        m_rwlock.unlock();

        if (resp_data.is_null()) {
            std::vector<pplx::task<web::json::value>> tasks;

            tasks.push_back(get_events(postal_code));
            tasks.push_back(get_weather(postal_code));
            tasks.push_back(get_pictures(location, U("4")));
            tasks.push_back(get_movies(postal_code));

            pplx::when_all(tasks.begin(), tasks.end()).wait();
            resp_data = web::json::value::object();

            for(auto& iter:tasks) {
                auto jval = iter.get();
                auto key = jval.as_object().begin()->first;
                resp_data[key] = jval.as_object().begin()->second;
            }

            m_rwlock.lock();
            m_data[postal_code] = resp_data;
            m_rwlock.unlock();
        }

        // Reply with the aggregated JSON data
        message.reply(web::http::status_codes::OK, resp_data).then([](pplx::task<void> t) { handle_error(t); });
    }
    catch(...) {
        message.reply(web::http::status_codes::InternalError).then([](pplx::task<void> t) { handle_error(t); });
    }
}

// Check if the input text is a number or string.
// If string => city name, use bing maps API to obtain the postal code for that city
// number => postal code, use google maps API to obtain city name (location data) for that postal code.
// then call fetch_data to query different services, aggregate movie, images, events, weather etc for that city and respond to the request.
void CasaLens::get_data(web::http::http_request message, const std::wstring& input_text) {
    if (!is_number(utility::conversions::to_utf8string(input_text))) {
        std::wstring bing_maps_url(casalens_creds::bmaps_url);
        web::http::uri_builder maps_builder;
        maps_builder.append_query(U("locality"), input_text);
        maps_builder.append_query(casalens_creds::bmaps_keyname, casalens_creds::bmaps_key);
        auto s = maps_builder.to_string();
        web::http::client::http_client bing_client(bing_maps_url);
        bing_client.request(web::http::methods::GET, s).then([=](web::http::http_response resp) {
            return resp.extract_json();
        }).then([=](web::json::value& maps_result) mutable {
            auto coordinates = maps_result[U("resourceSets")][0][U("resources")][0][U("point")];
            auto lattitude = coordinates[U("coordinates")][0].serialize();
            auto longitude = coordinates[U("coordinates")][1].serialize();
            web::http::uri_builder ub;
            ub.append_path(lattitude + U(",") + longitude).append_query(casalens_creds::bmaps_keyname, casalens_creds::bmaps_key);
            auto s2 = ub.to_string();
            return bing_client.request(web::http::methods::GET, s2);
        }).then([](web::http::http_response resp) {
            return resp.extract_json();
        }).then([=](web::json::value& maps_result) {
            auto postal_code = maps_result[U("resourceSets")][0][U("resources")][0][U("address")][U("postalCode")].as_string();
            fetch_data(message, postal_code, input_text);            
        }).then([=](pplx::task<void> t) {
            try {
                t.get();
            }
            catch(...) {
                message.reply(web::http::status_codes::InternalError, U("Failed to fetch the postal code"));
            }
        });
    }
    else { // get location from postal code
        web::http::client::http_client client(casalens_creds::gmaps_url);

        web::http::uri_builder ub;
        ub.append_query(U("address"), input_text);
        ub.append_query(U("sensor"), U("false"));

        client.request(web::http::methods::GET, ub.to_string()).then([](web::http::http_response resp) {
            return resp.extract_json();
        }).then([=](web::json::value jval) {
            auto locationstr =  jval[U("results")][0][U("address_components")][1][U("long_name")].as_string();
            fetch_data(message, input_text, locationstr);
        }).then([=](pplx::task<void> t) {
            try {
                t.get();
            }
            catch(...) {
                message.reply(web::http::status_codes::InternalError, U("Failed to fetch the location from postal code"));
            }
        });
    }
    return;
}
