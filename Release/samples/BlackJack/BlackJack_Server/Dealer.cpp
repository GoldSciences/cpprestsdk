#include "Dealer.h"

std::map<utility::string_t, std::shared_ptr<BJTable>>	Dealer::s_tables				= {};
int														Dealer::nextId					= 1;

// A GET of the dealer resource produces a list of existing tables.
void									Dealer::handle_get								(web::http::http_request message)				{
    ucout <<  message.to_string() << std::endl;

    auto														paths							= web::http::uri::split_path(web::http::uri::decode(message.relative_uri().path()));
    if (paths.empty()) {
        message.reply(web::http::status_codes::OK, TablesAsJSON(U("Available Tables"), s_tables));
        return;
    }

    utility::string_t											wtable_id						= paths[0];
    const utility::string_t										table_id						= wtable_id;

    // Get information on a specific table.
    auto														found							= s_tables.find(table_id);
    if (found == s_tables.end())
        message.reply(web::http::status_codes::NotFound);
    else
        message.reply(web::http::status_codes::OK, found->second->AsJSON());
};

// A POST of the dealer resource creates a new table and returns a resource for that table.
void									Dealer::handle_post								(web::http::http_request message)				{
    ucout <<  message.to_string() << std::endl;

    auto														paths							= web::http::uri::split_path(web::http::uri::decode(message.relative_uri().path()));
    
    if (paths.empty()) {
        utility::ostringstream_t									nextIdString;
        nextIdString << nextId;

        std::shared_ptr<DealerTable>								tbl								= std::make_shared<DealerTable>(nextId, 8, 6);
        s_tables[nextIdString.str()]							= tbl;
        nextId													+= 1;

        message.reply(web::http::status_codes::OK, BJPutResponse(ST_PlaceBet, tbl->AsJSON()).AsJSON());
        return;
    }
    utility::string_t											wtable_id						= paths[0];
    const utility::string_t										table_id						= wtable_id;

    // Join an existing table.
    auto														found							= s_tables.find(table_id);
    if (found == s_tables.end()) {
        message.reply(web::http::status_codes::NotFound);
        return;
    }

    auto														table							= std::static_pointer_cast<DealerTable>(found->second);
    if ( table->Players.size() < table->Capacity ) {
        std::map<utility::string_t, utility::string_t>				query							= web::http::uri::split_query(web::http::uri::decode(message.request_uri().query()));
        auto														cntEntry						= query.find(QUERY_NAME);

        if (cntEntry != query.end() && !cntEntry->second.empty()) {
            table->AddPlayer(Player(cntEntry->second));
            message.reply(web::http::status_codes::OK, BJPutResponse(ST_PlaceBet, table->AsJSON()).AsJSON());
        }
        else 
            message.reply(web::http::status_codes::Forbidden, U("Player name is required in query"));
    }
    else {
        utility::ostringstream_t									os;
        os << U("Table ") << table->Id << U(" is full");
        message.reply(web::http::status_codes::Forbidden, os.str());
    }
}

// A DELETE of the player resource leaves the table.
void									Dealer::handle_delete							(web::http::http_request message)				{
    ucout <<  message.to_string() << std::endl;

    auto												paths									= web::http::uri::split_path(web::http::uri::decode(message.relative_uri().path()));
    if (paths.empty()) {
        message.reply(web::http::status_codes::Forbidden, U("TableId is required."));
        return;
    }
    utility::string_t									wtable_id								= paths[0];
    const utility::string_t								table_id								= wtable_id;

    // Get information on a specific table.
    auto												found									= s_tables.find(table_id);
    if (found == s_tables.end()) {
        message.reply(web::http::status_codes::NotFound);
        return;
    }
    auto												table									= std::static_pointer_cast<DealerTable>(found->second);
    std::map<utility::string_t, utility::string_t>		query									= web::http::uri::split_query(web::http::uri::decode(message.request_uri().query()));
    auto												cntEntry								= query.find(QUERY_NAME);
    if ( cntEntry != query.end() ) {
        if ( table->RemovePlayer(cntEntry->second) )
            message.reply(web::http::status_codes::OK);
        else
            message.reply(web::http::status_codes::NotFound);
    }
    else
        message.reply(web::http::status_codes::Forbidden, U("Player name is required in query"));
}

// A PUT to a table resource makes a card request (hit / stay).
void									Dealer::handle_put								(web::http::http_request message)				{
    ucout <<  message.to_string() << std::endl;

    auto rel_uri_path	= message.relative_uri().path	();		auto	decoded_paths	= web::http::uri::decode(rel_uri_path	); auto		paths	= web::http::uri::split_path	(decoded_paths);
    auto rel_uri_query	= message.relative_uri().query	();		auto	decoded_query	= web::http::uri::decode(rel_uri_query	); auto		query	= web::http::uri::split_query	(decoded_query);
    auto												queryItr								= query.find(REQUEST);
    if (paths.empty() || queryItr == query.end())
        message.reply(web::http::status_codes::Forbidden, U("TableId and request are required."));

	utility::string_t									wtable_id								= paths[0];
    utility::string_t									request									= queryItr->second;
    const utility::string_t								table_id								= wtable_id;

    // Get information on a specific table.
    auto												found									= s_tables.find(table_id);
    if ( found == s_tables.end() )
        message.reply(web::http::status_codes::NotFound);

    auto												table									= std::static_pointer_cast<DealerTable>(found->second);

		 if ( request == BET		)	table->Bet			(message);
    else if ( request == DOUBLEDOWN	)	table->DoubleDown	(message);
    else if ( request == INSURE		)	table->Insure		(message);
    else if ( request == HIT		)	table->Hit			(message);
    else if ( request == STAY		)	table->Stay			(message);
    else if ( request == REFRESH	)	table->Wait			(message);
    else 
        message.reply(web::http::status_codes::Forbidden, U("Unrecognized request"));
}
