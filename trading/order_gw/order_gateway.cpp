#include "order_gateway.h"

namespace Trading
{
    OrderGateway::OrderGateway(
        /*
        The constructor accepts the client_id ID of the trading client, a pointer to a ClientRequestsLFQueue object (client_requests),
        a pointer to a ClientResponseLFQueue object (client_responses), and the ip, port, and interface information (iface) for the
        TCP connection. It initializes its own internal variables with these arguments and initializes the Logger data member (logger_)
        with a filename for the order gateway logs for this client. It updates the recv_callback_ member inside the tcp_socket_ variable
        of the TCPSocket type so that callbacks dispatched on data reads will go to the OrderGateway::recvCallback() method
        */
        ClientId client_id,
        Exchange::ClientRequestLFQueue *client_requests,
        Exchange::ClientResponseLFQueue *client_responses,
        std::string ip, const std::string &iface, int port)
        : client_id_(client_id), ip_(ip), iface_(iface), port_(port), outgoing_requests_(client_requests), incoming_responses_(client_responses),
          logger_("trading_order_gateway_" + std::to_string(client_id) + ".log"), tcp_socket_(logger_)
    {
        tcp_socket_.recv_callback_ = [this](auto socket, auto rx_time)
        { recvCallback(socket, rx_time); };
    }

    /// Main thread loop - sends out client requests to the exchange and reads and dispatches incoming client responses.
    /*
    The goal of this method is to send out any client requests that are ready to be sent out on the TCP socket to 
    read any data available on the socket and dispatch the recv_callback_() method.
    */
    auto OrderGateway::run() noexcept -> void
    {   
        /*
        First, it calls the TCPSocket::sendAndRecv() method to send and receive data on the established TCP connection
        */
        logger_.log("%:% %() %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_));
        while (run_)
        {
            tcp_socket_.sendAndRecv();
            /*
            It also reads any MEClientRequest messages available on the outgoing_requests_ LFQueue sent by the TradeEngine
            engine and writes them to the tcp_socket_ send buffer using the TCPSocket::send() method. Note that it needs 
            to write out OMClientRequest messages, which it achieves by first writing the next_outgoing_seq_num_ field and 
            then the MEClientRequest object that the TradeEngine sent. This works because we designed the OMClientRequest object 
            to be a struct that contains a size_t seq_num_ field followed by a MEClientRequest object. We also increment
            the next_outgoing_seq_num_ instance for the next outgoing socket message
            */
            for (auto client_request = outgoing_requests_->getNextToRead(); client_request; client_request = outgoing_requests_->getNextToRead())
            {
                logger_.log("%:% %() % Sending cid:% seq:% %\n", __FILE__, __LINE__, __FUNCTION__,
                            Common::getCurrentTimeStr(&time_str_), client_id_, next_outgoing_seq_num_, client_request->toString());
                tcp_socket_.send(&next_outgoing_seq_num_, sizeof(next_outgoing_seq_num_));
                tcp_socket_.send(client_request, sizeof(Exchange::MEClientRequest));
                outgoing_requests_->updateReadIndex();

                next_outgoing_seq_num_++;
            }
        }
    }

    /// Callback when an incoming client response is read, we perform some checks and forward it to the lock free queue connected to the trade engine.
    auto OrderGateway::recvCallback(TCPSocket *socket, Nanos rx_time) noexcept -> void
    {
        /*
        The recvCallback() method is called when there is data available on the tcp_socket_ and the TCPSocket::sendAndRecv() method is called from the run() method in the previous section. 
        We go through the rcv_buffer_ buffer on TCPSocket and re-interpret the data as OMClientResponse messages
        */
        logger_.log("%:% %() % Received socket:% len:% %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), socket->socket_fd_, socket->next_rcv_valid_index_, rx_time);

        if (socket->next_rcv_valid_index_ >= sizeof(Exchange::OMClientResponse))
        {
            size_t i = 0;
            for (; i + sizeof(Exchange::OMClientResponse) <= socket->next_rcv_valid_index_; i += sizeof(Exchange::OMClientResponse))
            {
                auto response = reinterpret_cast<const Exchange::OMClientResponse *>(socket->inbound_data_.data() + i);
                logger_.log("%:% %() % Received %\n", __FILE__, __LINE__, __FUNCTION__, Common::getCurrentTimeStr(&time_str_), response->toString());

                /*
                For the OMClientResponse message we just read into the response variable, we check to make sure the client ID on the response matches 
                the OrderGateway’s client ID and ignore the response if it does not match
                */

                if (response->me_client_response_.client_id_ != client_id_)
                { // this should never happen unless there is a bug at the exchange.
                    logger_.log("%:% %() % ERROR Incorrect client id. ClientId expected:% received:%.\n", __FILE__, __LINE__, __FUNCTION__,
                                Common::getCurrentTimeStr(&time_str_), client_id_, response->me_client_response_.client_id_);
                    continue;
                }

                /*
                We also check to make sure that the sequence number on OMClientResponse matches what we expect it to be. If there is a mismatch, 
                we log an error and ignore the response. There is an opportunity to improve the error handling here, but for the sake of simplicity, 
                we just log an error and continue:
                */
                if (response->seq_num_ != next_exp_seq_num_)
                { // this should never happen since we use a reliable TCP protocol, unless there is a bug at the exchange.
                    logger_.log("%:% %() % ERROR Incorrect sequence number. ClientId:%. SeqNum expected:% received:%.\n", __FILE__, __LINE__, __FUNCTION__,
                                Common::getCurrentTimeStr(&time_str_), client_id_, next_exp_seq_num_, response->seq_num_);
                    continue;
                }
                /*
                Finally, we increment the expected sequence number on the next OMClientResponse and write the response we just read to the
                incoming_responses_ LFQueue for the TradeEngine to read. It also updates the rcv_buffer_ buffer and the next receive 
                index into the TCPSocket buffer we just consumed some messages from
                */
                ++next_exp_seq_num_;

                auto next_write = incoming_responses_->getNextToWriteTo();
                *next_write = std::move(response->me_client_response_);
                incoming_responses_->updateWriteIndex();
            }
            memcpy(socket->inbound_data_.data(), socket->inbound_data_.data() + i, socket->next_rcv_valid_index_ - i);
            socket->next_rcv_valid_index_ -= i;
        }
    }
}