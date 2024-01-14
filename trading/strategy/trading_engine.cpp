#include "trade_engine.h"

namespace Trading {
    /*
    The constructor for the TradeEngine class requires a ClientId argument to identify the trading application used in the
     client order requests. It also needs pointers to the three LFQueue of types ClientRequestLFQueue, ClientResponseLFQueue, 
     and MEMarketUpdateLFQueue to initialize the outgoing_ogw_requests_, incoming_ogw_responses_ and incoming_md_updates_ 
     data members, respectively. It also needs an algo_type argument of type AlgoType to specify the type of trading strategy
    and a ticker_cfg argument of type reference-to const TradeEngineCfgHashMap, which contains the configuration parameters 
    for the risk manager and the trading strategy.
    The constructor also initializes the Logger logger_ member variable with a log file and creates a MarketOrderBook
    component for each possible TickerId value, holding them in the ticker_order_book_ container. It calls the 
    setTradeEngine() method on each MarketOrderBook component so that callbacks from the book can be received in TradeEngine. 
    We also initialize the data members corresponding to the trading sub-components – feature_engine_, 
    position_keeper_, order_manager_, and risk_manager_
    */
  TradeEngine::TradeEngine(Common::ClientId client_id,
                           AlgoType algo_type,
                           const TradeEngineCfgHashMap &ticker_cfg,
                           Exchange::ClientRequestLFQueue *client_requests,
                           Exchange::ClientResponseLFQueue *client_responses,
                           Exchange::MEMarketUpdateLFQueue *market_updates)
      : client_id_(client_id), outgoing_ogw_requests_(client_requests), incoming_ogw_responses_(client_responses),
        incoming_md_updates_(market_updates), logger_("trading_engine_" + std::to_string(client_id) + ".log"),
        feature_engine_(&logger_),
        position_keeper_(&logger_),
        order_manager_(&logger_, this, risk_manager_),
        risk_manager_(&logger_, &position_keeper_, ticker_cfg) {
    for (size_t i = 0; i < ticker_order_book_.size(); ++i) {
      ticker_order_book_[i] = new MarketOrderBook(i, &logger_);
      ticker_order_book_[i]->setTradeEngine(this);
    }

    // Initialize the function wrappers declared in the header file for the callbacks for order book changes, trade events and client responses.
    algoOnOrderBookUpdate_ = [this](auto ticker_id, auto price, auto side, auto book) {
      defaultAlgoOnOrderBookUpdate(ticker_id, price, side, book);
    };
    algoOnTradeUpdate_ = [this](auto market_update, auto book) { defaultAlgoOnTradeUpdate(market_update, book); };
    algoOnOrderUpdate_ = [this](auto client_response) { defaultAlgoOnOrderUpdate(client_response); };

    // Create the trading algorithm instance based on the AlgoType provided.
    // The constructor will override the callbacks above for order book changes, trade events and client responses
    
    /*
    we will initialize a trading strategy instance, either mm_algo_ of type MarketMaker or taker_algo_ of the LiquidityTaker 
    type trading strategy. This initialization is shown as follows; remember that the MarketMaker or LiquidityTaker object 
    will update/override the members – algoOnOrderBookUpdate_, algoOnTradeUpdate_, and algoOnOrderUpdate_ – to point to their 
    own method implementations
    */
    if (algo_type == AlgoType::MAKER) {
      mm_algo_ = new MarketMaker(&logger_, this, &feature_engine_, &order_manager_, ticker_cfg);
    } else if (algo_type == AlgoType::TAKER) {
      taker_algo_ = new LiquidityTaker(&logger_, this, &feature_engine_, &order_manager_, ticker_cfg);
    }

    for (TickerId i = 0; i < ticker_cfg.size(); ++i) {
      logger_.log("%:% %() % Initialized % Ticker:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                  Common::getCurrentTimeStr(&time_str_),
                  algoTypeToString(algo_type), i,
                  ticker_cfg.at(i).toString());
    }
  }

  TradeEngine::~TradeEngine() {
    /*
    The destructor does some simple de-initialization of the variables. First, it sets 
    the run_ flag to false and waits a little bit to let the main thread exit, then it proceeds 
    to delete each MarketOrderBook instance and clear out the ticker_order_book_ container, 
    and finally, it resets the LFQueue pointers it holds. It also deletes the mm_algo_ and 
    taker_algo_ members corresponding to the trading strategies
    */
    run_ = false;

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);

    delete mm_algo_; mm_algo_ = nullptr;
    delete taker_algo_; taker_algo_ = nullptr;

    for (auto &order_book: ticker_order_book_) {
      delete order_book;
      order_book = nullptr;
    }

    outgoing_ogw_requests_ = nullptr;
    incoming_ogw_responses_ = nullptr;
    incoming_md_updates_ = nullptr;
  }

  /// Write a client request to the lock free queue for the order server to consume and send to the exchange.
  auto TradeEngine::sendClientRequest(const Exchange::MEClientRequest *client_request) noexcept -> void {
    logger_.log("%:% %() % Sending %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                client_request->toString().c_str());
    auto next_write = outgoing_ogw_requests_->getNextToWriteTo();
    *next_write = std::move(*client_request);
    outgoing_ogw_requests_->updateWriteIndex();
  }

  /// Main loop for this thread - processes incoming client responses and market data updates which in turn may generate client requests.
  auto TradeEngine::run() noexcept -> void {
    logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
    while (run_) {
      for (auto client_response = incoming_ogw_responses_->getNextToRead(); client_response; client_response = incoming_ogw_responses_->getNextToRead()) {
        logger_.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                    client_response->toString().c_str());
        onOrderUpdate(client_response);
        incoming_ogw_responses_->updateReadIndex();
        last_event_time_ = Common::getCurrentNanos();
      }

      for (auto market_update = incoming_md_updates_->getNextToRead(); market_update; market_update = incoming_md_updates_->getNextToRead()) {
        logger_.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                    market_update->toString().c_str());
        ASSERT(market_update->ticker_id_ < ticker_order_book_.size(),
               "Unknown ticker-id on update:" + market_update->toString());
        ticker_order_book_[market_update->ticker_id_]->onMarketUpdate(market_update);
        incoming_md_updates_->updateReadIndex();
        last_event_time_ = Common::getCurrentNanos();
      }
    }
  }

  /// Process changes to the order book - updates the position keeper, feature engine and informs the trading algorithm about the update.
  auto TradeEngine::onOrderBookUpdate(TickerId ticker_id, Price price, Side side, MarketOrderBook *book) noexcept -> void {
    logger_.log("%:% %() % ticker:% price:% side:%\n", __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&time_str_), ticker_id, Common::priceToString(price).c_str(),
                Common::sideToString(side).c_str());

    auto bbo = book->getBBO();

    position_keeper_.updateBBO(ticker_id, bbo);

    feature_engine_.onOrderBookUpdate(ticker_id, price, side, book);

    algoOnOrderBookUpdate_(ticker_id, price, side, book);
  }

  /// Process trade events - updates the  feature engine and informs the trading algorithm about the trade event.
  auto TradeEngine::onTradeUpdate(const Exchange::MEMarketUpdate *market_update, MarketOrderBook *book) noexcept -> void {
    logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                market_update->toString().c_str());

    feature_engine_.onTradeUpdate(market_update, book);

    algoOnTradeUpdate_(market_update, book);
  }

  /// Process client responses - updates the position keeper and informs the trading algorithm about the response.
  auto TradeEngine::onOrderUpdate(const Exchange::MEClientResponse *client_response) noexcept -> void {
    logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                client_response->toString().c_str());

    if (UNLIKELY(client_response->type_ == Exchange::ClientResponseType::FILLED)) {
      position_keeper_.addFill(client_response);
    }

    algoOnOrderUpdate_(client_response);
  }
}