// Day 1
// First Comment

/* 
--CONVENTIONS--

OrderId to identify orders
TickerId to identify trading instruments
ClientId for the exchange to identify different clients
Price to hold prices for instruments
Qty to hold quantity values for orders
Priority to capture the position of an order in the First In First Out (FIFO) queue at a price level
Side to signify the side (buy/sell) of an order

*/
#pragma once
#include <cstdint>
#include <limits>
#include "common/macros.h"

namespace Common{
    typedef uint64_t OrderId;
    /*we define the OrderId type to identify orders,
    which is simply uint64_t, and a corresponding orderIdToString() 
    method to log it. We also add a OrderId_INVALID  
    method to signify invalid values
    */
    constexpr auto OrderId_INVALID = std::numeric_limits<OrderId>:: max;
    /* 
    This line defines a constant named INVALID of type OrderId.
    std::numeric_limits<OrderId>::max retrieves the maximum representable
    value for the OrderId type. In this case, it's used to represent an invalid order ID.
    */
    inline auto orderIdToString(OrderId order_id) -> std::string{
        if (UNLIKELY(order_id == OrderId_INVALID)){
            return "INVALID" ;
        }
        return std::to_string(order_id) ;
    }
    /*
    This declares an inline function named orderIdToString that takes
    an OrderId parameter and returns a std::string.
    */


   typedef uint32_t TickerId ;
   /*
   we define the OrderId type to identify orders, which is simply uint64_t,
   and a corresponding orderIdToString() method to log it. We also add a OrderId_INVALID
   sentinel method to signify invalid values:
   */
   constexpr auto TickerIdINVALID = std::numeric_limits<TickerId>::max() ;
   /* Same pattern as the previous constexpr declaration of OrderId */
   inline auto tickerIdToString(TickerId ticker_id) -> std::string{
    if (UNLIKELY(ticker_id == TickerId_INVALID)){
        return "INVALID" ;
    }
    return std::to_string(ticker_id) ;
   }

   /* Same pattern followed as the previous inline function*/

   typedef uint32_t ClientId ;
   /*
   We define the TickerId type to identify trading instruments, which is
   simply a uint32_t type, and add a corresponding tickerIdToString() method for it. 
   We have a TickerId_INVALID sentinel value for invalid instruments
   */
   constexpr auto ClientId_INVALID = std::numeric_limits<ClientId>::max();
    inline auto clientIdToString(ClientId client_id) ->std::string {
    if (UNLIKELY(client_id == ClientId_INVALID)) {
      return "INVALID";
    }
    return std::to_string(client_id);
  }

  /* Same pattern of code as above */



    typedef int64_t Price;
    /*
    The next type is Price, which is used to capture prices
    on orders. We also add a Price_INVALID constant to represent 
    invalid prices. Finally, a priceToString() method to stringify these values
    */
    constexpr auto Price_INVALID = std::numeric_limits<Price>::max();
    inline auto priceToString(Price price) -> std::string {
    if (UNLIKELY(price == Price_INVALID)) {
      return "INVALID";
    }
    return std::to_string(price);
  }
    /* Similar to the previous code pattern*/



    typedef uint32_t Qty;
    /*
    The Qty type is typedef for uint32_t and represents order quantities.
    We also provide the usual Qty_INVALID sentinel and the qtyToString() method
    to convert them to strings
    */
    constexpr auto Qty_INVALID = std::numeric_limits<Qty>::max();
    inline auto qtyToString(Qty qty) -> std::string {
        if (UNLIKELY(qty == Qty_INVALID)) {
        return "INVALID";
        }
        return std::to_string(qty);
    }
    /* Same as above */



    typedef uint64_t Priority;
    /*
    The Priority type is just a position in the queue of type uint64_t.
    We assign the Priority_INVALID sentinel value and the priorityToString() method
    */
    constexpr auto Priority_INVALID = std::numeric_limits<Priority>::max();

    inline auto priorityToString(Priority priority) -> std::string {
        if (UNLIKELY(priority == Priority_INVALID)) {
        return "INVALID";
        }

        return std::to_string(priority);
    }
    /* Same as above */

    enum class Side : int8_t {
        INVALID = 0, 
        BUY = 1,
        SELL = -1
    };
    /*
    The Side type is an enumeration and contains two valid values, as shown in 
    the following code block. We also define a sideToString() method, as
    we did for the other types previously
    */
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
