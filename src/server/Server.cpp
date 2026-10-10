//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/Server.h"

#include <ranges>
#include <iostream>

#include "../../constants/Util.h"


Server::Server(
    boost::asio::io_context& input_io_context
) : logger(Logger::getLogger(C::SERVER)),
    io_context(input_io_context),
    acceptor(input_io_context),
    msg_broker_ptr(std::make_shared<MsgBroker>(input_io_context)),
    remove_session_task(
        input_io_context,
        boost::asio::make_strand(input_io_context),
        std::chrono::milliseconds(C::CLOSED_SESSION_REMOVAL_INTERVAL_MS)
    ),
    heart_beat_task(
        input_io_context,
        boost::asio::make_strand(input_io_context),
        std::chrono::milliseconds(C::HEART_BEAT_THRESHOLD_MS)
    )
{
}

Server::~Server()
{
    logger->warning("Server shuts down...");
    is_shutdown.store(true);
    remove_session_task.stop();
    heart_beat_task.stop();
}

void Server::start()
{
    logger->info2("Server Starts!");

    // 세션 제거 타이머
    remove_session_task.setTask([&]()
        {
            shutdown_session_map.clear();
            logger->severe("Dongvin, completely removed sessions.");
        }
    );
    remove_session_task.start();
    logger->info3("Dongvin, timer for closed session removal starts!");

    // heart-beat 타이머
    heart_beat_task.setTask(
        [&]()
        {
            // 가장 최근 heart beat 받은 시각이 '임계값'보다 과거이면 그 세션은 제거한다.
            // 단, 원본 맵은 참조하지 않고, 복사본 맵으로 본다.
            // 세션을 복사하는게 아니라 포인터를 복사하는 것이므로 deep copy 해도 상관 없다.
            // 여기서 원본 맵으로 순회하면 동일한 맵을 순회하면서 동시에 수정하는 data race 가 발생한다.
            // 그러면 SIGSEGV, abort() 등이 뜨면서 서버가 예외도 던지지 못하고 바로 죽는다.
            auto copied_session_map = session_id_map;
            int alive_session_count = 0;
            for (const auto& [session_id, session_ptr] : copied_session_map)
            {
                // 가장 최근에 해당 세션의 클라이언트에게서 온 heart beat 시각과 현재 시각을 대조한다.
                const int64_t beat_arrive_time_millis = Util::getCurrentTimeMillis();
                const int64_t latest_heart_beat_time_millis
                    = session_ptr->getLatestHeartBeatTimeMillis();
                if (
                    const int64_t diff = beat_arrive_time_millis - latest_heart_beat_time_millis;
                    diff > static_cast<int64_t>(C::HEART_BEAT_THRESHOLD_MS)
                )
                {
                    session_ptr->setShutdownTrue();
                    afterTerminationSession(session_ptr->getSessionId());
                    continue;
                }
                if (session_ptr->isShutDown() == false)
                {
                    alive_session_count++;
                    session_ptr->sendHeartBeatToClient();
                }
            } //for
            logger->severe(
                "Dongvin, sent hear-beat to alive sessions. cnt : "
                + std::to_string(alive_session_count)
            );
        }
    );
    heart_beat_task.start();

    tcp::acceptor acceptor(
        io_context, tcp::endpoint(tcp::v4(), C::STOMP_PORT)
    );

    try
    {
        while (is_shutdown.load() == false)
        {
            auto raw_tcp_socket_ptr
                = std::make_shared<boost::asio::ip::tcp::socket>(acceptor.accept());

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
            sessionPtr != nullptr
        )
        {
            shutdown_session_map.insert({session_id, std::move(sessionPtr)});
            session_id_map.erase(session_id);
            logger->warning(
                "Session, " + std::to_string(session_id) + " shuts down. Remaining session cnt : "
                + std::to_string(session_id_map.size())
            );
        }
    }
}
