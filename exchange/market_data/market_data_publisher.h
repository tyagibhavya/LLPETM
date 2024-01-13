#pragma once

#include <functional>

#include "market_data/snapshot_synthesizer.h"

namespace Exchange {
  class MarketDataPublisher {
  public:
    MarketDataPublisher(MEMarketUpdateLFQueue *market_updates, const std::string &iface,const std::string &snapshot_ip, int snapshot_port,const std::string &incremental_ip, int incremental_port);
    
    /*
    The destructor calls the stop() method to stop the running MarketDataPublisher thread, then waits a short amount of time 
    to let the thread finish any pending tasks and deletes the SnapshotSynthesizer object
    */
    ~MarketDataPublisher() {
      stop();

      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(5s);

      delete snapshot_synthesizer_;
      snapshot_synthesizer_ = nullptr;
    }


    /*
    it sets the run_ flag to true, then creates and launches a new thread and assigns the run() method to that thread, 
    which will be our main run loop for the MarketDataPublisher component. It also calls the start() method on the 
    SnapshotSynthesizer object so that the SnapshotSynthesizer thread can also be launched
    */
    auto start() {
      run_ = true;

      ASSERT(Common::createAndStartThread(-1, "Exchange/MarketDataPublisher", [this]() { run(); }) != nullptr, "Failed to start MarketData thread.");

      snapshot_synthesizer_->start();
    }

    /*
    his method simply sets the run_ flag to false and instructs the SnapshotSynthesizer thread 
    to stop as well
    */
    auto stop() -> void {
      run_ = false;

      snapshot_synthesizer_->stop();
    }

    auto run() noexcept -> void;

    // Deleted default, copy & move constructors and assignment-operators.
    MarketDataPublisher() = delete;

    MarketDataPublisher(const MarketDataPublisher &) = delete;

    MarketDataPublisher(const MarketDataPublisher &&) = delete;

    MarketDataPublisher &operator=(const MarketDataPublisher &) = delete;

    MarketDataPublisher &operator=(const MarketDataPublisher &&) = delete;

  private:


    size_t next_inc_seq_num_ = 1; //represents the sequence number to set on the next outgoing incremental market data message
    MEMarketUpdateLFQueue *outgoing_md_updates_ = nullptr; //a lock-free queue of MEMarketUpdate messages

    MDPMarketUpdateLFQueue snapshot_md_updates_; 
    /*
    a lock-free queue containing MDPMarketUpdate messages. This queue is used by the market data publisher
    thread to publish MDPMarketUpdate messages that it sends on the incremental stream to the SnapshotSynthesizer
    component. This LFQueue is necessary since SnapshotSynthesizer runs on a different thread than MarketDataPublisher, 
    which is primarily so that the snapshot synthesis and publishing process do not slow down the latency-sensitive 
    MarketDataPublisher component.
    */

    volatile bool run_ = false;
    /*
    used to control when the MarketDataPublisher thread is started and stopped. Since it is accessed from
    different threads, like the run_ variable in the OrderServer class, it is also marked as volatile
    */

    std::string time_str_;
    Logger logger_;

    Common::McastSocket incremental_socket_; //to be used to publish UDP messages on the incremental multicast stream

    SnapshotSynthesizer *snapshot_synthesizer_ = nullptr;
    /*
    This object will be responsible for generating a snapshot of the limit
    order book from the updates that the matching engine provides and periodically publishing a snapshot of the full order book
    on the snapshot multicast stream
    */
    
  };
}