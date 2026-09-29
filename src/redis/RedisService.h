//
// Created by 박동빈 on 2026. 9. 28..
//

#ifndef REDISMESSAGESUBSCRIBER_H
#define REDISMESSAGESUBSCRIBER_H

#include <iostream>
/*
 * boost/redis 관련 두 가지 include 는 프로젝트 전체에서 오직 1 번씩만 정의돼야 한다.
 * 그렇지 않을 경우(ex : boost::redis::connection 객체를 다른 곳에서 만들고 생성자에 참조 전달 하는 경우)
 * 'duplicate symbol' 관련 에러가 뜨면서 빌드에 실패한다.
 * 따라서, RedisService 같은 레디스 전용 클래스를 만들고,
 * 그 안에서만 connection을 딱 하나만 만들어서 사용하는 구조가 권장된다.
 */
#include <boost/redis.hpp>
#include <boost/redis/src.hpp>

#include "../constants/C.h"
#include <../include/Logger.h>
#include <boost/asio.hpp>

class RedisService : public std::enable_shared_from_this<RedisService>
{
public:
  explicit RedisService(
    boost::asio::io_context& input_io_context,
    std::vector<std::shared_ptr<boost::asio::io_context>>& input_worker_io_context_pool
    // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
    //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
  ) : logger(Logger::getLogger(C::REDIS_MSG_SUBSCRIBER)),
      io_context(input_io_context),
      worker_io_context_pool(input_worker_io_context_pool),
      redis_conn(io_context),
      strand(boost::asio::make_strand(io_context))
  {
  }

  ~RedisService()
  {
    redis_conn.cancel();
  }

  void init()
  {
    auto self = shared_from_this();
    boost::redis::config cfg;
    cfg.addr.host = C::REDIS_HOST_IP;
    cfg.addr.port = C::REDIS_PORT;

    // 이 부분은 건드리지 않는게 좋다. boost::asio::detached 대신
    // 다른 Completion Token 람다를 넣어 봤지만, 무슨 이유에서인지 람다가 실행되지 않아서 그렇다.
    // 번거롭지만 초기화를 확인하는건 아예 별도의 테스트 함수(void verifyRedisConnection())로 실행하고,
    // 연결 여부를 caller 쪽에서 확인하고 대응하게 만들었다.
    redis_conn.async_run(cfg, {}, boost::asio::detached);
  }

  bool get_is_ready()
  {
    return is_ready.load();
  }


  void verifyRedisConnection()
  {
    // 이 함수의 호출자가 호출을 마치고나면 이 안에서 만들어진 지역변수들은 원래는 소멸하는게 맞다.
    // 그런데, io_context 에게 지역 변수들을 참조해야 하는 태스크를 전달해야 하는 경우가 있다.
    // 예를 들면, redis_conn.async_exec() 같은 것들이다.
    // io_context가 테스크를 실행하기도 전에 지역변수들(redis req/res)이 소멸해버리면 잘못된 포인터 access
    // 가 발생하면서 SIGABRT(macOS), 프로그램 종료(Windows) 가 발생한다.
    // 따라서 io_context 가 참조할 수 있는 포인터들(self, shared_ptr 들)을 만들어서 task lambda 한테
    // 전달해줘야 한다.
    // unique_ptr을 써도 되지만, std::move()를 계속 호출해줘야 해서 번거롭다.

    // 이걸로 RedisSession class 멤버필드에 접근 가능.
    // shared_from_this() 롤 사용하기 위해서는 RedisService.h 자체가 shared_ptr로 초기화 돼야 한다.
    auto self = shared_from_this();
    auto reqPtr = std::make_shared<boost::redis::request>();
    auto resPtr = std::make_shared<boost::redis::response<std::string>>();
    reqPtr->push("PING");

    Util::delayedExecutorAsyncByIoContext(
      *worker_io_context_pool[1], 0, [self, reqPtr, resPtr]()
      {
        std::cout << "Redis Ping Pong Test Start! \n";

        // exec redis cmd in async mode
        self->redis_conn.async_exec(
          *reqPtr,
          *resPtr,
          [self, resPtr](const boost::system::error_code& ec, std::size_t)
          {
            if (ec)
            {
              std::cerr << "Redis PING failed: " << ec.message() << "\n";
              return;
            }
            self->is_ready.store(true); // ping pong test 결과 기록
            std::cout << "PING: " << std::get<0>(*resPtr).value() << "\n";
          }
        );
      }
    );
  }


  void publishMsg(const std::string& msg)
  {
    // TODO : implement later
  }

  void startPubSubListening()
  {
    auto req_ptr = std::make_shared<boost::redis::request>();
    req_ptr->push("SUBSCRIBE", C::REDIS_PUB_SUB_CHANNEL);
    auto res_ptr = std::make_shared<boost::redis::response<std::string>>();

    redis_conn.async_exec(
      *req_ptr, *res_ptr, [this](const boost::system::error_code& ec, std::size_t)
      {
        if (ec)
        {
          logger->severe("Redis SUBSCRIBE failed: " + ec.message());
          return;
        }

        std::cout << "Subscribed! \n";
        receiveRedisMessage();
      }
    );
  }

  void shutdown()
  {
    is_ready.store(false);
    is_shutdown = true;
    redis_conn.cancel();
  }

private:
  void receiveRedisMessage()
  {
    /* 이 코드는 윈도우 환경(boost 1.86)에서는 빌드 됐지만, Mac 환경(boost 1.9x)에서는 빌드 되지 않았다.
     * 아무래도 테스트가 더 필요한 듯 하다.
     * 만약 라이브러리 버전에 따라서 서로 완전 다른 함수 시그니처를 가지고 있어서 동일한 코드로는 전혀 대응할 수 없을 때는
     * 버전 또는 OS에 따라서 별개의 소스코드로 컴파일되게 만드는 등의 작업이 필요할 수 있다.
     */

    // 이 버전은 일단 M1 Mac에서 컴파일은 되지만, 정상 작동하지는 않는다.
    auto response_ptr = std::make_shared<boost::redis::generic_response>();
    redis_conn.async_receive(
      [this, response_ptr](const boost::system::error_code& ec, std::size_t)
      {
        if (ec)
        {
          if (ec == boost::asio::error::operation_aborted)
          {
            return;
          }

          logger->severe("Redis receive failed: " + ec.message());
          return;
        }

        // do work with received msg
        std::cout << "Received Redis message! \n";
        std::cout << response_ptr->value()[0].value[0] << "\n";

        // TODO : implement later - 나중에 여기에 '팬들한테 답장 보내기' 기능 넣어야 한다.

        // wait for the next msg - if alive
        if (!is_shutdown)
        {
          receiveRedisMessage();
        }
      } //lambda
    ); //async_receive()
  }

  std::shared_ptr<Logger> logger;
  boost::asio::io_context& io_context;
  std::vector<std::shared_ptr<boost::asio::io_context>>& worker_io_context_pool;
  boost::redis::connection redis_conn;

  // used strand to reduce cache miss
  boost::asio::strand<boost::asio::io_context::executor_type> strand;

  std::atomic<bool> is_ready;
  bool is_shutdown = false;

  // TODO : 나중에 여기에는 각종 msg_tx 용 객체들이 정의돼야 한다.
  //  server socket 을 돌리는 Server 객체는 Session 을 만들 뿐, 응답을 전송하지는 않기 때문이다.
};

#endif //REDISMESSAGESUBSCRIBER_H
