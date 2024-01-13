#pragma once

#include <array>
#include <sstream>
#include "common/types.h"

using namespace Common;

namespace Trading {

    /*
    we will define the MarketOrder structure, which represents a single order in the market data stream. 
    This structure contains the OrderId, Side, Price, Qty, and Priority attributes. It also contains a prev_order_ 
    and a next_order_ member of type MarketOrder pointer since we will chain these objects in a doubly linked list
    */
  struct MarketOrder {
    OrderId order_id_ = OrderId_INVALID;
    Side side_ = Side::INVALID;
    Price price_ = Price_INVALID;
    Qty qty_ = Qty_INVALID;
    Priority priority_ = Priority_INVALID;

    MarketOrder *prev_order_ = nullptr;
    MarketOrder *next_order_ = nullptr;

    // only needed for use with MemPool.
    MarketOrder() = default;

    MarketOrder(OrderId order_id, Side side, Price price, Qty qty, Priority priority, MarketOrder *prev_order, MarketOrder *next_order) noexcept
        : order_id_(order_id), side_(side), price_(price), qty_(qty), priority_(priority), prev_order_(prev_order), next_order_(next_order) {}

    auto toString() const -> std::string;
  };

  typedef std::array<MarketOrder *, ME_MAX_ORDER_IDS> OrderHashMap; //an std::array array of MarketOrder pointer objects and of size ME_MAX_ORDER_IDS , in the same way as we did in the matching engine order book


  /*
  The MarketOrdersAtPrice struct is identical to the MEOrdersAtPrice struct we built for the matching MEOrderBook engine. It contains Side, Price, and a MarketOrder first_mkt_order_ pointer to represent the 
  beginning of the MarketOrder-linked list at this price. It also contains two MarketOrdersAtPrice pointers, prev_entry_ and next_entry_, since we will create a doubly linked list of MarketOrdersAtPrice objects 
  to represent the price levels
  */
  struct MarketOrdersAtPrice {
    Side side_ = Side::INVALID;
    Price price_ = Price_INVALID;

    MarketOrder *first_mkt_order_ = nullptr;

    MarketOrdersAtPrice *prev_entry_ = nullptr;
    MarketOrdersAtPrice *next_entry_ = nullptr;

    MarketOrdersAtPrice() = default;

    MarketOrdersAtPrice(Side side, Price price, MarketOrder *first_mkt_order, MarketOrdersAtPrice *prev_entry, MarketOrdersAtPrice *next_entry)
        : side_(side), price_(price), first_mkt_order_(first_mkt_order), prev_entry_(prev_entry), next_entry_(next_entry) {}

    auto toString() const {
      std::stringstream ss;
      ss << "MarketOrdersAtPrice["
         << "side:" << sideToString(side_) << " "
         << "price:" << priceToString(price_) << " "
         << "first_mkt_order:" << (first_mkt_order_ ? first_mkt_order_->toString() : "null") << " "
         << "prev:" << priceToString(prev_entry_ ? prev_entry_->price_ : Price_INVALID) << " "
         << "next:" << priceToString(next_entry_ ? next_entry_->price_ : Price_INVALID) << "]";

      return ss.str();
    }
  };

  typedef std::array<MarketOrdersAtPrice *, ME_MAX_PRICE_LEVELS> OrdersAtPriceHashMap; //represents a hash map from Price to MarketOrdersAtPrice pointers


  /*
  Finally, we need to define another structure that will represent the total quantity available at the best bid and ask prices. This will 
  represent the best (most aggressive) buy and sell prices available in the market as well as the sum of the quantities of all the orders 
  at those prices. This structure, called BBO, only has four members – bid_price_ and ask_price_ (both Price types to represent the 
  best prices), and bid_qty_ and ask_qty_ to represent the total quantity of all orders at these prices.
  */

  /*
  The BBO abstraction is used by many different components inside the trade engine. Typically, this is used by components that need 
  a summary of the best market prices and liquidity, instead of the full depth of the book and all the details about each order in the 
  book. For example, a component such as the RiskManager component, which only needs to compute the open Profit and Loss (PnL) 
  for an open position when the top-of-book prices change, does not need access to the full order book and instead can be 
  simplified using a BBO abstraction. Other components, such as FeatureEngine, PositionKeeper, LiquidityTaker, and MarketMaker, also use 
  the BBO abstraction where the full order book is not needed
  */
  struct BBO {
    Price bid_price_ = Price_INVALID, ask_price_ = Price_INVALID;
    Qty bid_qty_ = Qty_INVALID, ask_qty_ = Qty_INVALID;

    auto toString() const {
      std::stringstream ss;
      ss << "BBO{"
         << qtyToString(bid_qty_) << "@" << priceToString(bid_price_)
         << "X"
         << priceToString(ask_price_) << "@" << qtyToString(ask_qty_)
         << "}";

      return ss.str();
    };
  };
}