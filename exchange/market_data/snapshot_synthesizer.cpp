

#include "snapshot_synthesizer.h"

namespace Exchange {

  /*
  The SnapshotSynthesizer constructor takes an argument of the MDPMarketUpdateLFQueue type passed to it from the MarketDataPublisher component. It 
  also receives the network interface name and the snapshot IP and port to represent the multicast stream. The constructor initializes 
  the snapshot_md_updates_ data member from the argument passed to it and initializes logger_ with a new filename. It initializes 
  MEMarketUpdate MemPool to be of the size ME_MAX_ORDER_IDS. It also initializes snapshot_socket_ and configures it to publish messages on 
  the snapshot multicast IP and port on the provided network interface  
  */
  SnapshotSynthesizer::SnapshotSynthesizer(MDPMarketUpdateLFQueue *market_updates, const std::string &iface,
                                           const std::string &snapshot_ip, int snapshot_port)
      : snapshot_md_updates_(market_updates), logger_("exchange_snapshot_synthesizer.log"), snapshot_socket_(logger_), order_pool_(ME_MAX_ORDER_IDS) {
    ASSERT(snapshot_socket_.init(snapshot_ip, iface, snapshot_port, /*is_listening*/ false) >= 0,
           "Unable to create snapshot mcast socket. error:" + std::string(std::strerror(errno)));
    for(auto& orders : ticker_orders_)
      orders.fill(nullptr);
  }


  SnapshotSynthesizer::~SnapshotSynthesizer() {
    stop();
  }

  /*
  This start() method sets the run_ flag to true, creates and launches a thread, and assigns the run() method to the thread
  */
  void SnapshotSynthesizer::start() {
    run_ = true;
    ASSERT(Common::createAndStartThread(-1, "Exchange/SnapshotSynthesizer", [this]() { run(); }) != nullptr,
           "Failed to start SnapshotSynthesizer thread.");
  }

  void SnapshotSynthesizer::stop() {
    run_ = false;
  }
  /*
  The process of synthesizing the snapshot of the order books for the different trading instruments is like building 
  OrderBook. However, the difference here is that the snapshot synthesis process only needs to maintain the last state of the 
  live orders, so it is a simpler container. The addToSnapshot() method we will build next receives an MDPMarketUpdate message 
  every time there is a new incremental market data update provided to SnapshotSynthesizer
  */
  auto SnapshotSynthesizer::addToSnapshot(const MDPMarketUpdate *market_update) {
    const auto &me_market_update = market_update->me_market_update_;
    auto *orders = &ticker_orders_.at(me_market_update.ticker_id_);
    switch (me_market_update.type_) {

      /*
      Above in the function so far,
      we extract the MEMarketUpdate piece of the MDPMarketUpdate message and store it in the me_market_update variable. It also 
      finds the std::array of MEMarketUpdate messages for the correct TickerId for this instrument from the ticker_orders_ 
      std::array hash map. We then have a switch case on the type of MarketUpdateType and then handle each of those cases 
      individually. Before we look at each of the cases under the switch case
      */
      case MarketUpdateType::ADD: {

        /*
        To handle a MarketUpdateType::ADD message, we simply insert it into the MEMarketUpdate std::array at the correct 
        OrderId location. We create a MEMarketUpdate message by allocating it from the order_pool_ memory pool using the allocate() 
        call and passing it the MEMarketUpdate object to copy the fields from
        */
        auto order = orders->at(me_market_update.order_id_);
        ASSERT(order == nullptr, "Received:" + me_market_update.toString() + " but order already exists:" + (order ? order->toString() : ""));
        orders->at(me_market_update.order_id_) = order_pool_.allocate(me_market_update);
      }
        break;


      case MarketUpdateType::MODIFY: {
        /*
        Similar to ADD ;
        The minor difference here is that we just update the qty_ and price_ fields and leave the type_ field on the entry as is
        */
        auto order = orders->at(me_market_update.order_id_);
        ASSERT(order != nullptr, "Received:" + me_market_update.toString() + " but order does not exist.");
        ASSERT(order->order_id_ == me_market_update.order_id_, "Expecting existing order to match new one.");
        ASSERT(order->side_ == me_market_update.side_, "Expecting existing order to match new one.");

        order->qty_ = me_market_update.qty_;
        order->price_ = me_market_update.price_;
      }
        break;


      case MarketUpdateType::CANCEL: {
        /*
        Here, we find MEMarketUpdate in the hash map and call deallocate() on it. We also set the entry in the 
        hash map style std::array to nullptr to mark it as canceled or a dead order
        */
        auto order = orders->at(me_market_update.order_id_);
        ASSERT(order != nullptr, "Received:" + me_market_update.toString() + " but order does not exist.");
        ASSERT(order->order_id_ == me_market_update.order_id_, "Expecting existing order to match new one.");
        ASSERT(order->side_ == me_market_update.side_, "Expecting existing order to match new one.");

        order_pool_.deallocate(order);
        orders->at(me_market_update.order_id_) = nullptr;
      }
        break;


        /*
        We do not need to do anything with the other enumeration values, so we ignore them. We just update 
        the last sequence number we have seen on the incremental market data stream, which is stored 
        in the last_inc_seq_num_ data members
        */
      case MarketUpdateType::SNAPSHOT_START:
      case MarketUpdateType::CLEAR:
      case MarketUpdateType::SNAPSHOT_END:
      case MarketUpdateType::TRADE:
      case MarketUpdateType::INVALID:
        break;
    }



    ASSERT(market_update->seq_num_ == last_inc_seq_num_ + 1, "Expected incremental seq_nums to increase.");
    last_inc_seq_num_ = market_update->seq_num_;
  }
  /*
  The below function is called whenever we want to publish a complete snapshot of the current state of the order book
  */
  auto SnapshotSynthesizer::publishSnapshot() {
    size_t snapshot_size = 0;
    /*
    The first MDPMarketUpdate message is of the MarketUpdateType::SNAPSHOT_START type with seq_num_ = 0 to mark the 
    beginning of the snapshot messages
    
    Then, for each instrument, we publish the following:

    > A MDPMarketUpdate message of the MarketUpdateType::CLEAR type to instruct the client to clear their order book before applying the messages that follow
    
    > For each order that exists in the snapshot for this instrument, we publish a MDPMarketUpdate message with MarketUpdateType::ADD till we have published the information for all the orders
    */

    /*
    Finally, we publish a MDPMarketUpdate message of the MarketUpdateType::SNAPSHOT_END type to mark the end of the snapshot messages. One thing to note is that for the SNAPSHOT_START and
    SNAPSHOT_END messages, we set the OrderId value to be the last incremental sequence number that was used to construct this snapshot. The market participants will use this sequence number 
    to synchronize the snapshot market data stream with the incremental market data stream.
    */


   /*
   First, we publish the MarketUpdateType::SNAPSHOT_START message
   */
    const MDPMarketUpdate start_market_update{snapshot_size++, {MarketUpdateType::SNAPSHOT_START, last_inc_seq_num_}};
    logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), start_market_update.toString());
    snapshot_socket_.send(&start_market_update, sizeof(MDPMarketUpdate));


  /* Now below,
  We iterate through all the instruments that we will publish the snapshots for. The first thing we do is publish the 
  MDPMarketUpdate message of the MarketUpdateType::CLEAR type for that instrument 
  */
    for (size_t ticker_id = 0; ticker_id < ticker_orders_.size(); ++ticker_id) {
      const auto &orders = ticker_orders_.at(ticker_id);

      MEMarketUpdate me_market_update;
      me_market_update.type_ = MarketUpdateType::CLEAR;
      me_market_update.ticker_id_ = ticker_id;

      const MDPMarketUpdate clear_market_update{snapshot_size++, me_market_update};
      logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), clear_market_update.toString());
      snapshot_socket_.send(&clear_market_update, sizeof(MDPMarketUpdate));

      /*
      we iterate through all the orders for this trading instrument and check for live orders – entries that do not have nullptr values. 
      For each valid order, we publish the MDPMarketUpdate message for that OrderId with MarketUpdateType::ADD
      */
      for (const auto order: orders) {
        if (order) {
          const MDPMarketUpdate market_update{snapshot_size++, *order};
          logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), market_update.toString());
          snapshot_socket_.send(&market_update, sizeof(MDPMarketUpdate));
          snapshot_socket_.sendAndRecv();
        }
      }
    }
     
     /*
     Finally, we publish the MDPMarketUpdate message with the MarketUpdateType::SNAPSHOT_END type to signify the end of the snapshot 
     messages this round
     */
    const MDPMarketUpdate end_market_update{snapshot_size++, {MarketUpdateType::SNAPSHOT_END, last_inc_seq_num_}};
    logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), end_market_update.toString());
    snapshot_socket_.send(&end_market_update, sizeof(MDPMarketUpdate));
    snapshot_socket_.sendAndRecv();

    logger_.log("%:% %() % Published snapshot of % orders.\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_), snapshot_size - 1);
  }

  /*
  SnapshotSynthesizer runs on its own thread separate from the MarketDataPublisher thread to not cause latencies on the component that 
  publishes the incremental market data stream. The run() method is the method assigned to the SnapshotSynthesizer thread. The only task 
  it performs is checking the snapshot_md_updates_ lock-free queue for new entries, which the MarketDataPublisher sends incremental 
  MDPMarketUpdate messages on. For each incremental MDPMarketUpdate message it reads, it calls the addToSnapshot() method we built 
  earlier. Additionally, it checks the last_snapshot_time_ variable against the current time obtained from getCurrentTime() to see whether 
  a minute has elapsed. If at least a minute has elapsed since the last time a snapshot was published, it calls the publishSnapshot() 
  method to publish a new snapshot. It also remembers the current time as the last time a full snapshot was published
  */
  void SnapshotSynthesizer::run() {
    logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_));
    while (run_) {
      for (auto market_update = snapshot_md_updates_->getNextToRead(); snapshot_md_updates_->size() && market_update; market_update = snapshot_md_updates_->getNextToRead()) {
        logger_.log("%:% %() % Processing %\n", __FILE__, __LINE__, __FUNCTION__, getCurrentTimeStr(&time_str_),
                    market_update->toString().c_str());

        addToSnapshot(market_update);

        snapshot_md_updates_->updateReadIndex();
      }

      if (getCurrentNanos() - last_snapshot_time_ > 60 * NANOS_TO_SECS) {
        last_snapshot_time_ = getCurrentNanos();
        publishSnapshot();
      }
    }
  }
}