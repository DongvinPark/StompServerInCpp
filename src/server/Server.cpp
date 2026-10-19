//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/Server.h"

#include <ranges>
#include <iostream>
#include <utility>

#include "../../constants/Util.h"


Server::Server(
    boost::asio::io_context& input_io_context,
    std::shared_ptr<MsgBroker> input_msg_broker_ptr
) : logger(Logger::getLogger(C::SERVER)),
    io_context(input_io_context),
    acceptor(input_io_context),
    msg_broker_ptr(input_msg_broker_ptr),
    strand(boost::asio::make_strand(input_io_context)),
    reserve_session_removal_task(
        input_io_context,
        boost::asio::make_strand(input_io_context),
        std::chrono::milliseconds(C::SESSION_REMOVAL_MARK_INTERVAL_MS)
    ),
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

    // 세션 예비 삭제 타이머  - shutDown == true 표시된 세션들을 삭제 예정 맵으로 이동 시킨다.
    // PeriodicTask 들은 전부 main_io_context 로 만들어진 strand 를 통해서
    // io_context에 bind 된 후 실행되기 때문에 여러 개의 periodic task 들은 서로 동시에 실행되는 일이 없다.
    reserve_session_removal_task.setTask([&]()
        {
            auto copied_session_map = session_id_map;
            for (const auto& [session_id, session_ptr] : copied_session_map)
            {
                if (session_ptr->isShutDown())
                {
                    shutdown_session_map.insert({session_id, std::move(session_ptr)});
                    session_id_map.erase(session_id);
                    logger->warning(
                        "Session, " + std::to_string(session_id) +
                        " shuts down. Remaining session cnt : "
                        + std::to_string(session_id_map.size())
                    );
                }
            }
        }
    );
    reserve_session_removal_task.start();
    logger->info3("Dongvin, timer for session delete reservation start!");

    // 세션 실제 제거 타이머
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
            // CppTips.md 5 번 참고.
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

    logger->info3("Dongvin, port " + std::to_string(C::STOMP_PORT) + " is listening...");
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

            // CppTips.md 12 번 참고.
            boost::asio::post(
                strand, [session_ptr, this, session_id]()
                {
                    if (session_ptr->start())
                    {
                        session_id_map.emplace(session_id, session_ptr);
                        logger->warning(
                            "Dongvin, new client arrives, id: " + std::to_string(session_id)
                            + " / total session cnt : " + std::to_string(session_id_map.size())
                        );
                    } else
                    {
                        // 웹소켓 초기화에 실패했다면 삭제 예정 세션 맵으로 이동시켜서 나중에 삭제되게 만든다.
                        session_ptr->setShutdownTrue();
                    }
                }
            );
        } //wh
    }
    catch (const std::exception& e)
    {
        std::ostringstream oss;
        oss << "Server stops with exception : " << e.what();
        logger->severe(oss.str());
    }
    catch (...)
    {
        std::ostringstream oss;
        oss << "Server stops with unknown exception!\n";
        logger->severe(oss.str());
    }
}
