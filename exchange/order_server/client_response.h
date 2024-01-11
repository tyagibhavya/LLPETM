#pragma once
#include<sstream>
#include "common/types.h"
#include "common/lf_queue.h"

using namespace Common;

namespace Exchange {
#pragma pack(push, 1)
/*
We now try to save space, limit the padding as this is the information that will flow 
through the channel and we want it to be as small as possible. So we
avoid padding!
*/
  enum class ClientResponseType : uint8_t {
    INVALID = 0,
    ACCEPTED = 1,
    CANCELED = 2,
    FILLED = 3,
    CANCEL_REJECTED = 4
  };

/*
A ClientResponseType enumeration to represent the type of response for
client orders. In addition to the INVALID sentinel value, it contains 
values that represent when a request for a new order is accepted, an order
is canceled, an order is executed, or a cancel request is rejected by the matching engine
*/
  inline std::string clientResponseTypeToString(ClientResponseType type) {
    switch (type) {
      case ClientResponseType::ACCEPTED:
        return "ACCEPTED";
      case ClientResponseType::CANCELED:
        return "CANCELED";
      case ClientResponseType::FILLED:
        return "FILLED";
      case ClientResponseType::CANCEL_REJECTED:
        return "CANCEL_REJECTED";
      case ClientResponseType::INVALID:
        return "INVALID";
    }
    return "UNKNOWN";
  }

  struct MEClientResponse {
    ClientResponseType type_ = ClientResponseType::INVALID;
    ClientId client_id_ = ClientId_INVALID;
    TickerId ticker_id_ = TickerId_INVALID;
    OrderId client_order_id_ = OrderId_INVALID;
    OrderId market_order_id_ = OrderId_INVALID;
    Side side_ = Side::INVALID;
    Price price_ = Price_INVALID;
    Qty exec_qty_ = Qty_INVALID;
    Qty leaves_qty_ = Qty_INVALID;

    auto toString() const {
      std::stringstream ss;
      ss << "MEClientResponse"
         << " ["
         << "type:" << clientResponseTypeToString(type_)
         << " client:" << clientIdToString(client_id_)
         << " ticker:" << tickerIdToString(ticker_id_)
         << " coid:" << orderIdToString(client_order_id_)
         << " moid:" << orderIdToString(market_order_id_)
         << " side:" << sideToString(side_)
         << " exec_qty:" << qtyToString(exec_qty_)
         << " leaves_qty:" << qtyToString(leaves_qty_)
         << " price:" << priceToString(price_)
         << "]";
      return ss.str();
    }
  };

#pragma pack(pop)

  typedef LFQueue<MEClientResponse> ClientResponseLFQueue;
}