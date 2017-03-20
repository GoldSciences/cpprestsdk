// Defines the entry point for the console application.
//
// Copyright (C) Microsoft. All rights reserved.
#include <stdio.h>

#ifdef _WIN32
#include <SDKDDKVer.h>	// Including SDKDDKVer.h defines the highest available Windows platform.
						// If you wish to build your application for a previous Windows platform, include WinSDKVer.h and set the _WIN32_WINNT macro to the platform you wish to support before including SDKDDKVer.h.
#include <tchar.h>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#define NOMINMAX
#include <windows.h>
#include <objbase.h>
#include <winsock2.h>

// ws2tcpip.h - isn't warning clean.
#pragma warning(push)
#pragma warning(disable : 6386)
#include <ws2tcpip.h>
#pragma warning(pop)

#include <iphlpapi.h>
#endif

#include "cpprest/http_client.h"
#include "../BlackJack_Server/messagetypes.h"

#ifdef _WIN32
# define iequals(x, y) (_stricmp((x), (y))==0)
#else
# define iequals(x, y) boost::iequals((x), (y))
#endif

web::http::http_response				CheckResponse						(const std::string &url, const web::http::http_response &response)						{
    ucout << response.to_string() << std::endl;
    return response;
}

web::http::http_response				CheckResponse						(const std::string &url, const web::http::http_response &response, bool &refresh)		{
    ucout << response.to_string() << std::endl;
    BJPutResponse								answer								= BJPutResponse::FromJSON(response.extract_json().get());
    refresh									= answer.Status == ST_Refresh;
    return response;
}

void									PrintResult							(BJHandResult result)																	{
    switch (result) {
    case HR_PlayerBlackJack	: ucout << "Black Jack"		; break;
    case HR_PlayerWin		: ucout << "Player wins"	; break;
    case HR_ComputerWin		: ucout << "Computer Wins"	; break;
    case HR_Push			: ucout << "Push"			; break;
    }
}

void									PrintCard							(const Card &card)																		{
    switch (card.value) {
    case CV_King			: ucout << "K"				; break;
    case CV_Queen			: ucout << "Q"				; break;
    case CV_Jack			: ucout << "J"				; break;
    case CV_Ace				: ucout << "A"				; break;
    default					: ucout << (int)card.value	; break;
    }
    switch (card.suit) {
    case CS_Club			: ucout << "C"; break;
    case CS_Spade			: ucout << "S"; break;
    case CS_Heart			: ucout << "H"; break;
    case CS_Diamond			: ucout << "D"; break;
    }
}

void									PrintHand							(bool suppress_bet, const BJHand &hand)													{
    if (!suppress_bet) {
        if ( hand.insurance > 0 )
            ucout << "Bet: " << hand.bet << " Insurance: " << hand.insurance << " Hand: ";
        else
            ucout << "Bet: " << hand.bet << " Hand: ";
    }
    for (auto iter = hand.cards.begin(); iter != hand.cards.end(); iter++)
        PrintCard(*iter); ucout << " ";

	PrintResult(hand.result);
}

void									PrintTable							(const web::http::http_response &response, bool &refresh)								{
    BJHand										hand;
    refresh									= false;

    if ( response.status_code() == web::http::status_codes::OK ) {
        if ( response.headers().content_type() == U("application/json") ) {
            BJPutResponse								answer								= BJPutResponse::FromJSON(response.extract_json().get());
            web::json::value							players								= answer.Data[PLAYERS];

            refresh									= answer.Status == ST_Refresh;

            for (auto iter = players.as_array().begin(); iter != players.as_array().end(); ++iter) {
                auto										& player							= *iter;

                web::json::value							name								= player[NAME];
                web::json::value							bet									= player[BALANCE];
				bool										suppressMoney						= iter == players.as_array().begin();
                if ( suppressMoney )
                    ucout << "'" << name.as_string() << "'" ;
                else
                    ucout << "'" << name.as_string() << "' Balance = $" << bet.as_double() << " ";

                PrintHand(suppressMoney, BJHand::FromJSON(player[HAND].as_object()));
                ucout << std::endl;
            }

            switch ( answer.Status ) {
            case ST_PlaceBet: ucout << "Place your bet!\n"	; break;
            case ST_YourTurn: ucout << "Your turn!\n"		; break;
            }
        }
    }
}

// Entry point for the blackjack client.
// Arguments: BlackJack_Client.exe <port>
// If port is not specified, client will assume that the server is listening on port 34568
#ifdef _WIN32
int										wmain								(int argc, wchar_t	*argv[])															{
#else																																								
int										main								(int argc, char		*argv[])															{
#endif
    utility::string_t							port								= U("34568");
    if(argc == 2)
        port									= argv[1];

    utility::string_t							address								= U("http://localhost:");
    address.append(port);

    web::http::uri								uri									= web::http::uri(address);

    web::http::client::http_client				bjDealer							(web::http::uri_builder(uri).append_path(U("/blackjack/dealer")).to_uri());

    utility::string_t							userName;
    utility::string_t							table;

    web::json::value							availableTables						= web::json::value::array();
    bool										was_refresh							= false;

    while (true) {
        while ( was_refresh ) {
            was_refresh								= false;
            utility::ostringstream_t					buf;
            buf << table << U("?request=refresh&name=") << userName;
            PrintTable(CheckResponse("blackjack/dealer", bjDealer.request(web::http::methods::PUT, buf.str()).get()), was_refresh);
        }

        std::string									method;
        ucout		<< "Enter method:";
        std::cin	>> method;

        if ( iequals(method.c_str(), "quit") ) {
            if ( !userName.empty() && !table.empty() ) {
                utility::ostringstream_t					buf;
                buf << table << U("?name=") << userName;
                CheckResponse("blackjack/dealer", bjDealer.request(web::http::methods::DEL, buf.str()).get());
            }
            break;
        }

        if ( iequals(method.c_str(), "name") ) {
            ucout	<< "Enter user name:";
            ucin	>> userName;
        }
        else if ( iequals(method.c_str(), "join") ) {
            ucout	<< "Enter table name:";
            ucin	>> table;

            if ( userName.empty() ) { ucout << "Must have a name first!\n"; continue; }

            utility::ostringstream_t					buf;
            buf << table << U("?name=") << userName;
            CheckResponse("blackjack/dealer", bjDealer.request(web::http::methods::POST, buf.str()).get(), was_refresh);
        }
        else if(iequals(method.c_str(), "hit"	)
			||	iequals(method.c_str(), "stay"	)
			||	iequals(method.c_str(), "double") 
			)
        {
            utility::ostringstream_t					buf;
            buf << table << U("?request=") << utility::conversions::to_string_t(method) << U("&name=") << userName;
            PrintTable(CheckResponse("blackjack/dealer", bjDealer.request(web::http::methods::PUT, buf.str()).get()), was_refresh);
        }
        else if(iequals(method.c_str(), "bet") 
			||	iequals(method.c_str(), "insure") 
			)
        {
            utility::string_t							bet;
            ucout	<< "Enter bet:";
            ucin	>> bet;

            if ( userName.empty() ) { ucout << "Must have a name first!\n"; continue; }

            utility::ostringstream_t					buf;
            buf		<< table << U("?request=") << utility::conversions::to_string_t(method) << U("&name=") << userName << U("&amount=") << bet;
            PrintTable(CheckResponse("blackjack/dealer", bjDealer.request(web::http::methods::PUT, buf.str()).get()), was_refresh);
        }
        else if ( iequals(method.c_str(), "newtbl") )
            CheckResponse("blackjack/dealer", bjDealer.request(web::http::methods::POST).get(), was_refresh);
        else if ( iequals(method.c_str(), "leave") ) {
            ucout	<< "Enter table:";
            ucin	>> table;

            if ( userName.empty() ) { ucout << "Must have a name first!\n"; continue; }

            utility::ostringstream_t					buf;
            buf		<< table << U("?name=") << userName;
            CheckResponse("blackjack/dealer", bjDealer.request(web::http::methods::DEL, buf.str()).get(), was_refresh);
        }
        else if ( iequals(method.c_str(), "list") ) {
            was_refresh								= false;
            web::http::http_response					response							= CheckResponse("blackjack/dealer", bjDealer.request(web::http::methods::GET).get());

            if ( response.status_code() == web::http::status_codes::OK ) {
                availableTables							= response.extract_json().get();
                for (auto iter = availableTables.as_array().begin(); iter != availableTables.as_array().end(); ++iter) {
                    BJTable										bj_table							= BJTable::FromJSON(iter->as_object());
                    web::json::value							id									= web::json::value::number(bj_table.Id);

                    ucout << "table " << bj_table.Id << ": {capacity: " << (long unsigned int)bj_table.Capacity << " no. players: " << (long unsigned int)bj_table.Players.size() << " }\n";
                }
                ucout << std::endl;
            }
        }
        else {
            ucout << utility::conversions::to_string_t(method) << ": not understood\n";
        }
    }

    return 0;
}

