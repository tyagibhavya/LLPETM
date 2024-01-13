/*
This file needs more reading
*/


#include "order_manager.h"
#include "trade_engine.h"

namespace Trading {
    /*
    The OrderManager::newOrder() method is the lower-level method in our order manager class. It requires a pointer to an 
    OMOrder object for which this new order is being sent. It also needs the TickerId, Price, Side, and Qty attributes 
    to be set on the new order that’s being sent out:
    */
  auto OrderManager::newOrder(OMOrder *order, TickerId ticker_id, Price price, Side side, Qty qty) noexcept -> void {

    /*
    It creates a MEClientRequest structure of the ClientRequestType::NEW type and fills in the attributes that are 
    passed through the arguments, sets OrderId to be next_order_id_ and ClientId to be the client ID of TradeEngine, 
    which can be obtained by calling the clientId() method. It also calls TradeEngine::sendClientRequest() and provides 
    the MEClientRequest object (new_request) it just initialized:
    */
    const Exchange::MEClientRequest new_request{Exchange::ClientRequestType::NEW, trade_engine_->clientId(), ticker_id,
                                                next_order_id_, side, price, qty};
    trade_engine_->sendClientRequest(&new_request);
    /*
    Finally, it updates the OMOrder object pointer it was provided in the method parameters and assigns it the attributes 
    that were just set on the new order that was sent out. Note that the state of this OMOrder is set to OMOrderState::PENDING_NEW 
    since it will be sent out shortly but will not be active until the exchange accepts it and we receive that response. It also 
    increments the next_order_id_ variable to maintain uniqueness on any new orders that might be sent out later:
    */
    *order = {ticker_id, next_order_id_, side, price, qty, OMOrderState::PENDING_NEW};
    ++next_order_id_;

    logger_->log("%:% %() % Sent new order % for %\n", __FILE__, __LINE__, __FUNCTION__,
                 Common::getCurrentTimeStr(&time_str_),
                 new_request.toString().c_str(), order->toString().c_str());
  }

  auto OrderManager::cancelOrder(OMOrder *order) noexcept -> void {

    /*
    OrderManager::cancelOrder() is the lower-level method in our order manager class and will be used to send a 
    cancel request for live orders being managed by OrderManager. It only accepts a single parameter, which is the 
    OMOrder object for which it is going to send the cancel request
    */

   /*
   Like the newOrder() method, we must initialize an MEClientRequest client_request object of the ClientRequestType::CANCEL
   type and populate the attributes in it from the OMOrder object that was passed into the method. It calls 
   the TradeEngine::sendClientRequest() method.to send the cancel request out. One thing to understand is that the next_order_id_ 
   member variable is only used for generating new order IDs for new outgoing order requests. Cancelling an existing order does not 
   change the next_order_id_ variable, as shown in the following code block. In our design, next_order_id_ keeps incrementing sequentially 
   each time we send an MEClientRequest of the ClientRequestType::NEW type. 
   
   ===================================================================================================================
   Theoretically, we could reuse the order_id_ value from the order we just cancelled on the next new order request, but that would 
   require us to track the free order IDs, which is not too difficult either. This was just a design choice we made, but feel free 
   to modify this scheme and track free order IDs if you wish
   ====================================================================================================================
   */

    const Exchange::MEClientRequest cancel_request{Exchange::ClientRequestType::CANCEL, trade_engine_->clientId(),
                                                   order->ticker_id_, order->order_id_, order->side_, order->price_,
                                                   order->qty_};
    trade_engine_->sendClientRequest(&cancel_request);

    /*
    Finally, we must update the order_state_ value of the OMOrder object to OMOrderState::PENDING_CANCEL to represent 
    the fact that a cancel request has been sent out
    */

    order->order_state_ = OMOrderState::PENDING_CANCEL;

    logger_->log("%:% %() % Sent cancel % for %\n", __FILE__, __LINE__, __FUNCTION__,
                 Common::getCurrentTimeStr(&time_str_),
                 cancel_request.toString().c_str(), order->toString().c_str());
  }
}