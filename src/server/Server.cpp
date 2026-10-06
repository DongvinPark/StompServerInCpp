//
// Created by 박동빈 on 2026. 10. 6..
//
#include "../include/Server.h"


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

void Server::shutdown()
{
  if (is_shutdown.load() == false) is_shutdown = true;
}

void Server::afterTerminationSession(long session_id)
{
  // TODO : 여기에서 세션 제거 후 동작을 정의해야 한다. : shutdown_session_map으로 이동시키는 것이다.
}
