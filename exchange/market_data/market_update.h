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
            
        }
    }
}