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

      // heart beat 도 체크한다. 가장 최근 heart beat 받은 시각이 현재시각 기준으로
      // '임계값'보다 과거이면 그 세션은 제거한다.
      int alive_session_count = 0;
      for (const auto& [session_id, session_ptr] : session_id_map)
      {
        // 가장 최근에 해당 세션의 클라이언트에게서 온 heart beat 시각과 현재 시각을 대조한다.
        const int64_t beat_arrive_time_millis = Util::getCurrentTimeMillis();
        const int64_t latest_heart_beat_time_millis = session_ptr->getLatestHeartBeatTimeMillis();
        if (
          const int64_t diff = beat_arrive_time_millis - latest_heart_beat_time_millis;
          diff > static_cast<int64_t>(/*C::HEART_BEAT_MS*/5000)
        )
        {
          // 이때는 세션을 버려야 한다.
          session_ptr->setShutdownTrue();
          std::cout << "!!! 세션 셧다운 트루 완료 !!!\n";
          afterTerminationSession(session_ptr->getSessionId()); // TODO : 왜 여기 호출 후 맥에서 SIGABRT 가 뜨지??
          std::cout << "!!! 애프터 터미네이션 콜 완료 !!!\n";
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
  remove_session_task.start();
  logger->info3("Dongvin, timer for closed session removal starts!");

  tcp::acceptor acceptor(
    io_context, tcp::endpoint(tcp::v4(), C::STOMP_PORT)
  );

  try
  {
    while (is_shutdown.load() == false)
    {
      auto raw_tcp_socket_ptr = std::make_shared<boost::asio::ip::tcp::socket>(acceptor.accept());

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

void Server::shutdown()
{
  if (is_shutdown.load() == false) is_shutdown = true;
  logger->warning("Shutting down server...");
  is_shutdown.store(true);
  remove_session_task.stop();
  session_id_map.clear();
  shutdown_session_map.clear();
}
