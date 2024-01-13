#pragma once

#include <array>
#include <sstream>
#include "common/types.h"

using namespace Common;



namespace Trading {
  /*
The INVALID state represents an invalid order state
The PENDING_NEW state signifies that a new order has been sent out by OrderManager but it has not been accepted 
by the electronic trading exchange yet

When we receive a response from the exchange to signify acceptance, the order goes from PENDING_NEW to LIVE

Like PENDING_NEW, the PENDING_CANCEL state represents the state of an order when a cancellation for an order 
has been sent to the exchange but has not been processed by the exchange or the response has not been received back

The DEAD state represents an order that does not exist – it has either not been sent yet or fully executed or successfully cancelled
*/
  
  enum class OMOrderState : int8_t {
    INVALID = 0,
    PENDING_NEW = 1,
    LIVE = 2,
    PENDING_CANCEL = 3,
    DEAD = 4
  };


  inline auto OMOrderStateToString(OMOrderState side) -> std::string {
    switch (side) {
      case OMOrderState::PENDING_NEW:
        return "PENDING_NEW";
      case OMOrderState::LIVE:
        return "LIVE";
      case OMOrderState::PENDING_CANCEL:
        return "PENDING_CANCEL";
      case OMOrderState::DEAD:
        return "DEAD";
      case OMOrderState::INVALID:
        return "INVALID";
    }

    return "UNKNOWN";
  }

  struct OMOrder {
    /*
    > A ticker_id_ variable of the TickerId type to represent which trading instrument this order is for
    > An order_id_ variable of the OrderId type, which is the unique order ID that’s been assigned to this order object
    > A side_ variable to hold the Side property of this order
    > The order’s Price is held in the price_ data member
    > The live or requested Qty for this order is saved in the qty_ variable
    > An order_state_ variable of the OMOrderState type, which we defined previously, to represent the current state of OMOrder
    */
    TickerId ticker_id_ = TickerId_INVALID;
    OrderId order_id_ = OrderId_INVALID;
    Side side_ = Side::INVALID;
    Price price_ = Price_INVALID;
    Qty qty_ = Qty_INVALID;
    OMOrderState order_state_ = OMOrderState::INVALID;

    auto toString() const {
      std::stringstream ss;
      ss << "OMOrder" << "["
         << "tid:" << tickerIdToString(ticker_id_) << " "
         << "oid:" << orderIdToString(order_id_) << " "
         << "side:" << sideToString(side_) << " "
         << "price:" << priceToString(price_) << " "
         << "qty:" << qtyToString(qty_) << " "
         << "state:" << OMOrderStateToString(order_state_) << "]";

      return ss.str();
    }
  };

    /*
    we define an OMOrderSideHashMap typedef to represent a std::array of OMOrder objects and indicate that the capacity 
    of this array is large enough to hold an entry for the buy side and another for the sell side. Objects of the 
    OMOrderSideHashMap type will be indexed by the sideToIndex(Side::BUY) and sideToIndex(Side::SELL) indices
    */
  typedef std::array<OMOrder, sideToIndex(Side::MAX) + 1> OMOrderSideHashMap;

  /*
  We must also define an OMOrderTickerSideHashMap, which is just another std::array of this OMOrderSideHashMap object 
  that’s large enough to hold all trading instruments – that is, of ME_MAX_TICKERS size
  */
  typedef std::array<OMOrderSideHashMap, ME_MAX_TICKERS> OMOrderTickerSideHashMap;
}