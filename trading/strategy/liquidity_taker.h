#pragma once

#include "common/macros.h"
#include "common/logging.h"

#include "order_manager.h"
#include "feature_engine.h"

using namespace Common;

namespace Trading {
  class LiquidityTaker {
  public:
    LiquidityTaker(Common::Logger *logger, TradeEngine *trade_engine, const FeatureEngine *feature_engine,
                   OrderManager *order_manager,
                   const TradeEngineCfgHashMap &ticker_cfg);

    /// Process order book updates, which for the liquidity taking algorithm is none.
    auto onOrderBookUpdate(TickerId ticker_id, Price price, Side side, MarketOrderBook *) noexcept -> void {
      logger_->log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                   Common::getCurrentTimeStr(&time_str_), ticker_id, Common::priceToString(price).c_str(),
                   Common::sideToString(side).c_str());
    }

    /// Process trade events, fetch the aggressive trade ratio from the feature engine, check against the trading threshold and send aggressive orders.
    auto onTradeUpdate(const Exchange::MEMarketUpdate *market_update, MarketOrderBook *book) noexcept -> void {
      /*
      For the MarketMaker trading strategy, we saw that it only makes trading decisions on order book updates and does nothing 
      on trade updates. The LiquidityTaker strategy does the opposite – it takes trading decisions in the onTradeUpdate() method 
      and does nothing in the onOrderBookUpdate() method. We will start by looking at the implementation of the LiquidityTaker::onTradeUpdate() 
      method first in the next code block:
      */
      
      logger_->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                   market_update->toString().c_str());

        /*
        We will fetch and save BBO using the getBBO() method in the bbo local variable. For this trading strategy, we will fetch 
        the aggressive trade quantity ratio feature from the feature engine by calling the FeatureEngine::getAggTradeQtyRatio() 
        method and saving it in the agg_qty_ratio variable
        */
      const auto bbo = book->getBBO();
      const auto agg_qty_ratio = feature_engine_->getAggTradeQtyRatio();
        /*
        we will check to make sure that bid_price_, ask_price_, and agg_qty_ratio are valid values before we decide to take 
        an order action
        */
      if (LIKELY(bbo->bid_price_ != Price_INVALID && bbo->ask_price_ != Price_INVALID && agg_qty_ratio != Feature_INVALID)) {
        logger_->log("%:% %() % % agg-qty-ratio:%\n", __FILE__, __LINE__, __FUNCTION__,
                     Common::getCurrentTimeStr(&time_str_),
                     bbo->toString().c_str(), agg_qty_ratio);

        /*
        If the validity check passes, we first need to fetch the clip_ member from the ticker_cfg_ object for the 
        TickerId of this trade message, as shown in the following code block, and save it in the clip local variable. 
        Similarly, we will fetch and save the threshold_ member from the ticker_cfg_ configuration object for that TickerId
        */
        const auto clip = ticker_cfg_.at(market_update->ticker_id_).clip_;
        const auto threshold = ticker_cfg_.at(market_update->ticker_id_).threshold_;

        /*
        To decide whether we send or adjust active orders for this algorithm, we will check whether the agg_qty_ratio 
        exceeds the threshold we previously fetched
        */
        if (agg_qty_ratio >= threshold) {
          if (market_update->side_ == Side::BUY)
          /*
          To send orders using the OrderManager::moveOrders() method, we will check whether the aggressive 
          trade was a buy trade or a sell trade. If it was a buy trade, we will send an aggressive buy order to 
          take liquidity at the best BBO ask_price_ and no sell order by specifying a sell price of Price_INVALID. 
          Conversely, if it was a sell trade and we wanted to send an aggressive sell order to take liquidity, 
          we would specify a sell price to be bid_price_ in the BBO object and no buy order by specifying a Price_INVALID 
          buy price. Remember that this trading strategy takes a direction in the market by aggressively sending a buy or 
          sell order one at a time, but not both like the MarketMaker algorithm:
          */
            order_manager_->moveOrders(market_update->ticker_id_, bbo->ask_price_, Price_INVALID, clip);
          else
            order_manager_->moveOrders(market_update->ticker_id_, Price_INVALID, bbo->bid_price_, clip);
        }
      }
    }

    /// Process client responses for the strategy's orders.
    auto onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept -> void {
        /*
        The LiquidityTaker::onOrderUpdate() method, as shown in the following code block, has an identical 
        implementation to the MarketMaker::onOrderUpdate() method and simply forwards the order update 
        to the order manager using the OrderManager::onOrderUpdate() method
        */
      logger_->log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                   client_response->toString().c_str());
      order_manager_->onOrderUpdate(client_response);
    }

    /// Deleted default, copy & move constructors and assignment-operators.
    LiquidityTaker() = delete;

    LiquidityTaker(const LiquidityTaker &) = delete;

    LiquidityTaker(const LiquidityTaker &&) = delete;

    LiquidityTaker &operator=(const LiquidityTaker &) = delete;

    LiquidityTaker &operator=(const LiquidityTaker &&) = delete;

  private:

    /*
    The LiquidityTaker class has a feature_engine_ member, which is a constant pointer 
    to a FeatureEngine object, an order_manager_ pointer to an OrderManager object, and a 
    constant ticker_cfg_ member, which is of type TradeEngineCfgHashMap. These members serve the 
    same purpose as they did in the MarketMaker class; feature_engine_ is used to extract
    the ratio of aggressive trade to top-of-book quantity. The order_manager_ object is used 
    to send and manage the orders for this trading strategy. Finally, the ticker_cfg_ object holds 
    the trading parameters that will be used by this algorithm to make trading decisions and 
    send orders to the exchange
    */
    /// The feature engine that drives the liquidity taking algorithm.
    const FeatureEngine *feature_engine_ = nullptr;

    /// Used by the liquidity taking algorithm to send aggressive orders.
    OrderManager *order_manager_ = nullptr;

    std::string time_str_;
    Common::Logger *logger_ = nullptr;

    /// Holds the trading configuration for the liquidity taking algorithm.
    const TradeEngineCfgHashMap ticker_cfg_;
  };
}