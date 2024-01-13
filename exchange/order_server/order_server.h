#pragma once

#include <functional>

#include "common/thread_utils.h"
#include "common/macros.h"
#include "common/tcp_server.h"

#include "order_server/client_request.h"
#include "order_server/client_response.h"
#include "order_server/fifo_sequencer.h"

namespace Exchange {
  class OrderServer {
  private:
    const std::string iface_;
    const int port_ = 0;

    /* Lock free queue of outgoing client responses to be sent out to connected clients. */
    ClientResponseLFQueue *outgoing_responses_ = nullptr;

    volatile bool run_ = false;

    std::string time_str_;
    Logger logger_;

    /* Hash map from ClientId -> the next sequence number to be sent on outgoing client responses. */
    std::array<size_t, ME_MAX_NUM_CLIENTS> cid_next_outgoing_seq_num_;

    /* Hash map from ClientId -> the next sequence number expected on incoming client requests. */
    std::array<size_t, ME_MAX_NUM_CLIENTS> cid_next_exp_seq_num_;

    /* Hash map from ClientId -> TCP socket / client connection. */
    std::array<Common::TCPSocket *, ME_MAX_NUM_CLIENTS> cid_tcp_socket_;

    /* 
      TCP server instance listening for new client connections. 
     tcp_server_ variable, which is an instance of the Common::TCPServer class, 
     which will be used to host a TCP server to poll for, accept incoming connections from market participants, 
     and poll the established TCP connections to see whether there is data to be read from any of the connections.
     */
    Common::TCPServer tcp_server_;

    /* FIFO sequencer responsible for making sure incoming client requests are processed 
    in the order in which they were received. */

  /* (book defination) fifo_sequencer_ variable, which is an instance of the FIFOSequencer class,
  is responsible for making sure that client requests that come in on different
  TCP connections are processed in the correct order in which they cam */
    FIFOSequencer fifo_sequencer_;
  
public:
    OrderServer(ClientRequestLFQueue *client_requests, ClientResponseLFQueue *client_responses, const std::string &iface, int port);

    ~OrderServer();

    /* Start and stop the order server main thread. */
    auto start() -> void;

    auto stop() -> void;

    /* Main run loop for this thread - accepts new client connections, receives client requests from them and sends client responses to them. */

/* (book) A Boolean run_ variable, which will be used to start and stop the OrderServer thread.
Note that it is marked volatile since it will be accessed from different threads,
and we want to prevent compiler optimizations here for correct functionality in a multi-threaded environment */

/* (Functioning of run method)
The run() method performs the following two main tasks:
  > It calls the poll() method on the TCPServer object it holds.
  Remember that the poll() method checks for and accepts new connections,
  removes dead connections, and checks whether there is data available on
  any of the established TCP connections, that is, client requests.
  
  > It also calls the sendAndRecv() method on the TCPServer object it holds.
  The sendAndRecv() method reads the data from each of the TCP connections
  and dispatches the callbacks for them. The sendAndRecv() call also sends out
  any outgoing data on the TCP connections, that is, client responses.
*/
    auto run() noexcept {
      logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
      while (run_) {
        tcp_server_.poll();

        tcp_server_.sendAndRecv();

        for (auto client_response = outgoing_responses_->getNextToRead(); outgoing_responses_->size() && client_response; client_response = outgoing_responses_->getNextToRead()) {
          auto &next_outgoing_seq_num = cid_next_outgoing_seq_num_[client_response->client_id_];
          logger_.log("%:% %() % Processing cid:% seq:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                      client_response->client_id_, next_outgoing_seq_num, client_response->toString());

          ASSERT(cid_tcp_socket_[client_response->client_id_] != nullptr,
                 "Dont have a TCPSocket for ClientId:" + std::to_string(client_response->client_id_));
          cid_tcp_socket_[client_response->client_id_]->send(&next_outgoing_seq_num, sizeof(next_outgoing_seq_num));
          cid_tcp_socket_[client_response->client_id_]->send(client_response, sizeof(MEClientResponse));

          outgoing_responses_->updateReadIndex();

          ++next_outgoing_seq_num;
        }
      }
    }

    /* Read client request from the TCP receive buffer, check for sequence gaps and forward it to the FIFO sequencer. */
    auto recvCallback(TCPSocket *socket, Nanos rx_time) noexcept {
      logger_.log("%:% %() % Received socket:% len:% rx:%\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_),
                  socket->socket_fd_, socket->next_rcv_valid_index_, rx_time);

      if (socket->next_rcv_valid_index_ >= sizeof(OMClientRequest)) {
        size_t i = 0;
        for (; i + sizeof(OMClientRequest) <= socket->next_rcv_valid_index_; i += sizeof(OMClientRequest)) {
          auto request = reinterpret_cast<const OMClientRequest *>(socket->inbound_data_.data() + i);
          logger_.log("%:% %() % Received %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), request->toString());

          if (UNLIKELY(cid_tcp_socket_[request->me_client_request_.client_id_] == nullptr)) { // first message from this ClientId.
            cid_tcp_socket_[request->me_client_request_.client_id_] = socket;
          }

          if (cid_tcp_socket_[request->me_client_request_.client_id_] != socket) { // TODO - change this to send a reject back to the client.
            logger_.log("%:% %() % Received ClientRequest from ClientId:% on different socket:% expected:%\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::getCurrentTimeStr(&time_str_), request->me_client_request_.client_id_, socket->socket_fd_,
                        cid_tcp_socket_[request->me_client_request_.client_id_]->socket_fd_);
            continue;
          }

          auto &next_exp_seq_num = cid_next_exp_seq_num_[request->me_client_request_.client_id_];
          if (request->seq_num_ != next_exp_seq_num) { // TODO - change this to send a reject back to the client.
            logger_.log("%:% %() % Incorrect sequence number. ClientId:% SeqNum expected:% received:%\n", __FILE__, __LINE__, __FUNCTION__,
                        Common::getCurrentTimeStr(&time_str_), request->me_client_request_.client_id_, next_exp_seq_num, request->seq_num_);
            continue;
          }

          ++next_exp_seq_num;

          fifo_sequencer_.addClientRequest(rx_time, request->me_client_request_);
        }
        memcpy(socket->inbound_data_.data(), socket->inbound_data_.data() + i, socket->next_rcv_valid_index_ - i);
        socket->next_rcv_valid_index_ -= i;
      }
    }

    /* End of reading incoming messages across all the TCP connections, sequence and publish the client requests to the matching engine. */
    auto recvFinishedCallback() noexcept {
      fifo_sequencer_.sequenceAndPublish();
    }

    /* Deleted default, copy & move constructors and assignment-operators. */
    OrderServer() = delete;

    OrderServer(const OrderServer &) = delete;

    OrderServer(const OrderServer &&) = delete;

    OrderServer &operator=(const OrderServer &) = delete;

    OrderServer &operator=(const OrderServer &&) = delete;
  };
}