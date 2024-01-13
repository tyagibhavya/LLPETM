

#pragma once

#include "common/types.h"
#include "common/thread_utils.h"
#include "common/lf_queue.h"
#include "common/macros.h"
#include "common/mcast_socket.h"
#include "common/mem_pool.h"
#include "common/logging.h"

#include "market_data/market_update.h"
#include "matcher/me_order.h"

using namespace Common;

namespace Exchange {
  class SnapshotSynthesizer {
  public:
    SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const std::string &iface,
                        const std::string &snapshot_ip, int snapshot_port);

    ~SnapshotSynthesizer();

    auto start() -> void;

    auto stop() -> void;

    auto addToSnapshot(const MDPMarketUpdate *market_update);

    auto publishSnapshot();

    auto run() -> void;

    // Deleted default, copy & move constructors and assignment-operators.
    SnapshotSynthesizer() = delete;

    SnapshotSynthesizer(const SnapshotSynthesizer &) = delete;

    SnapshotSynthesizer(const SnapshotSynthesizer &&) = delete;

    SnapshotSynthesizer &operator=(const SnapshotSynthesizer &) = delete;

    SnapshotSynthesizer &operator=(const SnapshotSynthesizer &&) = delete;

  private:
    MDPMarketUpdateLFQueue *snapshot_md_updates_ = nullptr; //is what MarketDataPublisher uses to publish incremental MDPMarketUpdates to this component

    Logger logger_;

    volatile bool run_ = false;
  /*
  serves a similar purpose as the run_ variables in the OrderServer and MarketDataPublisher components we built before. This will be used to start 
  and stop the SnapshotSynthesizer thread and will be marked volatile since it will be accessed from multiple threads:
  */
    std::string time_str_;

    McastSocket snapshot_socket_; //an McastSocket to be used to publish snapshot market data updates to the snapshot multicast stream

    std::array<std::array<MEMarketUpdate *, ME_MAX_ORDER_IDS>, ME_MAX_TICKERS> ticker_orders_;
    /*
    a std::array of size ME_MAX_TICKERS to represent the snapshot of the book for each trading instrument. Each 
    element of this array is a std::array of MEMarketUpdate pointers and a maximum size of ME_MAX_ORDER_IDS to represent 
    a hash map from OrderId to the order corresponding to that OrderId. As we have done before, we use the 
    first std::array as a hash map from TickerId to the snapshot of the limit order book. The second std::array is 
    also a hash map from OrderId to the order information. We will also have an order_pool_ data member of the MemPool 
    type of MEMarketUpdate objects. This memory pool is what we will use to allocate and deallocate MEMarketUpdate 
    objects from as we update the order book snapshot in the ticker_orders_ container.
    */



   /*
   Below we have two variables to track information about the last incremental market data update that SnapshotSynthesizer 
   has processed. The first one is the last_inc_seq_num_ variable to track the sequence number on the last incremental 
   MDPMarketUpdate it has received. The second one is the last_snapshot_time_ variable used to track when the last snapshot 
   was published over UDP since this component will only periodically publish the full snapshot of all the books.
   */
    size_t last_inc_seq_num_ = 0;
    Nanos last_snapshot_time_ = 0;

    MemPool<MEMarketUpdate> order_pool_;
  };
}