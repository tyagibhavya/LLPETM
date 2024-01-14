/*
First, we will include the necessary header files and create some basic variables to represent 
the different components we need. Specifically, we will have a Logger object pointer to be used 
for logging purposes, a TradeEngine object pointer for the basic trading engine framework, a MarketDataConsumer 
object pointer to consumer market data, and an OrderGateway object pointer to connect to and communicate with 
the exchange’s order server:
*/
#include <csignal>

#include "strategy/trade_engine.h"
#include "order_gw/order_gateway.h"
#include "market_data/market_data_consumer.h"

#include "common/logging.h"

/// Main components.
Common::Logger *logger = nullptr;
Trading::TradeEngine *trade_engine = nullptr;
Trading::MarketDataConsumer *market_data_consumer = nullptr;
Trading::OrderGateway *order_gateway = nullptr;


/// We will accept the command line argument as follows: 
// ./trading_main CLIENT_ID ALGO_TYPE [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...
int main(int argc, char **argv) {
  if(argc < 3) {
    FATAL("USAGE trading_main CLIENT_ID ALGO_TYPE [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...");
  }
    /*Extracting the AlgoType*/
  const Common::ClientId client_id = atoi(argv[1]);
  srand(client_id);

  const auto algo_type = stringToAlgoType(argv[2]);

    /*
    We will initialize the component variables we declared before – Logger, the client_requests LFQueue, the client_responses 
    LFQueue, and the market_updates LFQueue. We will also define a sleep_time variable and set it to 20 microseconds. 
    We will use this value to pause between consecutive order requests we send to the trading exchange’s OrderGatewayServer 
    component, only in the random trading strategy
    */
  logger = new Common::Logger("trading_main_" + std::to_string(client_id) + ".log");

  const int sleep_time = 20 * 1000;

  // The lock free queues to facilitate communication between order gateway <-> trade engine and market data consumer -> trade engine.
  Exchange::ClientRequestLFQueue client_requests(ME_MAX_CLIENT_UPDATES);
  Exchange::ClientResponseLFQueue client_responses(ME_MAX_CLIENT_UPDATES);
  Exchange::MEMarketUpdateLFQueue market_updates(ME_MAX_MARKET_UPDATES);

  std::string time_str;
    /*
    nitialize an object of type TradeEngineCfgHashMap from the remaining command-line arguments
    */
  TradeEngineCfgHashMap ticker_cfg;

  // Parse and initialize the TradeEngineCfgHashMap above from the command line arguments.
  // [CLIP_1 THRESH_1 MAX_ORDER_SIZE_1 MAX_POS_1 MAX_LOSS_1] [CLIP_2 THRESH_2 MAX_ORDER_SIZE_2 MAX_POS_2 MAX_LOSS_2] ...
  size_t next_ticker_id = 0;
  for (int i = 3; i < argc; i += 5, ++next_ticker_id) {
    ticker_cfg.at(next_ticker_id) = {static_cast<Qty>(std::atoi(argv[i])), std::atof(argv[i + 1]),
                                     {static_cast<Qty>(std::atoi(argv[i + 2])),
                                      static_cast<Qty>(std::atoi(argv[i + 3])),
                                      std::atof(argv[i + 4])}};
  }
    /*
    The first component we will initialize and start will be TradeEngine. We will pass client_id, algo_type, the strategy 
    configurations in the ticker_cfg object, and the lock-free queues that TradeEngine needs in the constructor. We then 
    call the start() method to get the main thread to start executing, as shown in the following code block
    */
  logger->log("%:% %() % Starting Trade Engine...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
  trade_engine = new Trading::TradeEngine(client_id, algo_type,
                                          ticker_cfg,
                                          &client_requests,
                                          &client_responses,
                                          &market_updates);
  trade_engine->start();

    /*
    We perform a similar initialization of the OrderGateway component next by passing it the IP and port information 
    of exchange_main’s OrderGateway server component. We also pass it the client_requests and client_responses 
    LFQueue variables to consume MEClientRequest messages from and write MEClientResponse messages to, and then 
    we use start() on the main thread
    */
  const std::string order_gw_ip = "127.0.0.1";
  const std::string order_gw_iface = "lo";
  const int order_gw_port = 12345;

  logger->log("%:% %() % Starting Order Gateway...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
  order_gateway = new Trading::OrderGateway(client_id, &client_requests, &client_responses, order_gw_ip, order_gw_iface, order_gw_port);
  order_gateway->start();

    /*
    Finally, we initialize and start the MarketDataConsumer component. It needs the IP and port information of the snapshot stream 
    and the incremental stream on which the exchange’s MarketDataPublisher publishes market data. It also needs the 
    market_updates LFQueue variable,
    */
  const std::string mkt_data_iface = "lo";
  const std::string snapshot_ip = "233.252.14.1";
  const int snapshot_port = 20000;
  const std::string incremental_ip = "233.252.14.3";
  const int incremental_port = 20001;

  logger->log("%:% %() % Starting Market Data Consumer...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
  market_data_consumer = new Trading::MarketDataConsumer(client_id, &market_updates, mkt_data_iface, snapshot_ip, snapshot_port, incremental_ip, incremental_port);
  market_data_consumer->start();

    /*
    we are almost ready to start sending orders to the exchange; we just need to perform a few more minor tasks 
    first. First, the main() application will sleep briefly so that the threads we just created and started in 
    each of our components can run for a few seconds
    */
  usleep(10 * 1000 * 1000);
    /*
    We will also initialize the first event time in TradeEngine by calling the TradeEngine::initLastEventTime() 
    method. We intentionally delayed this member’s initialization until we were ready to start trading
    */
  trade_engine->initLastEventTime();

  // For the random trading algorithm, we simply implement it here instead of creating a new trading algorithm which is another possibility.
  // Generate random orders with random attributes and randomly cancel some of them.
  if (algo_type == AlgoType::RANDOM) {
    Common::OrderId order_id = client_id * 1000;
    /*
    Since we send orders with a random price, quantity, and side in our current test setup, we will initialize a random 
    reference price for each instrument, for which we will send orders. We will send orders with prices that are randomly distributed around this 
    reference price value shortly. We do this purely so that different trading instruments have orders of slightly different 
    and random prices. The random reference price for each instrument is held in the ticker_base_price variable. We will also 
    create std::vector of MEClientRequest messages to store the order requests we send to the exchange. We will also send 
    cancellations for some of these orders to exercise that functionality; hence, we will save them for when we try to cancel them
    */
    std::vector<Exchange::MEClientRequest> client_requests_vec;
    std::array<Price, ME_MAX_TICKERS> ticker_base_price;
    for (size_t i = 0; i < ME_MAX_TICKERS; ++i)
      ticker_base_price[i] = (rand() % 100) + 100;
    for (size_t i = 0; i < 10000; ++i) {
        /*
        For 10000 iterations:
        We will pick a random TickerId, generate a random Price close to the ticker_base_price reference price value 
        for that instrument, generate a random Qty, and generate a random Side for the order we will send:
        */
      const Common::TickerId ticker_id = rand() % Common::ME_MAX_TICKERS;
      const Price price = ticker_base_price[ticker_id] + (rand() % 10) + 1;
      const Qty qty = 1 + (rand() % 100) + 1;
      const Side side = (rand() % 2 ? Common::Side::BUY : Common::Side::SELL);

        /*
        We will create an MEClientRequest message of type ClientRequestType::NEW with these attributes and 
        pass it along to TradeEngine using the sendClientRequest() method call. We will pause for sleep_time 
        (20 microseconds) after we send the order request, and we will also save the MEClientRequest message 
        we just sent out in the client_requests_vec container
        */
      Exchange::MEClientRequest new_request{Exchange::ClientRequestType::NEW, client_id, ticker_id, order_id++, side,
                                            price, qty};
      trade_engine->sendClientRequest(&new_request);
      usleep(sleep_time);

      client_requests_vec.push_back(new_request);

      /*
      After the pause, we randomly pick a client request we sent from our container of client requests. 
      We change the request type to ClientRequestType::CANCEL and send it through to TradeEngine. Then, we pause 
      again and continue with the loop iteration
      */
      const auto cxl_index = rand() % client_requests_vec.size();
      auto cxl_request = client_requests_vec[cxl_index];
      cxl_request.type_ = Exchange::ClientRequestType::CANCEL;
      trade_engine->sendClientRequest(&cxl_request);
      usleep(sleep_time);

      if (trade_engine->silentSeconds() >= 60) {
        logger->log("%:% %() % Stopping early because been silent for % seconds...\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_str), trade_engine->silentSeconds());

        break;
      }
    }
  }


  while (trade_engine->silentSeconds() < 60) {
    logger->log("%:% %() % Waiting till no activity, been silent for % seconds...\n", __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&time_str), trade_engine->silentSeconds());

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(30s);
  }

    /*
    After a period of inactivity, this application exits. We first stop each of our 
    components and pause for a brief period, before de-initializing and exiting the application:
    */
  trade_engine->stop();
  market_data_consumer->stop();
  order_gateway->stop();

  using namespace std::literals::chrono_literals;
  std::this_thread::sleep_for(10s);

  delete logger;
  logger = nullptr;
  delete trade_engine;
  trade_engine = nullptr;
  delete market_data_consumer;
  market_data_consumer = nullptr;
  delete order_gateway;
  order_gateway = nullptr;

  std::this_thread::sleep_for(10s);

  exit(EXIT_SUCCESS);
}