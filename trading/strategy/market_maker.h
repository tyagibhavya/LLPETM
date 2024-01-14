#pragma once

#include "common/macros.h"
#include "common/logging.h"

#include "order_manager.h"
#include "feature_engine.h"

using namespace Common;

namespace Trading
{
    class MarketMaker
    {
    public:
        MarketMaker(Common::Logger *logger, TradeEngine *trade_engine, const FeatureEngine *feature_engine,
                    OrderManager *order_manager,
                    const TradeEngineCfgHashMap &ticker_cfg);

        /// Process order book updates, fetch the fair market price from the feature engine, check against the trading threshold and modify the passive orders.
        auto onOrderBookUpdate(TickerId ticker_id, Price price, Side side, const MarketOrderBook *book) noexcept -> void
        {    /*
            The MarketMaker::onOrderBookUpdate() method is called by TradeEngine through the TradeEngine::algoOnOrderBookUpdate_ 
            std::function member variable. This is where the MarketMaker trading strategy makes trading decisions with regard 
            to what prices it wants its bid and ask orders to be at
            */
            logger_->log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                         Common::getCurrentTimeStr(&time_str_), ticker_id, Common::priceToString(price).c_str(),
                         Common::sideToString(side).c_str());

            /*
            It fetches BBO from w using the getBBO() method and saves it in the bbo variable. We also fetch 
            the market quantity-weighted BBO price and save it in the fair_price variable
            */
            const auto bbo = book->getBBO();
            const auto fair_price = feature_engine_->getMktPrice();

            /*
            We perform a sanity check on the best bid_price_ and ask_price_ values from bbo and fair_price 
            to make sure that the prices are not Price_INVALID and the feature value is not Feature_INVALID. 
            Only if this is true will we take any action; otherwise, we risk acting on invalid features or sending orders 
            at invalid prices
            */
            if (LIKELY(bbo->bid_price_ != Price_INVALID && bbo->ask_price_ != Price_INVALID && fair_price != Feature_INVALID))
            {
                logger_->log("%:% %() % % fair-price:%\n", __FILE__, __LINE__, __FUNCTION__,
                             Common::getCurrentTimeStr(&time_str_),
                             bbo->toString().c_str(), fair_price);

                /*
                We fetch and save the clip quantity from the ticker_cfg_ container, which will be the quantity on the passive 
                orders we send to the exchange. We also extract and save the threshold value, which we will use to decide 
                what prices to send the buy and sell orders at:
                */
                const auto clip = ticker_cfg_.at(ticker_id).clip_;
                const auto threshold = ticker_cfg_.at(ticker_id).threshold_;

                /*
                We initialize two price variables, bid_price and ask_price, to represent the prices on our buy and sell 
                orders, respectively. We set bid_price to be the best bid price if the difference between fair_price we 
                computed from the FeatureEngine::getMktPrice() method and the market bid price exceeds the threshold value. 
                Otherwise, we set bid_price to be a price lower than the best market bid price. We compute ask_price 
                using the same logic – use the best ask price if the difference from the fair price exceeds the threshold 
                and a higher price otherwise. The motivation behind this is straightforward; when we think the fair price 
                is higher than the best bid price, we are willing to buy at the best bid price, expecting the prices 
                to go higher. When we think the fair price is lower than the best ask price, we are willing to sell at 
                the best ask price, expecting the prices to go lower
                */

                const auto bid_price = bbo->bid_price_ - (fair_price - bbo->bid_price_ >= threshold ? 0 : 1);
                const auto ask_price = bbo->ask_price_ + (bbo->ask_price_ - fair_price >= threshold ? 0 : 1);

                /*
                We use the bid_price and ask_price variables we computed in the preceding code block a and pass them 
                to the OrderManager::moveOrders() method to move the orders to the desired prices:
                */
                order_manager_->moveOrders(ticker_id, bid_price, ask_price, clip);
            }
        }

        /// Process trade events, which for the market making algorithm is none.


        auto onTradeUpdate(const Exchange::MEMarketUpdate *market_update, MarketOrderBook * /* book */) noexcept -> void
        {   /*
            The MarketMaker trading algorithm does not do anything when there are trade events and simply logs the 
            trade message it receives
            */
            logger_->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                         market_update->toString().c_str());
        }

        /// Process client responses for the strategy's orders.
        auto onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept -> void
        {
            logger_->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                         client_response->toString().c_str());

            order_manager_->onOrderUpdate(client_response);
        }

        /// Deleted default, copy & move constructors and assignment-operators.
        MarketMaker() = delete;

        MarketMaker(const MarketMaker &) = delete;

        MarketMaker(const MarketMaker &&) = delete;

        MarketMaker &operator=(const MarketMaker &) = delete;

        MarketMaker &operator=(const MarketMaker &&) = delete;

    private:
        /// The feature engine that drives the market making algorithm that 
        //will use to fetch the fair market price, using the FeatureEngine::getMktPrice() method we saw earlier
        const FeatureEngine *feature_engine_ = nullptr;

        /// Used by the market making algorithm to manage its passive orders.
        OrderManager *order_manager_ = nullptr;

        std::string time_str_;
        Common::Logger *logger_ = nullptr;

        /// Holds the trading configuration for the market making algorithm.
        const TradeEngineCfgHashMap ticker_cfg_;
    };
}