/*
Aditya : Created client_request.h file
*/

#pragma once
#include<sstream>
#include "common/types.h"
#include "common/lf_queue.h"


using namespace Common;

namespace Exchange{
    #pragma pack(push, 1) 
    /*
    we use the #pragma pack() directive to make sure these structures are packed and
    do not contain any extra padding. This is important because these will be sent and
    received over a network as flat binary structures
    */


   enum class ClientRequestType : uint8_t {
    INVALID = 0,
    NEW = 1,
    CANCEL =2
   };
   /*
   ClientRequestType enumeration to define what type of order request it is – whether it is 
   a new order or a cancel request for an existing order.
   */

   inline std::string clientRequestTypeToString(ClientRequestType type){
    switch(type){
        case ClientRequestType :: NEW : return "NEW";
        case ClientRequestType :: CANCEL : return "CANCEL";
        case ClientRequestType :: INVALID : return "INVALID"; 
    }
    return "UNKNOWN";
   }

   struct MEClientRequest{
    ClientRequestType type_ = ClientRequestType::INVALID ;
    ClientId client_id_ = ClientId_INVALID;
    TickerId ticker_id_ = TickerId_INVALID;
    OrderId order_id_ = OrderId_INVALID;
    Side side_ = Side::INVALID;
    Price price_ = Price_INVALID;
    Qty qty_ = Qty_INVALID;
    auto toString() const {
      std::stringstream ss;
      ss << "MEClientRequest"
         << " ["
         << "type:" << clientRequestTypeToString(type_)
         << " client:" << clientIdToString(client_id_)
         << " ticker:" << tickerIdToString(ticker_id_)
         << " oid:" << orderIdToString(order_id_)
         << " side:" << sideToString(side_)
         << " qty:" << qtyToString(qty_)
         << " price:" << priceToString(price_)
         << "]";
      return ss.str();
    }
  
   };

  /*
     Client request structure published over the network by the order gateway client.
     OMClientRequest has a sequence number field (seq_num_)
*/
  struct OMClientRequest {
    size_t seq_num_ = 0;
    MEClientRequest me_client_request_;

    auto toString() const {
      std::stringstream ss;
      ss << "OMClientRequest"
         << " ["
         << "seq:" << seq_num_
         << " " << me_client_request_.toString()
         << "]";
      return ss.str();
    }
  };

   #pragma pack(pop)
   typedef LFQueue<MEClientRequest> ClientRequestLFQueue ;

   /*
   The MEClientRequest structure is used by the order server to forward order requests from
   the clients to the matching engine. Remember that the communication from the order server
   to the matching engine is established through the lock-free queue component we built earlier.
   ClientRequestLFQueue is a typedef for a lock-free queue of MEClientRequest objects
   
    We define the MEClientRequest structure, which will contain information 
    for a single order request from the trading participant to the exchange. Note
    that this is the internal representation that the matching engine uses, not necessarily
    the exact format that the client sends.
    */
  
}
