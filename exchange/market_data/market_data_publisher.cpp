#include "market_data_publisher.h"

namespace Exchange {

    /*
    Constructor : The market_updates argument passed to it is the MEMarketUpdateLFQueue object, which the matching engine
    will publish market updates on. The constructor also receives the network interface and two sets of IPs and ports – one 
    for the incremental market data stream and one for the snapshot market data stream. 
    
    In the constructor, it initializes 
    the outgoing_md_updates_ member with the argument passed in the constructor and the snapshot_md_updates_ LFQueue to be 
    of the size ME_MAX_MARKET_UPDATES 

    It also initializes the logger_ object with a log file for this class and initializes the incremental_socket_ variable 
    with the incremental IP and port provided in the constructor. Finally, it creates a SnapshotSynthesizer object and passes the 
    snapshot_md_updates_ LFQueue and the snapshot multicast stream information
    */
  MarketDataPublisher::MarketDataPublisher(MEMarketUpdateLFQueue *market_updates, const std::string &iface,
                                           const std::string &snapshot_ip, int snapshot_port,
                                           const std::string &incremental_ip, int incremental_port)
      : outgoing_md_updates_(market_updates), snapshot_md_updates_(ME_MAX_MARKET_UPDATES),
        run_(false), logger_("exchange_market_data_publisher.log"), incremental_socket_(logger_) {
    ASSERT(incremental_socket_.init(incremental_ip, iface, incremental_port, /*is_listening*/ false) >= 0,
           "Unable to create incremental mcast socket. error:" + std::string(std::strerror(errno)));
    snapshot_synthesizer_ = new SnapshotSynthesizer(&snapshot_md_updates_, iface, snapshot_ip, snapshot_port);
  }

    /*
    
    */
  auto MarketDataPublisher::run() noexcept -> void {
    logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
    while (run_) {
      for (auto market_update = outgoing_md_updates_->getNextToRead();
           outgoing_md_updates_->size() && market_update; market_update = outgoing_md_updates_->getNextToRead()) {
        logger_.log("%:% %() % Sending seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), next_inc_seq_num_,
                    market_update->toString().c_str());
        /*
        In the above code, the run function so far drains the outgoing_md_updates_ queue by reading any new MEMarketDataUpdates 
        published by the matching engine
        */

        incremental_socket_.send(&next_inc_seq_num_, sizeof(next_inc_seq_num_));
        incremental_socket_.send(market_update, sizeof(MEMarketUpdate));
        outgoing_md_updates_->updateReadIndex();
        /*
        After the above code, 
        Once it has a MEMarketUpdate message from the matching engine, it will proceed to write it to the incremental_socket_ 
        UDP socket. But it needs to write out the message in the MDPMarketUpdate format, which is just a sequence number followed 
        by a MEMarketUpdate message. As we saw with OrderServer, it will achieve this here by first writing next_inc_seq_num_, which 
        is the next incremental sequence number to be sent out on the incremental stream, and then write MEMarketUpdate, which it 
        received from the matching engine.
        */


        auto next_write = snapshot_md_updates_.getNextToWriteTo();
        next_write->seq_num_ = next_inc_seq_num_;
        next_write->me_market_update_ = *market_update;
        snapshot_md_updates_.updateWriteIndex();
        /*
        Above, It needs to do one additional step here, which is to write the same incremental update it wrote to the socket to the snapshot_md_updates_ 
        LFQueue to inform the SnapshotSynthesizer component about the new incremental update from the matching engine that was 
        sent to the clients.
        */

       
        ++next_inc_seq_num_;

        /*
        
        */
      }

      incremental_socket_.sendAndRecv();
    }
  }
}