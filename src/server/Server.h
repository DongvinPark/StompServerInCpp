//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef SERVER_H
#define SERVER_H

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <string>

#include "../src/service/MsgBroker.h"
#include "../include/Logger.h"
#include "../constants/C.h"
#include "../include/PeriodicTask.h"
#include "../src/service/StompHandler.h"

using boost::asio::ip::tcp;

class Session;

class Server
{
public:
    explicit Server(
        boost::asio::io_context& input_io_context
    ) : logger(Logger::getLogger(C::SERVER)),
        io_context(input_io_context),
        acceptor(input_io_context),
        msg_broker_ptr(std::make_shared<MsgBroker>()),
        remove_session_task(
            input_io_context,
            boost::asio::make_strand(input_io_context),
            std::chrono::milliseconds(C::CLOSED_SESSION_REMOVAL_INTERVAL_MS)
        )
    {
    }

    ~Server()
    {
        logger->warning("Shutting down server...");
        is_shutdown.store(true);

        for (const auto& [fst, snd] : session_id_map)
        {
            long id = fst;
            auto session_ptr = snd;
            session_ptr->shutdown();
            shutdown_session_map.emplace(id, session_ptr);
        }
        session_id_map.clear();
        shutdown_session_map.clear();
        remove_session_task.stop();
    }

    void start()
    {
        logger->info2("Server Starts!");

        // 연결 끊어진 세션 제거 타이머
        remove_session_task.setTask([&]()
        {
            shutdown_session_map.clear();
            logger->severe("Dongvin, completely removed sessions.");
        });
        remove_session_task.start();
        logger->info3("Dongvin, timer for closed session removal starts!");

        const auto endpoint =
            boost::beast::net::ip::tcp::endpoint(
                boost::beast::net::ip::tcp::v4(),
                C::STOMP_PORT
            );

        acceptor.open(endpoint.protocol());
        acceptor.bind(endpoint);
        acceptor.listen();

        try
        {
            while (is_shutdown.load() == false)
            {
                auto websocket_stream_ptr = std::make_shared<
                    boost::beast::websocket::stream<boost::beast::tcp_stream>
                >(acceptor.accept());

                session_id_counter += 1;
                auto session_id = session_id_counter.load();

                auto session_ptr = std::make_shared<Session>(
                    session_id, websocket_stream_ptr, io_context, *this
                );
                session_ptr->setMsgBroker(msg_broker_ptr);

                auto stomp_handler = std::make_shared<StompHandler>(session_ptr);
                session_ptr->setStompHandler(stomp_handler);
                session_ptr->start();

                session_id_map.emplace(session_id, session_ptr);
                logger->warning(
                    "Dongvin, new client arrives, id: " + std::to_string(session_id)
                    + " / total session cnt : " + std::to_string(session_id_map.size())
                );
            } //wh
        }
        catch (const std::exception& e)
        {
            std::ostringstream oss;
            oss << "Server stops with exception : " << e.what();
            logger->severe(oss.str());
        }
    }

    void shutdown()
    {
        if (is_shutdown.load() == false) is_shutdown = true;
    }

    void afterTerminationSession(long session_id)
    {
        // TODO : 여기에서 세션 제거 후 동작을 정의해야 한다. : shutdown_session_map으로 이동시키는 것이다.
    }

private:
    std::string getSessionId();

    std::shared_ptr<Logger> logger;
    boost::asio::io_context& io_context;
    boost::beast::net::ip::tcp::acceptor acceptor;
    std::shared_ptr<MsgBroker> msg_broker_ptr;

    // TODO : 만들어진(==live) session 들을 어떻게 저장하고 있을 것인가?


    std::unordered_map<long, std::shared_ptr<Session>> session_id_map{};
    // shutdown 된 세션들은 별도의 맵에 모아뒀다가
    // 별도의 periodic task로 주기적으로(ex : 30 sec) 제거한다. 그래야 SIGABRT 에러 피할 수 있다.
    std::unordered_map<long, std::shared_ptr<Session>> shutdown_session_map{};
    PeriodicTask remove_session_task;

    std::atomic<bool> is_shutdown{false};
    std::atomic<long> session_id_counter{0L};
};

#endif //SERVER_H
