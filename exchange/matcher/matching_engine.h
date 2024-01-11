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
            MatchingEngine(const MatchingEngine &) = delete;
            MatchingEngine(const MatchingEngine &&) = delete;
            MatchingEngine &operator = (const MatchingEngine &) = delete ;
            MatchingEngine &operator = (const MatchingEngine &&)= delete;
    };
}
