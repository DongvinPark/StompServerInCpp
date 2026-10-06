//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/Server.h"

#include <ranges>
#include <iostream>


Server::Server(
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

Server::~Server()
{
    shutdown();
}

void Server::start()
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

    tcp::acceptor acceptor(
        io_context, tcp::endpoint(tcp::v4(), C::STOMP_PORT)
    );

    try
    {
        while (is_shutdown.load() == false)
        {
            auto raw_tcp_socket_ptr = std::make_shared<boost::asio::ip::tcp::socket>(
                acceptor.accept());

            session_id_counter += 1;
            auto session_id = session_id_counter.load();

            auto session_ptr = std::make_shared<Session>(
                session_id, raw_tcp_socket_ptr, io_context, *this
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

void Server::afterTerminationSession(const long session_id)
{
    // C++20 부터는 if (map.find(session_id) != map.end()){...} 이렇게 안 해도 된다.
    if (session_id_map.contains(session_id))
    {
        // 메시지 브록커에서 먼저 제거한다.
        msg_broker_ptr->deleteSession(session_id);

        // 그 후 '삭제 예정인 세션 맵'으로 이동시킨다.
        if (
            auto sessionPtr = session_id_map[session_id];
            sessionPtr != nullptr && sessionPtr->isShutDown() == false
        )
        {
            session_id_map.erase(session_id);
            std::cout << "!!! session map erase complete !!!\n";
            shutdown_session_map.insert({session_id, std::move(sessionPtr)});
            std::cout << "!!! move to remove target map completes !!!\n";
            logger->warning(
                "Session, " + std::to_string(session_id) + " shuts down. Remaining session cnt : "
                + std::to_string(session_id_map.size())
            );
        }
    }
}

void Server::shutdown()
{
    if (is_shutdown.load() == false) is_shutdown = true;
    logger->warning("Shutting down server...");
    is_shutdown.store(true);
    remove_session_task.stop();
    session_id_map.clear();
    shutdown_session_map.clear();
}
