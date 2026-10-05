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
            boost::asio::ip::tcp::socket
        > input_web_socket_ptr,
        boost::asio::io_context& input_io_context,
        Server& input_server
    ):
        logger(Logger::getLogger(C::SESSION)),
        raw_tcp_socket_ptr(std::move(input_web_socket_ptr)),
        io_context(input_io_context),
        strand(boost::asio::make_strand(input_io_context)),
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

        // a function to select the most preferred protocol from a comma-separated list
        auto select_protocol = [](boost::beast::string_view offered_tokens) -> std::string
        {
            // tokenize the Sec-Websocket-Protocol header offered by the client
            boost::beast::http::token_list offered(offered_tokens);

            // an array of protocols supported by this server
            // in descending order of preference
            static constexpr std::array<boost::beast::string_view, 1>
                supported = {
                    {
                        "v12.stomp"
                    }
                };

            std::string result;

            for (auto proto : supported)
            {
                if (
                    auto iter = std::ranges::find(offered, proto);
                    iter != offered.end()
                )
                {
                    // we found a supported protocol in the list offered by the client
                    result.assign(proto.begin(), proto.end());
                    break;
                }
            }

            return result;
        };

        std::string s;
        size_t read_bytes = 0; // 여기엔 함부로 음수를 넣어서는 안 된다. overflow 되면서 MAX값 뜬다.

        boost::beast::flat_buffer buffer;
        boost::beast::http::request<boost::beast::http::string_body> req;
        try
        {
            boost::beast::http::read(*raw_tcp_socket_ptr, buffer, req);
            if (boost::beast::websocket::is_upgrade(req))
            {
                std::string protocol =
                    select_protocol(req[boost::beast::http::field::sec_websocket_protocol]);

                if (protocol.empty())
                {
                    // none of our supported protocols were offered
                    boost::beast::http::response<boost::beast::http::string_body> res;
                    res.result(boost::beast::http::status::bad_request);
                    res.body() = "No valid sub-protocol was offered."
                        " This server implements"
                        " v12.stomp only!";
                    boost::beast::http::write(*raw_tcp_socket_ptr, res);
                }
                else
                {
                    // Construct the stream, transferring ownership of the socket
                    web_socket_ptr = std::make_shared<
                        boost::beast::websocket::stream<boost::beast::tcp_stream>
                    >(std::move(*raw_tcp_socket_ptr));


                    web_socket_ptr->set_option(
                        boost::beast::websocket::stream_base::decorator(
                            [protocol](boost::beast::http::response_header<>& hdr)
                            {
                                hdr.set(
                                    boost::beast::http::field::sec_websocket_protocol,
                                    protocol
                                );
                            }
                        ) //decorator
                    ); //set option

                    // Accept the upgrade request
                    web_socket_ptr->accept(req);
                    read();
                }
            }
        } // try
        catch (const std::exception& e)
        {
            std::cout << "!!! std::exception !!! : ";
            std::cerr << e.what() << std::endl;
        } catch (...)
        {
            std::cerr << "!!! unknown exception" << std::endl;
        }
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
            [self](const boost::system::error_code& ec, std::size_t bytes_transferred)
            {
                if (ec)
                {
                    self->logger->severe("WebSocket read failed!");
                    self->logger->severe("error category: " + std::string(ec.category().name()));
                    self->logger->severe("error value: " + std::to_string(ec.value()));
                    self->logger->severe("error message: " + ec.message());
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

                // TODO : 나중에 async read 루프 제대로 구현하고, 지금은 야매로 한 번 connected 응답한다.
                std::string frame =
                    "CONNECTED\n"
                    "version:1.2\n"
                    "heart-beat:10000,10000\n"
                    "\n";
                frame.push_back('\0');
                auto connected_frame = std::make_shared<std::string>(frame);
                self->web_socket_ptr->async_write(
                    boost::asio::buffer(*connected_frame),
                    [self, connected_frame](
                    const boost::system::error_code& ec,
                    std::size_t bytes_transferred
                )
                    {
                        if (ec)
                        {
                            self->logger->severe(
                                "WebSocket write failed: " + ec.message()
                            );
                            return;
                        }

                        self->logger->info3(
                            "CONNECTED sent! bytes: " +
                            std::to_string(bytes_transferred)
                        );
                    }
                );
                //boost::asio::post(self->strand, [self]() { self->read(); });
            }
        );
    }

    std::shared_ptr<Logger> logger;

    std::shared_ptr<
        boost::asio::ip::tcp::socket
    > raw_tcp_socket_ptr;

    std::shared_ptr<
        boost::beast::websocket::stream<boost::beast::tcp_stream>
    > web_socket_ptr = nullptr;

    boost::beast::flat_buffer read_buffer;
    boost::asio::io_context& io_context;
    boost::asio::strand<boost::asio::io_context::executor_type> strand;
    Server& parent_server;

    std::shared_ptr<MsgBroker> msg_broker_ptr = nullptr;
    std::shared_ptr<StompHandler> stomp_handler_ptr = nullptr;

    long session_id;
    std::atomic<bool> is_shutdown{false};
};

#endif //SESSION_H
