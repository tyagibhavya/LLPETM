#include "liquidity_taker.h"

#include "trade_engine.h"

namespace Trading {
    /*
    The constructor expects the following arguments – a Logger object, the TradeEngine object within which this algorithm runs, a 
    FeatureEngine object to compute the feature, an OrderManager object used to manage orders for this trading strategy, 
    and a TradeEngineCfgHashMap object containing the trading parameters for this strategy
    */
  LiquidityTaker::LiquidityTaker(Common::Logger *logger, TradeEngine *trade_engine, const FeatureEngine *feature_engine,
                                 OrderManager *order_manager,
                                 const TradeEngineCfgHashMap &ticker_cfg)
      : feature_engine_(feature_engine), order_manager_(order_manager), logger_(logger),
        ticker_cfg_(ticker_cfg) {
            /*
            > Overrides the callbacks in the TradeEngine object for order book updates, trade events, and updates to the 
            algorithm’s orders like the MarketMaker algorithm
            
            > The std::function members, algoOnOrderBookUpdate_, algoOnTradeUpdate_, and algoOnOrderUpdate_, in TradeEngine are 
            bound, respectively, to the onOrderBookUpdate, onTradeUpdate, and onOrderUpdate methods within LiquidityTaker using 
            lambda methods
            */
    trade_engine->algoOnOrderBookUpdate_ = [this](auto ticker_id, auto price, auto side, auto book) {
      onOrderBookUpdate(ticker_id, price, side, book);
    };
    trade_engine->algoOnTradeUpdate_ = [this](auto market_update, auto book) { onTradeUpdate(market_update, book); };
    trade_engine->algoOnOrderUpdate_ = [this](auto client_response) { onOrderUpdate(client_response); };
  }
}