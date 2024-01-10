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
    method to log it. We also add a OrderId_INVALID sentinel 
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
}
