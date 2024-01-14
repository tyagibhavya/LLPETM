#include "market_maker.h"

#include "trade_engine.h"

namespace Trading
{
    /*
    > A Logger object, which will be saved in the logger_ member variable and used for logging purposes
    
    > A pointer to a TradeEngine object, which will be used to bind the algoOnOrderBookUpdate, algoOnTradeUpdate, 
    and algoOnOrderUpdate callbacks in the parent TradeEngine instance to the corresponding methods in the 
    MarketMaker object. This is so that the MarketMaker trading strategy receives and processes the callbacks 
    when TradeEngine receives them
    
    > A pointer to a constant FeatureEngine object, which will be stored in the feature_engine_ data member and used 
    to extract the feature values this algorithm needs, as described before.
    
    > A pointer to an OrderManager object, which will be used to manage the orders for this strategy, and the 
    constructor will simply be saved in the order_manager_ data member.
    
    > A reference to a constant TradeEngineCfgHashMap, which will be saved in the ticker_cfg_ member and used to make 
    trading decisions, since this contains the trading parameters
    */
    MarketMaker::MarketMaker(Common::Logger *logger, TradeEngine *trade_engine, const FeatureEngine *feature_engine,
                             OrderManager *order_manager, const TradeEngineCfgHashMap &ticker_cfg)
        : feature_engine_(feature_engine), order_manager_(order_manager), logger_(logger),
          ticker_cfg_(ticker_cfg)
    {
        /*
        we will override the TradeEngine: :algoOnOrderBookUpdate(), TradeEngine::algoOnTradeUpdate(), and 
        TradeEngine::algoOnOrderUpdate() methods using lambda methods to forward them to the MarketMaker::onOrderBookUpdate(), 
        MarketMaker::onTradeUpdate(), and MarketMaker::onOrderUpdate() methods, respectively
        */
        trade_engine->algoOnOrderBookUpdate_ = [this](auto ticker_id, auto price, auto side, auto book)
        {
            onOrderBookUpdate(ticker_id, price, side, book);
        };

       
        trade_engine->algoOnTradeUpdate_ = [this](auto market_update, auto book)
        { onTradeUpdate(market_update, book); };
        trade_engine->algoOnOrderUpdate_ = [this](auto client_response)
        { onOrderUpdate(client_response); };
    }
}