#include "market_data_consumer.h"

/*
 Constructor performs the following tasks:
   >  The constructor creates a Logger instance for this class and uses that logger_ object to initialize
      the incremental_mcast_socket_ variable and the snapshot_mcast_socket_ variable.
      It also initializes the iface_, snapshot_ip_, and snapshot_port_ members from the arguments passed to it.
   
   >  The key point here is that we expect the recvCallback() method to be called when there is data available 
      on the incremental or the snapshot multicast socket
   
   >  Fully initialize incremental_mcast_socket_ by calling the init() method, which creates the actual socket internally.
      It also calls the join() method to subscribe to the multicast stream for this socket.
      Note that we do not do the same for snapshot_mcast_socket_ yet.
*/
namespace Trading {
  MarketDataConsumer::MarketDataConsumer(Common::ClientId client_id, Exchange::MEMarketUpdateLFQueue *market_updates,
                                         const std::string &iface,
                                         const std::string &snapshot_ip, int snapshot_port,
                                         const std::string &incremental_ip, int incremental_port)
      : incoming_md_updates_(market_updates), run_(false),
        logger_("trading_market_data_consumer_" + std::to_string(client_id) + ".log"),
        incremental_mcast_socket_(logger_), snapshot_mcast_socket_(logger_),
        iface_(iface), snapshot_ip_(snapshot_ip), snapshot_port_(snapshot_port) {
    auto recv_callback = [this](auto socket) {
      recvCallback(socket);
    };

    incremental_mcast_socket_.recv_callback_ = recv_callback;
    ASSERT(incremental_mcast_socket_.init(incremental_ip, iface, incremental_port, /*is_listening*/ true) >= 0,
           "Unable to create incremental mcast socket. error:" + std::string(std::strerror(errno)));

    ASSERT(incremental_mcast_socket_.join(incremental_ip),
           "Join failed on:" + std::to_string(incremental_mcast_socket_.socket_fd_) + " error:" + std::string(std::strerror(errno)));

    snapshot_mcast_socket_.recv_callback_ = recv_callback;
  }

  /// Main loop for this thread - reads and processes messages from the multicast sockets - the heavy lifting is in the recvCallback() and checkSnapshotSync() methods.

/* (book) run() method is simple for our market data consumer component. It simply calls the sendAndRecv() method
on the incremental_mcast_socket_ socket and the snapshot_mcast_socket_ object, which in our case,
consumes any additional data received on the incremental or snapshot channels and dispatches the callbacks: */  
auto MarketDataConsumer::run() noexcept -> void {
    logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
    while (run_) {
      incremental_mcast_socket_.sendAndRecv();
      snapshot_mcast_socket_.sendAndRecv();
    }
  }

  /// Start the process of snapshot synchronization by subscribing to the snapshot multicast stream.

/*
 (PURPOSE) Prepares the MarketDataConsumer class to start the snapshot synchronization mechanism on sequence number gaps.
 
 (PROCESS 1)  Clear the two std::map containers – snapshot_queued_msgs_ and incremental_queued_msgs_,
              which we use to queue upmarket update messages from the snapshot and incremental streams.
 
 (PROCESS 2)  Initialize the snapshot_mcast_socket_ object using the init() method so that 
              the socket gets created for the snapshot_ip_ and snapshot_port_ address.

 (PROCESS 3)  Call the McastSocket::join() method to start the multicast subscription for the snapshot market data stream.
*/


  auto MarketDataConsumer::startSnapshotSync() -> void {
    snapshot_queued_msgs_.clear();
    incremental_queued_msgs_.clear();

    ASSERT(snapshot_mcast_socket_.init(snapshot_ip_, iface_, snapshot_port_, /*is_listening*/ true) >= 0,
           "Unable to create snapshot mcast socket. error:" + std::string(std::strerror(errno)));
    ASSERT(snapshot_mcast_socket_.join(snapshot_ip_), // IGMP multicast subscription.
           "Join failed on:" + std::to_string(snapshot_mcast_socket_.socket_fd_) + " error:" + std::string(std::strerror(errno)));
  }

  /// Check if a recovery / synchronization is possible from the queued up market data updates from the snapshot and incremental market data streams.

/*
(MAIN IDEA)
(1) Queue up messages received on the snapshot and incremental market data streams.
(2) Make sure that no messages were dropped on the snapshot market data stream by checking
    that there is no gap in the sequence number field on the snapshot message.
(3) Check messages following the last message that was used to synthesize this round of snapshot messages.
    We do this by checking if we have market updates in the incremental queue starting with a sequence number
    equal to the OrderId + 1 value from the SNAPSHOT_END message in the
*/

/* WILL ADD PROPER WORKING LATER. */

auto MarketDataConsumer::checkSnapshotSync() -> void {
    if (snapshot_queued_msgs_.empty()) {
      return;
    }

    const auto &first_snapshot_msg = snapshot_queued_msgs_.begin()->second;
    if (first_snapshot_msg.type_ != Exchange::MarketUpdateType::SNAPSHOT_START) {
      logger_.log("%:% %() % Returning because have not seen a SNAPSHOT_START yet.\n",
                  __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
      snapshot_queued_msgs_.clear();
      return;
    }

    std::vector<Exchange::MEMarketUpdate> final_events;

    auto have_complete_snapshot = true;
    size_t next_snapshot_seq = 0;
    for (auto &snapshot_itr: snapshot_queued_msgs_) {
      logger_.log("%:% %() % % => %\n", __FILE__, __LINE__, __FUNCTION__,
                  Common::getCurrentTimeStr(&time_str_), snapshot_itr.first, snapshot_itr.second.toString());
      if (snapshot_itr.first != next_snapshot_seq) {
        have_complete_snapshot = false;
        logger_.log("%:% %() % Detected gap in snapshot stream expected:% found:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_str_), next_snapshot_seq, snapshot_itr.first, snapshot_itr.second.toString());
        break;
      }

      if (snapshot_itr.second.type_ != Exchange::MarketUpdateType::SNAPSHOT_START &&
          snapshot_itr.second.type_ != Exchange::MarketUpdateType::SNAPSHOT_END)
        final_events.push_back(snapshot_itr.second);

      ++next_snapshot_seq;
    }

    if (!have_complete_snapshot) {
      logger_.log("%:% %() % Returning because found gaps in snapshot stream.\n",
                  __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
      snapshot_queued_msgs_.clear();
      return;
    }

    const auto &last_snapshot_msg = snapshot_queued_msgs_.rbegin()->second;
    if (last_snapshot_msg.type_ != Exchange::MarketUpdateType::SNAPSHOT_END) {
      logger_.log("%:% %() % Returning because have not seen a SNAPSHOT_END yet.\n",
                  __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
      return;
    }

    auto have_complete_incremental = true;
    size_t num_incrementals = 0;
    next_exp_inc_seq_num_ = last_snapshot_msg.order_id_ + 1;
    for (auto inc_itr = incremental_queued_msgs_.begin(); inc_itr != incremental_queued_msgs_.end(); ++inc_itr) {
      logger_.log("%:% %() % Checking next_exp:% vs. seq:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                  Common::getCurrentTimeStr(&time_str_), next_exp_inc_seq_num_, inc_itr->first, inc_itr->second.toString());

      if (inc_itr->first < next_exp_inc_seq_num_)
        continue;

      if (inc_itr->first != next_exp_inc_seq_num_) {
        logger_.log("%:% %() % Detected gap in incremental stream expected:% found:% %.\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_str_), next_exp_inc_seq_num_, inc_itr->first, inc_itr->second.toString());
        have_complete_incremental = false;
        break;
      }

      logger_.log("%:% %() % % => %\n", __FILE__, __LINE__, __FUNCTION__,
                  Common::getCurrentTimeStr(&time_str_), inc_itr->first, inc_itr->second.toString());

      if (inc_itr->second.type_ != Exchange::MarketUpdateType::SNAPSHOT_START &&
          inc_itr->second.type_ != Exchange::MarketUpdateType::SNAPSHOT_END)
        final_events.push_back(inc_itr->second);

      ++next_exp_inc_seq_num_;
      ++num_incrementals;
    }

    if (!have_complete_incremental) {
      logger_.log("%:% %() % Returning because have gaps in queued incrementals.\n",
                  __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
      snapshot_queued_msgs_.clear();
      return;
    }

    for (const auto &itr: final_events) {
      auto next_write = incoming_md_updates_->getNextToWriteTo();
      *next_write = itr;
      incoming_md_updates_->updateWriteIndex();
    }

    logger_.log("%:% %() % Recovered % snapshot and % incremental orders.\n", __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&time_str_), snapshot_queued_msgs_.size() - 2, num_incrementals);

    snapshot_queued_msgs_.clear();
    incremental_queued_msgs_.clear();
    in_recovery_ = false;

    snapshot_mcast_socket_.leave(snapshot_ip_, snapshot_port_);;
  }

  /// Queue up a message in the *_queued_msgs_ containers, first parameter specifies if this update came from the snapshot or the incremental streams.

/*
  It captures whether it was received from the snapshot stream or the incremental stream.
  If the message came over the incremental market data stream, then it adds it to incremental_queued_msgs_ std::map.
  If it is received over the snapshot stream, then first, it checks to see if a market update for that sequence number
  already exists in the snapshot_queued_msgs_ container. If the entry for that sequence number already exists in 
  the container, then that means that we are receiving a new snapshot messages cycle and we were not able to successfully
  recover from the previous snapshot messages cycle. In this case, it clears the snapshot_queued_msgs_ container since
  we will have to restart the snapshot recovery process from the beginning.
*/

auto MarketDataConsumer::queueMessage(bool is_snapshot, const Exchange::MDPMarketUpdate *request) {
    if (is_snapshot) {
      if (snapshot_queued_msgs_.find(request->seq_num_) != snapshot_queued_msgs_.end()) {
        logger_.log("%:% %() % Packet drops on snapshot socket. Received for a 2nd time:%\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_str_), request->toString());
        snapshot_queued_msgs_.clear();
      }
      snapshot_queued_msgs_[request->seq_num_] = request->me_market_update_;
    } else {
      incremental_queued_msgs_[request->seq_num_] = request->me_market_update_;
    }

    logger_.log("%:% %() % size snapshot:% incremental:% % => %\n", __FILE__, __LINE__, __FUNCTION__,
                Common::getCurrentTimeStr(&time_str_), snapshot_queued_msgs_.size(), incremental_queued_msgs_.size(), request->seq_num_, request->toString());

    checkSnapshotSync();
  }

  /// Process a market data update, the consumer needs to use the socket parameter to figure out whether this came from the snapshot or the incremental stream.

/* 
  (PROCESS 1 : Comparing the file descriptor of the socket.)
  The first code block in the recvCallback() method determines if the data we are processing came from 
  the incremental or snapshot stream by comparing the file descriptor of the socket on which it was received.
  In the extremely unlikely edge case that we received data on the snapshot socket but we are not in recovery,
  we simply log a warning, reset the socket receive buffer index, and return:

  (PROCESS 2 : Reading MarketUpdate messages from the socket.)
  Oherwise, we proceed further and read Exchange::MDPMarketUpdate messages from the socket buffer.
  We go through the data contained in the socket->rcv_buffer_ buffer and read it in chunks of 
  size equal to the size of Exchange::MDPMarketUpdate. The goal here is to read as many full 
  MDPMarketUpdate messages as possible until we have read them all from the buffer.

  (PROCESS 3 : Check for recovery and start snapshot sync.)
  Check the already_in_recovery_ flag to see if we were previously not in recovery and 
  just started recovery due to this message or not. If we were previously not in recovery 
  and started recovery because we saw a sequence number gap, we call the  startSnapshotSync() method.
*/

auto MarketDataConsumer::recvCallback(McastSocket *socket) noexcept -> void {
    const auto is_snapshot = (socket->socket_fd_ == snapshot_mcast_socket_.socket_fd_);
    if (UNLIKELY(is_snapshot && !in_recovery_)) { // market update was read from the snapshot market data stream and we are not in recovery, so we dont need it and discard it.
      socket->next_rcv_valid_index_ = 0;

      logger_.log("%:% %() % WARN Not expecting snapshot messages.\n",
                  __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));

      return;
    }

    if (socket->next_rcv_valid_index_ >= sizeof(Exchange::MDPMarketUpdate)) {
      size_t i = 0;
      for (; i + sizeof(Exchange::MDPMarketUpdate) <= socket->next_rcv_valid_index_; i += sizeof(Exchange::MDPMarketUpdate)) {
        auto request = reinterpret_cast<const Exchange::MDPMarketUpdate *>(socket->inbound_data_.data() + i);
        logger_.log("%:% %() % Received % socket len:% %\n", __FILE__, __LINE__, __FUNCTION__,
                    Common::getCurrentTimeStr(&time_str_),
                    (is_snapshot ? "snapshot" : "incremental"), sizeof(Exchange::MDPMarketUpdate), request->toString());

        const bool already_in_recovery = in_recovery_;
        in_recovery_ = (already_in_recovery || request->seq_num_ != next_exp_inc_seq_num_);

        if (UNLIKELY(in_recovery_)) {
          if (UNLIKELY(!already_in_recovery)) { // if we just entered recovery, start the snapshot synchonization process by subscribing to the snapshot multicast stream.
            logger_.log("%:% %() % Packet drops on % socket. SeqNum expected:% received:%\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::getCurrentTimeStr(&time_str_), (is_snapshot ? "snapshot" : "incremental"), next_exp_inc_seq_num_, request->seq_num_);
            startSnapshotSync();
          }

          queueMessage(is_snapshot, request); // queue up the market data update message and check if snapshot recovery / synchronization can be completed successfully.
        } else if (!is_snapshot) { // not in recovery and received a packet in the correct order and without gaps, process it.
          logger_.log("%:% %() % %\n", __FILE__, __LINE__, __FUNCTION__,
                      Common::getCurrentTimeStr(&time_str_), request->toString());

          ++next_exp_inc_seq_num_;

          auto next_write = incoming_md_updates_->getNextToWriteTo();
          *next_write = std::move(request->me_market_update_);
          incoming_md_updates_->updateWriteIndex();
        }
      }
      memcpy(socket->inbound_data_.data(), socket->inbound_data_.data() + i, socket->next_rcv_valid_index_ - i);
      socket->next_rcv_valid_index_ -= i;
    }
  }
}
