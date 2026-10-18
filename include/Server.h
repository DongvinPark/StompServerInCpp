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

class Server : public std::enable_shared_from_this<Server>
{
public:
  explicit Server(
    boost::asio::io_context& input_io_context,
    std::shared_ptr<MsgBroker> input_msg_broker_ptr
  );

  ~Server();

  void start();

private:

  std::shared_ptr<Logger> logger;
  boost::asio::io_context& io_context;
  boost::beast::net::ip::tcp::acceptor acceptor;
  std::shared_ptr<MsgBroker> msg_broker_ptr;

  boost::asio::strand<boost::asio::io_context::executor_type> strand;

  std::unordered_map<long, std::shared_ptr<Session>> session_id_map{};
  // shutdown 된 세션들은 별도의 맵에 모아뒀다가
  // 별도의 periodic task로 주기적으로(ex : 30 sec) 제거한다. 그래야 SIGABRT 에러 피할 수 있다.
  std::unordered_map<long, std::shared_ptr<Session>> shutdown_session_map{};
  PeriodicTask reserve_session_removal_task;
  PeriodicTask remove_session_task;
  PeriodicTask heart_beat_task;

  std::atomic<bool> is_shutdown{false};
  std::atomic<long> session_id_counter{0L};
};

#endif //SERVER_H
