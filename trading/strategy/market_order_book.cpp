#include "market_order_book.h"

#include "trade_engine.h"

namespace Trading
{

    /*
    The constructor is straightforward and accepts the TickerId and Logger instances it will use to log.
    It initializes orders_at_price_pool_ of MarketOrdersAtPrice objects to be of the ME_MAX_PRICE_LEVELS size
    and order_pool_ of the MarketOrder objects to be of the ME_MAX_ORDER_IDS size
    */
    MarketOrderBook::MarketOrderBook(TickerId ticker_id, Logger *logger)
        : ticker_id_(ticker_id), orders_at_price_pool_(ME_MAX_PRICE_LEVELS), order_pool_(ME_MAX_ORDER_IDS), logger_(logger)
    {
    }

    /*
    Destructor resets the internal data members
    */
    MarketOrderBook::~MarketOrderBook()
    {
        logger_->log("%:% %() % OrderBook\n%\n", __FILE__, __LINE__, __FUNCTION__,
                     Common::getCurrentTimeStr(&time_str_), toString(false, true));

        trade_engine_ = nullptr;
        bids_by_price_ = asks_by_price_ = nullptr;
        oid_to_order_.fill(nullptr);
    }

    /*
    The onMarketUpdate() method is called along with the MEMarketUpdate message that needs to be
    processed. This method updates the order book from the market update, which is passed as an argument
    */
    auto MarketOrderBook::onMarketUpdate(const Exchange::MEMarketUpdate *market_update) noexcept -> void
    {   
        /*
        Before we get into the handling of the actual messages, we will first initialize a 
        bid_updated boolean flag and an ask_updated boolean flag, which will represent if BBO 
        will need to be updated because of this market update. We figure that out by checking if the 
        market update we received corresponds to side_ == Side::BUY and price_ of 
        market_update is equal to or greater than price_ of the current best bid, which we 
        fetch from the bids_by_price_->price_ variable. We do the same thing for the ask side by 
        checking for Side::SELL on market_update_->side_ and checking if price_ of market_update is less
        than or equal to the price of the best ask (asks_by_price_->price_):
        */
        const auto bid_updated = (bids_by_price_ && market_update->side_ == Side::BUY && market_update->price_ >= bids_by_price_->price_);
        const auto ask_updated = (asks_by_price_ && market_update->side_ == Side::SELL && market_update->price_ <= asks_by_price_->price_);

        switch (market_update->type_)
        {
        case Exchange::MarketUpdateType::ADD:
        {   
            /*
            We will allocate a new MarketOrder object and call the addOrder() method on it. This addOrder() method is identical to the 
            addOrder() method we built for the matching engine except it operates on MarketOrder and MarketOrdersAtPrice objects
            */
            auto order = order_pool_.allocate(market_update->order_id_, market_update->side_, market_update->price_,
                                              market_update->qty_, market_update->priority_, nullptr, nullptr);
            addOrder(order);
        }
        break;
        case Exchange::MarketUpdateType::MODIFY:
        {   
            /*
            The handling for the MarketUpdateType::MODIFY case finds the MarketOrder structure for which the 
            modified message is targeted. It then updates the qty_ attribute on that order
            */
            auto order = oid_to_order_.at(market_update->order_id_);
            order->qty_ = market_update->qty_;
        }
        break;
        case Exchange::MarketUpdateType::CANCEL:
        {   
            /*
            The handling for MarketUpdateType::CANCEL is straightforward, and it finds 
            MarketOrder, for which the cancel message is, and then calls the removeOrder() method on it
            */
            auto order = oid_to_order_.at(market_update->order_id_);
            removeOrder(order);
        }
        break;
        case Exchange::MarketUpdateType::TRADE:
        {   
            /*
            The MarketUpdateType::TRADE messages do not change the order book, so here, we simply forward
            that trade message back to the TradeEngine engine using the onTradeUpdate() method. One thing 
            to note here is that in the case of MarketUpdateType::TRADE, we simply return after calling the 
            TradeEngine::onTradeUpdate() method. This is because the trade messages do not update the order 
            book in our market data protocol, so the subsequent code after this switch case does not need 
            to be executed:
            */
            trade_engine_->onTradeUpdate(market_update, this);
            return;
        }
        break;
        case Exchange::MarketUpdateType::CLEAR:
        {   
            /*
            The MarketOrderBook class needs to handle the MarketUpdateType::CLEAR messages. It receives these 
            messages when the book needs to be cleared because we dropped a packet and are recovering from the 
            snapshot stream. All it does here is deallocate all the valid MarketOrder objects in the book and
            clear the oid_to_order_ container by setting each entry to nullptr. It then iterates through the 
            double-linked list starting with the bids_by_price_ pointer and deallocates each MarketOrdersAtPrice 
            object back to the orders_at_price_pool_ memory pool. It does the same thing with the asks_by_price_ 
            linked list and, finally, sets both bids_by_price_ and asks_by_price_ to be nullptr to represent an empty book:
            */
            for (auto &order : oid_to_order_)
            {
                if (order)
                    order_pool_.deallocate(order);
            }
            oid_to_order_.fill(nullptr);

            if (bids_by_price_)
            {
                for (auto bid = bids_by_price_->next_entry_; bid != bids_by_price_; bid = bid->next_entry_)
                    orders_at_price_pool_.deallocate(bid);
                orders_at_price_pool_.deallocate(bids_by_price_);
            }

            if (asks_by_price_)
            {
                for (auto ask = asks_by_price_->next_entry_; ask != asks_by_price_; ask = ask->next_entry_)
                    orders_at_price_pool_.deallocate(ask);
                orders_at_price_pool_.deallocate(asks_by_price_);
            }

            bids_by_price_ = asks_by_price_ = nullptr;
        }
        break;
        /*
        The MarketOrderBook class does not need to handle INVALID, SNAPSHOT_START, and SNAPSHOT_END MarketUpdateTypes, 
        so it does nothing with those messages:
        */
        case Exchange::MarketUpdateType::INVALID:
        case Exchange::MarketUpdateType::SNAPSHOT_START:
        case Exchange::MarketUpdateType::SNAPSHOT_END:
            break;
        }

        /*
        At this point, we will call the updateBBO() method and pass it to the two boolean flags we 
        computed: bid_updated and ask_updated
        */
        updateBBO(bid_updated, ask_updated);


        /*
        Finally, it notifies the TradeEngine engine that the order book was updated using the 
        onOrderBookUpdate() method
        */
        logger_->log("%:% %() % % %", __FILE__, __LINE__, __FUNCTION__,
                     Common::getCurrentTimeStr(&time_str_), market_update->toString(), bbo_.toString());

        trade_engine_->onOrderBookUpdate(market_update->ticker_id_, market_update->price_, market_update->side_, this);
    }

    auto MarketOrderBook::toString(bool detailed, bool validity_check) const -> std::string
    {
        std::stringstream ss;
        std::string time_str;

        auto printer = [&](std::stringstream &ss, MarketOrdersAtPrice *itr, Side side, Price &last_price,
                           bool sanity_check)
        {
            char buf[4096];
            Qty qty = 0;
            size_t num_orders = 0;

            for (auto o_itr = itr->first_mkt_order_;; o_itr = o_itr->next_order_)
            {
                qty += o_itr->qty_;
                ++num_orders;
                if (o_itr->next_order_ == itr->first_mkt_order_)
                    break;
            }
            sprintf(buf, " <px:%3s p:%3s n:%3s> %-3s @ %-5s(%-4s)",
                    priceToString(itr->price_).c_str(), priceToString(itr->prev_entry_->price_).c_str(),
                    priceToString(itr->next_entry_->price_).c_str(),
                    priceToString(itr->price_).c_str(), qtyToString(qty).c_str(), std::to_string(num_orders).c_str());
            ss << buf;
            for (auto o_itr = itr->first_mkt_order_;; o_itr = o_itr->next_order_)
            {
                if (detailed)
                {
                    sprintf(buf, "[oid:%s q:%s p:%s n:%s] ",
                            orderIdToString(o_itr->order_id_).c_str(), qtyToString(o_itr->qty_).c_str(),
                            orderIdToString(o_itr->prev_order_ ? o_itr->prev_order_->order_id_ : OrderId_INVALID).c_str(),
                            orderIdToString(o_itr->next_order_ ? o_itr->next_order_->order_id_ : OrderId_INVALID).c_str());
                    ss << buf;
                }
                if (o_itr->next_order_ == itr->first_mkt_order_)
                    break;
            }

            ss << std::endl;

            if (sanity_check)
            {
                if ((side == Side::SELL && last_price >= itr->price_) || (side == Side::BUY && last_price <= itr->price_))
                {
                    FATAL("Bids/Asks not sorted by ascending/descending prices last:" + priceToString(last_price) + " itr:" +
                          itr->toString());
                }
                last_price = itr->price_;
            }
        };

        ss << "Ticker:" << tickerIdToString(ticker_id_) << std::endl;
        {
            auto ask_itr = asks_by_price_;
            auto last_ask_price = std::numeric_limits<Price>::min();
            for (size_t count = 0; ask_itr; ++count)
            {
                ss << "ASKS L:" << count << " => ";
                auto next_ask_itr = (ask_itr->next_entry_ == asks_by_price_ ? nullptr : ask_itr->next_entry_);
                printer(ss, ask_itr, Side::SELL, last_ask_price, validity_check);
                ask_itr = next_ask_itr;
            }
        }

        ss << std::endl
           << "                          X" << std::endl
           << std::endl;

        {
            auto bid_itr = bids_by_price_;
            auto last_bid_price = std::numeric_limits<Price>::max();
            for (size_t count = 0; bid_itr; ++count)
            {
                ss << "BIDS L:" << count << " => ";
                auto next_bid_itr = (bid_itr->next_entry_ == bids_by_price_ ? nullptr : bid_itr->next_entry_);
                printer(ss, bid_itr, Side::BUY, last_bid_price, validity_check);
                bid_itr = next_bid_itr;
            }
        }

        return ss.str();
    }
}