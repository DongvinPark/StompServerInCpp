//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef SERVER_H
#define SERVER_H

#include <boost/asio.hpp>
#include <boost/beast.hpp>

#include <string>

#include "MsgBroker.h"
#include "Logger.h"
#include "../constants/C.h"
#include "PeriodicTask.h"
#include "StompHandler.h"

class Session;

class Server
{
public:
  explicit Server(
    boost::asio::io_context& input_io_context
  );

  ~Server();

  void start();

  void afterTerminationSession(long session_id);

private:
  void shutdown();

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
