//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>

#include "../src/server/Server.h"
#include "../include/Logger.h"
#include "../src/service/StompHandler.h"
#include "../src/service/MsgBroker.h"

using boost::asio::ip::tcp;

class Server;
class MsgBroker;
class StompHandler;

class Session : public std::enable_shared_from_this<Session>
{
public:
    explicit Session(
        long input_session_id,
        std::shared_ptr<
            boost::beast::websocket::stream<boost::beast::tcp_stream>
        > input_web_socket_ptr,
        boost::asio::io_context& input_io_context,
        Server& input_server
    ):
        logger(Logger::getLogger(C::SESSION)),
        web_socket_ptr(std::move(input_web_socket_ptr)),
        io_context(input_io_context),
        parent_server(input_server),
        session_id(input_session_id)
    {
    }

    ~Session()
    {
    }

    // Rule of five. Session object is not allowed to copy or move.
    Session(const Session&) = delete;
    Session& operator=(const Session&) = delete;
    Session& operator=(Session&&) noexcept = delete;
    Session(Session&&) noexcept = delete;

    void start()
    {
        auto self = shared_from_this();

        // TODO : 현재는 웹소켓 핸드셰이크가 완료 되는지 까지만 체크했다.
        // TODO : STOMP 트랜잭션용 프로토콜 핸들러를 구현해서 적절한 응답을 전송해야 웹소켓 연결이 유지된다.
        // TODO : async read 루프를 만들어야 한다.
        web_socket_ptr->async_accept(
            [self](const boost::system::error_code& ec)
            {
                if (ec)
                {
                    self->logger->severe(
                        "WebSocket handshake failed!"
                    );
                    return;
                }

                self->logger->info3(
                    "WebSocket handshake completed!"
                );

                self->read();
            }
        );
    }

    void shutdown()
    {
    }

    void setMsgBroker(const std::shared_ptr<MsgBroker>& msg_broker_ptr)
    {
        this->msg_broker_ptr = msg_broker_ptr;
    }

    void setStompHandler(const std::shared_ptr<StompHandler>& stomp_handler_ptr)
    {
        this->stomp_handler_ptr = stomp_handler_ptr;
    }

private:
    void read()
    {
        auto self = shared_from_this();

        web_socket_ptr->async_read(
            read_buffer,
            [self](
            const boost::system::error_code& ec,
            std::size_t bytes_transferred)
            {
                if (ec)
                {
                    self->logger->severe(
                        "failed to receive data from web socket!"
                    );
                    self->logger->severe(ec.message());
                    return;
                }

                const auto req =
                    boost::beast::buffers_to_string(
                        self->read_buffer.data()
                    );

                self->logger->info3("req from client!");
                self->logger->info3(req);

                self->read_buffer.consume(
                    self->read_buffer.size()
                );

                self->read();
            }
        );
    }

    std::shared_ptr<Logger> logger;
    std::shared_ptr<
        boost::beast::websocket::stream<boost::beast::tcp_stream>
    > web_socket_ptr;
    boost::beast::flat_buffer read_buffer;
    boost::asio::io_context& io_context;
    Server& parent_server;

    std::shared_ptr<MsgBroker> msg_broker_ptr = nullptr;
    std::shared_ptr<StompHandler> stomp_handler_ptr = nullptr;

    long session_id;
};

#endif //SESSION_H
