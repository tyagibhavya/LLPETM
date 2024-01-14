#pragma once
//Commenting changes
#include <cstdint>
#include <limits>

#include "common/macros.h"

namespace Common {
//  Represents the number of trading instruments the exchange supports
  constexpr size_t ME_MAX_TICKERS = 8;
//  Holds the maximum number of unprocessed order requests from all clients that the matching engine has not processed yet.
  constexpr size_t ME_MAX_CLIENT_UPDATES = 256 * 1024;
// Represents the maximum number of market updates generated by the matching engine that have not yet been published by the market data publisher.
  constexpr size_t ME_MAX_MARKET_UPDATES = 256 * 1024;
//  Holds how many maximum simultaneous market participants can exist in our trading ecosystem.
  constexpr size_t ME_MAX_NUM_CLIENTS = 256;
// The maximum number of orders possible for a single trading instrument
  constexpr size_t ME_MAX_ORDER_IDS = 1024 * 1024;
//  Represents the maximum depth of price levels for the limit order book that the matching engine maintains.
  constexpr size_t ME_MAX_PRICE_LEVELS = 256;

//  OrderId type to identify orders
  typedef uint64_t OrderId;
  /* 
  This code defines an order ID type (OrderId), a constant representing an invalid order ID (INVALID), and a function (orderIdToString) 
  to convert order IDs to strings. The use of UNLIKELY suggests that invalid order IDs are expected to be uncommon, and the function returns 
  a specific string for the invalid case. 
  */
  constexpr auto OrderId_INVALID = std::numeric_limits<OrderId>::max();

  inline auto orderIdToString(OrderId order_id) -> std::string {
    if (UNLIKELY(order_id == OrderId_INVALID)) {
      return "INVALID";
    }

    return std::to_string(order_id);
  }


//  TickerId type to identify trading instruments
  typedef uint32_t TickerId;
  constexpr auto TickerId_INVALID = std::numeric_limits<TickerId>::max();

  inline auto tickerIdToString(TickerId ticker_id) -> std::string {
    if (UNLIKELY(ticker_id == TickerId_INVALID)) {
      return "INVALID";
    }

    return std::to_string(ticker_id);
  }

// The ClientId type is used to differentiate between different trading participants.
  typedef uint32_t ClientId;
  constexpr auto ClientId_INVALID = std::numeric_limits<ClientId>::max();

  inline auto clientIdToString(ClientId client_id) -> std::string {
    if (UNLIKELY(client_id == ClientId_INVALID)) {
      return "INVALID";
    }

    return std::to_string(client_id);
  }

// The next type is Price, which is used to capture prices on orders.
  typedef int64_t Price;
  constexpr auto Price_INVALID = std::numeric_limits<Price>::max();

  inline auto priceToString(Price price) -> std::string {
    if (UNLIKELY(price == Price_INVALID)) {
      return "INVALID";
    }

    return std::to_string(price);
  }

// The Qty type represents order quantities.
  typedef uint32_t Qty;
  constexpr auto Qty_INVALID = std::numeric_limits<Qty>::max();

  inline auto qtyToString(Qty qty) -> std::string {
    if (UNLIKELY(qty == Qty_INVALID)) {
      return "INVALID";
    }

    return std::to_string(qty);
  }

// The Priority type is just a position in the queue.
  typedef uint64_t Priority;
  constexpr auto Priority_INVALID = std::numeric_limits<Priority>::max();

  inline auto priorityToString(Priority priority) -> std::string {
    if (UNLIKELY(priority == Priority_INVALID)) {
      return "INVALID";
    }

    return std::to_string(priority);
  }

// Side type is an enumeration and contains two valid values, as shown in the following code block.
  enum class Side : int8_t {
    INVALID = 0,
    BUY = 1,
    SELL = -1
  };

  inline auto sideToString(Side side) -> std::string {
    switch (side) {
      case Side::BUY:
        return "BUY";
      case Side::SELL:
        return "SELL";
      case Side::INVALID:
        return "INVALID";
    }

    return "UNKNOWN";
  }
}
