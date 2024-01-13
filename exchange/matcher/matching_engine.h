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

namespace Exchange
{

    class MatchingEngine Final
    {
    public:
        MatchingEngine(ClientLFQueue *client_requests,
                       ClientLFQueue *client_responses, MEMarketUpdateLFQueue *market_updates);
        ~MatchingEngine();
        auto start() -> void;
        auto stop() -> void;
        
        /*
        noexcept : if the function throws an error, it isn't called
        */
        auto run() noexcept{
            logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
            while(run_){
                const auto me_client_request = incoming_requests_ -> getNextToRead();
                if(LIKELY(me_client_request)){
                    
                }
            }
        }

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

        MatchingEngine &operator=(const MatchingEngine &) = delete;
        MatchingEngine &operator=(const MatchingEngine &&) = delete;

    private:
        OrderBookHashMap ticker_order_book_;
        ClientRequestLFQueue *incoming_requests_ = nullptr;
        ClientResponseLFQueue *outgoing_ogw_responses_ = nullptr;
        MEMarketUpdateLFQueue *outgoing_md_updates_ = nullptr;
        volatile bool run_ = false;
        /*
        The volatile keyword is used here to indicate that the value of run_ might
        change at any time without any action being taken by the code that the compiler
        finds nearby. This is often used when the variable can be modified by hardware,
        other threads, or interrupt service routines.

        The significance of volatile in more detail:

        Prevents Compiler Optimization: When a variable is declared as volatile, the compiler is instructed not
        to perform certain optimizations that might assume the variable's value doesn't change outside the
        current code path. This is crucial for variables that can be modified by external entities like hardware.

        Forces Every Access: Each time the variable is accessed, the compiler generates code to actually read
        the value from memory and not rely on any cached values. Similarly, when the variable is modified, the
        new value is written directly to memory.

        Use Cases: volatile is often used in situations where a variable is shared among different parts
        of a program or between a program and an external entity (like hardware). For example, a flag indicating
        whether a program should continue running (as in your run_ example) might be modified by an interrupt
        service routine or another thread.

        Here, the use of volatile indicates to the compiler that it should not make assumptions about
        the stability of the run_ variable and should always fetch its current value from memory. This is
        crucial in scenarios where the variable can be changed asynchronously, outside the normal flow of
        the program.
        */
        std::string time_str_;
        Logger logger_;
    };
}
