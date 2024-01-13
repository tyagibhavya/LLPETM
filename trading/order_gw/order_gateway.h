#pragma once

#include <functional>

#include "common/thread_utils.h"
#include "common/macros.h"
#include "common/tcp_server.h"

#include "exchange/order_server/client_request.h"
#include "exchange/order_server/client_response.h"

namespace Trading {
  class OrderGateway {
  public:
    OrderGateway(ClientId client_id,
               
                 Exchange::ClientRequestLFQueue *client_requests,
                 Exchange::ClientResponseLFQueue *client_responses,

                 
                 std::string ip, const std::string &iface, int port);

    /*
    The destructor for the OrderGateway class calls the stop() method to 
    stop the execution of the run() method and waits for a little bit before returning
    */
    ~OrderGateway() {
      stop();
      using namespace std::literals::chrono_literals;
      std::this_thread::sleep_for(5s);
    }

    /*
    start() method, which will enable the run_ flag and create and launch a thread to execute the run() method. 
    We will also initialize our tcp_socket_ member variable and have it connect to the ip_ and port_ interface 
    information of the order gateway server at the exchange
    */
    auto start() {
      run_ = true;
      ASSERT(tcp_socket_.connect(ip_, iface_, port_, false) >= 0,
             "Unable to connect to ip:" + ip_ + " port:" + std::to_string(port_) + " on iface:" + iface_ + " error:" + std::string(std::strerror(errno)));
      ASSERT(Common::createAndStartThread(-1, "Trading/OrderGateway", [this]() { run(); }) != nullptr, "Failed to start OrderGateway thread.");
    }

    /*
    The stop() method simply sets the run_ flag to be false to stop the execution of the run() loop
    */
    auto stop() -> void {
      run_ = false;
    }

    // Deleted default, copy & move constructors and assignment-operators.
    OrderGateway() = delete;

    OrderGateway(const OrderGateway &) = delete;

    OrderGateway(const OrderGateway &&) = delete;

    OrderGateway &operator=(const OrderGateway &) = delete;

    OrderGateway &operator=(const OrderGateway &&) = delete;

  private:
    const ClientId client_id_;
    
    std::string ip_;
    /*
    It also saves the network interface in the iface_ variable and the IP and port 
    of the exchange’s order gateway server in the ip_ and port_ member variables
    */
    const std::string iface_;
    const int port_ = 0;
     /*
     Two lock-free queue pointers. The first one is named outgoing_requests_ of the 
    ClientRequestLFQueue type, which we defined before as an LFQueue instance of MEClientRequest 
    structures. The other member is called incoming_responses_, which is of the ClientResponseLFQueue 
    type, which we also defined earlier as an LFQueue instance of the MEClientResponse structures. 
    These will be used by OrderGateway to receive order requests and send order responses to TradeEngine
    */

    Exchange::ClientRequestLFQueue *outgoing_requests_ = nullptr;
    Exchange::ClientResponseLFQueue *incoming_responses_ = nullptr;

    /*
    A boolean run_ flag, which serves a similar purpose as it did in all the other components we saw before. 
    It will be used to start and stop the execution of the OrderGateway thread and is marked volatile since it 
    is accessed from different threads
    */
    volatile bool run_ = false;

    std::string time_str_;
    Logger logger_;

    /*
    Two size_t variables to represent sequence numbers. The first one, next_outgoing_seq_num_, tracks 
    the sequence number that will be sent on the next outgoing OMClientRequest message sent to the 
    exchange. The second one, next_exp_seq_num_, is used to check and validate that the OMClientResponse messages 
    received from the exchange are in sequence
    */
    size_t next_outgoing_seq_num_ = 1;
    size_t next_exp_seq_num_ = 1;

    /*
    It also contains a tcp_socket_ member variable of the TCPSocket type, which is the TCP socket 
    client to be used to connect to the exchange order gateway server and to send and receive messagesIt 
    also contains a tcp_socket_ member variable of the TCPSocket type, which is the TCP socket client 
    to be used to connect to the exchange order gateway server and to send and receive messages
    */
    Common::TCPSocket tcp_socket_;

  private:
    auto run() noexcept -> void;

    auto recvCallback(TCPSocket *socket, Nanos rx_time) noexcept -> void;
  };
}