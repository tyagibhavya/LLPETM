#pragma once
#include "common/thread_utils.h"
#include "common/lf_queue.h"
#include "common/macros.h"
#include "order_server/client_response.h"
#include "order_server/client_request.h"
#include "market_data/market_update.h"
/*
Not read yet
*/
#include "me_order_book.h"


namespace Exchange{

    class MatchingEngine Final {
        public:
            MatchingEngine(ClientLFQueue *client_requests,
            ClientLFQueue *client_responses, MEMarketUpdateLFQueue *market_updates) ;
            ~MatchingEngine();
            auto start()->void;
            auto stop()->void;
            MatchingEngine() = delete;

            /*Copy constructor disabled*/
            MatchingEngine(const MatchingEngine &) = delete;
            /*Move constructor disabled*/
            MatchingEngine(const MatchingEngine &&) = delete;
            /*
            The move assignment operator below :
            It performs a similar role as the move constructor but is used when the object
            already exists. The move assignment operator efficiently transfers ownership of
            resources from source to destination. After the move, source is in a valid but unspecified
            state, and destination has taken ownership of the resources.
            */

            MatchingEngine &operator = (const MatchingEngine &) = delete ;
            MatchingEngine &operator = (const MatchingEngine &&)= delete;

            private:
                OrderBookHashMap ticker_order_book_;
                ClientRequestLFQueue *incoming_requests_ = nullptr;
                ClientResponseLFQueue *outgoing_ogw_responses_ =
                nullptr;
                MEMarketUpdateLFQueue *outgoing_md_updates_ = nullptr;
                volatile bool run_ = false;
                std::string time_str_;
                Logger logger_;

        };
}

#include "matching_engine.h"
namespace Exchange{
    MatchingEngine::MatchingEngine(ClientRequestLFQueue
    *client_requests, ClientResponseLFQueue
    *client_responses, MEMarketUpdateLFQueue
    *market_updates):incoming_requests_(client_requests),
        outgoing_ogw_responses_(client_responses),
        outgoing_md_updates_(market_updates),
        logger_("exchange_matching_engine.log"){
            for(size_t i = 0; i < ticker_order_book_.size(); ++i) {
                ticker_order_book_[i] = new MEOrderBook(i, &logger_, this);
    }
}