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

            // session.start() 이 부분 만을 io_context 에 post 해서 비동기 실행시킨 이유가 있다.
            // 이 부분이 http 요청을 STOMP websocket 으로 업그레이드 & 초기화 시킨 후
            // 클라이언트에게 websocket 연결 요청 완료 응답을 보내는 부분이기 때문이다.
            // 이렇게 하지 않으면 1 만 명의 클라이언트들이 동시에 접속 요청을 했을 때
            // session.start() 부분이 후속 실행들을 blocking 해버리면서
            // 후속 연결 요청들이 큐에 쌓이다가 버려진다.
            // 그 결과 일부 클라이언트는 연결이 거부됐으며, 서버가 먹통이 되면서 비정상 종료됐다.
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
