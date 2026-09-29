//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef SESSION_H
#define SESSION_H

#include <boost/asio.hpp>
#include <memory>

#include "../include/Logger.h"

class Server;

class Session : public std::enable_shared_from_this<Session>
{
public:
  explicit Session(

  );
  ~Session();

  // Rule of five. Session object is not allowed to copy or move.
  Session(const Session&) = delete;
  Session& operator=(const Session&) = delete;
  Session& operator=(Session&&) noexcept = delete;
  Session(Session&&) noexcept = delete;

private:
  std::shared_ptr<Logger> logger;
  boost::asio::io_context& io_context;
  std::shared_ptr<boost::asio::io_context> worker_io_context_ptr;
  Server& parent_server;

  // TODO : 웹소켓 관련 필드를 추가해야 한다.

  // used strand to reduce cache miss
  boost::asio::strand<boost::asio::io_context::executor_type> strand;

  std::string session_id;
};

#endif //SESSION_H
