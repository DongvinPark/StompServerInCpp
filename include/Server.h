//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef SERVER_H
#define SERVER_H

#include <boost/asio.hpp>

#include <string>

#include "PeriodicTask.h"
#include "../include/Logger.h"
#include "../constants/C.h"
#include "../include/PeriodicTask.h"

class Session;
class ResponseSender;

class Server {
public:
  explicit Server(
    boost::asio::io_context& input_io_context,
    std::vector<std::shared_ptr<boost::asio::io_context>>& input_worker_io_context_pool,
    ResponseSender& input_response_sender,
    std::chrono::milliseconds input_periodic_task_millis
    );
  ~Server();

  void start();
  void shutdown();
  void afterTerminationSession(const std::string& session);

private:
  std::string getSessionId();
  std::shared_ptr<boost::asio::io_context> getNextWorkerIoContextPtr();

  long io_context_id{C::INVALID};

  std::shared_ptr<Logger> logger;
  boost::asio::io_context& io_context;
  std::vector<std::shared_ptr<boost::asio::io_context>>& worker_io_context_pool;
  ResponseSender& response_sender;

  // TODO : 만들어진(==live) session 들을 어떻게 저장하고 있을 것인가?

  // shutdown 된 세션들은 별도의 맵에 모아뒀다가 별도의 periodic task로 주기적으로(ex : 30 sec) 제거한다.
  std::unordered_map<std::string, std::shared_ptr<Session>> shutdown_session_map;
  PeriodicTask remove_session_task;
};

#endif //SERVER_H
