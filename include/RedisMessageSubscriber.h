//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef REDISMESSAGESUBSCRIBER_H
#define REDISMESSAGESUBSCRIBER_H

#include <boost/redis.hpp>
#include <boost/redis/src.hpp>

#include <../include/Logger.h>
#include <boost/asio/io_context.hpp>

class RedisMessageSubscriber : public std::enable_shared_from_this<RedisMessageSubscriber> {
public:
  explicit RedisMessageSubscriber();
  ~RedisMessageSubscriber();

  void start();

private:
  std::shared_ptr<Logger> logger;
  boost::asio::io_context& io_context;
  std::shared_ptr<boost::asio::io_context> worker_io_context_ptr;
  boost::redis::connection& redis_conn;

  // 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
  // server socket 을 돌리는 Server 객체는 Session 을 만들어주는 역할만 할 뿐,
  // 그 자체로는 응답을 전송하지는 않기 때문이다.
};

#endif //REDISMESSAGESUBSCRIBER_H
