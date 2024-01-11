/*
The market update structure is used by the matching engine to provide market
data updates to the market data publishing component. We also have a MEMarketUpdateLFQueue
type to represent a lock-free queue of the MEMarketUpdate objects
*/

#pragma once
#include<sstream>
#include "common/types.h"
using namespace Common ;
namespace Exchange{
    /*
   The MEMarketUpdate struct also needs to be a packed structure, 
   since it will be part of the message that is sent and received over the network; hence,
   we use the #pragma pack() directive again
    */
    #pragma pack(push,1)
    enum class MarketUpdateType : uint8_t {
        INVALID = 0,
        ADD = 1,
        MODIFY = 2,
        CANCEL = 3,
        TRADE = 4
    } ;

    inline std:: string marketUpdateTypeToString(MarketUpdateType type) {
        switch(type){
            case MarketUpdateType::INVALID: return "INVALID" ;
            case MarketUpdateType::ADD: return "ADD" ;
            case MarketUpdateType::TRADE: return "TRADE";
            case MarketUpdateType::CANCEL: return "CANCEL";
        }
        return "UNKNOWN" ;
    }

    struct MEMarketUpdate{
        MarketUpdateType type_ = MarketUpdateType::INVALID;
        OrderId order_id_ = OrderId_INVALID;
        TickerId ticker_id_ = TickerId_INVALID;
        Side side_ = Side::INVALID;
        Price price_ = Price_INVALID;
        Qty qty_ = Qty_INVALID;
        Priority priority_ = Priority_INVALID;
        /*
        A priority_ field of type Priority, which, as we
        discussed before, will be used to specify the exact position of this order in 
        the FIFO queue. We build a FIFO queue of all orders at the same price. This
        field specifies the position/location of this order in that queue.
        */
        auto toString() const {
        std::stringstream ss;
        ss << "MEMarketUpdate"
            << " ["
            << " type:" << marketUpdateTypeToString(type_)
            << " ticker:" << tickerIdToString(ticker_id_)
            << " oid:" << orderIdToString(order_id_)
            << " side:" << sideToString(side_)
            << " qty:" << qtyToString(qty_)
            << " price:" << priceToString(price_)
            << " priority:" << priorityToString(priority_)
            << "]";
        return ss.str();
    }
}