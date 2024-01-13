#pragma once

#include "common/types.h"
#include "common/mem_pool.h"
#include "common/logging.h"

#include "market_order.h"
#include "exchange/market_data/market_update.h"

namespace Trading {
  class TradeEngine;

  class MarketOrderBook final {
  public:
    MarketOrderBook(TickerId ticker_id, Logger *logger);

    ~MarketOrderBook();

    auto onMarketUpdate(const Exchange::MEMarketUpdate *market_update) noexcept -> void;

    /*
    an additional utility method called setTradeEngine(), which is a better method 
    to set the trade_engine_ variable with an instance of a TradeEngine object
    */
    auto setTradeEngine(TradeEngine *trade_engine) {
      trade_engine_ = trade_engine;
    }

    auto updateBBO(bool update_bid, bool update_ask) noexcept {

        /*
        The first thing we do is check if the update_bid parameter passed to it is true. Only then do we have to update the bid side of the BBO object. Next, we check 
        if the bids_by_price_ member is not nullptr. If it is not valid, then we set the bid_price_ variable and the bid_qty_ variable to be invalid (Price_INVALID and 
        Qty_INVALID respectively) since the side is empty. The more interesting handling is in the case where the bids_by_price_ member is valid.
        In that case, we set the bid_price_ member variable in the bbo_ object to be the price of the best bid: bids_by_price_->price_. To compute bid_qty_ in the bbo_ object, 
        we first assign it qty_ of the first order at that price level, which we access using the bids_by_price_->first_mkt_order_->qty_ value. Then, we linearly iterate over all 
        the orders at that price level by following the next_order_ pointers until we wrap around, i.e. the next_order_ points to the first_mkt_order_ object. For each order we 
        iterate over, we accumulate the qty_ value of that order into the bid_qty_ member in our bbo_ object. At this point, we are done updating the bid side of the BBO object
        */
      if(update_bid) {
        if(bids_by_price_) {
          bbo_.bid_price_ = bids_by_price_->price_;
          bbo_.bid_qty_ = bids_by_price_->first_mkt_order_->qty_;


          /*
          ==============================================
          SCOPE FOR IMPROVEMENT

          IMPROVISE THE FOR LOOP

          linear iteration is slightly inefficient and can be improved for example by 
          tracking and updating these values during the processing of the MEMarketUpdate messages itself
          
          ===============================================
          */
          for(auto order = bids_by_price_->first_mkt_order_->next_order_; order != bids_by_price_->first_mkt_order_; order = order->next_order_)
            bbo_.bid_qty_ += order->qty_;
        }
        else {
          bbo_.bid_price_ = Price_INVALID;
          bbo_.bid_qty_ = Qty_INVALID;
        }
      }

      if(update_ask) {
        if(asks_by_price_) {
          bbo_.ask_price_ = asks_by_price_->price_;
          bbo_.ask_qty_ = asks_by_price_->first_mkt_order_->qty_;
          for(auto order = asks_by_price_->first_mkt_order_->next_order_; order != asks_by_price_->first_mkt_order_; order = order->next_order_)
            bbo_.ask_qty_ += order->qty_;
        }
        else {
          bbo_.ask_price_ = Price_INVALID;
          bbo_.ask_qty_ = Qty_INVALID;
        }
      }
    }

    auto getBBO() const noexcept -> const BBO* {
      return &bbo_;
    }

    auto toString(bool detailed, bool validity_check) const -> std::string;

    // Deleted default, copy & move constructors and assignment-operators.
    MarketOrderBook() = delete;

    MarketOrderBook(const MarketOrderBook &) = delete;

    MarketOrderBook(const MarketOrderBook &&) = delete;

    MarketOrderBook &operator=(const MarketOrderBook &) = delete;

    MarketOrderBook &operator=(const MarketOrderBook &&) = delete;

  private:
    const TickerId ticker_id_;

    TradeEngine *trade_engine_ = nullptr;
    /*
    TradeEngine represents the class that is the trading engine framework. We will 
    communicate changes to the order book using this variable
    */
    
    
    OrderHashMap oid_to_order_; //used to track MarketOrder objects by OrderId



    /*
    Two memory pools, order_pool_ for MarketOrder objects and orders_at_price_pool_ 
    for MarketOrdersAtPrice objects, are to be used to allocate and deallocate these objects 
    as needed. The first pool, order_pool_, is used to allocate and deallocate MarketOrder 
    objects. The second pool, orders_at_price_pool_, is used to allocate and deallocate 
    MarketOrdersAtPrice objects. Remember that a single MemPool instance is tied to a 
    specific object type provided to it as a template parameter
    */

    MemPool<MarketOrdersAtPrice> orders_at_price_pool_;

    /*
    Two pointers to MarketOrdersAtPrice – bids_by_price_ to represent the 
    doubly linked list of bids sorted by price and asks_by_price_ to represent the doubly 
    linked list of asks sorted by price
    */
    MarketOrdersAtPrice *bids_by_price_ = nullptr;
    MarketOrdersAtPrice *asks_by_price_ = nullptr;

    OrdersAtPriceHashMap price_orders_at_price_;  //to track OrdersAtPrice objects by Price

    MemPool<MarketOrder> order_pool_;

    BBO bbo_;
    /*
    will be used to compute and maintain a BBO-view of the order book when 
    there are updates and provided to any components that require it
    */
    std::string time_str_;
    Logger *logger_ = nullptr;

  private:
    auto priceToIndex(Price price) const noexcept {
      return (price % ME_MAX_PRICE_LEVELS);
    }

    auto getOrdersAtPrice(Price price) const noexcept -> MarketOrdersAtPrice * {
      return price_orders_at_price_.at(priceToIndex(price));
    }

    auto addOrdersAtPrice(MarketOrdersAtPrice *new_orders_at_price) noexcept {
      price_orders_at_price_.at(priceToIndex(new_orders_at_price->price_)) = new_orders_at_price;

      const auto best_orders_by_price = (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_);
      if (UNLIKELY(!best_orders_by_price)) {
        (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_) = new_orders_at_price;
        new_orders_at_price->prev_entry_ = new_orders_at_price->next_entry_ = new_orders_at_price;
      } else {
        auto target = best_orders_by_price;
        bool add_after = ((new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) ||
                          (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_));
        if (add_after) {
          target = target->next_entry_;
          add_after = ((new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) ||
                       (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_));
        }
        while (add_after && target != best_orders_by_price) {
          add_after = ((new_orders_at_price->side_ == Side::SELL && new_orders_at_price->price_ > target->price_) ||
                       (new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ < target->price_));
          if (add_after)
            target = target->next_entry_;
        }

        if (add_after) { // add new_orders_at_price after target.
          if (target == best_orders_by_price) {
            target = best_orders_by_price->prev_entry_;
          }
          new_orders_at_price->prev_entry_ = target;
          target->next_entry_->prev_entry_ = new_orders_at_price;
          new_orders_at_price->next_entry_ = target->next_entry_;
          target->next_entry_ = new_orders_at_price;
        } else { // add new_orders_at_price before target.
          new_orders_at_price->prev_entry_ = target->prev_entry_;
          new_orders_at_price->next_entry_ = target;
          target->prev_entry_->next_entry_ = new_orders_at_price;
          target->prev_entry_ = new_orders_at_price;

          if ((new_orders_at_price->side_ == Side::BUY && new_orders_at_price->price_ > best_orders_by_price->price_) ||
              (new_orders_at_price->side_ == Side::SELL &&
               new_orders_at_price->price_ < best_orders_by_price->price_)) {
            target->next_entry_ = (target->next_entry_ == best_orders_by_price ? new_orders_at_price
                                                                               : target->next_entry_);
            (new_orders_at_price->side_ == Side::BUY ? bids_by_price_ : asks_by_price_) = new_orders_at_price;
          }
        }
      }
    }

    auto removeOrdersAtPrice(Side side, Price price) noexcept {
      const auto best_orders_by_price = (side == Side::BUY ? bids_by_price_ : asks_by_price_);
      auto orders_at_price = getOrdersAtPrice(price);

      if (UNLIKELY(orders_at_price->next_entry_ == orders_at_price)) { // empty side of book.
        (side == Side::BUY ? bids_by_price_ : asks_by_price_) = nullptr;
      } else {
        orders_at_price->prev_entry_->next_entry_ = orders_at_price->next_entry_;
        orders_at_price->next_entry_->prev_entry_ = orders_at_price->prev_entry_;

        if (orders_at_price == best_orders_by_price) {
          (side == Side::BUY ? bids_by_price_ : asks_by_price_) = orders_at_price->next_entry_;
        }

        orders_at_price->prev_entry_ = orders_at_price->next_entry_ = nullptr;
      }

      price_orders_at_price_.at(priceToIndex(price)) = nullptr;

      orders_at_price_pool_.deallocate(orders_at_price);
    }

    auto removeOrder(MarketOrder *order) noexcept -> void {
      auto orders_at_price = getOrdersAtPrice(order->price_);

      if (order->prev_order_ == order) { // only one element.
        removeOrdersAtPrice(order->side_, order->price_);
      } else { // remove the link.
        const auto order_before = order->prev_order_;
        const auto order_after = order->next_order_;
        order_before->next_order_ = order_after;
        order_after->prev_order_ = order_before;

        if (orders_at_price->first_mkt_order_ == order) {
          orders_at_price->first_mkt_order_ = order_after;
        }

        order->prev_order_ = order->next_order_ = nullptr;
      }

      oid_to_order_.at(order->order_id_) = nullptr;
      order_pool_.deallocate(order);
    }

    auto addOrder(MarketOrder *order) noexcept -> void {
      const auto orders_at_price = getOrdersAtPrice(order->price_);

      if (!orders_at_price) {
        order->next_order_ = order->prev_order_ = order;

        auto new_orders_at_price = orders_at_price_pool_.allocate(order->side_, order->price_, order, nullptr, nullptr);
        addOrdersAtPrice(new_orders_at_price);
      } else {
        auto first_order = (orders_at_price ? orders_at_price->first_mkt_order_ : nullptr);

        first_order->prev_order_->next_order_ = order;
        order->prev_order_ = first_order->prev_order_;
        order->next_order_ = first_order;
        first_order->prev_order_ = order;
      }

      oid_to_order_.at(order->order_id_) = order;
    }
  };
    /*
    Below is just a hash map from TickerId to MarketOrderBook objects of the ME_MAX_TICKERS size.
    */
  typedef std::array<MarketOrderBook *, ME_MAX_TICKERS> MarketOrderBookHashMap;
}