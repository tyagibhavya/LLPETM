#include "order_server.h"

/*
Constructor for OrderServer accepts pointers to two lock-free queue objects: 
  > one to forward MEClientRequests to the matching engine.
  > one to receive MEClientResponses from the matching engine. 

It also accepts a network interface and port to use that the order gateway server will listen toand accept client connections.
*/

namespace Exchange {
  OrderServer::OrderServer(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, const std::string &iface, int port)
      : iface_(iface), port_(port), outgoing_responses_(client_responses), logger_("exchange_order_server.log"),
        tcp_server_(logger_), fifo_sequencer_(client_requests, &logger_) {
    cid_next_outgoing_seq_num_.fill(1);
    cid_next_exp_seq_num_.fill(1);
    cid_tcp_socket_.fill(nullptr);

    /*  set the two callback members, recv_callback_ and recv_finished_callback_,
    to point to the recvCallback() and recvFinishedCallback() member functions. */
        
    tcp_server_.recv_callback_ = [this](auto socket, auto rx_time) { recvCallback(socket, rx_time); };
    tcp_server_.recv_finished_callback_ = [this]() { recvFinishedCallback(); };
  }

  OrderServer::~OrderServer() {
    stop();

    using namespace std::literals::chrono_literals;
    std::this_thread::sleep_for(1s);
  }

/// Start and stop the order server main thread.

/* The start() method, which will set the bool run_ to be true. 
This is the flag that controls how long the main thread will run. 
We also initialize the TCPServer member object to start listening
on the interface and port that OrderServer was provided in the constructor. */
  auto OrderServer::start() -> void {
    run_ = true;
    tcp_server_.listen(iface_, port_);

    ASSERT(Common::createAndStartThread(-1, "Exchange/OrderServer", [this]() { run(); }) != nullptr, "Failed to start OrderServer thread.");
  }

/* stop() method will cause the run() method to finish execution */
  auto OrderServer::stop() -> void {
    run_ = false;
  }

  
}
