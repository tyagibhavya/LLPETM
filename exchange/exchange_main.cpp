#include <csignal>

#include "matcher/matching_engine.h"
#include "market_data/market_data_publisher.h"
#include "order_server/order_server.h"

Common::Logger *logger = nullptr;
Exchange::MatchingEngine *matching_engine = nullptr;
Exchange::MarketDataPublisher *market_data_publisher = nullptr;
Exchange::OrderServer *order_server = nullptr;

/*
 Installs the signal_handler() method using the std::signal() routine to trap external SIGINT signals.
 The SIGINT signal is the signal value 2, which is sent to a running process when either Ctrl + C is pressed in Linux
 or kill –2 PID is sent to that Process ID (PID).

 The signal handler simply deletes these objects and exits.

 (main) This is the common way to terminate processes gracefully.
*/

void signal_handler(int) {
  using namespace std::literals::chrono_literals;
  std::this_thread::sleep_for(10s);

  delete logger;
  logger = nullptr;
  delete matching_engine;
  matching_engine = nullptr;
  delete market_data_publisher;
  market_data_publisher = nullptr;
  delete order_server;
  order_server = nullptr;

  std::this_thread::sleep_for(10s);

  exit(EXIT_SUCCESS);
}

int main(int, char **) {
  logger = new Common::Logger("exchange_main.log");

  std::signal(SIGINT, signal_handler);

  const int sleep_time = 100 * 1000;

  Exchange::ClientRequestLFQueue client_requests(ME_MAX_CLIENT_UPDATES);
  Exchange::ClientResponseLFQueue client_responses(ME_MAX_CLIENT_UPDATES);
  Exchange::MEMarketUpdateLFQueue market_updates(ME_MAX_MARKET_UPDATES);

  std::string time_str;
  
  /* Initialising matching engine. */
  logger->log("%:% %() % Starting Matching Engine...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
  matching_engine = new Exchange::MatchingEngine(&client_requests, &client_responses, &market_updates);
  matching_engine->start();

  const std::string mkt_pub_iface = "lo";
  const std::string snap_pub_ip = "233.252.14.1", inc_pub_ip = "233.252.14.3";
  const int snap_pub_port = 20000, inc_pub_port = 20001;
  
  /* Initialising market data publisher. */
  logger->log("%:% %() % Starting Market Data Publisher...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
  market_data_publisher = new Exchange::MarketDataPublisher(&market_updates, mkt_pub_iface, snap_pub_ip, snap_pub_port, inc_pub_ip, inc_pub_port);
  market_data_publisher->start();

  const std::string order_gw_iface = "lo";
  const int order_gw_port = 12345;
  
  /* Initialising order server. */
  logger->log("%:% %() % Starting Order Server...\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
  order_server = new Exchange::OrderServer(&client_requests, &client_responses, order_gw_iface, order_gw_port);
  order_server->start();

  while (true) {
    logger->log("%:% %() % Sleeping for a few milliseconds..\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str));
    usleep(sleep_time * 1000);
  }
}
