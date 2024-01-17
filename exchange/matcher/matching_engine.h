#pragma once

#include "common/lf_queue.h"
#include "common/macros.h"
#include "order_server/client_response.h"
#include "order_server/client_request.h"
#include "market_data/market_update.h"
#include "common/thread_utils.h"
/*
Not read yet
*/
#include "me_order_book.h"

namespace Exchange
{

    class MatchingEngine final
    {
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

    public:
        MatchingEngine(ClientRequestLFQueue *client_requests,
                       ClientResponseLFQueue *client_responses,
                       MEMarketUpdateLFQueue *market_updates);
        ~MatchingEngine();
        auto start() -> void;
        auto stop() -> void;

        /*
        noexcept : if the function throws an error, it isn't called
        */
        auto run() noexcept
        {
            logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
            while (run_)
            {
                const auto me_client_request = incoming_requests_->getNextToRead();
                if (LIKELY(me_client_request))
                {
                    logger_.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), me_client_request->toString());
                    processClientRequest(me_client_request);
                    incoming_requests_->updateReadIndex();
                }
            }
        }

        /*
        processClientRequest is a dispatcher function that determines the appropriate action to take based on the type
        of client request (new order or order cancellation) and delegates the execution of that action to the corresponding
        order book. If an invalid client request type is encountered, it logs a fatal error.
        */
        auto processClientRequest(const MEClientRequest *client_request) noexcept
        {
            auto order_book = ticker_order_book_[client_request->ticker_id_];
            /*
            It retrieves the order book associated with the provided ticker_id_. The ticker_order_book_
            is assumed to be some kind of mapping or array holding order books for different financial
            instruments
            */

            switch (client_request->type_)
            {
            case ClientRequestType::NEW:
            {
                order_book->add(client_request->client_id_, client_request->order_id_, client_request->ticker_id_,
                                client_request->side_, client_request->price_, client_request->qty_);
            }
            break;
                /*
                In the absence of break statements, the control flow would "fall through" to the next case.
                The break statements are crucial here because they prevent this fall-through behavior. They
                ensure that only the code block associated with the matched case is executed, and the control
                flow exits the switch statement. The break statements also prevent unnecessary execution of
                subsequent cases, improving the efficiency of the code.
                */

            case ClientRequestType::CANCEL:
            {
                order_book->cancel(client_request->client_id_, client_request->order_id_, client_request->ticker_id_);
            }
            break;
            default:
            {
                FATAL("Received invalid client-request-type : " + clientRequestTypeToString(client_request->type_));
            }
            break;
            }
        }

        /*
        We will also define a method in the same class that the limit order book will use to publish order responses through MEClientResponse
        messages. This simply writes the response to the outgoing_ogw_responses_ lock-free queue and advances the writer index.
        It does that by finding the next valid index to write the MEClientResponse message to by calling the LFQueue::getNextToWriteTo() method
        , moving the data into that slot, and updating the next write index by calling the LFQueue::updateWriteIndex() method:
        */
        auto sendClientResponse(const MEClientResponse *client_response) noexcept
        {
            logger_.log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), client_response->toString());
            auto next_write = outgoing_ogw_responses_->getNextToWriteTo();
            *next_write = std::move(*client_response);
            outgoing_ogw_responses_->updateWriteIndex();
        }
        /*
        The sendMarketUpdate() method is used by the limit order book to publish market data updates through the MEMarketUpdate structure. It
        simply writes to the outgoing_md_updates_ lock-free queue and advances the writer. It does this exactly the same way we saw before – by
        calling the getNextToWriteTo() method, writing the MEMarketUpdate message to that slot, and updating the next write index using updateWriteIndex()
        */
        auto sendMarketUpdate(const MEMarketUpdate *market_update) noexcept
        {
            logger_.log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), market_update->toString());
            auto next_write = outgoing_md_updates_->getNextToWriteTo();
            *next_write = *market_update;
            outgoing_md_updates_->updateWriteIndex();
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
    };
}
